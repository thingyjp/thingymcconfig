#ifndef THINGYMCCONFIG_CTRL_H_
#define THINGYMCCONFIG_CTRL_H_

#define THINGYMCCONFIG_MSGTYPE_NULL                                   0
#define THINGYMCCONFIG_MSGTYPE_EVENT_NETWORKSTATEUPDATE               1

#define THINGYMCCONFIG_FIELDTYPE_NETWORKSTATEUPDATE_SUPPLICANTSTATE   1
#define THINGYMCCONFIG_FIELDTYPE_NETWORKSTATEUPDATE_DHCPSTATE         2

/*
 * Header before each message with the type of message and
 * the number of fields in the message.
 */

struct __attribute__((__packed__)) thingymcconfig_ctrl_msgheader {
	unsigned char type;
	unsigned char numfields;
	// pad up to 32bits, might be used for something later
	unsigned char pad0, pad1;
};

/*
 * Opaque field in message. Type is namespaced by the type in the
 * header. i.e. field type 0 in message type 0 is different to field type
 * 0 in message type 1.
 */

struct __attribute__((__packed__)) thingymcconfig_ctrl_field {
	unsigned char type;
	unsigned char v0, v1, v2;
};

#endif
