/*
 * This file is part of Office Runner.
 *
 * Author: Bastien Nocera <hadess@hadess.net>
 *
 * SPDX-FileCopyrightText: 2011  Bastien Nocera
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Office Runner is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Office Runner is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Office Runner.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "config.h"

#include <gio/gunixfdlist.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <math.h>

#define LOGIND_DBUS_NAME                       "org.freedesktop.login1"
#define LOGIND_DBUS_PATH                       "/org/freedesktop/login1"
#define LOGIND_DBUS_INTERFACE                  "org.freedesktop.login1.Manager"

#define MAX_TIME 600.0
#define NUM_CUPS 3

#define WID(x) GTK_WIDGET(gtk_builder_get_object (run->ui, x))
#define IWID(x) GTK_IMAGE(gtk_builder_get_object (run->ui, x))
#define LWID(x) GTK_LABEL(gtk_builder_get_object (run->ui, x))

enum {
	RUN_PAGE,
	RUNNING_PAGE,
	SCORES_PAGE
};

enum {
	GOLD,
	SILVER,
	BRONZE
};

static const char *cups[] = {
	"gold",
	"silver",
	"bronze"
};

static gdouble cup_times[] = {
	MAX_TIME,
	MAX_TIME + 1,
	MAX_TIME + 2
};

static const char *cup_str[] = {
	N_("Gold Trophy!"),
	N_("Silver Trophy!"),
	N_("Bronze Trophy!")
};

typedef struct {
	gdouble time;
	char *date;
} ORecord;

typedef struct {
	GDBusConnection *connection;
	guint reenable_block_id;

	/* Lid switch */
	GCancellable *cancellable;
	guint upower_watch_id;
	GDBusProxy *upower_proxy;
	int lid_switch_fd;

	GtkBuilder *ui;
	GtkWidget *window;
	GtkWidget *run_button;
	GtkWidget *notebook;
	GtkWidget *time_label;
	GtkWidget *your_time_label;
	GtkIconSize large_icon_size;

	GList *records; /* of ORecords */
	gboolean dirty_records;

	GTimer *timer;
	guint timeout;
	gdouble elapsed;
} OfficeRunner;

static void switch_to_page (OfficeRunner *run, int page);
static void set_running_settings (OfficeRunner *run, gboolean running);

static char *
get_records_path (void)
{
	return g_build_filename (g_get_user_cache_dir (), GETTEXT_PACKAGE, "records.ini", NULL);
}

static char *
get_records_dir (void)
{
	return g_build_filename (g_get_user_cache_dir (), GETTEXT_PACKAGE, NULL);
}

static ORecord *
new_orecord (gdouble time)
{
	ORecord *o;
	o = g_new0 (ORecord, 1);
	o->time = time;
	return o;
}

static void
free_orecord (ORecord *o)
{
	g_free (o->date);
	g_free (o);
}

static void
save_records (OfficeRunner *run)
{
	GKeyFile *keyfile;
	GList *l;
	char *data, *path;
	GError *error = NULL;
	guint i;

	path = get_records_dir ();
	if (g_mkdir_with_parents (path, 0755) < 0) {
		g_warning ("Failed to create directory '%s'", path);
		g_free (path);
		return;
	}
	g_free (path);

	keyfile = g_key_file_new ();
	for (l = run->records, i = 0; l != NULL; l = l->next, i++) {
		ORecord *o = l->data;
		g_key_file_set_double (keyfile, cups[i], "time", o->time);
	}

	data = g_key_file_to_data (keyfile, NULL, NULL);
	g_key_file_free (keyfile);

	path = get_records_path ();

	if (g_file_set_contents (path, data, -1, &error) == FALSE) {
		g_warning ("Failed to save records to '%s': %s", path, error->message);
		g_error_free (error);
	}
	g_free (path);
	g_free (data);
}

