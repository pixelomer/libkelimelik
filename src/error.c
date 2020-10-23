#include <kelimelik.h>
#include <string.h>

const char *other_errors[] = {
	"The packet contained one or more objects with no specified type.",
	"Invalid argument at index %d.",
	"Encountered an invalid type while parsing.",
	"This function is not implemented.",
	"Invalid format passed to kelimelik_verify_packet().",
	"Packet format doesn't match the specified format."
};

const char *function_names[] = {
	"socket",
	"gethostbyname",
	"connect",
	"malloc"
};

static char error_buffer[100];

char *kelimelik_strerror_buf(kelimelik_error error, char *buffer, size_t len) {
	if (error.kelimelik_errno == 0) {
		strncpy(buffer, "The operation succeeded.", len);
	}
	else if (error.kelimelik_errno > 0) {
		snprintf(
			buffer,
			len,
			other_errors[error.kelimelik_errno-1],
			error.details
		);
	}
	else {
		snprintf(
			buffer,
			len,
			"%s() failed: %s",
			function_names[-error.kelimelik_errno+1],
			strerror(error.syscall_errno)
		);
	}
	return buffer;
}

const char *kelimelik_strerror(kelimelik_error error) {
	return kelimelik_strerror_buf(error, error_buffer, sizeof(error_buffer));
}