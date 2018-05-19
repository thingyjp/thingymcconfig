#include <json-glib/json-glib.h>
#include "config.h"
#include "utils.h"

static char* cfgpath = "./config.json";
static struct config cfg;

static void config_save() {
	JsonBuilder* jsonbuilder = json_builder_new();
	json_builder_begin_object(jsonbuilder);

	json_builder_end_array(jsonbuilder);

	gsize jsonsz;
	gchar* json = utils_jsonbuildertostring(jsonbuilder, &jsonsz);
	g_file_set_contents(cfgpath, json, jsonsz, NULL);
	g_free(json);
}

void config_init() {
	gchar* cfgjson;
	gsize cfgsz;
	gboolean cfgvalid = FALSE;
	if (g_file_get_contents(cfgpath, &cfgjson, &cfgsz, NULL)) {

	}

	if (!cfgvalid)
		g_message("config doesn't exist or is invalid");
}