static void
free_runner (OfficeRunner *run)
{
	if (run->cancellable) {
		g_cancellable_cancel (run->cancellable);
		g_object_unref (run->cancellable);
	}
	if (run->timer)
		g_timer_destroy (run->timer);
	if (run->timeout)
		gtk_widget_remove_tick_callback (run->time_label, run->timeout);
	g_object_unref (run->ui);
	if (run->reenable_block_id > 0)
		g_source_remove (run->reenable_block_id);
	if (run->upower_watch_id > 0)
		g_bus_unwatch_name (run->upower_watch_id);
	g_clear_object (&run->upower_proxy);
	if (run->lid_switch_fd > 0)
		close (run->lid_switch_fd);
	g_object_unref (run->connection);

	if (run->dirty_records) {
		save_records (run);
	}

	g_list_free_full (run->records, (GDestroyNotify) free_orecord);

	g_free (run);
}

static gboolean
reenable_block_timeout_cb (gpointer user_data)
{
	OfficeRunner *run = user_data;

	set_running_settings (run, TRUE);
	run->reenable_block_id = 0;
	return G_SOURCE_REMOVE;
}

static void
disable_block_timeout (OfficeRunner *run)
{
	if (run->reenable_block_id > 0) {
		g_source_remove (run->reenable_block_id);
		run->reenable_block_id = 0;
	}
}

static void
set_running_settings (OfficeRunner *run,
		      gboolean      running)
{
	if (running) {
		GVariant *ret;
		GUnixFDList *fd_list;
		gint idx;
		GError *error = NULL;

		if (run->lid_switch_fd != -1) {
			g_debug ("Already blocked");
			return;
		}
		g_debug ("Blocking lid action");

		ret = g_dbus_connection_call_with_unix_fd_list_sync (run->connection,
								     LOGIND_DBUS_NAME,
								     LOGIND_DBUS_PATH,
								     LOGIND_DBUS_INTERFACE,
								     "Inhibit",
								     g_variant_new ("(ssss)", "handle-lid-switch", g_get_user_name(), _("Office Runner is running"), "block"),
								     NULL,
								     G_DBUS_CALL_FLAGS_NONE,
								     -1,
								     NULL,
								     &fd_list,
								     NULL,
								     &error);

		if (ret == NULL)
			g_error ("Failed to inhibit: %s", error->message);
		g_variant_get (ret, "(h)", &idx);
		run->lid_switch_fd = g_unix_fd_list_get (fd_list, idx, NULL);

		g_object_unref (fd_list);
		g_variant_unref (ret);
	} else {
		if (run->lid_switch_fd != -1) {
			close (run->lid_switch_fd);
			run->lid_switch_fd = -1;

			g_debug ("Unblocking lid action");
		} else {
			g_debug ("Already released the blocking inhibitor");
		}
	}
}

static gboolean
window_delete_event_cb (GtkWidget    *widget,
			GdkEvent     *event,
			OfficeRunner *run)
{
	gtk_main_quit ();
	return FALSE;
}

static void
load_default_records (OfficeRunner *run)
{
	GList *l;

	l = g_list_prepend (NULL, new_orecord (cup_times[BRONZE]));
	l = g_list_prepend (l, new_orecord (cup_times[SILVER]));
	l = g_list_prepend (l, new_orecord (cup_times[GOLD]));

	run->records = l;
}

static void
load_records (OfficeRunner *run)
{
	GKeyFile *keyfile;
	char *path;
	guint i;

	path = get_records_path ();
	keyfile = g_key_file_new ();
	if (g_key_file_load_from_file (keyfile, path, G_KEY_FILE_NONE, NULL) == FALSE) {
		g_key_file_free (keyfile);
		g_free (path);
		load_default_records (run);
		return;
	}
	g_free (path);

	run->records = NULL;
	for (i = GOLD; i <= BRONZE; i++) {
		gdouble time;

		time = g_key_file_get_double (keyfile, cups[i], "time", NULL);
		run->records = g_list_prepend (run->records,
					       new_orecord (time ? time : cup_times[i]));
	}
	run->records = g_list_reverse (run->records);
	g_key_file_free (keyfile);
}

static char *
elapsed_to_countdown (gdouble elapsed)
{
	int seconds;
	char *label;

	seconds = floorl (elapsed);
	elapsed = (elapsed - (gdouble) seconds) * 100;

	label = g_strdup_printf ("%d:%02d.%02d",
				 seconds / 60,
				 seconds % 60,
				 (int) elapsed);

	return label;
}

