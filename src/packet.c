#include <errno.h>
#include "kelimelik-private.h"
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <stdarg.h>

static void re_sprintf(char **buffer, char *format, ...) {
	va_list args;
  va_start(args, format);
	size_t size = vsnprintf(NULL, 0, format, args);
	va_end(args);
	*buffer = realloc(*buffer, size+1+strlen(*buffer));
  va_start(args, format);
	vsnprintf((*buffer)+strlen(*buffer), size+1, format, args);
	va_end(args);
}

char *kelimelik_packet_description(kelimelik_packet *self) {
	char *description = malloc(1);
	description[0] = 0;
	re_sprintf(
		&description,
		"{\n"
		"  \"header\" = \"%s\",\n"
		"  \"data\" = [\n",
		self->header->string
	);
	for (uint16_t i=0; i<self->object_count; i++) {
		kelimelik_object *object = &self->objects[i];
		re_sprintf(&description, "    ");
		switch (object->type) {
			case KELIMELIK_OBJECT_UINT64:
				re_sprintf(&description, "%lu", object->uint64);
				break;
			case KELIMELIK_OBJECT_UINT32:
				re_sprintf(&description, "%u", object->uint32);
				break;
			case KELIMELIK_OBJECT_UINT8:
				re_sprintf(&description, "%hhu", object->uint8);
				break;
			case KELIMELIK_OBJECT_STRING:
				re_sprintf(&description, "\"%s\"", object->string->string);
				break;
			case KELIMELIK_OBJECT_ARRAY:
				if (object->array->item_count == 0) {
					re_sprintf(&description, "[]");
					break;
				}
				re_sprintf(&description, "[\n");
				for (uint64_t i=0; i<object->array->item_count; i++) {
					re_sprintf(&description, "      ");
					switch (object->array->type) {
						case KELIMELIK_OBJECT_UINT64:
							re_sprintf(&description, "%lu,\n", object->array->uint64s[i]);
							break;
						case KELIMELIK_OBJECT_UINT32:
							re_sprintf(&description, "%u,\n", object->array->uint32s[i]);
							break;
						case KELIMELIK_OBJECT_UINT8:
							re_sprintf(&description, "%hhu,\n", object->array->uint8s[i]);
							break;
						case KELIMELIK_OBJECT_STRING:
							re_sprintf(&description, "\"%s\",\n", object->array->strings[i]->string);
							break;
						default:
							re_sprintf(&description, "?,\n");
							break;
					}
				}
				re_sprintf(&description, "    ]");
				break;
			default:
				re_sprintf(&description, "?");
				break;
		}
		re_sprintf(&description, " (%d),\n", (
			(object->type == KELIMELIK_OBJECT_ARRAY) ?
			object->array->type :
			object->type
		));
	}
	re_sprintf(&description, "  ]\n}");
	return description;
}

void kelimelik_packet_free(kelimelik_packet *self) {
	if (self->object_count) kelimelik_objects_free(self->objects);
	kelimelik_string_free(self->header);
	free(self);
}

kelimelik_error kelimelik_packet_new_v2(kelimelik_packet **out, kelimelik_string *header, uint8_t size) {
	if (!out) {
		return _KELIMELIK_ERROR_INVALID_ARGUMENT(0);
	}
	kelimelik_packet *packet = malloc(sizeof(*packet) + (sizeof(*(packet->objects)) * size));
	if (!packet) {
		return _KELIMELIK_ERROR_SYSCALL(malloc);
	}
	packet->header = header;
	packet->object_count = size;
	for (uint8_t i=0; i<size; i++) {
		kelimelik_object *object = &packet->objects[i];
		object->type = KELIMELIK_OBJECT_UNSPECIFIED;
		object->next = (i == (size - 1)) ? NULL : &packet->objects[i+1];
		object->first = packet->objects;

		// On some systems, maybe sizeof(uint64_t) != sizeof(void *). Probably not
		// but maybe
		object->string = NULL;
		object->uint64 = 0;
	}
	*out = packet;
	return _KELIMELIK_SUCCESS;
}

static kelimelik_error _kelimelik_packet_set_value(
	kelimelik_packet *self,
	uint8_t index,
	enum kelimelik_object_type object_type,
	void *data,
	size_t data_size
) {
	if (!self) return _KELIMELIK_ERROR_INVALID_ARGUMENT(0);
	if (self->object_count <= index) return _KELIMELIK_ERROR_INVALID_ARGUMENT(1);
	self->objects[index].type = object_type;
	memcpy(&(self->objects[index].uint8), data, data_size);
	return _KELIMELIK_SUCCESS;
}

kelimelik_error kelimelik_packet_new_v1(kelimelik_packet **out, const char *header, uint8_t size) {
	kelimelik_string *string;
	kelimelik_error error = kelimelik_string_new_v1(&string, header);
	if (KELIMELIK_IS_ERROR(error)) return error;
	return kelimelik_packet_new_v2(out, string, size);
}

