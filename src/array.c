#include "kelimelik-private.h"
#include <string.h>

static int kelimelik_array_bytes_for_type(enum kelimelik_object_type type) {
	switch (type) {
		case KELIMELIK_OBJECT_UINT8:
			return 1;
		case KELIMELIK_OBJECT_UINT32:
			return 4;
		case KELIMELIK_OBJECT_UINT64:
			return 8;
		case KELIMELIK_OBJECT_STRING:
			return sizeof(kelimelik_string *);
		default:
			return -1;
	}
}

kelimelik_error kelimelik_array_free(kelimelik_array *self) {
	if (!self) {
		return _KELIMELIK_ERROR_INVALID_ARGUMENT(0);
	}
	if (self->type == KELIMELIK_OBJECT_STRING) {
		for (uint64_t i=0; i<self->item_count; i++) {
			kelimelik_string_free(self->strings[i]);
		}
	}
	free(self);
	return _KELIMELIK_SUCCESS;
}

static size_t kelimelik_count_pointers(void **pointers) {
	size_t count = 0;
	while (*(pointers++)) count++;
	return count;
}

kelimelik_error kelimelik_string_array_new_v2(kelimelik_array **out, const char **strings, const size_t count) {
	if (!count) {
		return kelimelik_array_new(out, KELIMELIK_OBJECT_STRING, NULL, 0);
	}
	kelimelik_string **kstrings = malloc(count * sizeof(*strings));
	if (!kstrings) {
		return _KELIMELIK_ERROR_SYSCALL(malloc);
	}
	kelimelik_error error;
	for (size_t i=0; i<count; i++) {
		error = kelimelik_string_new_v1(&kstrings[i], strings[i]);
	}
	error = kelimelik_string_array_new_v4(out, kstrings, count);
	free(kstrings);
	return error;
}

kelimelik_error kelimelik_string_array_new_v1(kelimelik_array **out, const char **strings) {
	return kelimelik_string_array_new_v2(out, strings, kelimelik_count_pointers((void **)strings));
}

kelimelik_error kelimelik_string_array_new_v4(kelimelik_array **out, kelimelik_string **strings, const size_t count) {
	return kelimelik_array_new(
		out,
		KELIMELIK_OBJECT_STRING,
		strings,
		count * kelimelik_array_bytes_for_type(KELIMELIK_OBJECT_STRING)
	);
}

kelimelik_error kelimelik_string_array_new_v3(kelimelik_array **out, kelimelik_string **strings) {
	return kelimelik_string_array_new_v4(out, strings, kelimelik_count_pointers((void **)strings));
}

#define KELIMELIK_ARRAY_UINT_INITIALIZER(bits) \
	kelimelik_error _KELIMELIK_CONCAT_3(kelimelik_uint, bits, _array_new)(kelimelik_array **out, const _KELIMELIK_CONCAT_3(uint, bits, _t) *values, const size_t count) { \
		return kelimelik_array_new(out, _KELIMELIK_CONCAT_2(KELIMELIK_OBJECT_UINT, bits), values, count * (bits/8)); \
	}

KELIMELIK_ARRAY_UINT_INITIALIZER(8)
KELIMELIK_ARRAY_UINT_INITIALIZER(32)
KELIMELIK_ARRAY_UINT_INITIALIZER(64)

#undef KELIMELIK_ARRAY_UINT_INITIALIZER

kelimelik_error kelimelik_array_new(kelimelik_array **out, enum kelimelik_object_type type, const void *values, const size_t size) {
	if (!out) {
		return _KELIMELIK_ERROR_INVALID_ARGUMENT(0);
	}
	else if (!values && size) {
		return _KELIMELIK_ERROR_INVALID_ARGUMENT(2);
	}
	int len_per_item = kelimelik_array_bytes_for_type(type);
	if (len_per_item == -1) {
		return _KELIMELIK_ERROR_INVALID_ARGUMENT(1);
	}
	kelimelik_array *array = malloc(sizeof(*array) + size);
	array->type = type;
	array->item_count = (size / len_per_item);
	memcpy(array + 1, values, size);
	*out = array;
	return _KELIMELIK_SUCCESS;
}
