#include <stdio.h>
#include <poll.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <limits.h>
#include <assert.h>
#include <kelimelik.h>

static int empty_fd;
static struct pollfd *poll_fds;
static struct {
	int fd;
	bool is_server;
	int peer_index;
	kelimelik_parser *parser;
} *connections;
static int allocated_connection_count = 2;
static int connection_count = 0;

static void initialize_connection(int client_fd) {
	// Establish the Kelimelik connection
	int server_fd;
	kelimelik_connection_new(&server_fd);

	// Variables
	connection_count += 2;
	int server_index;
	int client_index;

	// Find the indexes for the new connections
	{
		int indexes[2];
		if (allocated_connection_count < connection_count) {
			allocated_connection_count = connection_count;
			poll_fds = realloc(poll_fds, sizeof(*poll_fds) * (allocated_connection_count+1));
			connections = realloc(connections, sizeof(*connections) * allocated_connection_count);
			server_index = connection_count - 1;
			client_index = connection_count - 2;
		}
		else {
			int i,j;
			for (i=0,j=0; (i<2) && (j<allocated_connection_count); j++) {
				if (connections[j].fd == -1) {
					indexes[i++] = j;
				}
			}
			assert(i == 2);
			server_index = indexes[0];
			client_index = indexes[1];
		}
	}

	// Connection structure setup
	connections[server_index].fd = server_fd;
	connections[server_index].is_server = true;
	connections[server_index].peer_index = client_index;
	assert(!KELIMELIK_IS_ERROR(kelimelik_parser_new(&(connections[server_index].parser))));
	connections[client_index].fd = client_fd;
	connections[client_index].is_server = false;
	connections[client_index].peer_index = server_index;
	assert(!KELIMELIK_IS_ERROR(kelimelik_parser_new(&(connections[client_index].parser))));

	// Poll structure setup
	poll_fds[server_index+1].fd = server_fd;
	poll_fds[server_index+1].events = POLLIN | POLLHUP;
	poll_fds[client_index+1].fd = client_fd;
	poll_fds[client_index+1].events = POLLIN | POLLHUP;
}

int main(int argc, char **argv) {
	// Create a socket for incoming connections.
	int accept_socket = socket(PF_INET, SOCK_STREAM, 0);
	assert(accept_socket != -1);

	// Create an empty file descriptor
	{
		int pipe_fds[2];
		pipe(pipe_fds);
		empty_fd = pipe_fds[0];
	}

	// Initialization
	{
		// Create the input socket address.
		struct sockaddr_in server_address = {0};

		// Any host may connect.
		server_address.sin_addr.s_addr = htonl(INADDR_ANY);

		// Use IPv4.
		server_address.sin_family = AF_INET;

		// The official server uses port 443, even though that's normally used
		// for HTTPS. This proxy also uses port 443.
		server_address.sin_port = htons(443);

		// Bind the socket to the specified address
		assert(bind(accept_socket, (struct sockaddr *)&server_address, sizeof(server_address)) != -1);

		// Start listening to new connections.
		assert(listen(accept_socket, 64) != -1);
	}

	poll_fds = malloc(sizeof(*poll_fds) * 3);
	connections = malloc(sizeof(*connections) * 2);
	connections[0].fd = -1;
	connections[1].fd = -1;
	poll_fds[0].fd = accept_socket;
	poll_fds[0].events = POLLIN;

	int poll_count;
	while ((poll_count = poll(poll_fds, allocated_connection_count+1, -1)) > 0) {
		while (poll(poll_fds, 1, 0)) {
			printf("New connection\n");
			int fd = accept(accept_socket, NULL, NULL);
			initialize_connection(fd);
			poll_count = 0;
		}
		if (!poll_count) {
			continue;
		}
		for (int i=1; (i<allocated_connection_count+1) && poll_count; i++) {
			if (!(poll_fds[i].revents & (POLLIN | POLLHUP))) continue;
			poll_count--;
			if (poll_fds[i].revents & POLLIN) {
				do {
					uint8_t byte;
					if (read(poll_fds[i].fd, &byte, 1) != 1) {
						break;
					}
					kelimelik_packet **new_packets;
					size_t new_packets_count;
					kelimelik_parser_advance_single(connections[i-1].parser, byte, &new_packets, &new_packets_count);
					if (new_packets_count) {
						kelimelik_packet *packet = *new_packets;
						char *description = kelimelik_packet_description(packet);
						printf("[%s] [#%d] Received: %s\n",
							connections[i-1].is_server ? "Server" : "Client",
							poll_fds[i].fd,
							description
						);
						free(description);
						void *encoded_packet;
						if (!strcmp((char *)packet->header->string, "GameModule_userPurchaseData")) {
							// Modify the purchase data to make the number of coins
							// shown in the client -100. This is used to verify that
							// the proxy works. This value is verified by the server
							// so this hack cannot be used to buy anything with
							// unlimited coins.
							assert(packet->object_count == 8);
							assert(packet->objects[7].type == KELIMELIK_OBJECT_UINT32);
							kelimelik_packet_set_uint32(packet, 7, (uint32_t)-100);
						}
						size_t encoded_packet_len;
						kelimelik_error error = kelimelik_packet_encode(packet, &encoded_packet, &encoded_packet_len);
						if (KELIMELIK_IS_ERROR(error)) {
							fprintf(stderr, "Encode error: %s\n", kelimelik_strerror(error));
							assert(0);
						}
						write(connections[connections[i-1].peer_index].fd, encoded_packet, encoded_packet_len);
						free(encoded_packet);
						printf("Transmitted this data to #%d.\n\n", connections[connections[i-1].peer_index].fd);
					}
				} while (poll(&poll_fds[i], 1, 0) == 1); 
			}
			if (poll_fds[i].revents & POLLHUP) {
				printf("%s #%d disconnected, closing connection to %s #%d\n",
					connections[i-1].is_server ? "Server" : "Client",
					connections[i-1].fd,
					connections[i-1].is_server ? "client" : "server",
					connections[connections[i-1].peer_index].fd
				);
				close(connections[i-1].fd);
				close(connections[connections[i-1].peer_index].fd);
				kelimelik_parser_free(connections[i-1].parser);
				kelimelik_parser_free(connections[connections[i-1].peer_index].parser);
				connections[i-1].fd = -1;
				connections[connections[i-1].peer_index].fd = -1;
				poll_fds[i].fd = empty_fd;
				poll_fds[connections[i-1].peer_index + 1].fd = empty_fd;
				break;
			}
		}
	}
	return EXIT_SUCCESS;
}