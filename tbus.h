#pragma once

#include <gio/gio.h>
#include "include/thingymcconfig/ctrl.h"

struct tbus_fieldandbuff {
	struct thingymcconfig_ctrl_field field;
	guint8* buff;
};

typedef void (*tbus_fieldproc)(struct tbus_fieldandbuff* field, gpointer target);

typedef void (*tbus_emitter)(gpointer target, gpointer user_data);

struct tbus_messageprocessor {
	gsize allocsize;
	tbus_fieldproc fieldprocessor;
	tbus_emitter emitter;
};

gboolean tbus_readmsg(GInputStream* is,
		struct tbus_messageprocessor* msgprocessors, int numprocessors,
		gpointer user_data);
