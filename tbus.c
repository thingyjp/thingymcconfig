#include "tbus.h"

gboolean tbus_readmsg(GInputStream* is,
		struct tbus_messageprocessor* msgprocessors, int numprocessors,
		gpointer user_data) {

	gboolean ret = FALSE;

	struct thingymcconfig_ctrl_msgheader msghdr;
	if (g_input_stream_read(is, &msghdr, sizeof(msghdr), NULL, NULL)
			!= sizeof(msghdr))
		goto err;

	g_message("have message type %d with %d fields", (int ) msghdr.type,
			(int ) msghdr.numfields);

	struct tbus_messageprocessor* processor = NULL;
	gpointer msgstruct = NULL;
	if (msghdr.type < numprocessors) {
		processor = &msgprocessors[msghdr.type];
		msgstruct = g_malloc0(processor->allocsize);
	}

	int fieldcount = 0;
	gboolean terminated;

	for (int f = 0; f < msghdr.numfields; f++) {
		struct tbus_fieldandbuff fieldandbuff;
		if (g_input_stream_read(is, &fieldandbuff.field,
				sizeof(fieldandbuff.field), NULL, NULL)
				!= sizeof(fieldandbuff.field))
			goto err;
		g_message("have field; type: %d, buflen: %d, v0: %d, v1: %d",
				(int ) fieldandbuff.field.type,
				(int ) fieldandbuff.field.buflen, (int ) fieldandbuff.field.v0,
				(int ) fieldandbuff.field.v1);

		if (fieldandbuff.field.buflen != 0) {
			fieldandbuff.buff = g_malloc0(fieldandbuff.field.buflen + 1);
			g_input_stream_read(is, fieldandbuff.buff,
					fieldandbuff.field.buflen, NULL, NULL);
		} else
			fieldandbuff.buff = NULL;

		fieldcount++;
		if (fieldandbuff.field.type == THINGYMCCONFIG_FIELDTYPE_TERMINATOR) {
			terminated = TRUE;
			break;
		}

		if (processor != NULL && processor->fieldprocessor != NULL)
			processor->fieldprocessor(&fieldandbuff, msgstruct);
	}

	if (fieldcount != msghdr.numfields)
		g_message("bad number of fields, expected %d but got %d",
				(int ) msghdr.numfields, fieldcount);
	if (!terminated)
		g_message("didn't see terminator");

	if (processor != NULL && processor->emitter != NULL)
		processor->emitter(msgstruct, user_data);

	ret = TRUE;

	err: //
	return ret;
}
