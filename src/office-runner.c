/*
 * This file is part of Office Runner.
 *
 * Author: Bastien Nocera <hadess@hadess.net>
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

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <gnome-settings-daemon/gsd-enums.h>
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
	guint i;

	if (run->timer)
		g_timer_destroy (run->timer);
	if (run->timeout)
		g_source_remove (run->timeout);
	g_object_unref (run->ui);
	if (run->lid_switch_fd > 0)
		close (run->lid_switch_fd);
	g_object_unref (run->connection);

	if (run->dirty_records) {
		save_records (run);
	}

	g_list_free_full (run->records, (GDestroyNotify) free_orecord);

	g_free (run);
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

		g_assert (run->lid_switch_fd == -1);

		ret = g_dbus_connection_call_with_unix_fd_list_sync (run->connection,
								     LOGIND_DBUS_NAME,
								     LOGIND_DBUS_PATH,
								     LOGIND_DBUS_INTERFACE,
								     "Inhibit",
								     g_variant_new ("(ssss)", "handle-lid-switch", g_get_user_name(), _("Running!"), "block"),
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
		g_assert (run->lid_switch_fd > 0);

		close (run->lid_switch_fd);
		run->lid_switch_fd = -1;
	}
}

static gboolean
window_delete_event_cb (GtkWidget    *widget,
			GdkEvent     *event,
			OfficeRunner *run)
{
	gtk_main_quit ();
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
elapsed_to_text (gdouble elapsed)
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

static gboolean
count_timeout (OfficeRunner *run)
{
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

	label = elapsed_to_text (elapsed);
	gtk_label_set_text (GTK_LABEL (run->time_label), label);
	g_free (label);

	return TRUE;
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
	guint i, cup;
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
	g_message ("Elapsed: %lf", o->time);
	for (l = run->records, i = GOLD; l != NULL; l = l->next, i++)
		g_message ("\t%d = %lf", i, ((ORecord *) l->data)->time);

	*new_pos = 0;
	for (l = run->records, i = GOLD; i <= BRONZE; l = l->next, i++) {
		ORecord *o = l->data;
		if (run->elapsed == o->time && i <= BRONZE) {
			new_record = TRUE;
			*new_pos = i;
			break;
		}
	}

	/* Trim the list */
	l = g_list_nth (run->records, BRONZE + 1);
	if (l) {
		l->prev->next = NULL;
		g_list_free_full (l, (GDestroyNotify) free_orecord);
	}

	if (new_record == FALSE)
		return FALSE;

	run->dirty_records = TRUE;

	return TRUE;
}

static void
set_records_page (OfficeRunner *run)
{
	const char *text;
	char *cur_time, *time_text;
	gboolean new_record;
	int cup;

	if (run->elapsed >= MAX_TIME) {
		text = _("Took too long, sorry!");
	} else {
		new_record = is_new_record (run, &cup);
		if (new_record) {
			char *str;
			text = _(cup_str[cup]);
			str = g_strdup_printf ("trophy-%s", cups[cup]);
			gtk_image_set_from_icon_name (IWID ("trophy_image"),
						      str, run->large_icon_size);
			g_free (str);
		} else
			text = _("Too slow for the podium");
	}
	gtk_label_set_text (LWID ("result_label"), text);

	time_text = elapsed_to_text (run->elapsed);
	cur_time = g_strdup_printf (_("You managed to finish the route in <b>%s</b>."),
				time_text);
	gtk_label_set_markup (LWID ("current_time_label"), cur_time);
	g_free (cur_time);

	/* FIXME: change the labels to match the mockups */
}

static void
switch_to_page (OfficeRunner *run,
		int           page)
{
	const char *label = NULL;

	gtk_notebook_set_current_page (GTK_NOTEBOOK (run->notebook), page);

	switch (page) {
	case RUN_PAGE:
		label = N_("Run!");
		break;
	case RUNNING_PAGE: {
		set_running_settings (run, TRUE);
		run->timer = g_timer_new ();
		label = N_("Done!");
		run->timeout = g_timeout_add (80, (GSourceFunc) count_timeout, run);
		count_timeout (run);
		break;
			   }
	case SCORES_PAGE: {
		run->elapsed = g_timer_elapsed (run->timer, NULL);
		g_timer_destroy (run->timer);
		run->timer = NULL;

		set_running_settings (run, FALSE);
		g_source_remove (run->timeout);
		run->timeout = 0;

		label = N_("Try Again");

		set_records_page (run);

		break;
			  }
	}

	gtk_label_set_text (GTK_LABEL (WID ("run_button_label")),
			    label);
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

static OfficeRunner *
new_runner (void)
{
	OfficeRunner *run;

	run = g_new0 (OfficeRunner, 1);
	run->connection = g_bus_get_sync (G_BUS_TYPE_SYSTEM,
					  NULL, NULL);
	run->lid_switch_fd = -1;

	run->ui = gtk_builder_new ();
	gtk_builder_add_from_file (run->ui, PKGDATADIR "office-runner.ui", NULL);
	run->window = WID ("window1");
	run->time_label = WID ("time_label");
	count_timeout (run);
	run->your_time_label = WID ("your_time_label");

	/* FIXME: No running man for now */
	gtk_widget_set_no_show_all (WID ("run_image"), TRUE);
	gtk_widget_hide (WID ("run_image"));
	gtk_widget_set_no_show_all (WID ("time_image"), TRUE);
	gtk_widget_hide (WID ("time_image"));

	run->large_icon_size = gtk_icon_size_register ("large", 256, 256);
	gtk_image_set_from_icon_name (IWID("trophy_image"), "trophy-silver", run->large_icon_size);

	/* FIXME: Add "better time" advice */
	gtk_widget_set_no_show_all (WID ("better_time_label"), TRUE);
	gtk_widget_hide (WID ("better_time_label"));

	g_signal_connect (run->window, "delete-event",
			  G_CALLBACK (window_delete_event_cb), run);
	run->run_button = WID ("run_button");
	g_signal_connect (run->run_button, "clicked",
			  G_CALLBACK (run_button_clicked_cb), run);
	run->notebook = WID ("notebook1");

	load_records (run);

	return run;
}

int main (int argc, char **argv)
{
	GSettings *settings;
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
