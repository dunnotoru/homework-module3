#include "linked_list.h"

#include <stdlib.h>

typedef struct ListNode {
  void* value;
  struct ListNode* prev;
  struct ListNode* next;
} ListNode;

struct List {
  ListNode* head;
  cmp_func cmp;
};

struct ListIterator {
  List* list;
  ListNode* current;
};

int default_cmp(void*, void*) { return -1; }

List* list_create(cmp_func cmp) {
  List* list = calloc(1, sizeof(List));
  list->cmp = cmp == NULL ? default_cmp : cmp;
  list->head = NULL;
  return list;
}

ListNode* create_node(void* value) {
  ListNode* node = calloc(1, sizeof(ListNode));
  node->value = value;
  node->next = node;
  node->prev = node;
  return node;
}

void insert_between(ListNode* prev, ListNode* next, ListNode* insert) {
  insert->prev = prev;
  insert->next = next;

  prev->next = insert;
  next->prev = insert;
}

void list_insert(List* list, void* value) {
  if (list == NULL) {
    return;
  }

  ListNode* node = create_node(value);

  if (list->head == NULL) {
    list->head = node;
    return;
  }

  if (list->cmp(value, list->head->value) < 0) {
    insert_between(list->head->prev, list->head, node);
    list->head = node;
    return;
  }

  ListNode* cur = list->head;
  do {
    if (list->cmp(value, cur->value) < 0) {
      insert_between(cur->prev, cur, node);
      return;
    }

    cur = cur->next;
  } while (cur != list->head);

  insert_between(list->head->prev, list->head, node);

  return;
}

void* remove_node(List* list, ListNode* node) {
  void* value = node->value;
  if (list->head == node && node->next == node) {
    free(list->head);
    list->head = NULL;
    return value;
  }

  ListNode* prev = node->prev;
  ListNode* next = node->next;

  if (list->head == node) {
    list->head = node->next;
  }

  prev->next = next;
  next->prev = prev;

  free(node);
  return value;
}

int list_remove_value(List* list, void* value) {
  if (!list || !list->head) {
    return 0;
  }

  ListNode* cur = list->head;
  do {
    ListNode* next = cur->next;
    if (cur->value == value) {
      remove_node(list, cur);
    }

    cur = next;
  } while (cur != list->head);

  return 1;
}

ListIterator* it_begin(List* list) {
  ListIterator* it = calloc(1, sizeof(ListIterator));
  if (list == NULL) {
    return it;
  }

  it->list = list;
  it->current = list->head;

  return it;
}

void* it_current(ListIterator* iterator) {
  if (iterator->current) {
    return iterator->current->value;
  }

  return NULL;
}

void* it_next(ListIterator* iterator) {
  if (!iterator || !iterator->current || !iterator->list) {
    return NULL;
  }

  void* tmp = iterator->current->value;

  if (iterator->current->next == iterator->list->head) {
    iterator->current = NULL;
  } else {
    iterator->current = iterator->current->next;
  }

  return tmp;
}

int it_has_next(ListIterator* iterator) {
  if (iterator->current == NULL) {
    return 0;
  }

  return 1;
}



void* it_remove_current(ListIterator* iterator) {
  if (!iterator || !iterator->current || !iterator->list) {
    return NULL;
  }

  ListNode* next = iterator->current->next;

  void* value = remove_node(iterator->list, iterator->current);

  if (iterator->list->head) {
    iterator->current = next;
  } else {
    iterator->current = NULL;
  }

  return value;
}

void it_delete(ListIterator* iterator) {
  free(iterator); 
}