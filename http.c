#include <microhttpd.h>
#include <json-glib/json-glib.h>
#include <string.h>
#include "http.h"
#include "network.h"
#include "utils.h"
#include "apps.h"
#include "jsonbuilderutils.h"

struct postconninfo {
	GByteArray* payload;
};

#define PORT 1338

#define ENDPOINT_SCAN "/scan"
#define ENDPOINT_CONFIG "/config"
#define ENDPOINT_STATUS "/status"
#define ENDPOINT_DEBUG "/debug"

static struct MHD_Daemon* mhd = NULL;

static int http_handleconnection_debug(struct MHD_Connection* connection) {
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

static int http_handleconnection_status(struct MHD_Connection* connection) {
	JsonBuilder* jsonbuilder = json_builder_new();
	json_builder_begin_object(jsonbuilder);
	network_dumpstatus(jsonbuilder);
	apps_dumpstatus(jsonbuilder);
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
			(void*) content, MHD_RESPMEM_MUST_COPY);
	if (response) {
		ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
		MHD_destroy_response(response);
	} else
		g_message("failed to create response");
	g_free(content);
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

static int http_handleconnection_configure(struct MHD_Connection* connection,
		void** con_cls) {
	struct postconninfo* con_info = *con_cls;
	int ret = MHD_NO;
	GBytes* bytes = g_byte_array_free_to_bytes(con_info->payload);
	con_info->payload = NULL;
	JsonParser* jsonparser = json_parser_new();
	gsize sz;
	gconstpointer data = g_bytes_get_data(bytes, &sz);
	if (sz == 0) {
		g_message("0 byte payload");
		goto invalidrequest;
	}

	if (!json_parser_load_from_data(jsonparser, data, sz, NULL)) {
		g_message("failed to parse json");
		goto invalidrequest;
	}

	JsonNode* root = json_parser_get_root(jsonparser);
	struct network_config* ntwkcfg = network_model_config_deserialise(root);
	if (ntwkcfg == NULL) {
		goto invalidrequest;
	}

	gboolean configuring = network_configure(ntwkcfg);

	JsonBuilder* jsonbuilder = json_builder_new();
	json_builder_begin_object(jsonbuilder);
	json_builder_set_member_name(jsonbuilder, "configuring");
	json_builder_add_boolean_value(jsonbuilder, configuring);
	json_builder_end_object(jsonbuilder);
	gsize contentln;
	char* content = utils_jsonbuildertostring(jsonbuilder, &contentln);

	struct MHD_Response* response = MHD_create_response_from_buffer(contentln,
			(void*) content, MHD_RESPMEM_MUST_COPY);
	if (response) {
		ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
		MHD_destroy_response(response);
	} else
		g_message("failed to create response");
	g_free(content);
	goto out;

	invalidrequest: ret = http_handleconnection_invalid(connection);
	out: g_object_unref(jsonparser);
	g_bytes_unref(bytes);

	return ret;
}

static int http_handleconnection_continuemunchingpost(const char* upload_data,
		size_t* upload_data_size, void** con_cls) {
	struct postconninfo* con_info = *con_cls;
	g_message("continuing to eat post, %d", *upload_data_size);
	g_byte_array_append(con_info->payload, upload_data, *upload_data_size);
	*upload_data_size = 0;
	return MHD_YES;
}

static int http_handleconnection_startmunchingpost(
		struct MHD_Connection* connection, void** con_cls) {
	g_message("starting to eat post");
	struct postconninfo* con_info = g_malloc0(sizeof(struct postconninfo));
	con_info->payload = g_byte_array_new();
	*con_cls = con_info;
	return MHD_YES;
}

static int http_handleconnection(void* cls, struct MHD_Connection* connection,
		const char* url, const char* method, const char* version,
		const char* upload_data, size_t* upload_data_size, void** con_cls) {
	g_message("handling request %s for %s", method, url);

	int ret = MHD_NO;
	gboolean isget = (strcmp(method, MHD_HTTP_METHOD_GET) == 0);
	gboolean ispost = (strcmp(method, MHD_HTTP_METHOD_POST) == 0);
	if (isget && (strcmp(url, ENDPOINT_STATUS) == 0))
		ret = http_handleconnection_status(connection);
	else if (isget && (strcmp(url, ENDPOINT_SCAN) == 0))
		ret = http_handleconnection_scan(connection);
	else if (isget && (strcmp(url, ENDPOINT_DEBUG) == 0)) {
		ret = http_handleconnection_debug(connection);
	} else if (ispost && (strcmp(url, ENDPOINT_CONFIG) == 0)) {
		if (*con_cls == NULL
				&& strcmp(
						MHD_lookup_connection_value(connection, MHD_HEADER_KIND,
						MHD_HTTP_HEADER_CONTENT_TYPE), "application/json")
						== 0) {
			ret = http_handleconnection_startmunchingpost(connection, con_cls);
		} else if (*upload_data_size != 0)
			ret = http_handleconnection_continuemunchingpost(upload_data,
					upload_data_size, con_cls);
		else
			ret = http_handleconnection_configure(connection, con_cls);
	} else {
		g_message("invalid request");
		ret = http_handleconnection_invalid(connection);
	}

	return ret;
}

static void http_requestcompleted(void *cls, struct MHD_Connection *connection,
		void **con_cls, enum MHD_RequestTerminationCode toe) {
	struct postconninfo* con_info = *con_cls;
	if (NULL == con_info)
		return;
	if (con_info->payload)
		g_byte_array_free(con_info->payload, TRUE);
	g_free(con_info);
	*con_cls = NULL;
}

int http_start() {
	mhd = MHD_start_daemon(MHD_USE_SELECT_INTERNALLY, PORT, NULL, NULL,
			http_handleconnection, NULL, MHD_OPTION_NOTIFY_COMPLETED,
			http_requestcompleted,
			NULL, MHD_OPTION_END);

	if (mhd == NULL)
		return 1;

	return 0;
}

void http_stop() {
	MHD_stop_daemon(mhd);
	mhd = NULL;
}
