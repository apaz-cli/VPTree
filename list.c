#include "list.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

// Metaparameters

#define LIST_DEFAULT_CAPACITY 16

static inline size_t resize_size(size_t capacity) {
    return (int)(1.2 * capacity + 24);
}

static inline list_t* resize_buffer(list_t* buffer, size_t capacity) {
    size_t newsize = resize_size(capacity);
    list_t* newbuff = (list_t*)realloc(buffer, newsize * sizeof(list_t));
    return newbuff;
}

/****************/
/* LIST METHODS */
/****************/

bool List_create_cap(List* l, size_t capacity) {
    list_t* buff_alloc = (list_t*)malloc(capacity * sizeof(list_t));
    if (buff_alloc == NULL) {
        return false;
    }

    l->buffer = buff_alloc;
    l->capacity = capacity;
    l->size = 0;
    return true;
}

bool List_create(List* l) {
    return List_create_cap(l, LIST_DEFAULT_CAPACITY);
}

bool List_create_with(List* l, list_t* items, size_t num_items, size_t capacity) {
    l->buffer = items;
    l->size = num_items;
    l->capacity = capacity;
    return true;
}

void List_destroy_nofree(List* l) {
    l->buffer = NULL;
    l->size = 0;
    l->capacity = 0;
}

void List_destroy(List* l) {
    free(l->buffer);
    List_destroy_nofree(l);
}

bool List_add(List* l, list_t item) {
    // Resize the buffer if necessary
    if (l->size == l->capacity) {
        size_t expected_cap = resize_size(l->capacity);
        list_t* newbuff = resize_buffer(l->buffer, l->capacity);
        if (newbuff == NULL) {
            return false;
        } else {
            l->buffer = newbuff;
            l->capacity = expected_cap;
        }
    }

    // Add the item now that there's space
    l->buffer[l->size] = item;
    ++l->size;
    return true;
}

list_t List_get(List* l, size_t idx) {
    if (idx >= l->size) return NULL;
    return l->buffer[idx];
}

list_t List_get_unchecked(List* l, size_t idx) {
    return l->buffer[idx];
}

list_t List_remove(List* l, size_t idx) {
    if (idx >= l->size) return NULL;

    list_t* toRemove = l->buffer + idx;
    list_t item = *toRemove;

    // If it was the last item of the list, we can just decrease size and return
    if (toRemove == l->buffer + (l->size - 1)) {
        l->size--;
        return item;
    }

    // Otherwise, slide the rest of the array over the item.
    list_t cont = toRemove + 1;
    memcpy(toRemove, cont, ((l->capacity - idx) - 1) * sizeof(void*));

    --l->size;
    return item;
}

size_t
List_size(List* l) {
    return l->size;
}

bool List_trim(List* l) {
    list_t* buff_alloc = (list_t*)realloc(l->buffer, l->size * sizeof(list_t));
    if (buff_alloc == NULL) return false;
    l->buffer = buff_alloc;
    return true;
}

// First and second are of type list_t
void List_sort(List* l, int (*comparator)(const void* first, const void* second)) {  // comparator_t expands to be named comparator
    qsort(l->buffer, l->size, sizeof(void*), comparator);
}