static char *
elapsed_to_text (gdouble elapsed)
{
	int seconds;
	char *label;

	seconds = floorl (elapsed);
	elapsed = (elapsed - (gdouble) seconds) * 100;

	label = g_strdup_printf (_("%d.%02d seconds"),
				 seconds,
				 (int) elapsed);

	return label;
}

static int
find_time (ORecord *o,
	   gdouble *time)
{
	if (o->time == *time)
		return 0;
	return -1;
}

static char *
time_to_better_time_text (OfficeRunner *run)
{
	GList *l;
	ORecord *o;

	g_debug ("Looking for %lf in better times", run->elapsed);

	l = g_list_find_custom (run->records, &run->elapsed, (GCompareFunc) find_time);
	g_assert (l);

	l = l->prev;
	o = l->data;
	return elapsed_to_text (run->elapsed - o->time);
}

static gboolean
count_tick (GtkWidget     *time_label,
	    GdkFrameClock *clock,
	    gpointer       user_data)
{
	OfficeRunner *run = user_data;
	gdouble elapsed;
	char *label;

	if (run->timer != NULL) {
		run->elapsed = g_timer_elapsed (run->timer, NULL);
		elapsed = run->elapsed;
		if (run->elapsed >= MAX_TIME) {
			switch_to_page (run, SCORES_PAGE);
			return FALSE;
		}
	} else {
		elapsed = 0.0;
	}

	label = elapsed_to_countdown (elapsed);
	gtk_label_set_text (GTK_LABEL (run->time_label), label);
	g_free (label);

	return G_SOURCE_CONTINUE;
}

static int
record_compare_func (ORecord *a,
		     ORecord *b)
{
	return (a->time > b->time);
}

static gboolean
is_new_record (OfficeRunner *run,
	       int          *new_pos)
{
	ORecord *o;
	guint i;
	gboolean new_record;
	GList *l;

#if 0
	GDateTime *dt;
	GTimeZone *tz;
	char *date;
#endif

	new_record = FALSE;

#if 0
	/* Unused */
	tz = g_time_zone_new_local ();
	dt = g_date_time_new_now (tz);
	date = g_date_time_format (dt, "%c");
	g_date_time_unref (dt);
	g_time_zone_unref (tz);
	g_free (date);
#endif
	o = new_orecord (run->elapsed);
	run->records = g_list_insert_sorted (run->records, o, (GCompareFunc) record_compare_func);
	g_debug ("Elapsed: %lf", o->time);
	for (l = run->records, i = GOLD; l != NULL; l = l->next, i++)
		g_debug ("\t%d = %lf", i, ((ORecord *) l->data)->time);

	*new_pos = 0;
	for (l = run->records, i = GOLD; i <= BRONZE; l = l->next, i++) {
		ORecord *o = l->data;
		if (run->elapsed == o->time && i <= BRONZE) {
			new_record = TRUE;
			*new_pos = i;
			break;
		}
	}

	if (new_record == FALSE)
		return FALSE;

	run->dirty_records = TRUE;

	return TRUE;
}

static void
trim_records_list (OfficeRunner *run)
{
	GList *l;

	l = g_list_nth (run->records, BRONZE + 1);
	if (l) {
		l->prev->next = NULL;
		g_list_free_full (l, (GDestroyNotify) free_orecord);
	}
}

