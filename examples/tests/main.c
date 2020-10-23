#include <stdio.h>
#include <kelimelik.h>
#include <string.h>
#include <assert.h>

int main(int argc, char **argv) {
	// Parser tests
	{
		kelimelik_parser *parser;
		assert(!KELIMELIK_IS_ERROR(kelimelik_parser_new(&parser)));
		const char *input = (
			"\x00\x00\x00\x6f"
			"\x00\x0ATestPacket" // Header
			"\x03" // Object count
			"\x08\x00\x00\x00\x05\x07" // String[5]
			"\x00\x05Hello"
			"\x00\x05World"
			"\x00\x07Testing"
			"\x00\x03the"
			"\x00\x07library"
			"\x08\x00\x00\x00\x05\x03" // UInt64[5]
			"\x00\x00\x00\x00\x00\x00\x00\x00"
			"\x00\x00\x00\x00\x00\x00\x00\x01"
			"\x00\x00\x00\x00\x00\x00\x00\x02"
			"\x00\x00\x00\x00\x00\x00\x00\x03"
			"\x00\x00\x00\x00\x00\x00\x00\x04"
			"\x03" // UInt64
			"\x01\x02\x03\x04\x05\x06\x07\x08"
		);
		uint32_t size = ntohl(*(uint32_t *)input) + 4;
		kelimelik_packet **new_packets;
		size_t count = 0;
		kelimelik_parser_advance(parser, (uint8_t *)input, size, &new_packets, &count);
		assert(count == 1);
		assert(new_packets != NULL);
		assert(*new_packets != NULL);
		kelimelik_packet *new_packet = *new_packets;
		assert(new_packet->object_count == 3);
		assert(new_packet->objects[0].type == new_packet->objects[1].type);
		assert(new_packet->objects[0].type == KELIMELIK_OBJECT_ARRAY);
		assert(new_packet->objects[0].array->type == KELIMELIK_OBJECT_STRING);
		assert(new_packet->objects[1].array->type == KELIMELIK_OBJECT_UINT64);
		assert(new_packet->objects[1].array->item_count == new_packet->objects[0].array->item_count);
		assert(new_packet->objects[1].array->item_count == 5);
		assert(strcmp((char *)new_packet->objects[0].array->strings[0]->string, "Hello") == 0);
		assert(strcmp((char *)new_packet->objects[0].array->strings[1]->string, "World") == 0);
		assert(strcmp((char *)new_packet->objects[0].array->strings[2]->string, "Testing") == 0);
		assert(strcmp((char *)new_packet->objects[0].array->strings[3]->string, "the") == 0);
		assert(strcmp((char *)new_packet->objects[0].array->strings[4]->string, "library") == 0);
		assert(new_packet->objects[2].type == KELIMELIK_OBJECT_UINT64);
		assert(new_packet->objects[2].uint64 == 0x0102030405060708);
		for (int i=0; i<5; i++) {
			assert(new_packet->objects[1].array->uint64s[i] == i);
		}
		kelimelik_error error = kelimelik_verify_packet(new_packet, "SQq");
		if (KELIMELIK_IS_ERROR(error)) {
			fprintf(stderr, "Verification error: %s\n", kelimelik_strerror(error));
			abort();
		}
		void *re_encoded;
		size_t re_encoded_size;
		kelimelik_packet_encode(new_packet, &re_encoded, &re_encoded_size);
		assert(memcmp(re_encoded, input, size) == 0);
		free(re_encoded);
		kelimelik_parser_free(parser);
		printf("Parser tests passed\n");
	}
	return 0;
}