#ifndef _BUILTINS_
#define _BUILTINS_

typedef struct {
  int (*BuiltinFunc)(char **args);
  char *BuiltinName;
} builtin;

typedef struct {
  int size;
  builtin tab[];
} builtinTab;

int ChangeDir(char **args);
int Echo(char **args);
int Quit(char **args);

int executeBuiltins(char **args);

#endif
