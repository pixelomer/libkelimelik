#ifndef __LIBKELIMELIK_H
#define __LIBKELIMELIK_H

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

typedef struct kelimelik_packet kelimelik_packet;
typedef struct kelimelik_string kelimelik_string;
typedef struct kelimelik_array kelimelik_array;
typedef struct kelimelik_error kelimelik_error;
typedef struct kelimelik_object kelimelik_object;
typedef struct kelimelik_parser kelimelik_parser;

#define KELIMELIK_IS_ERROR(kelimelik_error) (kelimelik_error.kelimelik_errno != KELIMELIK_SUCCESS)

#if 0
// Used for debugging memory leaks.
#define free(x) do { fprintf(stderr, "==> free(%p);\n", x); free(x); } while(0)
#endif

struct kelimelik_error {
	union {
		int syscall_errno;
		int details;
	};
	enum kelimelik_errno {
		// Success
		KELIMELIK_SUCCESS = 0,

		// Syscall errors
		KELIMELIK_ERROR_socket = -1,
		KELIMELIK_ERROR_gethostbyname = -2,
		KELIMELIK_ERROR_connect = -3,
		KELIMELIK_ERROR_malloc = -4,

		// Other errors
		KELIMELIK_ERROR_UNSPECIFIED_TYPES = 1,
		KELIMELIK_ERROR_INVALID_ARGUMENT = 2,
		KELIMELIK_ERROR_INVALID_TYPE = 3,
		KELIMELIK_ERROR_NOT_IMPLEMENTED = 4
	} kelimelik_errno;
};

enum kelimelik_object_type {
	// Default type for all objects in packets created manually. You
	// will never receive this an object of this type from any server.
	KELIMELIK_OBJECT_UNSPECIFIED = -1,

	// At some places, types 0 and 1 are signed values, while at other places
	// they are unsigned. Cast when needed.
	KELIMELIK_OBJECT_UINT32 = 0,
	KELIMELIK_OBJECT_UINT8 = 1,

	// This type is used mostly for timestamps (number of seconds since
	// 00:00:00 UTC on 1 January 1970). This value may or may not be signed,
	// but since a date earlier than 1st of January 1970 is never used in
	// the game, it is fair to assume that it is unsigned.
	KELIMELIK_OBJECT_UINT64 = 3,

	// String type. Strings have the UTF-8 encoding and are not null
	// terminated. Strings cannot be bigger than 65535 bytes (excluding
	// the null terminator) since the string length is transmitted as a
	// 16-bit unsigned integer. 
	KELIMELIK_OBJECT_STRING = 7,

	// Array type. An array may be empty, and an array may only contain
	// one type. For example, [ "x", "y" ] and [ (UInt8)0, (UInt8)1 ]
	// are valid arrays but [ "x", 10 ] and [ (UInt64)0, (UInt32)0 ]
	// are not. The packet format makes it possible for arrays to contain
	// other arrays, but the official Kelimelik server never uses nested
	// arrays like these so this isn't supported.
	KELIMELIK_OBJECT_ARRAY = 8
};

// Arrays are represented using a type other than regular objects
// since they are simpler. An array can only contain 1 type. For
// details, see the comment above the declaration of
// KELIMELIK_OBJECT_ARRAY.
struct kelimelik_array {
	// Can be anything other than KELIMELIK_OBJECT_UNSPECIFIED and
	// KELIMELIK_OBJECT_ARRAY. See the comment above the declaration
	// of KELIMELIK_OBJECT_ARRAY for more details.
	enum kelimelik_object_type type;

	// The number of items in the array. This value is limited to
	// 4294967295 (0xFFFFFFFF, the largest 32-bit number) by the
	// protocol.
	uint32_t item_count;

	// The values in the array. Only one of these arrays contain valid
	// data. This depends on the type element of the struct.
	union {
		kelimelik_string *strings[0];
		uint64_t uint64s[0];
		uint32_t uint32s[0];
		uint8_t uint8s[0];
	};
};

struct kelimelik_string {
	// The length of the string in bytes excluding the null terminator.
	uint16_t length;

	// The contents of the string. Strings are not null terminated while
	// sending them, but the string pointer in the kelimelik_string
	// structure points to a null terminated string for convenience. The
	// official Kelimelik server uses UTF-8 strings so don't treat this
	// string as an ASCII string while doing string operations.
	const uint8_t string[];
};

struct kelimelik_object {
	// Type of the object. If the value of this property is
	// KELIMELIK_OBJECT_UNSPECIFIED, then the values of the properties
	// other than "next" and "first" are undefined.
	enum kelimelik_object_type type;

	// For objects in a packet or an array, the next object. If this
	// is the last object, this value is NULL.
	kelimelik_object *next;

	// For objects in a packet or an array, the first object.
	kelimelik_object *first;

	union {
		// String value. Only valid if type is KELIMELIK_OBJECT_STRING.
		kelimelik_string *string;

