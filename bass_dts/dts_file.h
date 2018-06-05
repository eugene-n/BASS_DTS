#include "bass_dts.h"

typedef enum {
	DTS_FILE_SEEK_BEGIN,
	DTS_FILE_SEEK_POSITION,
	DTS_FILE_SEEK_END
} DTS_FILE_SEEK;

BOOL dts_file_create(BASSFILE bass_file, DTS_FILE** dts_file);

BOOL dts_file_read(DTS_FILE* dts_file);

BOOL dts_file_seek(DTS_FILE* dts_file, QWORD position, DTS_FILE_SEEK mode);

QWORD dts_file_position(DTS_FILE* dts_file);

QWORD dts_file_length(DTS_FILE* dts_file);

BOOL dts_file_free(DTS_FILE* dts_file);