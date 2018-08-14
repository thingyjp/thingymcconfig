#pragma once

#include "network_wpasupplicant.h"

#define MATCHBSSID "((?:[0-9a-f]{2}:{0,1}){6})"
#define MATCHFREQ "([1-9]{4})"
#define MATCHRSSI "(-[1-9]{2,3})"
#define FLAGPATTERN "[A-Z2\\-\\+]*"
#define MATCHFLAG "\\[("FLAGPATTERN")\\]"
#define MATCHFLAGS "((?:\\["FLAGPATTERN"\\]){1,})"
#define MATCHSSID "([a-zA-Z0-9\\-]*)"
#define SCANRESULTREGEX MATCHBSSID"\\s*"MATCHFREQ"\\s*"MATCHRSSI"\\s*"MATCHFLAGS"\\s*"MATCHSSID

#define NETWORK_WPASUPPLICANT_REGEX_KEYVALUE "([a-z]{1,})=(([0-9]{1,}|[A-Z,_]{1,}|\".*\"))"

typedef void (*wpaeventhandler)(NetworkWpaSupplicant* supplicant,
		const gchar* event);

struct wpaeventhandler_entry {
	const gchar* command;
	const wpaeventhandler handler;
};
