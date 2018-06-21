#define GETTEXT_PACKAGE "gtk20"
#include <glib.h>
#include "buildconfig.h"
#include "config.h"
#include "network.h"
#include "http.h"

int main(int argc, char** argv) {

	gchar* interface = NULL;
	gboolean noap = FALSE;

	GError* error;
	GOptionEntry entries[] = { { "interface", 'i', 0, G_OPTION_ARG_STRING,
			&interface, "interface", NULL },
#ifdef DEVELOPMENT
			{ "noap", 'n', 0, G_OPTION_ARG_NONE, &noap,
					"don't create an ap, only useful for development", NULL },
#endif
			{ NULL } };
	GOptionContext* optioncontext = g_option_context_new(NULL);
	g_option_context_add_main_entries(optioncontext, entries, GETTEXT_PACKAGE);
	if (!g_option_context_parse(optioncontext, &argc, &argv, &error)) {
		g_print("option parsing failed: %s\n", error->message);
		goto out;
	}

	if (interface == NULL) {
		g_message("interface not specified");
		goto out;
	}

	GMainLoop* mainloop = g_main_loop_new(NULL, FALSE);

	config_init();
	network_init(interface, noap);

	network_start();
	http_start();

	//todo should only be called when entering provisioning mode
	network_startap();

	g_main_loop_run(mainloop);

	http_stop();
	network_stop();

	out: return 0;
}
