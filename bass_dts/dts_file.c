#include <stdio.h>

#include "dts_file.h"
#include "../libdcadec/common.h"
#include "../libdcadec/ta.h"
#include "../libdcadec/dca_frame.h"

#define BUFFER_ALIGN 4096

#define DTSHDHDR UINT64_C(0x4454534844484452)

static BOOL dts_file_core_sync_word(uint32_t value) {
	return
		//DTS Core
		value == SYNC_WORD_CORE ||
		value == SYNC_WORD_CORE_LE ||
		value == SYNC_WORD_CORE_LE14 ||
		value == SYNC_WORD_CORE_BE14;
}

static BOOL dts_file_ext_sync_word(uint32_t value) {
	return
		//XCH 
		value == SYNC_WORD_XCH ||
		//XXCH 
		value == SYNC_WORD_XXCH ||
		//X96K 
		value == SYNC_WORD_X96 ||
		//EXSS 
		value == SYNC_WORD_EXSS ||
		value == SYNC_WORD_EXSS_LE;
}

static BOOL dts_file_sync_word(uint32_t value) {
	return dts_file_core_sync_word(value) || dts_file_ext_sync_word(value);
}

static BOOL dts_file_read_byte(DTS_FILE* dts_file, BYTE* data) {
	return bassfunc->file.Read(dts_file->bass_file, data, 1);
}

static BOOL dts_file_read_required(DTS_FILE* dts_file, void* buffer, DWORD length) {
	return bassfunc->file.Read(dts_file->bass_file, buffer, length) == length;
}

BOOL dts_file_create(BASSFILE bass_file, DTS_FILE** dts_file) {
	*dts_file = ta_znew(NULL, DTS_FILE);
	if (!*dts_file) {
		return FALSE;
	}

	(*dts_file)->bass_file = bass_file;
	(*dts_file)->info.length = bassfunc->file.GetPos(bass_file, BASS_FILEPOS_END);

	if (!((*dts_file)->frame.buffer = ta_zalloc_size(*dts_file, BUFFER_ALIGN * 2))) {
		dts_file_free(*dts_file);
		return FALSE;
	}

	return TRUE;
}

static BYTE* dts_file_get_buffer(DTS_FILE* dts_file, const size_t size) {
	const size_t old_size = ta_get_size(dts_file->frame.buffer);
	const size_t new_size = DCA_ALIGN(dts_file->frame.size + size, BUFFER_ALIGN);

	if (old_size < new_size) {
		BYTE* buffer = ta_realloc_size(dts_file, dts_file->frame.buffer, new_size);
		if (buffer) {
			memset(buffer + old_size, 0, new_size - old_size);
			dts_file->frame.buffer = buffer;
		}
		else {
			return NULL;
		}
	}

	return dts_file->frame.buffer + dts_file->frame.size;
}

static BOOL dts_file_read_sync_word(DTS_FILE* dts_file, UINT* sync_word) {
#if _DEBUG
	QWORD count = 0;
#endif
	*sync_word = dts_file->frame.sync_word;
	while (!dts_file_sync_word(*sync_word)) {
		BYTE data;
		if (!dts_file_read_byte(dts_file, &data)) {
			return FALSE;
		}
		*sync_word = (*sync_word << 8) | data;
#if _DEBUG
		count++;
#endif
	}
#if _DEBUG
	if (count > sizeof(UINT)) {
//#if _DEBUG
//		fprintf(stderr, "Data was ignored: %llu bytes.\n", count);
//#endif
	}
#endif
	return TRUE;
}

static int dts_file_read_frame_header(DTS_FILE* dts_file, size_t* size) {
	int result;

	dts_file->frame.header[0] = (dts_file->frame.sync_word >> 24) & 0xff;
	dts_file->frame.header[1] = (dts_file->frame.sync_word >> 16) & 0xff;
	dts_file->frame.header[2] = (dts_file->frame.sync_word >> 8) & 0xff;
	dts_file->frame.header[3] = (dts_file->frame.sync_word >> 0) & 0xff;

	if (!dts_file_read_required(dts_file, dts_file->frame.header + 4, DCADEC_FRAME_HEADER_SIZE - 4)) {
		return FALSE;
	}

	if ((result = dcadec_frame_parse_header(dts_file->frame.header, size)) < 0) {
		return result;
	}

	if (!dts_file->info.initialized) {
		dts_file->info.start = dts_file_position(dts_file) - DCADEC_FRAME_HEADER_SIZE;
	}

	return TRUE;
}

