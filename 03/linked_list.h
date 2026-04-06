#ifndef LINKED_LIST_H
#define LINKED_LIST_H

typedef int (*cmp_func)(void*, void*);

typedef struct List List;
typedef struct ListIterator ListIterator;

List* list_create(cmp_func cmp);
void list_insert(List* list, void* value);
int list_remove_value(List* list, void* value);

ListIterator* it_begin(List* list);
void* it_current(ListIterator* iterator);
void* it_remove_current(ListIterator* iterator);
void* it_next(ListIterator* iterator);
int it_has_next(ListIterator* iterator);
void it_delete(ListIterator* iterator);

#endif