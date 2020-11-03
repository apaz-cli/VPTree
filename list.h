#ifndef __LIST
#define __LIST

#include <stdbool.h>
#include <stdlib.h>

struct List {
    void** buffer;
    size_t capacity;
    size_t size;
};
typedef struct List List;

/****************/
/* LIST METHODS */
/****************/

// Bool methods return true if successful, false if not.

bool List_create(List* l);

bool List_create_cap(List* l, size_t capacity);

// The create_with methods use those items as the internal buffer. This just constructs literally with what's given, and always returns true.
bool List_create_with(List* l, void** items, size_t num_items, size_t capacity);

void List_destroy_nofree(List* l);

void List_destroy(List* l);

bool List_add(List* l, void* item);

// Returns NULL if not successful
void* List_get(List* l, size_t idx);

void* List_get_unchecked(List* l, size_t idx);

void* List_remove(List* l, size_t idx);

size_t List_size(List* l);

bool List_trim(List* l);

/*
ret<0: p1 goes before p2
ret=0: p1 and p2 could go either way
ret>0: p1 goes after p2
*/
void List_sort(List* l, int (*comparator)(const void* first, const void* second));

#endif
