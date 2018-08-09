#include "tbus.h"
#include "utils.h"

gboolean tbus_writemsg(GOutputStream* os, unsigned char type,
		struct tbus_fieldandbuff* fields, int numfields) {
	gboolean ret = FALSE;

	GByteArray* pktbuff = g_byte_array_new();

	struct thingymcconfig_ctrl_msgheader msghdr = { .type = type, .numfields =
			numfields + 1 };
	g_byte_array_append(pktbuff, (void*) &msghdr, sizeof(msghdr));

	for (int f = 0; f < numfields; f++) {
		struct tbus_fieldandbuff* field = fields + f;
		g_byte_array_append(pktbuff, (void*) &field->field,
				sizeof(field->field));
		if (field->field.raw.buflen > 0)
			g_byte_array_append(pktbuff, field->buff, field->field.raw.buflen);
	}

	g_byte_array_append(pktbuff, (void*) &thingymcconfig_terminator,
			sizeof(thingymcconfig_terminator));

	if (g_output_stream_write(os, pktbuff->data, pktbuff->len, NULL, NULL)
			!= pktbuff->len) {
		goto err;
	}

	ret = TRUE;

	err: //
	g_byte_array_free(pktbuff, TRUE);

	return ret;
}

gboolean tbus_readmsg(GInputStream* is,
		struct tbus_messageprocessor* msgprocessors, int numprocessors,
		gpointer user_data) {

	gboolean ret = FALSE;
	gpointer msgstruct = NULL;

	struct thingymcconfig_ctrl_msgheader msghdr;
	if (g_input_stream_read(is, &msghdr, sizeof(msghdr), NULL, NULL)
			!= sizeof(msghdr))
		goto err;

	g_message("have message type %d with %d fields", (int ) msghdr.type,
			(int ) msghdr.numfields);

	struct tbus_messageprocessor* processor = NULL;
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
				(int ) fieldandbuff.field.raw.type,
				(int ) fieldandbuff.field.raw.buflen,
				(int ) fieldandbuff.field.raw.v0,
				(int ) fieldandbuff.field.raw.v1);

		gsize buflen = fieldandbuff.field.raw.buflen;
		if (buflen > 0) {
			fieldandbuff.buff = g_malloc0(buflen + 1);
			if (g_input_stream_read(is, fieldandbuff.buff, buflen, NULL, NULL)
					!= buflen)
				goto err;
			thingymcconfig_utils_hexdump(fieldandbuff.buff, buflen);
		} else
			fieldandbuff.buff = NULL;

		fieldcount++;
		if (fieldandbuff.field.raw.type == THINGYMCCONFIG_FIELDTYPE_TERMINATOR) {
			terminated = TRUE;
			break;
		}

		if (processor != NULL && processor->fieldprocessor != NULL)
			processor->fieldprocessor(&fieldandbuff, msgstruct, user_data);
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

	if (msgstruct != NULL)
		g_free(msgstruct);
	return ret;
}
