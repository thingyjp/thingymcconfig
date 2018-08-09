#ifndef THINGYMCCONFIG_CTRL_H_
#define THINGYMCCONFIG_CTRL_H_

#define THINGYMCCONFIG_CTRLSOCKPATH "/tmp/thingymcconfig.socket"

/* Messages to/from the control socket look like this:
 *
 * [ msg hdr                  ]
 * [ field 0                  ]
 * [ optional freeform buffer ]
 * [ field 1                  ]
 * .
 * .
 * [ terminator               ]
 */

/* Generic states, these should be valid for anywhere a state can
 * be expressed. Message specific states should use values higher
 * than GENERICEND
 */
#define THINGYMCCONFIG_NULL                                           0
#define THINGYMCCONFIG_OK                                             1
#define THINGYMCCONFIG_ERR                                            2
#define THINGYMCCONFIG_PENDING                                        3
#define THINGYMCCONFIG_ACTIVE                                         4
#define THINGYMCCONFIG_GENERICEND                                     31

#define THINGYMCCONFIG_MSGTYPE_CONFIG_APPS                            1
#define THINGYMCCONFIG_MSGTYPE_EVENT_NETWORKSTATEUPDATE               2
#define THINGYMCCONFIG_MSGTYPE_EVENT_APPSTATEUPDATE                   3

#define THINGMCCONFIG_FIELDTYPE_CONFIG_APPS_APP                       1

#define THINGYMCCONFIG_FIELDTYPE_NETWORKSTATEUPDATE_SUPPLICANTSTATE   1
#define THINGYMCCONFIG_FIELDTYPE_NETWORKSTATEUPDATE_DHCPSTATE         2

#define THINGYMCCONFIG_FIELDTYPE_APPSTATEUPDATE_APPINDEX              1
#define THINGYMCCONFIG_FIELDTYPE_APPSTATEUPDATE_APPSTATE              2
#define THINGYMCCONFIG_FIELDTYPE_APPSTATEUPDATE_CONNECTIVITY          3

#define THINGYMCCONFIG_FIELDTYPE_TERMINATOR                           255

/*
 * Header before each message with the type of message and
 * the number of fields in the message.
 */

struct __attribute__((__packed__)) thingymcconfig_ctrl_msgheader {
	unsigned char type;
	/* number of fields being sent *including* the terminator */
	unsigned char numfields;
	/* pad up to 32bits, might be used for something later,
	 should be 0'd. */
	unsigned char pad0, pad1;
};

/* Opaque field in message. Types except for TERMINATOR are namespaced by
 * the type in the header. i.e. field type 0 in message type 0 is different
 * to field type 0 in message type 1.
 */

struct __attribute__((__packed__)) thingymcconfig_ctrl_field_raw {
	unsigned char type;
	unsigned char buflen;
	unsigned char v0, v1;
};

struct __attribute__((__packed__)) thingymcconfig_ctrl_field_index {
	unsigned char type;
	unsigned char buflen;
	unsigned char index;
	// pad up to 32 bits
	unsigned char pad0;
};

struct __attribute__((__packed__)) thingymcconfig_ctrl_field_stateanderror {
	unsigned char type;
	unsigned char buflen;
	unsigned char state;
	unsigned char error;
};

union _thingymcconfig_ctrl_field {
	struct thingymcconfig_ctrl_field_raw raw;
	struct thingymcconfig_ctrl_field_index index;
	struct thingymcconfig_ctrl_field_stateanderror stateanderror;
};

typedef union _thingymcconfig_ctrl_field thingymcconfig_ctrl_field;

static const thingymcconfig_ctrl_field thingymcconfig_terminator = { .raw = {
		.type = THINGYMCCONFIG_FIELDTYPE_TERMINATOR } };

#endif
