#pragma once
#include <json-glib/json-glib.h>

void utils_addwatchforsocket(GSocket* sock, GIOCondition cond, GIOFunc callback,
		gpointer user_data);
void utils_addwatchforsocketfd(int fd, GIOCondition cond, GIOFunc callback,
		gpointer user_data);
gchar* utils_jsonbuildertostring(JsonBuilder* jsonbuilder, gsize* jsonlen);
