#include "kelimelik-private.h"
#include <stdio.h>
#include <string.h>

#define KELIMELIK_SWAP_ARRAYS 0

void kelimelik_parser_free_old_packets(kelimelik_parser *self) {
	for (size_t i=0; i<self->packet_count; i++) {
		if (self->packets[i]) {
			kelimelik_packet_free(self->packets[i]);
			self->packets[i] = NULL;
		}
	}
}

void kelimelik_parser_free(kelimelik_parser *self) {
	if (self->packet_buffer) free(self->packet_buffer);
	kelimelik_parser_free_old_packets(self);
	free(self);
}

void kelimelik_parser_reset(kelimelik_parser *parser) {
	parser->packet_buffer = NULL;
	parser->bytes_remaining = 4;
	parser->state = KELIMELIK_PARSER_WAITING_FOR_SIZE;
	parser->index = 0;
}

kelimelik_error kelimelik_parser_new(kelimelik_parser **out) {
	kelimelik_parser *parser = malloc(sizeof(**out));
	parser->packet_count = 0;
	parser->packets = NULL;
	kelimelik_parser_reset(parser);
	*out = parser;
	return _KELIMELIK_SUCCESS;
}

kelimelik_error kelimelik_parser_decode(
	uint8_t *bytes,
	size_t bytes_length,
	kelimelik_packet **new_packet
) {
	// Check the input
	if (!bytes) return _KELIMELIK_ERROR_INVALID_ARGUMENT(0);
	if (bytes_length < 7) return _KELIMELIK_ERROR_INVALID_ARGUMENT(1);
	if (!new_packet) return _KELIMELIK_ERROR_INVALID_ARGUMENT(2);

	// Get the packet size and the header size
	uint32_t packet_size = ntohl(*(uint32_t *)bytes);
	uint16_t header_size = ntohs(*(uint16_t *)(bytes += 4));
	bytes_length -= 7;
	if (bytes_length < header_size) {
		return _KELIMELIK_ERROR_INVALID_ARGUMENT(0);
	}

	// Create the header object
	kelimelik_string *header;
	kelimelik_error error = kelimelik_string_new_v2(&header, (bytes += 2), header_size);
	bytes += header_size;
	bytes_length -= header_size;

	// Create the packet object
	kelimelik_packet *packet;
	uint8_t object_count = *(bytes++);
	error = kelimelik_packet_new_v2(&packet, header, object_count);

	// Starting parsing the other objects in the packet
	uint16_t i;
	struct {
		uint32_t item_count;
		struct {
			uint16_t length;
			size_t offset;
		} items[];
	} *string_array_data = NULL;
	for (i=0; i<object_count; i++) {
		// If no bytes are left, break, since we can't safely read
		// the type byte
		if (!bytes_length) break;
		bytes_length--;

		// Get the type value
		uint8_t type = *(bytes++);

		// First switch: Calculate the size of the object in bytes.
		// This is needed to avoid out-of-bounds reads.
		size_t bytes_needed = 0;
		switch (type) {
			// Integers are easy, the bytes needed is always the same.
			case KELIMELIK_OBJECT_UINT8:
				bytes_needed = 1;
				break;
			case KELIMELIK_OBJECT_UINT32:
				bytes_needed = 4;
				break;
			case KELIMELIK_OBJECT_UINT64:
				bytes_needed = 8;
				break;

			// Strings are a bit more complicated, the size depends on
			// the next 2 bytes.
			case KELIMELIK_OBJECT_STRING:
				if (bytes_length < 2) bytes_needed = 2;
				else bytes_needed = ntohs(*(uint16_t *)bytes) + 2;
				break;

			// Arrays involve a bit more work. Continue reading.
			case KELIMELIK_OBJECT_ARRAY: {
				// First 4 bytes is the number of items in the array. The
				// next byte represents the types of those items.
				bytes_needed = 5;
				if (bytes_length < 5) break;
				uint32_t count = ntohl(*(uint32_t *)bytes);
				uint8_t type_in_array = *(uint8_t *)(bytes + 4);

				// What we do next depends on the item type.
				switch (type_in_array) {
					// An array technically *can* contain other arrays, but this
					// is never used, so it's not implemented.
					case KELIMELIK_OBJECT_ARRAY:
						bytes_needed = 0;
						break;

					// Integers are easy, since the size can be easily calculated
					// by multiplying the count with the size of the integer type.
					case KELIMELIK_OBJECT_UINT32:
						bytes_needed += (count * 4);
						break;
					case KELIMELIK_OBJECT_UINT64:
						bytes_needed += (count * 8);
						break;
					case KELIMELIK_OBJECT_UINT8:
						bytes_needed += count;
						break;

					// When an array contains strings, things get much more complicated.
					// Since it is basically impossible to know the number of bytes needed
					// for String arrays without parsing, half of the parsing is done here.
					case KELIMELIK_OBJECT_STRING:
						if (string_array_data) free(string_array_data);
						string_array_data = malloc(sizeof(*string_array_data) + (sizeof(*(string_array_data->items)) * count));
						string_array_data->item_count = count;
						for (uint64_t i=0; i<count; i++) {
							size_t bytes_remaining = bytes_length - bytes_needed;
							if ((bytes_remaining > bytes_length) || (bytes_remaining < 2)) {
								bytes_needed = 0;
								break;
							}
							uint16_t len = ntohs(*(uint16_t *)(bytes + bytes_needed));
							string_array_data->items[i].length = len;
							string_array_data->items[i].offset = bytes_needed + 2;
							bytes_needed += len + 2;
						}
						break;
				}
				break;
			}
			default:
				fprintf(stderr, "[libkelimelik] Attempted to parse unknown type: %u\n", type);
				break;
		}
		if (!bytes_needed) {
			// Invalid type
			error = _KELIMELIK_ERROR(KELIMELIK_ERROR_INVALID_TYPE, 0);
			break;
		} 
		else if (bytes_needed > bytes_length) {
			// Not enough bytes left
			error = _KELIMELIK_ERROR_INVALID_ARGUMENT(0);
			break;
		}

		// Second switch: Read the actual data and place it in the packet.
		switch (type) {
			case KELIMELIK_OBJECT_UINT8:
				kelimelik_packet_set_uint8(packet, i, *(bytes++));
				break;
			case KELIMELIK_OBJECT_UINT32:
				kelimelik_packet_set_uint32(packet, i, ntohl(*(uint32_t *)bytes));
				bytes += 4;
				break;
			case KELIMELIK_OBJECT_UINT64:
				kelimelik_packet_set_uint64(packet, i, ntohll(*(uint64_t *)bytes));
				bytes += 8;
				break;
			case KELIMELIK_OBJECT_STRING: {
				bytes += 2;
				kelimelik_string *header_str;
				error = kelimelik_string_new_v2(&header_str, bytes, bytes_needed - 2);
				bytes += bytes_needed - 2;
				kelimelik_packet_set_string_v2(packet, i, header_str);
				break;
			}
			case KELIMELIK_OBJECT_ARRAY: {
				//FIXME: Arrays not implemented
				kelimelik_array *array = NULL;
				uint32_t count = ntohl(*(uint32_t *)bytes);
				uint8_t swapped_bytes[8];
				switch (*(bytes + 4)) {
					case KELIMELIK_OBJECT_UINT8:
					case KELIMELIK_OBJECT_UINT32:
					case KELIMELIK_OBJECT_UINT64:
						error = kelimelik_array_new(
							&array,
							*(bytes + 4), 
							(void *)&bytes[5],
							bytes_needed - 5
						);
						break;
					case KELIMELIK_OBJECT_STRING:
					default: {
						kelimelik_string **strings = malloc(sizeof(*strings) * count);
						for (size_t i=0; i<string_array_data->item_count; i++) {
							error = kelimelik_string_new_v2(
								#if KELIMELIK_SWAP_ARRAYS
								&strings[string_array_data->item_count-i-1],
								#else
								&strings[i],
								#endif
								bytes + string_array_data->items[i].offset,
								string_array_data->items[i].length
							);
						}
						error = kelimelik_string_array_new_v4(
							&array,
							strings,
							count
						);
						free(string_array_data);
						string_array_data = NULL;
						free(strings);
					}
				}
				switch (array->type) {
					case KELIMELIK_OBJECT_UINT32:
						for (uint64_t i=0; i<array->item_count; i++) {
							array->uint32s[i] = ntohl(array->uint32s[i]);
						}
						break;
					case KELIMELIK_OBJECT_UINT64:
						for (uint64_t i=0; i<array->item_count; i++) {
							array->uint64s[i] = ntohll(array->uint64s[i]);
						}
					default:
						break;
				}
				#if KELIMELIK_SWAP_ARRAYS
				for (size_t i=0, j=array->item_count-1; i<array->item_count; i++) {
					switch (array->type) {
						#define case(x) case _KELIMELIK_CONCAT_2(KELIMELIK_OBJECT_UINT, x): { \
							_KELIMELIK_CONCAT_3(uint, x, _t) tmp = array-> _KELIMELIK_CONCAT_3(uint, x, s) [i]; \
							array-> _KELIMELIK_CONCAT_3(uint, x, s) [i] = array-> _KELIMELIK_CONCAT_3(uint, x, s) [j]; \
							array-> _KELIMELIK_CONCAT_3(uint, x, s) [j] = tmp; \
							break; \
						}
						case(8)
						case(32)
						case(64)
						#undef case
						default:
							break;
					}
				}
				#endif
				bytes += bytes_needed;
				kelimelik_packet_set_array(packet, i, array);
				break;
			}
		}
		bytes_length -= bytes_needed;
	}
	if (string_array_data) free(string_array_data);
	if (i != object_count) {
		kelimelik_packet_free(packet);
		return error;
	}
	if (bytes_length > 0) {
		fprintf(stderr, "[libkelimelik] %zu bytes remaining after parsing\n", bytes_length);
	}
	*new_packet = packet;
	return _KELIMELIK_SUCCESS;
}

