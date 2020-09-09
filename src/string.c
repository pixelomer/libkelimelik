#include <string.h>
#include "kelimelik-private.h"

void kelimelik_string_free(kelimelik_string *string) {
	free(string);
}

kelimelik_error kelimelik_string_new_v1(kelimelik_string **out, const char *c_string) {
	if (!out) return _KELIMELIK_ERROR_INVALID_ARGUMENT(0);
	unsigned long c_string_len = strlen(c_string);
	if (c_string_len > 0xFFFF) _KELIMELIK_ERROR_INVALID_ARGUMENT(1);
	kelimelik_string *string = malloc(sizeof(*string) + c_string_len + 1);
	memcpy(string + 1, c_string, c_string_len + 1);
	string->length = c_string_len;
	*out = string;
	return _KELIMELIK_SUCCESS;
}

kelimelik_error kelimelik_string_new_v2(kelimelik_string **out, void *bytes, size_t len) {
	if (!out) return _KELIMELIK_ERROR_INVALID_ARGUMENT(0);
	if (len && !bytes) return _KELIMELIK_ERROR_INVALID_ARGUMENT(1);
	kelimelik_string *string = malloc(sizeof(*string) + len + 1);
	memcpy(string + 1, bytes, len);
	((uint8_t *)(string + 1))[len] = 0;
	string->length = len;
	*out = string;
	return _KELIMELIK_SUCCESS;
}