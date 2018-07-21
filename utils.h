#pragma once
#include <json-glib/json-glib.h>

guint utils_addwatchforsocket(GSocket* sock, GIOCondition cond,
		GIOFunc callback, gpointer user_data);
guint utils_addwatchforsocketfd(int fd, GIOCondition cond, GIOFunc callback,
		gpointer user_data);