#define KELIMELIK_CONCAT(x,y) x##y
#define KELIMELIK_SETTER(name, c_type, k_type) \
	kelimelik_error name(kelimelik_packet *self, uint8_t index, c_type value) { \
		return _kelimelik_packet_set_value(self, index, k_type, &value, sizeof(value)); \
	}

KELIMELIK_SETTER(kelimelik_packet_set_uint64, uint64_t, KELIMELIK_OBJECT_UINT64)
KELIMELIK_SETTER(kelimelik_packet_set_uint32, uint32_t, KELIMELIK_OBJECT_UINT32)
KELIMELIK_SETTER(kelimelik_packet_set_uint8, uint8_t, KELIMELIK_OBJECT_UINT8)
KELIMELIK_SETTER(kelimelik_packet_set_array, kelimelik_array *, KELIMELIK_OBJECT_ARRAY)
KELIMELIK_SETTER(kelimelik_packet_set_string_v2, kelimelik_string *, KELIMELIK_OBJECT_STRING)

kelimelik_error kelimelik_packet_set_string_v1(kelimelik_packet *self, uint8_t index, const char *c_string) {
	kelimelik_string *string;
	kelimelik_error error = kelimelik_string_new_v1(&string, c_string);
	if (KELIMELIK_IS_ERROR(error)) return error;
	return kelimelik_packet_set_string_v2(self, index, string);
}

kelimelik_error kelimelik_packet_encoded_size(kelimelik_packet *self, size_t *size_pt) {
	// Packet size (4 bytes)
	// Header size (2 bytes)
	// Header (<=65535 bytes)
	// Object count (1 byte)
	if (!self) return _KELIMELIK_ERROR_INVALID_ARGUMENT(0);
	if (!size_pt) return _KELIMELIK_ERROR_INVALID_ARGUMENT(1);
	size_t size = 4 + 2 + 1 + self->header->length;
	if (self->object_count) {
		kelimelik_object *object = &self->objects[0];
		do {
			size += 1; // Type byte
			switch (object->type) {
				case KELIMELIK_OBJECT_UINT64:
					size += 8;
					break;
				case KELIMELIK_OBJECT_UINT32:
					size += 4;
					break;
				case KELIMELIK_OBJECT_UINT8:
					size += 1;
					break;
				case KELIMELIK_OBJECT_STRING:
					size += object->string->length + 2; // Length (2 bytes) + Data (<=65535 bytes)
					break;
				case KELIMELIK_OBJECT_ARRAY:
					size += 5; // Array type byte and array count bytes
					switch (object->array->type) {
						case KELIMELIK_OBJECT_UINT64:
							size += object->array->item_count * 8;
							break;
						case KELIMELIK_OBJECT_UINT32:
							size += object->array->item_count * 4;
							break;
						case KELIMELIK_OBJECT_UINT8:
							size += object->array->item_count;
							break;
						case KELIMELIK_OBJECT_STRING:
							size += object->array->item_count * 2;
							for (uint64_t i=0; i<object->array->item_count; i++) {
								size += object->array->strings[i]->length;
							}
							break;
						case KELIMELIK_OBJECT_ARRAY:
						default:
							// Arrays can't contain arrays
							return _KELIMELIK_ERROR(KELIMELIK_ERROR_INVALID_TYPE, 0);
					}
					break;
				default:
					// Unknown type
					return _KELIMELIK_ERROR(KELIMELIK_ERROR_INVALID_TYPE, 0);
			}
		} while ((object = object->next));
	}
	*size_pt = size;
	return _KELIMELIK_SUCCESS;
}

kelimelik_error kelimelik_verify_packet(kelimelik_packet *self, const char *format) {
	uint16_t i;
	for (i=0; i<self->object_count; i++) {
		enum kelimelik_object_type value_type;
		bool is_array = ((format[i] >= 'A') && (format[i] <= 'Z'));
		if ((format[i] < 'A') || (format[i] > 'z')) {
			return _KELIMELIK_ERROR_INVALID_FORMAT;
		}
		switch (format[i] | 0x20) {
			case 'b': // BYTE (8-bit)
				value_type = KELIMELIK_OBJECT_UINT8;
				break;
			case 'd': // DWORD (32-bit)
				value_type = KELIMELIK_OBJECT_UINT32;
				break;
			case 'q': // QWORD (64-bit)
				value_type = KELIMELIK_OBJECT_UINT64;
				break;
			case 'i': // Integer (BYTE, DWORD or QWORD)
				value_type = KELIMELIK_OBJECT_UNSPECIFIED;
				break;
			case 's': // String
				value_type = KELIMELIK_OBJECT_STRING;
				break;
			default: // Invalid format
				return _KELIMELIK_ERROR_INVALID_FORMAT;
		}
		enum kelimelik_object_type type_in_packet;
		if (is_array) {
			if (self->objects[i].type != KELIMELIK_OBJECT_ARRAY) {
				return _KELIMELIK_ERROR_DIFFERENT_FORMAT;
			}
			type_in_packet = self->objects[i].array->type;
		}
		else {
			type_in_packet = self->objects[i].type;
		}
		if (
			(value_type != KELIMELIK_OBJECT_UNSPECIFIED) ?
			(value_type != type_in_packet) :
			(
				(type_in_packet != KELIMELIK_OBJECT_UINT64) &&
				(type_in_packet != KELIMELIK_OBJECT_UINT32) &&
				(type_in_packet != KELIMELIK_OBJECT_UINT8)
			)
		) {
			return _KELIMELIK_ERROR_DIFFERENT_FORMAT;
		}
	}
	if (format[i] != 0) {
		return _KELIMELIK_ERROR_DIFFERENT_FORMAT;
	}
	return _KELIMELIK_SUCCESS;
}

