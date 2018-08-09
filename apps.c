#include "apps.h"
#include "jsonbuilderutils.h"
#include "tbus.h"

static GPtrArray* apps;

struct apps_app_state {
	unsigned char appstate;
	unsigned char apperror;
	unsigned char connectivity;
	unsigned char connectivityerror;
};

struct apps_app {
	guint index;
	const gchar* name;
	struct apps_app_state state;
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
			app->index = 1 + numapps++;
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

static struct apps_app* apps_findappbyindex(guint index) {
	int arridx = index - 1;
	if (arridx >= apps->len)
		return NULL;
	struct apps_app* appstate = g_ptr_array_index(apps, arridx);
	return appstate;
}

gboolean apps_onappstateupdate(const struct apps_appstateupdate* update) {
	struct apps_app* appstate = apps_findappbyindex(update->appindex);
	if (appstate == NULL) {
		g_message("bad app index %d", (int )update->appindex);
		goto err;
	}
	g_assert(appstate->index == 0 || appstate->index == update->appindex);

	if (update->appstate != 0) {
		appstate->state.appstate = update->appstate;
		appstate->state.apperror = update->apperror;
		g_message("updated app status for %s", appstate->name);
	}

	if (update->connectivitystate != 0) {
		appstate->state.connectivity = update->connectivitystate;
		appstate->state.connectivityerror = update->connectivityerror;
		g_message("updated connectivity status for %s", appstate->name);
	}

	return TRUE;

	err: //
	return FALSE;
}

void apps_onappdisconnected(guint index) {
	struct apps_app* appstate = apps_findappbyindex(index);
	if (appstate != NULL) {
		g_message("app %s disconnected, clearing state", appstate->name);
		memset(&appstate->state, 0, sizeof(appstate->state));
	}
}

static const gchar* statestrings[] = { "UNKNOWN", "OK", "ERROR", "SEECODE" };

#define FIELD_NAME "name"
#define OBJECT_APPSTATE "appstate"
#define OBJECT_CONNECTIVITY "connectivity"
#define FIELD_STATE "state"
#define FIELD_CODE "code"
#define FIELD_ERROR "err"

static void apps_dumpapp(gpointer data, gpointer user_data) {
	const struct apps_app* app = data;
	JsonBuilder* builder = user_data;
	json_builder_begin_object(builder);
	JSONBUILDER_ADD_STRING(builder, FIELD_NAME, app->name);

	JSONBUILDER_START_OBJECT(builder, OBJECT_APPSTATE);
	JSONBUILDER_ADD_STRING(builder, FIELD_STATE,
			statestrings[MIN(app->state.appstate, G_N_ELEMENTS(statestrings))]);
	JSONBUILDER_ADD_INT(builder, FIELD_CODE, app->state.appstate);
	if (app->state.appstate == THINGYMCCONFIG_ERR)
		JSONBUILDER_ADD_INT(builder, FIELD_ERROR, app->state.appstate);
	json_builder_end_object(builder);

	JSONBUILDER_START_OBJECT(builder, OBJECT_CONNECTIVITY);
	JSONBUILDER_ADD_STRING(builder, FIELD_STATE,
			statestrings[MIN(app->state.connectivity, G_N_ELEMENTS(statestrings))]);
	JSONBUILDER_ADD_INT(builder, FIELD_CODE, app->state.connectivity);
	if (app->state.connectivity == THINGYMCCONFIG_ERR)
		JSONBUILDER_ADD_INT(builder, FIELD_ERROR, app->state.connectivityerror);
	json_builder_end_object(builder);

	json_builder_end_object(builder);
}

void apps_dumpstatus(JsonBuilder* builder) {
	json_builder_set_member_name(builder, "apps");
	json_builder_begin_array(builder);
	g_ptr_array_foreach(apps, apps_dumpapp, builder);
	json_builder_end_array(builder);
}

gboolean apps_ctrl_sendconfig(GOutputStream* os) {
	struct tbus_fieldandbuff* fields = g_malloc0(sizeof(*fields) * apps->len);
	for (int i = 0; i < apps->len; i++) {
		struct tbus_fieldandbuff* f = fields + i;
		struct apps_app* appstate = g_ptr_array_index(apps, i);
		f->field.index.type = THINGMCCONFIG_FIELDTYPE_CONFIG_APPS_APP;
		f->field.index.index = appstate->index;
		f->buff = appstate->name;
		f->field.index.buflen = strlen(appstate->name);
	}
	gboolean ret = tbus_writemsg(os, THINGYMCCONFIG_MSGTYPE_CONFIG_APPS, fields,
			apps->len);
	g_free(fields);
	return ret;
}
