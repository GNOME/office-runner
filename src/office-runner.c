#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <gnome-settings-daemon/gsd-enums.h>
#include <math.h>

#define POWER_SETTINGS "org.gnome.settings-daemon.plugins.power"
#define AC_ACTION "lid-close-ac-action"
#define BATTERY_ACTION "lid-close-battery-action"
#define MAX_TIME 600.0

#define WID(x) GTK_WIDGET(gtk_builder_get_object (run->ui, x))
#define IWID(x) GTK_IMAGE(gtk_builder_get_object (run->ui, x))

enum {
	RUN_PAGE,
	RUNNING_PAGE,
	SCORES_PAGE
};

typedef struct {
	GSettings *settings;
	GsdPowerActionType ac_action;
	GsdPowerActionType battery_action;

	GtkBuilder *ui;
	GtkWidget *window;
	GtkWidget *run_button;
	GtkWidget *notebook;
	GtkWidget *time_label;

	GTimer *timer;
	guint timeout;
} OfficeRunner;

static void
free_runner (OfficeRunner *run)
{
	if (run->timer)
		g_timer_destroy (run->timer);
	if (run->timeout)
		g_source_remove (run->timeout);
	g_object_unref (run->settings);
	g_object_unref (run->ui);
	g_free (run);
}

static void
set_running_settings (OfficeRunner *run,
		      gboolean      running)
{
	if (running) {
		g_settings_set_enum (run->settings, AC_ACTION, GSD_POWER_ACTION_NOTHING);
		g_settings_set_enum (run->settings, BATTERY_ACTION, GSD_POWER_ACTION_NOTHING);
	} else {
		g_settings_set_enum (run->settings, AC_ACTION, run->ac_action);
		g_settings_set_enum (run->settings, BATTERY_ACTION, run->battery_action);
	}
}

static gboolean
window_delete_event_cb (GtkWidget    *widget,
			GdkEvent     *event,
			OfficeRunner *run)
{
	gtk_main_quit ();
}

static gboolean
count_timeout (OfficeRunner *run)
{
	gdouble elapsed;
	int seconds;
	char *label;

	if (run->timer != NULL) {
		elapsed = g_timer_elapsed (run->timer, NULL);
		if (elapsed >= MAX_TIME) {
			//FIXME bail
		}
	} else {
		elapsed = 0.0;
	}

	seconds = floorl (elapsed);
	elapsed = (elapsed - (gdouble) seconds) * 100;

	label = g_strdup_printf ("%d:%02d.%02d",
				 seconds / 60,
				 seconds % 60,
				 (int) elapsed);
	gtk_label_set_text (GTK_LABEL (run->time_label), label);
	g_free (label);

	return TRUE;
}

static void
switch_to_page (OfficeRunner *run,
		int           page)
{
	const char *label;

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
	case SCORES_PAGE:
		set_running_settings (run, FALSE);
		g_source_remove (run->timeout);
		label = N_("Try Again");
		break;
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
		page = RUN_PAGE;
	switch_to_page (run, page);
}

static OfficeRunner *
new_runner (void)
{
	OfficeRunner *run;

	run = g_new0 (OfficeRunner, 1);
	run->settings = g_settings_new (POWER_SETTINGS);
	run->ac_action = g_settings_get_enum (run->settings, AC_ACTION);
	run->battery_action = g_settings_get_enum (run->settings, BATTERY_ACTION);

	run->ui = gtk_builder_new ();
	gtk_builder_add_from_file (run->ui, "office-runner.ui", NULL);
	run->window = WID ("window1");
	run->time_label = WID ("time_label");
	count_timeout (run);

	/* FIXME: No running man for now */
	gtk_widget_set_no_show_all (WID ("run_image"), TRUE);
	gtk_widget_hide (WID ("run_image"));
	gtk_widget_set_no_show_all (WID ("time_image"), TRUE);
	gtk_widget_hide (WID ("time_image"));

	gtk_image_set_from_file (IWID("gold_image"), "gold-cup.png");
	gtk_image_set_from_file (IWID("silver_image"), "silver-cup.png");
	gtk_image_set_from_file (IWID("bronze_image"), "bronze-cup.png");

	g_signal_connect (run->window, "delete-event",
			  G_CALLBACK (window_delete_event_cb), run);
	run->run_button = WID ("run_button");
	g_signal_connect (run->run_button, "clicked",
			  G_CALLBACK (run_button_clicked_cb), run);
	run->notebook = WID ("notebook1");

	return run;
}

int main (int argc, char **argv)
{
	GSettings *settings;
	OfficeRunner *run;

	gtk_init (&argc, &argv);

	run = new_runner ();
	gtk_widget_show_all (run->window);

	gtk_main ();

	free_runner (run);

	return 0;
}
