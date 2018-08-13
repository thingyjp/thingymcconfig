#include "logging.h"

#include <gio/gio.h>

static GLogWriterOutput logging_writer(GLogLevelFlags log_level,
		const GLogField *fields, gsize n_fields, gpointer user_data) {
	GFile* file = user_data;
	GFileOutputStream* fos = g_file_append_to(file, 0, NULL, NULL);

	const gchar newline[] = "\n";

	for (int f = 0; f < n_fields; f++) {
		if (fields->length == -1) {
			if (strcmp(fields->key, "MESSAGE") == 0) {
				g_output_stream_write(G_OUTPUT_STREAM(fos), fields->value,
						strlen(fields->value),
						NULL, NULL);
				g_output_stream_write(G_OUTPUT_STREAM(fos), newline,
						sizeof(newline), NULL, NULL);
			}
		}
		fields++;
	}

	g_object_unref(fos);

	return G_LOG_WRITER_HANDLED;
}

void logging_init(const gchar* logfile) {
	if (logfile != NULL) {
		g_message("logging to %s", logfile);
		GFile* file = g_file_new_for_path(logfile);
		g_log_set_writer_func(logging_writer, file, g_object_unref);
	} else
		g_message("no logfile specified, logging to console");
}
