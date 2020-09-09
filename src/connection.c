#include <kelimelik.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include "kelimelik-private.h"
#include <sys/types.h>

// Doesn't do anything special, just returns a socket that is
// connected to the official Kelimelik server.
kelimelik_error kelimelik_connection_new(int *fd_out) {
	// Check arguments
	if (!fd_out) {
		return _KELIMELIK_ERROR_INVALID_ARGUMENT(0);
	}

	// Create socket
	int fd;
	if (((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)) {
		return _KELIMELIK_ERROR_SYSCALL(socket);
	}

	// Get host
	//struct hostent *server = gethostbyname("kelimelikserver.he2apps.com");
	struct hostent *server = gethostbyname("141.98.204.163");
	if (!server) {
		return _KELIMELIK_ERROR_SYSCALL(gethostbyname);
	}

	// Create socket address
	struct sockaddr_in server_address;
	memset(&server_address, 0, sizeof(server_address));
	server_address.sin_family = AF_INET;
	bcopy(server->h_addr, &server_address.sin_addr.s_addr, server->h_length);
	server_address.sin_port = htons(443);

	// Connect
	if (connect(fd, (struct sockaddr *)&server_address, sizeof(server_address)) == -1) {
		return _KELIMELIK_ERROR_SYSCALL(connect);
	}
	
	// Finalize
	*fd_out = fd;
	return _KELIMELIK_SUCCESS;
}