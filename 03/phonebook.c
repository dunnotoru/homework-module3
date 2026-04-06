#include "phonebook.h"
#include "linked_list.h"

#include <fcntl.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void update_value(Contact *c, char key, const char *value);

ContactStorage *create_storage() {
  ContactStorage *storage = malloc(sizeof(ContactStorage));
  storage->list = NULL;
  storage->size = 0;
  storage->nextid = 1;
  return storage;
}

void delete_storage(ContactStorage *storage) {
  if (storage->list == NULL) {
    free(storage);
    return;
  }

  ListIterator *it = it_begin(storage->list);

  while (it_has_next(it)) {
    Contact *c = it_remove_current(it);
    delete_contact(c);
  }

  it_delete(it);
  free(storage->list);
  free(storage);
}

bool load_data(ContactStorage *storage) {
  int fd = open("datafile", O_RDONLY | O_CREAT);
  if (fd == -1) {
    return false;
  }

  int retval = -1;
  do {
    printf("loading\n");
    Contact *stored = calloc(1, sizeof(Contact));
    retval = read(fd, stored, sizeof(Contact));
    if (retval != sizeof(Contact)) {
      free(stored);
      close(fd);
      return false;
    }

    if (retval == -1) {
      close(fd);
      return false;
    }

    add_contact(storage, stored);
  } while (retval > 0);

  close(fd);
  return true;
}

bool dump_data(ContactStorage *storage) {
  creat("datafile", S_IRUSR | S_IWUSR | S_IROTH | S_IWOTH);
  int fd = open("datafile", O_WRONLY);
  ListIterator *it = it_begin(storage->list);
  while (it_has_next(it)) {
    Contact *contact = it_next(it);
    
    int retval = write(fd, contact, sizeof(Contact));
    printf("save %d\n", retval);
    if (retval == -1) {
      it_delete(it);
      close(fd);
      return false;
    }
  }
  it_delete(it);
  close(fd);
  return true;
}

int lastname_cmp(void *left, void *right) {
  return strcmp(((Contact *)left)->lastname, ((Contact *)right)->lastname);
}

void add_contact(ContactStorage *storage, Contact *contact) {
  if (storage == NULL) {
    return;
  }

  if (storage->list == NULL) {
    storage->list = list_create(lastname_cmp);
  }

  list_insert(storage->list, contact);

  contact->id = storage->nextid++;
  storage->size++;
}

void remove_contact(ContactStorage *storage, Contact *contact) {
  int retval = list_remove_value(storage->list, contact);
  if (retval) {
    storage->size--;
  }
}

Contact *get_contact(const ContactStorage *storage, int id) {
  ListIterator *it = it_begin(storage->list);
  Contact *result = NULL;
  while (it_has_next(it)) {
    Contact *c = it_next(it);
    if (id == c->id) {
      result = c;
      break;
    }
  }

  it_delete(it);

  return result;
}

void set_value(char *field, const char *value) {
  strncpy(field, value, FIELD_SIZE);
  field[FIELD_SIZE - 1] = '\0';
}

Contact *create_contact(const char *firstname, const char *lastname,
                        const char *data_format, ...) {
  if (firstname == NULL || lastname == NULL) {
    return NULL;
  }

  Contact *c = calloc(1, sizeof(Contact));
  if (c == NULL) {
    return NULL;
  }

  set_value(c->firstname, firstname);
  set_value(c->lastname, lastname);

  if (data_format == NULL) {
    return c;
  }

  va_list args;
  va_start(args, data_format);
  size_t format_size = strlen(data_format);

  for (size_t i = 0; i < format_size; i++) {
    char *value = va_arg(args, char *);
    update_value(c, data_format[i], value);
  }

  va_end(args);
  return c;
}

int update_contact(Contact *c, const char *data_format, ...) {
  if (data_format == NULL || c == NULL) {
    return 0;
  }

  va_list args;
  va_start(args, data_format);
  size_t format_size = strlen(data_format);

  for (size_t i = 0; i < format_size; i++) {
    char *value = va_arg(args, char *);
    update_value(c, data_format[i], value);
  }

  va_end(args);

  return 0;
}

void delete_contact(Contact *contact) { free(contact); }

void update_value(Contact *c, char key, const char *value) {
  switch (key) {
  case 'f':
    set_value(c->firstname, value);
    break;
  case 'l':
    set_value(c->lastname, value);
    break;
  case 'm':
    set_value(c->middlename, value);
    break;
  case 'n':
    set_value(c->phone_numbers[0], value);
    break;
  case 'j':
    set_value(c->job, value);
    break;
  case 'p':
    set_value(c->position, value);
    break;
  }
}