static void
set_records_page (OfficeRunner *run)
{
	const char *text;
	char *cur_time, *time_text;
	char *better_time_text;
	gboolean new_record;
	int cup;

	if (run->elapsed >= MAX_TIME) {
		text = _("Took too long, sorry!");
		gtk_label_set_text (LWID ("result_label"), text);
		gtk_widget_hide (WID ("current_time_label"));
		gtk_widget_hide (WID ("better_time_label"));
		return;
	}

	time_text = elapsed_to_text (run->elapsed);
	better_time_text = NULL;
	new_record = is_new_record (run, &cup);
	if (new_record) {
		char *str, *better_time;

		text = _(cup_str[cup]);
		str = g_strdup_printf ("trophy-%s", cups[cup]);
		gtk_image_set_from_icon_name (IWID ("trophy_image"),
					      str, run->large_icon_size);
		g_free (str);

		switch (cup) {
		case GOLD:
			cur_time = g_strdup_printf (_("You managed to finish the route with the best time ever, <b>%s</b>."), time_text);
			break;
		case SILVER:
			cur_time = g_strdup_printf (_("You managed to finish the route with the 2<span rise=\"2048\">nd</span> best time ever, <b>%s</b>."), time_text);
			better_time = time_to_better_time_text (run);
			better_time_text = g_strdup_printf (_("Only <b>%s</b> separate you from the gold trophy!"), better_time);
			g_free (better_time);
			break;
		case BRONZE:
			cur_time = g_strdup_printf (_("You managed to finish the route with the 3<span rise=\"2048\">rd</span> best time ever, <b>%s</b>."), time_text);
			better_time = time_to_better_time_text (run);
			better_time_text = g_strdup_printf (_("Only <b>%s</b> separate you from the silver trophy!"), better_time);
			g_free (better_time);
			break;
		}
	} else {
		char *better_time;

		gtk_image_set_from_icon_name (IWID ("trophy_image"), "face-uncertain", run->large_icon_size);

		text = _("Too slow for the podium");
		cur_time = g_strdup_printf (_("You managed to finish the route in <b>%s</b>."),
					    time_text);
		better_time = time_to_better_time_text (run);
		better_time_text = g_strdup_printf (_("Only <b>%s</b> separate you from the bronze trophy!"), better_time);;
		g_free (better_time);
	}
	gtk_label_set_text (LWID ("result_label"), text);

	gtk_widget_show (WID ("current_time_label"));
	gtk_label_set_markup (LWID ("current_time_label"), cur_time);
	g_free (cur_time);

	if (better_time_text) {
		gtk_widget_show (WID ("better_time_label"));
		gtk_label_set_markup (LWID ("better_time_label"), better_time_text);
		g_free (better_time_text);
	} else {
		gtk_widget_hide (WID ("better_time_label"));
	}

	trim_records_list (run);
}

static void
switch_to_page (OfficeRunner *run,
		int           page)
{
	gtk_notebook_set_current_page (GTK_NOTEBOOK (run->notebook), page);

	switch (page) {
	case RUN_PAGE:
		set_running_settings (run, TRUE);
		gtk_label_set_text (GTK_LABEL (WID ("run_button_label")), _("Run!"));
		break;
	case RUNNING_PAGE: {
		set_running_settings (run, TRUE);
		disable_block_timeout (run);
		run->timer = g_timer_new ();
		run->timeout = gtk_widget_add_tick_callback (run->time_label, count_tick, run, NULL);
		gtk_label_set_text (GTK_LABEL (WID ("run_button_label")), _("Done!"));
		break;
			   }
	case SCORES_PAGE: {
		run->elapsed = g_timer_elapsed (run->timer, NULL);
		g_timer_destroy (run->timer);
		run->timer = NULL;

		gtk_widget_remove_tick_callback (run->time_label, run->timeout);
		run->timeout = 0;

		gtk_label_set_text (GTK_LABEL (WID ("run_button_label")), _("Try Again"));
		set_records_page (run);

		/* This should be enough time for the machine to go to sleep */
		set_running_settings (run, FALSE);
		run->reenable_block_id = g_timeout_add_seconds (3, reenable_block_timeout_cb, run);

		break;
			  }
	}
}

static void
run_button_clicked_cb (GtkWidget    *button,
		       OfficeRunner *run)
{
	int page;

	page = gtk_notebook_get_current_page (GTK_NOTEBOOK (run->notebook));
	page++;
	if (page > SCORES_PAGE)
		page = RUNNING_PAGE;
	switch_to_page (run, page);
}

