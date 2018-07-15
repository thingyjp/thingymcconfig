#include "apps.h"

static GPtrArray* apps;

struct apps_app {
	unsigned index;
	const gchar* name;
};

static void apps_printapp(gpointer data, gpointer user_data) {
	const struct apps_app* app = data;
	g_message("index: %u, name: %s", app->index, app->name);
}

void apps_init(const gchar** appnames) {
	apps = g_ptr_array_new();

	unsigned numapps = 0;
	if (appnames != NULL) {
		while (*appnames != NULL) {
			g_message("found app %s", *appnames);
			struct apps_app* app = g_malloc0(sizeof(*app));
			app->index = numapps++;
			app->name = *appnames++;
			g_ptr_array_add(apps, app);
		}
	}
	if (numapps == 0)
		g_message("no apps are registered");
	else {
		g_message("%d apps are registered", (int ) apps->len);
		g_ptr_array_foreach(apps, apps_printapp, NULL);
	}
}

void apps_onappstateupdate(const struct apps_appstateupdate* update) {

}

static void apps_dumpapp(gpointer data, gpointer user_data) {
	const struct apps_app* app = data;
	JsonBuilder* builder = user_data;
	json_builder_begin_object(builder);
	json_builder_set_member_name(builder, "name");
	json_builder_add_string_value(builder, app->name);
	json_builder_end_object(builder);
}

void apps_dumpstatus(JsonBuilder* builder) {
	json_builder_set_member_name(builder, "apps");
	json_builder_begin_array(builder);
	g_ptr_array_foreach(apps, apps_dumpapp, builder);
	json_builder_end_array(builder);
}
