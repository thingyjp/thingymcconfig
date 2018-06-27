#include "network_model.h"

#define SSID "ssid"
#define PSK	"psk"

struct network_config* network_model_config_deserialise(JsonNode* root) {
	if (json_node_get_node_type(root) == JSON_NODE_OBJECT) {
		JsonObject* rootobj = json_node_get_object(root);
		if (json_object_has_member(rootobj, SSID)
				&& json_object_has_member(rootobj, PSK)) {
			struct network_config* ntwkcfg = g_malloc0(
					sizeof(struct network_config));
			const gchar* ssid = json_object_get_string_member(rootobj, SSID);
			const gchar* psk = json_object_get_string_member(rootobj, PSK);
			strcpy(ntwkcfg->ssid, ssid);
			strcpy(ntwkcfg->psk, psk);
			return ntwkcfg;
		} else
			g_message("network config is missing required fields");
	} else
		g_message("root of network config should be an object");

	return NULL;
}

void network_model_config_serialise(struct network_config* config,
		JsonBuilder* jsonbuilder) {
	json_builder_begin_object(jsonbuilder);
	json_builder_set_member_name(jsonbuilder, SSID);
	json_builder_add_string_value(jsonbuilder, config->ssid);
	json_builder_set_member_name(jsonbuilder, PSK);
	json_builder_add_string_value(jsonbuilder, config->psk);
	json_builder_end_object(jsonbuilder);
}
