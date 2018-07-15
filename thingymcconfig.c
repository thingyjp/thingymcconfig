#define GETTEXT_PACKAGE "gtk20"
#include <glib.h>
#include <glib-unix.h>
#include "buildconfig.h"
#include "config.h"
#include "ctrl.h"
#include "network.h"
#include "http.h"
#include "apps.h"

static GMainLoop* mainloop;

gboolean siginthandler(gpointer user_data) {
	g_message("terminating...");
	g_main_loop_quit(mainloop);
	return TRUE;
}

int main(int argc, char** argv) {

	gchar* nameprefix = "thingy";
	gchar* interface = NULL;
	gchar** apps = NULL;
	gboolean waitforinterface = FALSE;
	gboolean nonetwork = FALSE;
	gboolean noap = FALSE;
	gchar* cert = NULL;
	gchar* key = NULL;
	int ret = 0;

	GError* error = NULL;
	GOptionEntry entries[] = {
	// ?display? stuff
			{ "nameprefix", 'n', 0, G_OPTION_ARG_STRING, &nameprefix,
					"name prefix", NULL },
			// networking stuff
			{ "interface", 'i', 0, G_OPTION_ARG_STRING, &interface, "interface",
			NULL }, { "waitforinterface", 'w', 0, G_OPTION_ARG_NONE,
					&waitforinterface, "wait for interface to appear", NULL },
			// apps
			{ "app", 'a', 0, G_OPTION_ARG_STRING_ARRAY, &apps,
					"register an app", NULL },
			// crypto options
			{ "cert", 'c', 0, G_OPTION_ARG_STRING, &cert, "device certificate",
			NULL }, { "key", 'k', 0, G_OPTION_ARG_STRING, &key, "private key",
			NULL },
#ifdef DEVELOPMENT
			{ "nonetwork", 0, 0, G_OPTION_ARG_NONE, &nonetwork,
					"no networking, for local testing", NULL }, { "noap", 0, 0,
					G_OPTION_ARG_NONE, &noap,
					"don't create an ap, only useful for development", NULL },
#endif
			{ NULL } };
	GOptionContext* optioncontext = g_option_context_new(NULL);
	g_option_context_add_main_entries(optioncontext, entries, GETTEXT_PACKAGE);
	if (!g_option_context_parse(optioncontext, &argc, &argv, &error)) {
		g_print("option parsing failed: %s\n", error->message);
		ret = 1;
		goto err_args;
	}

	if (interface == NULL) {
		g_message("interface not specified");
		ret = 1;
		goto err_args;
	}

#if 0
	if (cert == NULL || key == NULL) {
		g_message("device certificate or private key not specified");
		ret = 1;
		goto err_args;
	}
#endif

	mainloop = g_main_loop_new(NULL, FALSE);

	config_init();
	ctrl_init();
	apps_init((const gchar**) apps);

	if (!nonetwork) {
		network_init(interface, noap);

		if (waitforinterface && network_waitforinterface()) {
			ret = 1;
			goto err_network_waitforinterface;
		}

		if (network_start()) {
			g_message("failed to start networking");
			ret = 1;
			goto err_network_start;
		}

	}

	if (http_start()) {
		g_message("failed to start http");
		ret = 1;
		goto err_http_start;
	}

	ctrl_start();

	//todo should only be called when entering provisioning mode
	if (!nonetwork)
		network_startap(nameprefix);

	g_unix_signal_add(SIGINT, siginthandler, NULL);
	g_main_loop_run(mainloop);

	ctrl_stop();

	http_stop();
	err_http_start: //
	if (!nonetwork)
		network_stop();
	err_network_waitforinterface: //
	err_network_start: //
	err_args: //
	return ret;
}
