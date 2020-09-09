#include "kelimelik-private.h"

kelimelik_error kelimelik_object_free(kelimelik_object *object) {
	switch (object->type) {
		case KELIMELIK_OBJECT_UINT8:
		case KELIMELIK_OBJECT_UINT32:
		case KELIMELIK_OBJECT_UINT64:
			break;
		case KELIMELIK_OBJECT_STRING:
			kelimelik_string_free(object->string);
			break;
		case KELIMELIK_OBJECT_ARRAY:
			kelimelik_array_free(object->array);
			break;
		default:
			fprintf(stderr, "[libkelimelik] Attempted to free an object of unknown type: %d\n", object->type);
			break;
	}
	return _KELIMELIK_SUCCESS;
}

kelimelik_error kelimelik_objects_free(kelimelik_object *first_object) {
	kelimelik_object *object = first_object;
	if (!object) {
		return _KELIMELIK_ERROR_INVALID_ARGUMENT(0);
	}
	do kelimelik_object_free(object);
	while ((object = object->next));
	return _KELIMELIK_SUCCESS;
}