
/*
 * Copyright (C) 2002, Simon Nieuviarts
 */

#include "builtins.h"
#include "csapp.h"
#include "readcmd.h"
#include "wordexp.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#define MAXCHILD 10
#define MAXARGS 100
#define MAXPIPE 10
#define MAXJOBS 10

enum jobState { EMPTY, FOREGROUND, BACKGROUND, STOPPED, DONE };

typedef struct {
  int fgNb;
  pid_t pgidTab[MAXJOBS];
  enum jobState stateTab[MAXJOBS];
  struct cmdline cmdTab[MAXJOBS];
} jobsTab;

#define KNRM "\x1B[0m"
#define KRED "\x1B[31m"
#define KGRN "\x1B[32m"
#define KYEL "\x1B[33m"
#define KBLU "\x1B[34;1m"
#define KMAG "\x1B[35m"
#define KCYN "\x1B[36m"
#define KWHT "\x1B[37m"