static void
lid_is_closed_cb (GDBusProxy *proxy,
		  GVariant   *changed_properties,
		  GStrv       invalidated_properties,
		  gpointer    user_data)
{
	OfficeRunner *run = user_data;
	int page;
	GVariant *v;
	gboolean lid_is_closed;

	v = g_variant_lookup_value (changed_properties,
				    "LidIsClosed",
				    G_VARIANT_TYPE_BOOLEAN);
	if (!v)
		return;

	lid_is_closed = g_variant_get_boolean (v);
	g_variant_unref (v);

	page = gtk_notebook_get_current_page (GTK_NOTEBOOK (run->notebook));

	switch (page) {
	case RUN_PAGE:
		if (lid_is_closed) {
			g_debug ("Switching from run page to running page (lid closed)");
			run_button_clicked_cb (NULL, run);
		}
		break;
	case RUNNING_PAGE:
		if (!lid_is_closed) {
			g_debug ("Switching from running page to scores page (lid open)");
			run_button_clicked_cb (NULL, run);
		}
		break;
	case SCORES_PAGE:
		if (lid_is_closed && run->lid_switch_fd != -1) {
			g_debug ("Switching from scores page to running page");
			run_button_clicked_cb (NULL, run);
		}
		break;
	}
}

static void
upower_ready_cb (GObject      *source_object,
		 GAsyncResult *res,
		 gpointer      user_data)
{
	OfficeRunner *run;
	GDBusProxy *proxy;
	GError *error = NULL;
	GVariant *v;

	proxy = g_dbus_proxy_new_finish (res, &error);
	if (!proxy) {
		if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
			g_warning ("Failed to create UPower proxy: %s", error->message);
		g_error_free (error);
		return;
	}

	run = user_data;
	run->upower_proxy = proxy;

	g_signal_connect (proxy, "g-properties-changed",
			  G_CALLBACK (lid_is_closed_cb), run);
}

static void
upower_appeared (GDBusConnection *connection,
		 const gchar     *name,
		 const gchar     *name_owner,
		 gpointer         user_data)
{
	OfficeRunner *run = user_data;

	g_dbus_proxy_new (connection,
			  G_DBUS_PROXY_FLAGS_NONE,
			  NULL,
			  "org.freedesktop.UPower",
			  "/org/freedesktop/UPower",
			  "org.freedesktop.UPower",
			  run->cancellable,
			  upower_ready_cb,
			  run);
}

static void
upower_vanished (GDBusConnection *connection,
		 const gchar     *name,
		 gpointer         user_data)
{
	OfficeRunner *run = user_data;

	g_clear_object (&run->upower_proxy);
}

static OfficeRunner *
new_runner (void)
{
	OfficeRunner *run;

	run = g_new0 (OfficeRunner, 1);
	run->connection = g_bus_get_sync (G_BUS_TYPE_SYSTEM,
					  NULL, NULL);
	run->lid_switch_fd = -1;
	run->upower_watch_id = g_bus_watch_name (G_BUS_TYPE_SYSTEM,
						 "org.freedesktop.UPower",
						 G_BUS_NAME_WATCHER_FLAGS_NONE,
						 upower_appeared,
						 upower_vanished,
						 run,
						 NULL);

	run->ui = gtk_builder_new ();
	gtk_builder_add_from_file (run->ui, PKGDATADIR "/office-runner.ui", NULL);
	run->window = WID ("window1");
	run->time_label = WID ("time_label");
	count_tick (NULL, NULL, run);
	run->your_time_label = WID ("your_time_label");

	/* FIXME: No running man for now */
	gtk_widget_set_no_show_all (WID ("run_image"), TRUE);
	gtk_widget_hide (WID ("run_image"));
	gtk_widget_set_no_show_all (WID ("time_image"), TRUE);
	gtk_widget_hide (WID ("time_image"));

	run->large_icon_size = gtk_icon_size_register ("large", 256, 256);
	gtk_image_set_from_icon_name (IWID("trophy_image"), "trophy-silver", run->large_icon_size);

	g_signal_connect (run->window, "delete-event",
			  G_CALLBACK (window_delete_event_cb), run);
	run->run_button = WID ("run_button");
	g_signal_connect (run->run_button, "clicked",
			  G_CALLBACK (run_button_clicked_cb), run);
	run->notebook = WID ("notebook1");

	load_records (run);

	/* Start the blocking here */
	switch_to_page (run, RUN_PAGE);

	return run;
}

int main (int argc, char **argv)
{
	OfficeRunner *run;

	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	gtk_init (&argc, &argv);

	run = new_runner ();
	gtk_widget_show_all (run->window);

	gtk_main ();

	free_runner (run);

	return 0;
}