static int dts_file_read_frame_data(DTS_FILE* dts_file, size_t* size) {
	BYTE* buffer;
	int result;

	if (!(buffer = dts_file_get_buffer(dts_file, *size))) {
		return FALSE;
	}

	memcpy(buffer, dts_file->frame.header, DCADEC_FRAME_HEADER_SIZE);
	if (!dts_file_read_required(dts_file, buffer + DCADEC_FRAME_HEADER_SIZE, *size - DCADEC_FRAME_HEADER_SIZE)) {
		return FALSE;
	}

	if ((result = dcadec_frame_convert_bitstream(buffer, size, buffer, *size)) < 0) {
		return result;
	}

	return TRUE;
}

static int dts_file_read_frame(DTS_FILE* dts_file, BOOL ext) {
	size_t size;
	int result;

	if (!dts_file_read_sync_word(dts_file, &dts_file->frame.sync_word)) {
		return FALSE;
	}

	if (ext && !dts_file_ext_sync_word(dts_file->frame.sync_word)) {
		return -DCADEC_ENOSYNC;
	}

	if ((result = dts_file_read_frame_header(dts_file, &size)) <= 0) {
		return result;
	}

	if ((result = dts_file_read_frame_data(dts_file, &size)) <= 0) {
		return result;
	}

	dts_file->frame.size += DCA_ALIGN(size, 4);

	return TRUE;
}

BOOL dts_file_read(DTS_FILE* dts_file) {
	int result;

	dts_file->frame.size = 0;

	while (TRUE) {
		result = dts_file_read_frame(dts_file, FALSE);
		if (result == TRUE || result == -DCADEC_ENOSYNC) {
			break;
		}
		else {
			return FALSE;
		}
	}

	if (dts_file_core_sync_word(dts_file->frame.sync_word)) {
		dts_file->frame.sync_word = 0;
		result = dts_file_read_frame(dts_file, TRUE);
		if (result != TRUE && result != -DCADEC_ENOSYNC) {
			return FALSE;
		}
	}
	else
	{
		dts_file->frame.sync_word = 0;
	}


	if (!dts_file->info.initialized) {
		dts_file->info.frame_count = (dts_file->info.length - dts_file->info.start) / dts_file->frame.size;
		dts_file->info.initialized = TRUE;
	}

	return TRUE;
}

BOOL dts_file_seek(DTS_FILE* dts_file, QWORD position, DTS_FILE_SEEK mode) {
	switch (mode) {
	case DTS_FILE_SEEK_BEGIN:
		position += bassfunc->file.GetPos(dts_file->bass_file, BASS_FILEPOS_START);
		if (!bassfunc->file.Seek(dts_file->bass_file, position)) {
			return FALSE;
		}
		break;
	case DTS_FILE_SEEK_POSITION:
		if (!bassfunc->file.Seek(dts_file->bass_file, position)) {
			return FALSE;
		}
		break;
	case DTS_FILE_SEEK_END:
		position += bassfunc->file.GetPos(dts_file->bass_file, BASS_FILEPOS_END);
		if (!bassfunc->file.Seek(dts_file->bass_file, position)) {
			return FALSE;
		}
		break;
	}
	dts_file->frame.sync_word = 0;
	return TRUE;
}

QWORD dts_file_position(DTS_FILE* dts_file) {
	return bassfunc->file.GetPos(dts_file->bass_file, BASS_FILEPOS_CURRENT);
}

QWORD dts_file_length(DTS_FILE* dts_file) {
	return bassfunc->file.GetPos(dts_file->bass_file, BASS_FILEPOS_END);
}

BOOL dts_file_free(DTS_FILE* dts_file) {
	ta_free(dts_file);
	return TRUE;
}