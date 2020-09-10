#ifndef __LIBKELIMELIK_PRIVATE_H
#define __LIBKELIMELIK_PRIVATE_H

#include <kelimelik.h>
#include <errno.h>

#ifndef htonll
#if __BIG_ENDIAN__
#define htonll(x) (x)
#else
#define htonll(x) ((uint64_t)htonl((x) & 0xFFFFFFFF) << 32) | htonl((x) >> 32))
#endif
#define ntohll(x) htonll(x)
#endif

#define _KELIMELIK_SUCCESS ((kelimelik_error){ \
	.kelimelik_errno = KELIMELIK_SUCCESS \
})
#define _KELIMELIK_ERROR(error, details_val) ((kelimelik_error){ \
	.kelimelik_errno = error, \
	.details = details_val \
})
#define _KELIMELIK_CONCAT_3(x,y,z) x##y##z
#define _KELIMELIK_CONCAT_2(x,y) x##y
#define _KELIMELIK_ERROR_INVALID_ARGUMENT(x) _KELIMELIK_ERROR(KELIMELIK_ERROR_INVALID_ARGUMENT, x)
#define _KELIMELIK_ERROR_NOT_IMPLEMENTED _KELIMELIK_ERROR(KELIMELIK_ERROR_NOT_IMPLEMENTED, 0)
#define _KELIMELIK_ERROR_SYSCALL(x) _KELIMELIK_ERROR((_KELIMELIK_CONCAT_2(KELIMELIK_ERROR_, x)), errno)

struct kelimelik_parser {
	enum {
		KELIMELIK_PARSER_WAITING_FOR_SIZE = 0,
		KELIMELIK_PARSER_WAITING_FOR_DATA = 1
	} state;
	uint8_t *packet_buffer;
	uint8_t packet_size_buffer[4];
	uint32_t bytes_remaining;
	uint32_t index;
	size_t packet_count;
	kelimelik_packet **packets;
};

#endif