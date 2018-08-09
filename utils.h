#pragma once
#include <gio/gio.h>
#include "include/thingymcconfig/utils.h"

guint utils_addwatchforsocket(GSocket* sock, GIOCondition cond,
		GIOFunc callback, gpointer user_data);
guint utils_addwatchforsocketfd(int fd, GIOCondition cond, GIOFunc callback,
		gpointer user_data);
