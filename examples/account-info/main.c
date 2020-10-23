#include <kelimelik.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>

// Warning: no libkelimelik error handling

int main(int argc, char **argv) {
	if (argc < 3) {
		fprintf(stderr, "Usage: %s <uid> <password>\n", argv[0]);
		return EXIT_FAILURE;
	}
	int fd;
	kelimelik_connection_new(&fd);
	kelimelik_packet *packet;
	kelimelik_packet_new_v1(&packet, "GameModule_requestLogin", 3);
	kelimelik_packet_set_uint32(packet, 0, atoi(argv[1]));
	kelimelik_packet_set_string_v1(packet, 1, argv[2]);
	kelimelik_packet_set_uint32(packet, 2, 238);
	void *encoded_packet;
	size_t encoded_packet_length;
	kelimelik_packet_encode(packet, &encoded_packet, &encoded_packet_length);
	kelimelik_packet_free(packet);
	send(fd, encoded_packet, encoded_packet_length, 0);
	free(encoded_packet);
	kelimelik_parser *parser;
	kelimelik_parser_new(&parser);
	char *email_address = NULL;
	char *username = NULL;
	uint32_t win_ratio = 0;
	uint32_t won = 0;
	uint32_t total = 0;
	while (1) {
		uint8_t val;
		if (recv(fd, &val, 1, 0) != 1) {
			fprintf(stderr, "recv() failed.\n");
			return EXIT_FAILURE;
		}
		kelimelik_packet *new_packet;
		assert(!KELIMELIK_IS_ERROR(kelimelik_parser_advance_single(parser, val, &new_packet)));
		if (new_packet) {
			if (!strcmp((char *)new_packet->header->string, "GameModule_loginRefused")) {
				fprintf(stderr, "Login refused.\n");
				return EXIT_FAILURE;
			}
			else if (!strcmp((char *)new_packet->header->string, "GameModule_loginAccepted")) {
				if (email_address) {
					free(email_address);
				}
				email_address = malloc(new_packet->objects[3].string->length + 1 + new_packet->objects[1].string->length + 1);
				if (!email_address) {
					perror("malloc");
					return EXIT_FAILURE;
				}
				username = email_address + new_packet->objects[3].string->length + 1;
				memcpy(email_address, new_packet->objects[3].string->string, new_packet->objects[3].string->length + 1);
				memcpy(username, new_packet->objects[1].string->string, new_packet->objects[1].string->length + 1);
			}
			else if (!strcmp((char *)new_packet->header->string, "GameModule_userProfile")) {
				win_ratio = new_packet->objects[5].uint32;
				won = new_packet->objects[2].uint32;
				total = new_packet->objects[1].uint32;
			}
			//char *description = kelimelik_packet_description(new_packet);
			//printf("Received packet: %s\n", description);
			//free(description);
			if (strcmp((char *)new_packet->header->string, "GameModule_userPurchaseData") == 0) {
				// This is the last packet
				break;
			}
		}
	}
	kelimelik_parser_free(parser);
	parser = NULL;
	printf(
		"Username ........ %s\n"
		"Email address ... %s\n"
		"Win ratio ....... %u%%\n"
		"Completed games . %u\n"
		"Won games ....... %u\n"
		"Lost games ...... %u\n",
		username, email_address, win_ratio, total, won, total-won
	);
	return EXIT_SUCCESS;
}
