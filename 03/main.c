#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "linked_list.h"
#include "phonebook.h"

#define A_FIELD_COUNT 5
#define U_FIELD_COUNT 7

ContactStorage *storage;

typedef enum ReadStatus {
  READ_OK = 0,
  READ_CANCEL,
  READ_INVALID,
  READ_FAIL,
} ReadStatus;

typedef enum MenuStatus {
  MENU_OK = 0,
  MENU_FAIL,
  MENU_INVALID,
  MENU_QUIT,
  MENU_DELETED
} MenuStatus;

void print_brief(Contact *contact);
void print_details(Contact *contact);

ReadStatus readline(char buffer[FIELD_SIZE]);
ReadStatus readline_required(char buffer[FIELD_SIZE]);

bool try_parse_int(char buffer[FIELD_SIZE], int *value);
bool try_parse_size_t(char buffer[FIELD_SIZE], size_t *value);
MenuStatus move_to_menu(char opt);
MenuStatus details_move_to_menu(char opt, Contact *c);
MenuStatus add_menu();
MenuStatus select_menu();
MenuStatus search_menu();
MenuStatus details_menu(Contact *c);
MenuStatus edit_menu(Contact *c);
MenuStatus delete_menu(Contact *c);
MenuStatus edit_numbers(Contact *c);

int main() {
  storage = create_storage();
  printf("Книга контактов!\n");
  load_data(storage);
  while (true) {
    size_t i = 0;
    
    
    ListIterator *it;
    it = it_begin(storage->list);
    while (it_has_next(it)) {
      Contact *contact = it_next(it);
      printf("[%zu] ", i + 1);
      print_brief(contact);
      i++;
    }

    it_delete(it);

    printf("[A] Добавить, [M] Подробнее, [S] Поиск, [Q] Завершить\n> ");
    char buffer[FIELD_SIZE];
    switch (readline(buffer)) {
    case READ_OK:
      break;
    case READ_FAIL:
      return 1;
    default:
      continue;
    }

    char opt = buffer[0];
    MenuStatus status = move_to_menu(opt);
    if (status == MENU_FAIL) {
      return 1;
    }

    if (status == MENU_QUIT) {
      break;
    }
  }

  dump_data(storage);
  delete_storage(storage);
  return 0;
}

MenuStatus move_to_menu(char opt) {
  switch (opt) {
  case 'A':
    return add_menu();
  case 'M':
    return select_menu();
  case 'S':
    return search_menu();
  case 'Q':
    return MENU_QUIT;
  default:
    printf("Неизвестная опция - %c.\n", opt);
    return MENU_INVALID;
  }
}

void print_brief(Contact *contact) {
  if (contact == NULL) {
    printf("Контакт не найден.\n");
    return;
  }

  printf("%s %s\n", contact->firstname, contact->lastname);
}

void print_details(Contact *contact) {
  if (contact == NULL) {
    printf("Контакт не найден.\n");
    return;
  }

  printf("Имя:\t\t%s\n", contact->firstname);
  printf("Фамилия:\t%s\n", contact->lastname);

  printf("Отчество:\t%s\n",
         contact->middlename[0] != '\0' ? contact->middlename : "...");
  printf("Место работы:\t%s\n", contact->job[0] != '\0' ? contact->job : "...");
  printf("Должность:\t%s\n",
         contact->position[0] != '\0' ? contact->position : "...");

  printf("Номера: \n");
  for (size_t i = 0; i < NUMBERS_SIZE; i++) {
    printf("[%ld] %s\n", i + 1,
           contact->phone_numbers[i][0] != '\0' ? contact->phone_numbers[i]
                                                : "...");
  }
}

ReadStatus readline(char buffer[FIELD_SIZE]) {
  if (fgets(buffer, FIELD_SIZE, stdin) == NULL) {
    return READ_FAIL;
  }

  if (buffer[0] == '\n') {
    return READ_CANCEL;
  }

  buffer[strlen(buffer) - 1] = '\0';

  return READ_OK;
}

ReadStatus readline_required(char buffer[FIELD_SIZE]) {
  ReadStatus status = READ_CANCEL;
  while (status != READ_OK) {
    status = readline(buffer);
    if (status == READ_FAIL) {
      return status;
    }
  }

  return READ_OK;
}

bool try_parse_int(char buffer[FIELD_SIZE], int *value) {
  if (sscanf(buffer, "%d", value) == 1) {
    return true;
  }

  return false;
}

bool try_parse_size_t(char buffer[FIELD_SIZE], size_t *value) {
  if (sscanf(buffer, "%zu", value) == 1) {
    return true;
  }

  return false;
}

