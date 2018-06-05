#include "bass_dts.h"

BOOL dts_stream_create(BASSFILE file, int flags, DTS_STREAM** stream);

BOOL dts_stream_update(DTS_STREAM* stream);

BOOL dts_stream_update_info(DTS_STREAM* stream);

DWORD dts_stream_read(DTS_STREAM* stream, void* buffer, DWORD length);

BOOL dts_stream_reset(DTS_STREAM* stream);

BOOL dts_stream_free(DTS_STREAM* stream);