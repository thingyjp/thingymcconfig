#pragma once

#define JSONBUILDER_ADD_INT(b, n, v) {json_builder_set_member_name(b, n);\
                                     json_builder_add_int_value(b, (int) v);}

#define JSONBUILDER_ADD_STRING(b, n, v) {json_builder_set_member_name(b, n);\
	                                    json_builder_add_string_value(b, v);}

#define JSONBUILDER_ADD_BOOL(b, n, v) {json_builder_set_member_name(b, n);\
	                                  json_builder_add_boolean_value(b, v);}

#define JSONBUILDER_START_OBJECT(b, n) {json_builder_set_member_name(b, n);\
                                       json_builder_begin_object(b);}

#define JSONBUILDER_START_ARRAY(b, n) {json_builder_set_member_name(b, n);\
                                      json_builder_begin_array(b);}
