#include "shell.h"
#include "builtins.h"
#include "csapp.h"
#include "jobs.h"
#include "readcmd.h"
#include <unistd.h>
#include <wordexp.h>

char cwd[BUFSIZ] = "\0";
char pwd[BUFSIZ] = "\0";
jobsTab Jobs = {.fgNb = -1};
unsigned char isInteractive;

builtinTab Builtins = {9,
                       {{Echo, "echo"},
                        {Quit, "quit"},
                        {Quit, "q"},
                        {ChangeDir, "cd"},
                        {ActiveJobsList, "jobs"},
                        {Foreground, "fg"},
                        {Background, "bg"},
                        {Terminate, "term"},
                        {Stop, "stop"}}};

void handlerSIGCHLD(int sig) {
  pid_t pid;
  int status;
  while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
    if (WIFEXITED(status)) {
      changeProccessState(pid, PDONE);
    }
    if (WIFSIGNALED(status)) {
      changeProccessState(pid, PDONE);
    }

    if (WIFSTOPPED(status)) {
      changeProccessState(pid, PSTOPPED);
    }
    if (WIFCONTINUED(status)) {
      changeProccessState(pid, PRUNNING);
    }

    status = 0;
  }
  int update = updateJobsList();
  if (update < 0) {
    exit(1);
  }
  if (Jobs.fgNb < 0 && update > 0) {

    printf("\n%s%s>\e[0m ", KBLU, cwd);
    fflush(stdout);
  }
  if (pid < 0 && errno != ECHILD) {
    unix_error("waitpid error");
  }

  return;
}
void messageHandler(int sig) {
  printf("call quit or q to exit shell\n \n%s%s>\e[0m ", KBLU, cwd);
  fflush(stdout);
}

