#ifndef THINGYMCCONFIG_CTRL_H_
#define THINGYMCCONFIG_CTRL_H_

/*
 * Messages to/from the control socket look like this:
 *
 * [ msg hdr    ]
 * [ field 0    ]
 * [ field 1    ]
 * .
 * .
 * [ terminator ]
 *
 */

/*
 * Generic states, these should be valid for anywhere a state can
 * be expressed. Message specific states should use values higher
 * than GENERICEND
 */
#define THINGYMCCONFIG_NULL                                           0
#define THINGYMCCONFIG_OK                                             1
#define THINGYMCCONFIG_ERR                                            2
#define THINGYMCCONFIG_GENERICEND                                     31

#define THINGYMCCONFIG_MSGTYPE_EVENT_NETWORKSTATEUPDATE               1
#define THINGYMCCONFIG_MSGTYPE_EVENT_APPSTATEUPDATE                   2

#define THINGYMCCONFIG_FIELDTYPE_NETWORKSTATEUPDATE_SUPPLICANTSTATE   1
#define THINGYMCCONFIG_FIELDTYPE_NETWORKSTATEUPDATE_DHCPSTATE         2

#define THINGYMCCONFIG_FIELDTYPE_APPSTATEUPDATE_APPINDEX              0
#define THINGYMCCONFIG_FIELDTYPE_APPSTATEUPDATE_APPSTATE              1
#define THINGYMCCONFIG_FIELDTYPE_APPSTATEUPDATE_CONNECTIVITY          2

#define THINGYMCCONFIG_FIELDTYPE_TERMINATOR                           255

/*
 * Header before each message with the type of message and
 * the number of fields in the message.
 */

struct __attribute__((__packed__)) thingymcconfig_ctrl_msgheader {
	unsigned char type;
	/* number of fields being sent *including* the terminator */
	unsigned char numfields;
	// pad up to 32bits, might be used for something later
	unsigned char pad0, pad1;
};

/*
 * Opaque field in message. Types except for TERMINATOR are namespaced by
 * the type in the header. i.e. field type 0 in message type 0 is different
 * to field type 0 in message type 1.
 */

struct __attribute__((__packed__)) thingymcconfig_ctrl_field {
	unsigned char type;
	unsigned char v0, v1, v2;
};

struct __attribute__((__packed__)) thingymcconfig_ctrl_field_index {
	unsigned char type;
	unsigned char index;
	// pad up to 32 bits
	unsigned char pad0, pad1;
};

struct __attribute__((__packed__)) thingymcconfig_ctrl_field_stateanderror {
	unsigned char type;
	unsigned char state, error;
	// pad up to 32 bits
	unsigned char pad0;
};

static const struct thingymcconfig_ctrl_field thingymcconfig_terminator = {
		.type = THINGYMCCONFIG_FIELDTYPE_TERMINATOR };

#endif