kelimelik_error kelimelik_packet_encode(kelimelik_packet *self, void **out_bytes, size_t *out_len) {
	size_t size;
	kelimelik_error error = kelimelik_packet_encoded_size(self, &size);
	if (KELIMELIK_IS_ERROR(error)) return error;
	uint8_t *encoded_packet_beginning = malloc(size);
	if (!encoded_packet_beginning) {
		return _KELIMELIK_ERROR_SYSCALL(malloc);
	}
	uint8_t *encoded_packet = encoded_packet_beginning;

	// Packet size
	*(uint32_t *)encoded_packet = htonl(size - 4);

	// Header size
	*(uint16_t *)(encoded_packet += 4) = htons(self->header->length);

	// Header
	memcpy((encoded_packet += 2), self->header->string, self->header->length);

	// Object count
	*(uint8_t *)(encoded_packet += self->header->length) = self->object_count;
	encoded_packet++;

	for (uint16_t i=0; i<self->object_count; i++) {
		// Object type
		uint8_t type = self->objects[i].type;
		*(uint8_t *)(encoded_packet++) = type;

		switch (type) {
			case KELIMELIK_OBJECT_UINT64:
				*(uint64_t *)encoded_packet = htonll(self->objects[i].uint64);
				encoded_packet += 8;
				break;
			case KELIMELIK_OBJECT_UINT32:
				*(uint32_t *)encoded_packet = htonl(self->objects[i].uint32);
				encoded_packet += 4;
				break;
			case KELIMELIK_OBJECT_UINT8:
				*(uint8_t *)(encoded_packet++) = self->objects[i].uint8;
				break;
			case KELIMELIK_OBJECT_STRING:
				*(uint16_t *)encoded_packet = htons(self->objects[i].string->length);
				memcpy((encoded_packet += 2), self->objects[i].string->string, self->objects[i].string->length);
				encoded_packet += self->objects[i].string->length;
				break;
			case KELIMELIK_OBJECT_ARRAY:
				*(uint32_t *)encoded_packet = htonl(self->objects[i].array->item_count);
				*(uint8_t *)(encoded_packet += 4) = self->objects[i].array->type;
				encoded_packet++;
				for (uint64_t j=0; j<self->objects[i].array->item_count; j++) {
					switch (self->objects[i].array->type) {
						case KELIMELIK_OBJECT_UINT64:
							*(uint64_t *)encoded_packet = htonll(self->objects[i].array->uint64s[j]);
							encoded_packet += 8;
							break;
						case KELIMELIK_OBJECT_UINT32:
							*(uint32_t *)encoded_packet = htonl(self->objects[i].array->uint32s[j]);
							encoded_packet += 4;
							break;
						case KELIMELIK_OBJECT_UINT8:
							*(uint8_t *)(encoded_packet++) = self->objects[i].array->uint8s[j];
							break;
						case KELIMELIK_OBJECT_STRING:
							*(uint16_t *)encoded_packet = htons(self->objects[i].array->strings[j]->length);
							encoded_packet += 2;
							memcpy(encoded_packet, self->objects[i].array->strings[j]->string, self->objects[i].array->strings[j]->length);
							encoded_packet += self->objects[i].array->strings[j]->length;
							break;
						case KELIMELIK_OBJECT_ARRAY:
						default:
							// Arrays can't contain arrays
							free(encoded_packet_beginning);
							return _KELIMELIK_ERROR(KELIMELIK_ERROR_INVALID_TYPE, 0);
					}
				}
				break;
			default:
				free(encoded_packet_beginning);
				return (kelimelik_error){
					.kelimelik_errno = KELIMELIK_ERROR_INVALID_TYPE
				};
		}
	}
	assert((encoded_packet - encoded_packet_beginning) == size);
	*out_bytes = encoded_packet_beginning;
	*out_len = size;
	return _KELIMELIK_SUCCESS;
}
