#include <microhttpd.h>
#include <json-glib/json-glib.h>
#include <string.h>
#include "http.h"
#include "network.h"

#define PORT 1338

static struct MHD_Daemon* mhd = NULL;

gchar* utils_jsonbuildertostring(JsonBuilder* jsonbuilder, gsize* jsonlen) {
	JsonNode* responseroot = json_builder_get_root(jsonbuilder);
	JsonGenerator* generator = json_generator_new();
	json_generator_set_root(generator, responseroot);

	gchar* json = json_generator_to_data(generator, jsonlen);
	json_node_free(responseroot);
	g_object_unref(generator);
	g_object_unref(jsonbuilder);
	return json;
}

static int http_handleconnection_status(struct MHD_Connection* connection) {
	JsonBuilder* jsonbuilder = json_builder_new();
	json_builder_begin_object(jsonbuilder);
	json_builder_end_object(jsonbuilder);

	gsize contentln;
	char* content = utils_jsonbuildertostring(jsonbuilder, &contentln);

	int ret = 0;
	struct MHD_Response* response = MHD_create_response_from_buffer(contentln,
			(void*) content, MHD_RESPMEM_MUST_COPY);
	if (response) {
		ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
		MHD_destroy_response(response);
	} else
		g_message("failed to create response");

	g_free(content);
	return ret;
}

static void http_handleconnection_scan_addscanresult(gpointer data,
		gpointer userdata) {

	struct network_scanresult* scanresult = (struct network_scanresult*) data;
	JsonBuilder* jsonbuilder = (JsonBuilder*) userdata;

	json_builder_begin_object(jsonbuilder);
	json_builder_set_member_name(jsonbuilder, "bssid");
	json_builder_add_string_value(jsonbuilder, scanresult->bssid);
	json_builder_set_member_name(jsonbuilder, "frequency");
	json_builder_add_int_value(jsonbuilder, scanresult->frequency);
	json_builder_set_member_name(jsonbuilder, "rssi");
	json_builder_add_int_value(jsonbuilder, scanresult->rssi);
	json_builder_set_member_name(jsonbuilder, "ssid");
	json_builder_add_string_value(jsonbuilder, scanresult->ssid);
	json_builder_end_object(jsonbuilder);
}

static int http_handleconnection_scan(struct MHD_Connection* connection) {

	int ret = MHD_NO;

	GPtrArray* scanresults = network_scan();
	JsonBuilder* jsonbuilder = json_builder_new();
	json_builder_begin_object(jsonbuilder);
	json_builder_set_member_name(jsonbuilder, "scanresults");
	json_builder_begin_array(jsonbuilder);
	g_ptr_array_foreach(scanresults, http_handleconnection_scan_addscanresult,
			jsonbuilder);
	json_builder_end_array(jsonbuilder);
	json_builder_end_object(jsonbuilder);

	gsize jsonlen;
	char* content = utils_jsonbuildertostring(jsonbuilder, &jsonlen);
	struct MHD_Response* response = MHD_create_response_from_buffer(jsonlen,
			(void*) content, MHD_RESPMEM_PERSISTENT);
	if (response) {
		ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
		MHD_destroy_response(response);
	} else
		g_message("failed to create response");
	g_free(content);
	return ret;
}

static int http_handleconnection_configure(struct MHD_Connection* connection) {
	int ret = MHD_NO;
	static const char* content = "{}";
	struct MHD_Response* response = MHD_create_response_from_buffer(
			strlen(content), (void*) content, MHD_RESPMEM_PERSISTENT);
	if (response) {
		ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
		MHD_destroy_response(response);
	} else
		g_message("failed to create response");
	return ret;
}

static int http_handleconnection_invalid(struct MHD_Connection* connection) {
	int ret = MHD_NO;
	static const char* content = "";
	struct MHD_Response* response = MHD_create_response_from_buffer(
			strlen(content), (void*) content, MHD_RESPMEM_PERSISTENT);
	if (response) {
		ret = MHD_queue_response(connection, MHD_HTTP_BAD_REQUEST, response);
		MHD_destroy_response(response);
	} else
		g_message("failed to create response");
	return ret;
}

static int http_handleconnection(void* cls, struct MHD_Connection* connection,
		const char* url, const char* method, const char* version,
		const char* upload_data, size_t* upload_data_size, void** con_cls) {
	g_message("handling request %s for %s", method, url);

	int ret = MHD_NO;
	gboolean isget = (strcmp(method, MHD_HTTP_METHOD_GET) == 0);
	gboolean ispost = (strcmp(method, MHD_HTTP_METHOD_POST) == 0);
	if (isget && (strcmp(url, "/status") == 0))
		ret = http_handleconnection_status(connection);
	else if (isget && (strcmp(url, "/scan") == 0))
		ret = http_handleconnection_scan(connection);
	else if (ispost && (strcmp(url, "/config") == 0))
		ret = http_handleconnection_configure(connection);
	else {
		g_message("invalid request");
		ret = http_handleconnection_invalid(connection);
	}

	return ret;
}

int http_start() {
	mhd = MHD_start_daemon(MHD_USE_SELECT_INTERNALLY, PORT, NULL, NULL,
			&http_handleconnection, NULL, MHD_OPTION_END);

	if (mhd == NULL)
		return 1;

	return 0;
}

void http_stop() {
	MHD_stop_daemon(mhd);
	mhd = NULL;
}
