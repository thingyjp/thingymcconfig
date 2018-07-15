#include "apps.h"

void apps_init(const gchar** apps) {

}

void apps_onappstateupdate(const struct apps_appstateupdate* update) {

}

void apps_dumpstatus(JsonBuilder* builder) {
	json_builder_set_member_name(builder, "apps");
	json_builder_begin_object(builder);

	json_builder_end_object(builder);
}
