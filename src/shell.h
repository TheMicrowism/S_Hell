
/*
 * Copyright (C) 2002, Simon Nieuviarts
 */

#include "csapp.h"
#include "readcmd.h"
#include "wordexp.h"
#include <stdio.h>
#include <stdlib.h>

#define MAXCHILD 10
#define MAXARGS 100
#define MAXPIPE 10

typedef struct {
  int (*BuiltinFunc)(char **args);
  char *BuiltinName;
} Builtin;

typedef struct {
  int size;
  Builtin tab[];
} BuiltinTab;

int ChangeDir(char **args);
int Echo(char **args);
int Quit(char **args);

#define KNRM "\x1B[0m"
#define KRED "\x1B[31m"
#define KGRN "\x1B[32m"
#define KYEL "\x1B[33m"
#define KBLU "\x1B[34;1m"
#define KMAG "\x1B[35m"
#define KCYN "\x1B[36m"
#define KWHT "\x1B[37m"