MenuStatus edit_numbers(Contact *c) {

  char buffer[FIELD_SIZE];
  int input_num = -1;
  while (true) {
    printf("Введите номер слота [1-5]: ");
    switch (readline(buffer)) {
    case READ_OK:
      break;
    case READ_CANCEL:
      return MENU_OK;
    case READ_INVALID:
      continue;
    case READ_FAIL:
      return MENU_FAIL;
    }

    if (try_parse_int(buffer, &input_num) && input_num > 0 && input_num < 6) {
      break;
    }
  }

  printf("Введите номер: ");
  switch (readline(buffer)) {
  case READ_OK:
    break;
  case READ_FAIL:
    return MENU_FAIL;
  case READ_CANCEL:
    strncpy(c->phone_numbers[input_num - 1], "", 1);
    return MENU_OK;
  default:
    return MENU_OK;
  }

  strncpy(c->phone_numbers[input_num - 1], buffer, FIELD_SIZE);
  return MENU_OK;
}

MenuStatus add_menu() {
  char name[FIELD_SIZE];
  char lastname[FIELD_SIZE];

  ReadStatus status = READ_OK;
  printf("Имя: ");
  status = readline_required(name);
  if (status == READ_FAIL) {
    return MENU_FAIL;
  }

  printf("Фамилия: ");
  status = readline_required(lastname);
  if (status == READ_FAIL) {
    return MENU_FAIL;
  }

  char keys[A_FIELD_COUNT] = "nmjp";
  char format[A_FIELD_COUNT + 1] = {0, 0, 0, 0};
  char args[A_FIELD_COUNT][FIELD_SIZE] = {0};
  char prompts[A_FIELD_COUNT][FIELD_SIZE] = {
      "Телефон: ", "Отчество: ", "Место работы: ", "Должность: "};

  size_t args_size = 0;
  for (size_t i = 0; i < A_FIELD_COUNT - 1; i++) {
    char buffer[FIELD_SIZE];
    printf("%s", prompts[i]);
    status = readline(buffer);
    switch (status) {
    case READ_FAIL:
      return MENU_FAIL;
    case READ_OK:
      format[args_size] = keys[i];
      strncpy(args[args_size++], buffer, FIELD_SIZE);
    default:
      break;
    }
  }

  Contact *c;
  switch (args_size) {
  case 0:
    c = create_contact(name, lastname, NULL);
    break;
  case 1:
    c = create_contact(name, lastname, format, args[0]);
    break;
  case 2:
    c = create_contact(name, lastname, format, args[0], args[1]);
    break;
  case 3:
    c = create_contact(name, lastname, format, args[0], args[1], args[2]);
    break;
  case 4:
    c = create_contact(name, lastname, format, args[0], args[1], args[2],
                       args[3]);
    break;
  }

  add_contact(storage, c);

  printf("\n");
  return MENU_OK;
}

MenuStatus details_move_to_menu(char opt, Contact *c) {
  switch (opt) {
  case 'E':
    return edit_menu(c);
  case 'D':
    return delete_menu(c);
  case 'N':
    return edit_numbers(c);
  case 'Q':
    return MENU_QUIT;
  default:
    printf("Неизвестная опция - %u.\n", opt);
    return MENU_INVALID;
  }
}

MenuStatus details_menu(Contact *c) {
  while (true) {
    print_details(c);
    printf("[E] Изменить, "
           "[N] Изменить номер, "
           "[D] Удалить контакт, "
           "[Q] Назад\n> ");

    char buffer[FIELD_SIZE];

    switch (readline(buffer)) {
    case READ_OK:
      break;
    case READ_FAIL:
      return MENU_FAIL;
    default:
      return MENU_OK;
    }

    char opt = buffer[0];
    switch (details_move_to_menu(opt, c)) {
    case MENU_FAIL:
      return MENU_FAIL;
    case MENU_QUIT:
      return MENU_OK;
    default:
    case MENU_DELETED:
      return MENU_OK;
      break;
    }
  }
}

MenuStatus select_menu() {
  char buffer[FIELD_SIZE];
  size_t input_num = 0;
  while (true) {
    printf("Введите номер контакта (Q чтобы отказаться): ");
    switch (readline(buffer)) {
    case READ_OK:
      break;
    case READ_CANCEL:
      return MENU_OK;
    case READ_FAIL:
      return MENU_FAIL;
    case READ_INVALID:
      continue;
    }

    if (buffer[0] == 'Q') {
      return MENU_OK;
    }

    if (try_parse_size_t(buffer, &input_num) && input_num > 0 &&
        input_num < storage->size + 1) {
      break;
    } else {
      printf("Неправильный ввод. Ожидается индекс контакта в списке.\n");
    }
  }

  ListIterator *it = it_begin(storage->list);
  size_t i = 0;
  Contact *c = NULL;
  while (it_has_next(it) && i < input_num) {
    c = it_next(it);
    i++;
  }

  it_delete(it);

  if (i != input_num || c == NULL) {
    printf("Контакт не найден.\n");
    return MENU_OK;
  }

  return details_menu(c);
}

