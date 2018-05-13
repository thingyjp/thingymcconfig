#include <glib.h>
#include "network.h"
#include "http.h"

int main(int argc, char** argv) {

	GMainLoop* mainloop = g_main_loop_new(NULL, FALSE);

	network_init();
	network_start();
	http_start();

	g_main_loop_run(mainloop);

	return 0;
}
