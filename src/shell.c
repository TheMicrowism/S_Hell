#include "shell.h"
#include "csapp.h"
#include <fcntl.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <wordexp.h>

char cwd[BUFSIZ] = "\0";
char pwd[BUFSIZ] = "\0";

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

BuiltinTab Builtins = {
    4, {{Echo, "echo"}, {Quit, "quit"}, {Quit, "q"}, {ChangeDir, "cd"}}};

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

void handlerSIGCHLD(int sig) {
  pid_t pid;
  while ((pid = waitpid(-1, NULL, WNOHANG)) > 0) {
    ;
    printf("Handler reaped child %d\n", (int)pid);
  }
  if (pid < 0 && errno != ECHILD) {
    unix_error("waitpid error");
  }
  return;
}

int main() {

  Signal(SIGCHLD, handlerSIGCHLD);

  while (1) {
    struct cmdline *l;
    int i, j;
    pid_t childrenPid[MAXCHILD];
    pid_t childPid;
    int fdIn, fdOut;
    int stdinCpy = dup(0);
    int stdoutCpy = dup(1);

    if (stdinCpy < 0 || stdoutCpy < 0) {
      unix_error("Failed backing up stdin/out");
    }

    int nbCmd = 0; // counting the nb of cmd

    // get working directory
    if (getcwd(cwd, BUFSIZ) == NULL) {
      unix_error("Error fetching cwd");
    }
    printf("%s%s>\e[0m ", KBLU, cwd);

    l = readcmd();

    /* If input stream closed, normal termination */
    if (!l) {
      printf("exit\n");
      exit(0);
    }

    if (l->err) {
      /* Syntax error, read another command */
      printf("error: %s\n", l->err);
      continue;
    }

    if (l->in) {
      fdIn = open(l->in, O_RDONLY, S_IRWXU);
      if (dup2(fdIn, 0) < 0) {
        perror("Input redirection failed");
        continue;
      }
      //     printf("in: %s\n", l->in);
    }

    if (l->out) {
      fdOut = open(l->out, O_WRONLY | O_CREAT, S_IRWXU);
      if (dup2(fdOut, 1) < 0) {
        perror("Output redirection failed");
        continue;
      }
      //    printf("out: %s\n", l->out);
    }
    // counting children
    for (i = 0; l->seq[i] != 0; i++) {
      nbCmd++;
    }
    // creating pipes
    int pipes[nbCmd - 1][2];
    for (i = 0; i < nbCmd - 1; i++) {
      if (pipe(pipes[i]) != 0) {
        unix_error("Fix your pipe man");
      };
    }
    // spawning children
    for (i = 0; i < nbCmd; i++) {
      wordexp_t eArgs;
      char **cmd = l->seq[i];
      //      printf("seq[%d]: ", i);
      for (j = 0; cmd[j] != 0; j++) {
        if (wordexp(cmd[j], &eArgs, j == 0 ? 0 : WRDE_APPEND) != 0) {
          perror("error expanding argument");
        }
      }

      // if no pipe needed then we can execute builtins
      int exInFunction = 1;
      if (nbCmd <= 1) {
        exInFunction = executeBuiltins(eArgs.we_wordv);
      }
      // CHILD HEREEEEEEEEEEEEEEEEEEEEEEEEEEE
      if (exInFunction != 0) {
        childPid = Fork();
        if (childPid == 0) {

          for (int termPipe = 0; termPipe < nbCmd - 1; termPipe++) {
            if (termPipe == i) {
              // printf("child %d closing pipe %d end 0\n", i, termPipe);
              close(pipes[termPipe][0]);
            } else if (termPipe == i - 1) {
              // printf("child %d closing pipe %d end 0\n", i, termPipe);
              close(pipes[termPipe][1]);
            } else {
              close(pipes[termPipe][0]);
              close(pipes[termPipe][1]);
            }
          }

          if (i > 0) {
            // printf("child %d connecting to pipe %d %d\n", i, i - 1, 0);
            if (dup2(pipes[i - 1][0], 0) < 0)
              unix_error("child receiving end of pipe error");
          }
          if (i < nbCmd - 1) {
            // printf("child %d connecting to pipe %d %d\n", i, i, 1);
            if (dup2(pipes[i][1], 1) < 0)
              unix_error("child sending end of pipe error");
          }

          execvp(cmd[0], eArgs.we_wordv);
          // error in case execvp failed
          unix_error("failed to execute ");

          // END CHILDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD
        } else {
          childrenPid[i] = childPid;
          childPid = -1;
          // waitpid(childrenPid[i], NULL, 0);
        }
      }

      wordfree(&eArgs);
    }

    // father doing his plumbing
    for (i = 0; i < nbCmd - 1; i++) {
      close(pipes[i][1]);
      close(pipes[i][0]);
    }

    pid_t pidWait;
    while ((pidWait = waitpid(-1, NULL, 0)) > 0) {
    }
    if (pidWait < 0 && errno != ECHILD) {
      unix_error("waitpid error");
    }

    if (dup2(stdoutCpy, 1) < 0) {
      unix_error("Failed to put back stdout");
    }
    if (dup2(stdinCpy, 0) < 0) {
      unix_error("Failed to put back stdin");
    }
    close(stdinCpy);
    close(stdoutCpy);
  }
}