kelimelik_error kelimelik_parser_advance(
	kelimelik_parser *self,
	uint8_t *bytes,
	size_t bytes_length,
	kelimelik_packet ***new_packets_pt,
	size_t *new_packets_length_pt
) {
	#warning FIXME: kelimelik_parser_advance doesn't work if (bytes_length > 1)
	size_t new_packets_count = 0;
	kelimelik_error error = _KELIMELIK_SUCCESS;
	while (bytes_length) {
		uint8_t *target_buffer = (
			(self->state == KELIMELIK_PARSER_WAITING_FOR_SIZE) ?
			self->packet_size_buffer :
			self->packet_buffer
		);
		uint32_t bytes_to_read = (
			(bytes_length > self->bytes_remaining) ?
			self->bytes_remaining :
			bytes_length
		);
		bytes_length -= bytes_to_read;
		memcpy(target_buffer + self->index, bytes, bytes_to_read);
		self->bytes_remaining -= bytes_to_read;
		if (!self->bytes_remaining) {
			uint32_t packet_size = ntohl(*(uint32_t *)&self->packet_size_buffer[0]);
			if ((self->state = !self->state) == KELIMELIK_PARSER_WAITING_FOR_SIZE) {
				self->index = 0;
				kelimelik_packet *packet;
				kelimelik_parser_free_old_packets(self);
				error = kelimelik_parser_decode(
					self->packet_buffer,
					packet_size + 4,
					&packet
				);
				//free(self->packet_buffer);
				//self->packet_buffer = NULL;
				if (!KELIMELIK_IS_ERROR(error)) {
					new_packets_count++;
					if (new_packets_count > self->packet_count) {
						size_t size = new_packets_count * sizeof(*(self->packets));
						if (!self->packets) {
							self->packets = malloc(size);
						}
						else {
							self->packets = realloc(self->packets, size);
						}
						self->packet_count = new_packets_count;
					}
					self->packets[new_packets_count-1] = packet;
				}
				else {
					fprintf(stderr, "[libkelimelik] Parser error: %s\n", kelimelik_strerror(error));
				}
				self->bytes_remaining = 4;
			}
			else {
				self->packet_buffer = malloc(packet_size+4);
				self->bytes_remaining = packet_size;
				self->index = 4;
				memcpy(self->packet_buffer, self->packet_size_buffer, 4);
			}
		}
		else {
			self->index += bytes_to_read;
		}
	}
	*new_packets_length_pt = new_packets_count;
	*new_packets_pt = self->packets;
	return error;
}

kelimelik_error kelimelik_parser_advance_single(
	kelimelik_parser *self,
	uint8_t byte,
	kelimelik_packet ***new_packets,
	size_t *new_packets_length
) {
	return kelimelik_parser_advance(
		self,
		&byte,
		1,
		new_packets,
		new_packets_length
	);
}