MenuStatus search_menu() {
  printf("Поиск по фамилии.\n> ");
  char buffer[FIELD_SIZE];
  bool ask = true;
  while (ask) {
    ReadStatus retval = readline(buffer);
    switch (retval) {
    case READ_OK:
      ask = false;
      break;
    case READ_CANCEL:
      return MENU_OK;
    case READ_FAIL:
      return MENU_FAIL;
    case READ_INVALID:
      continue;
    }
  }

  ListIterator *it = it_begin(storage->list);
  size_t i = 0;
  while (it_has_next(it)) {
    Contact *c = it_next(it);
    if (strstr(c->lastname, buffer)) {
      i++;
      printf("[%zu] ", i);
      print_brief(c);
    }
  }

  it_delete(it);

  if (i == 0) {
    printf("Контактов по такому запросу не найдено.\n");
    return MENU_OK;
  }

  size_t input_num = -1;
  while (true) {
    printf("Введите номер контакта (Q чтобы отказаться): ");
    switch (readline(buffer)) {
    case READ_OK:
      break;
    case READ_CANCEL:
      return MENU_OK;
    case READ_FAIL:
      return MENU_FAIL;
    case READ_INVALID:
      continue;
    }

    if (buffer[0] == 'Q') {
      return MENU_OK;
    }

    if (try_parse_size_t(buffer, &input_num) && input_num > 0 &&
        input_num < storage->size + 1) {
      break;
    } else {
      printf("Неправильный ввод. Ожидается индекс контакта в списке");
    }
  }

  it = it_begin(storage->list);
  i = 0;
  Contact *c = NULL;
  while (it_has_next(it) && i < input_num) {
    c = it_next(it);
    if (strstr(c->lastname, buffer)) {
      i++;
    }
  }

  it_delete(it);

  if (i != input_num || c == NULL) {
    printf("Контакт не найден.\n");
    return MENU_OK;
  }

  return details_menu(c);
}

MenuStatus edit_menu(Contact *c) {
  char keys[U_FIELD_COUNT] = "flnmjp";
  char format[U_FIELD_COUNT] = {0, 0, 0, 0, 0, 0};
  char args[U_FIELD_COUNT][FIELD_SIZE] = {0};
  char prompts[U_FIELD_COUNT][FIELD_SIZE] = {
      "Имя: ",      "Фамилия: ",      "Телефон: ",
      "Отчество: ", "Место работы: ", "Должность: "};

  size_t args_size = 0;

  for (size_t i = 0; i < U_FIELD_COUNT - 1; i++) {
    char buffer[FIELD_SIZE];
    printf("%s", prompts[i]);
    switch (readline(buffer)) {
    case READ_FAIL:
      return MENU_FAIL;
    case READ_OK:
      format[args_size] = keys[i];
      strncpy(args[args_size++], buffer, FIELD_SIZE);
      args[args_size][FIELD_SIZE - 1] = '\0';
    default:
      break;
    }
  }

  switch (args_size) {
  case 1:
    update_contact(c, format, args[0]);
    break;
  case 2:
    update_contact(c, format, args[0], args[1]);
    break;
  case 3:
    update_contact(c, format, args[0], args[1], args[2]);
    break;
  case 4:
    update_contact(c, format, args[0], args[1], args[2], args[3]);
    break;
  case 5:
    update_contact(c, format, args[0], args[1], args[2], args[3], args[4]);
    break;
  case 6:
    update_contact(c, format, args[0], args[1], args[2], args[3], args[4],
                   args[5]);
    break;
  }

  printf("\n");
  return MENU_OK;
}

MenuStatus delete_menu(Contact *c) {
  while (true) {
    printf("Вы уверены? [y/n]: ");
    char buffer[FIELD_SIZE];
    switch (readline(buffer)) {
    case READ_CANCEL:
      return MENU_OK;
    case READ_FAIL:
      return MENU_FAIL;
    case READ_INVALID:
      continue;
    default:
      break;
    }

    char opt = buffer[0];
    switch (opt) {
    case 'y':
      remove_contact(storage, c);
      delete_contact(c);
      return MENU_DELETED;
    case 'n':
      return MENU_OK;
    }
  }
}