int main() {

  Signal(SIGCHLD, handlerSIGCHLD);
  Signal(SIGINT, messageHandler);
  Signal(SIGTSTP, messageHandler);

  if (!(isInteractive = isatty(0))) {
    printf("\n%s NON-INTERACTIVE MODE \e[0m\n", KBLU);
    fflush(stdout);
  };

  if (!(isInteractive = isatty(0))) {
    printf("\n%s NON-INTERACTIVE MODE \e[0m\n", KBLU);
    fflush(stdout);
  };

  for (int i = 0; i < MAXJOBS; i++) {
    Jobs.stateTab[i] = EMPTY;
    for (int j = 0; j < MAXCHILD; j++) {
      Jobs.childrenPids[i].pidStateTab[j] = PEMPTY;
    }
  }

  while (1) {
    struct cmdline *l;
    int i, j;
    pid_t childPid, childPgid = 0;
    int fdIn, fdOut;
    int stdinCpy = dup(0);
    int stdoutCpy = dup(1);
    unsigned char backgroundProcess = 0;
    char commandLine[BUFSIZ];

    int newjobnb;

    if (stdinCpy < 0 || stdoutCpy < 0) {
      unix_error("Failed backing up stdin/out");
    }

    int nbCmd = 0; // counting the nb of cmd

    // get working directory
    if (getcwd(cwd, BUFSIZ) == NULL) {
      unix_error("Error fetching cwd");
    }

    if (isInteractive) {
      printf("%s%s>\e[0m ", KBLU, cwd);
      fflush(stdout);
    }

    l = readcmd(commandLine);

    /* If input stream closed, normal termination */
    if (!l) {
      printf("exit\n");
      Quit(NULL);
      exit(0);
    }

    if (l->err) {
      /* Syntax error, read another command */
      printf("error: %s\n", l->err);
      continue;
    }
    // counting children
    for (i = 0; l->seq[i] != 0; i++) {
      nbCmd++;
    }
    if (nbCmd == 0) {
      continue;
    }
    backgroundProcess = l->isBackground;
    if (nbCmd > MAXCHILD) {
      fprintf(stderr, "can only run at max %d commands per job\n", MAXCHILD);
      continue;
    }

    if (l->in) {
      if ((fdIn = open(l->in, O_RDONLY, S_IRWXU)) < 0) {
        fprintf(stderr, "%s: %s\n", l->in, strerror(errno));
        continue;
      } else if (dup2(fdIn, 0) < 0) {
        perror("Input redirection failed");
        continue;
      }
    }

    if (l->out) {
      if ((fdOut = open(l->out, O_WRONLY | O_CREAT, S_IRWXU)) < 0) {

        fprintf(stderr, "%s: %s\n", l->out, strerror(errno));
        continue;
      } else if (dup2(fdOut, 1) < 0) {
        perror("Output redirection failed");
        continue;
      }
    }

    // creating pipes
    int pipes[nbCmd - 1][2];
    for (i = 0; i < nbCmd - 1; i++) {
      if (pipe(pipes[i]) != 0) {
        unix_error("Fix your pipe man");
      };
    }

    // blocking SIGCHILD to avoid race condition in job creating process

    sigset_t newSet, oldSet;
    if (sigemptyset(&newSet) < 0 || sigaddset(&newSet, SIGCHLD) < 0) {
      unix_error(
          "Error in creating child processes, failed to set up SIGCHLD mask");
    }

    else if (sigprocmask(SIG_BLOCK, &newSet, &oldSet) < 0) {
      unix_error("Error in creating child processes, failed to proc mask");
    }

    newjobnb = emptyJobNb();
    for (i = 0; i < nbCmd; i++) {
      wordexp_t eArgs;
      char **cmd = l->seq[i];
      for (j = 0; cmd[j] != 0; j++) {
        if (wordexp(cmd[j], &eArgs, j == 0 ? 0 : WRDE_APPEND) != 0) {
          fprintf(stderr, "fatal: error expanding arguments, segfault "
                          "imminent, quitting\n");
          Quit(NULL);
        }
      }

      // if no pipe needed then we can execute builtins
      int exInFunction = 1;
      if (nbCmd <= 1) {
        exInFunction = executeBuiltins(eArgs.we_wordv);
      }

      if (newjobnb < 0 && exInFunction != 0) {
        fprintf(stderr, "MAXJOBS of %d is reached, cannot execute %s \n",
                MAXJOBS, commandLine);
        break;
      }
      if (exInFunction != 0 && newjobnb >= 0) {
        childPid = Fork();
        // CHILD HEREEEEEEEEEEEEEEEEEEEEEEEEEEE
        if (childPid == 0) {
          Signal(SIGTSTP, SIG_DFL);
          Signal(SIGINT, SIG_DFL);
          // setting up pipes for child
          for (int termPipe = 0; termPipe < nbCmd - 1; termPipe++) {
            if (termPipe == i) {
              close(pipes[termPipe][0]);
            } else if (termPipe == i - 1) {
              close(pipes[termPipe][1]);
            } else {
              close(pipes[termPipe][0]);
              close(pipes[termPipe][1]);
            }
          }

          if (i > 0) {
            if (dup2(pipes[i - 1][0], 0) < 0)
              unix_error("child receiving end of pipe error");
          }
          if (i < nbCmd - 1) {
            if (dup2(pipes[i][1], 1) < 0)
              unix_error("child sending end of pipe error");
          }

          execvp(cmd[0], eArgs.we_wordv);
          // error in case execvp failed
          fprintf(stderr, "%s", cmd[0]);
          unix_error("");

          // END CHILDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD
        } else {
          if (i == 0) {
            childPgid = childPid;
            newjobnb = emptyJobNb();
            if (newjobnb < 0) {
              fprintf(stderr, "No empty job slot, killing spawned children\n");
              Kill(childPid, SIGKILL);
              break;
            }
            if (backgroundProcess) {
              if (changeJobsAll(newjobnb, childPgid, commandLine, BACKGROUND) <
                  0) {
                Kill(childPid, SIGKILL);
                break;
              }
              printf("Job [%d] %d     BACKGROUND     %s\n", newjobnb + 1,
                     childPgid, commandLine);
            } else {
              if (changeJobsAll(newjobnb, childPgid, commandLine, FOREGROUND) <
                  0) {
                Kill(childPid, SIGKILL);
                break;
              }
            }
          }
          if (setpgid(childPid, childPgid) < 0) {
            fprintf(stderr, "failed to set child %d to pgid %d\n", childPid,
                    childPgid);
            Kill(childPid, SIGKILL);
            break;
          }
          if (newProcess(childPid, newjobnb, PRUNNING) < 0)
            break;
          childPid = -1;
        }
      }

      wordfree(&eArgs);
    }
    // restoring mask
    if (sigprocmask(SIG_SETMASK, &oldSet, NULL) < 0) {
      unix_error("Error restoring mask for after spawning child processes");
    }

    // father doing his plumbing
    for (i = 0; i < nbCmd - 1; i++) {
      close(pipes[i][1]);
      close(pipes[i][0]);
    }

    if (!backgroundProcess && childPgid > 0) {
      if (isInteractive && !l->in && tcsetpgrp(0, childPgid) < 0) {
        fprintf(stderr,
                "Error setting up tcsetpgrp (shell), killing processes\n");
        kill(childPgid, SIGKILL);
      }
      // ignoring SIGTTOU
      Signal(SIGTTOU, SIG_IGN);
      pid_t pidWait;
      int status;
      while ((pidWait = waitpid(-childPgid, &status, WUNTRACED)) > 0) {
        if (WIFSTOPPED(status)) {
          // printf("children group stopped via signal");
          changeProccessState(pidWait, PSTOPPED);
          break;
        } else {
          changeProccessState(pidWait, PDONE);
        }
      }
      if (updateJobsList() < 0) {
        exit(1);
      }
      if (pidWait < 0 && errno != ECHILD) {
        unix_error("waitpid error");
      }
      if (isInteractive && !l->in && tcsetpgrp(0, getpgid(0)) < 0) {
        fprintf(stderr, "Error recovering tcsetpgrp (shell)\n");
        kill(childPgid, SIGKILL);
        exit(1);
      }
      Signal(SIGTTOU, SIG_DFL);
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