		// UInt32 value. Only valid if type is KELIMELIK_OBJECT_UINT32.
		uint32_t uint32;

		// UInt64 value. Only valid if type is KELIMELIK_OBJECT_UINT64.
		uint64_t uint64;
		
		// UInt8 value. Only valid if type is KELIMELIK_OBJECT_UINT8.
		uint8_t uint8;

		// Array value.
		kelimelik_array *array;
	};
};

struct kelimelik_packet {
	// The header for the packet. 
	kelimelik_string *header;

	// The number of objects that is in the packet itself. Arrays are counted as a
	// single object, regardless of how many items they contain. The number of
	// objects is limited to 255 by the server implementation.
	uint8_t object_count;

	// Contains object_count items.
	kelimelik_object objects[0];
};

// Creates a new kelimelik_string with the specified null-terminated C string. The
// string is copied.
kelimelik_error kelimelik_string_new_v1(kelimelik_string **out, const char *string);

// Creates a new kelimelik_string with the given bytes[len]. This function creates a
// copy of the bytes buffer and appends a null character to it, so if you don't want the
// string to contain a null byte other than the null terminator, make sure bytes[len-1]
// is not '\0'.
kelimelik_error kelimelik_string_new_v2(kelimelik_string **out, void *bytes, size_t len);

// Calls free().
void kelimelik_string_free(kelimelik_string *string);

// Objects
kelimelik_error kelimelik_objects_free(kelimelik_object *first_object);
kelimelik_error kelimelik_object_free(kelimelik_object *object);

// Arrays
kelimelik_error kelimelik_array_free(kelimelik_array *self);
kelimelik_error kelimelik_string_array_new_v1(kelimelik_array **out, const char **strings);
kelimelik_error kelimelik_string_array_new_v2(kelimelik_array **out, const char **strings, const size_t count);
kelimelik_error kelimelik_string_array_new_v3(kelimelik_array **out, kelimelik_string **strings);
kelimelik_error kelimelik_string_array_new_v4(kelimelik_array **out, kelimelik_string **strings, const size_t count);
kelimelik_error kelimelik_uint8_array_new(kelimelik_array **out, const uint8_t *values, const size_t count);
kelimelik_error kelimelik_uint32_array_new(kelimelik_array **out, const uint32_t *values, const size_t count);
kelimelik_error kelimelik_uint64_array_new(kelimelik_array **out, const uint64_t *values, const size_t count);
kelimelik_error kelimelik_array_new(kelimelik_array **out, enum kelimelik_object_type type, const void *values, const size_t size_in_bytes);
kelimelik_error kelimelik_uint_array_new(kelimelik_array **out, enum kelimelik_object_type type, const void *values, const size_t count);

// Packets
void kelimelik_packet_free(kelimelik_packet *packet);
char *kelimelik_packet_description(kelimelik_packet *self);
kelimelik_error kelimelik_packet_new_v1(kelimelik_packet **out, const char *header, uint8_t size);
kelimelik_error kelimelik_packet_new_v2(kelimelik_packet **out, kelimelik_string *header, uint8_t size);
kelimelik_error kelimelik_packet_set_uint64(kelimelik_packet *packet, uint8_t index, uint64_t value);
kelimelik_error kelimelik_packet_set_uint32(kelimelik_packet *packet, uint8_t index, uint32_t value);
kelimelik_error kelimelik_packet_set_uint16(kelimelik_packet *packet, uint8_t index, uint16_t value);
kelimelik_error kelimelik_packet_set_uint8(kelimelik_packet *packet, uint8_t index, uint8_t value);
kelimelik_error kelimelik_packet_set_string_v1(kelimelik_packet *packet, uint8_t index, const char *string);
kelimelik_error kelimelik_packet_set_string_v2(kelimelik_packet *packet, uint8_t index, kelimelik_string *string);
kelimelik_error kelimelik_packet_set_array(kelimelik_packet *packet, uint8_t index, kelimelik_array *array);
kelimelik_error kelimelik_packet_encode(kelimelik_packet *packet, void **out_bytes, size_t *out_len);

// Errors
const char *kelimelik_strerror(kelimelik_error error); // Not thread-safe
char *kelimelik_strerror_buf(kelimelik_error error, char *buffer, size_t len); // Is thread-safe

// Connections
kelimelik_error kelimelik_connection_new(int *fd_out);

// Parsers
kelimelik_error kelimelik_parser_new(kelimelik_parser **out);
kelimelik_error kelimelik_parser_advance(
	kelimelik_parser *self,
	uint8_t *bytes,
	size_t bytes_length,
	kelimelik_packet ***new_packets,
	size_t *new_packets_length
);
kelimelik_error kelimelik_parser_advance_single(
	kelimelik_parser *self,
	uint8_t byte,
	kelimelik_packet ***new_packets,
	size_t *new_packets_length
);
void kelimelik_parser_free(kelimelik_parser *self);

#endif