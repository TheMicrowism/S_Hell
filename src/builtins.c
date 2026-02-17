#include "builtins.h"
#include "shell.h"

extern char pwd[];
extern char cwd[];
extern builtinTab Builtins;

int Echo(char **args) {
  for (int arg = 0; arg < MAXARGS; arg++) {
    if (args[arg] == NULL) {
      return 0;
    }
    printf("%s ", args[arg]);
  }
  fprintf(stderr, "echo error");
  return 1;
}

int Quit(char **args) {
  exit(0);
  unix_error("error quitting");
}

int ChangeDir(char **args) {
  int res;
  if (strcmp(args[0], "-") == 0) {
    if (strcmp(pwd, "\0")) {
      res = chdir(pwd);
    } else {
      fprintf(stderr, "No previous directory");
      return 1;
    }
  } else {
    res = chdir(args[0]);
  }
  if (res != 0) {
    perror("ChangeDir error");
    return res;
  }
  strcpy(pwd, cwd);
  return res;
}

int executeBuiltins(char **args) {
  // printf("DEBUG: executing %s", args[0]);
  for (int i = 0; i < Builtins.size; i++) {
    if (strcmp(args[0], Builtins.tab[i].BuiltinName) == 0) {
      Builtins.tab[i].BuiltinFunc(args + 1);
      printf("\n");
      return 0;
    }
  }
  return 1;
}
