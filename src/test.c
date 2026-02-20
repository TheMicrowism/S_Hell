#include "shell.h"
#include "csapp.h"
#include "readcmd.h"
#include <fcntl.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <wordexp.h>

char cwd[BUFSIZ] = "\0";
char pwd[BUFSIZ] = "\0";
jobsTab Jobs = {.fgNb = 1};
builtinTab Builtins = {
    4, {{Echo, "echo"}, {Quit, "quit"}, {Quit, "q"}, {ChangeDir, "cd"}}};

int emptyJobNb()
{
  for (int i = 0; i < MAXJOBS; i++)
  {
    if (Jobs.stateTab[i] == EMPTY)
      return i;
  }
  return -1;
}

int changJobsState(int jobNb, pid_t pgid, struct cmdline cmd,
                   enum jobState state)
{
  // blocking all signals
  sigset_t newSet, oldSet;
  if (sigfillset(&newSet) < 0)
  {
    perror("Error setting up sig mask for changeJobsState");
    return -1;
  }

  if (sigprocmask(SIG_BLOCK, &newSet, &oldSet) < 0)
  {
    perror("Error blocking signals for changeJobsState");
    return -1;
  }

  if (Jobs.stateTab[jobNb] == EMPTY)
  {
    Jobs.stateTab[jobNb] = state;
    Jobs.cmdTab[jobNb] = cmd;
    Jobs.pgidTab[jobNb] = pgid;
  }
  if (state == FOREGROUND)
    Jobs.fgNb = jobNb;

  // restoring mask
  if (sigprocmask(SIG_SETMASK, &oldSet, NULL) < 0)
  {
    perror("Error restoring mask for changeJobsState;");
    return -1;
  }

  printf("Jobs: [%d] %d\n", jobNb + 1, state);
  return 0;
}
/*
int supprime_jobs()
{
  sigset_t newSet, oldSet;
  if (sigfillset(&newSet) < 0)
  {
    perror("Error setting up sig mask for changeJobsState");
    return -1;
  }

  if (sigprocmask(SIG_BLOCK, &newSet, &oldSet) < 0)
  {
    perror("Error blocking signals for changeJobsState");
    return -1;
  }

  for (int i = 0; i < MAXJOBS; i++)
  {
    printf("i:%d state: %d\n", i + 1, Jobs.stateTab[i]);
    if (Jobs.stateTab[i] == EMPTY)
    {
      continue;
    }
    pid_t pgid = Jobs.pgidTab[i];
    if (pgid < 0)
    {
      perror("pgid");
    }
    if (kill(-pgid, 0) < 0)
    {
      Jobs.stateTab[i] = EMPTY;
      Jobs.pgidTab[i] = 0;
      if (Jobs.fgNb == i)
        Jobs.fgNb = -1;
    }
    printf("supprime le jobs %d\n", i + 1);
  }

  if (sigprocmask(SIG_SETMASK, &oldSet, NULL) < 0)
  {
    perror("Error restoring mask for changeJobsState;");
    return -1;
  }

  return 0;
}
*/

int supprime_jobs(pid_t pgid, int i)
{
  sigset_t newSet, oldSet;
  if (sigfillset(&newSet) < 0)
  {
    perror("Error setting up sig mask for changeJobsState");
    return -1;
  }

  if (sigprocmask(SIG_BLOCK, &newSet, &oldSet) < 0)
  {
    perror("Error blocking signals for changeJobsState");
    return -1;
  }

  if (Jobs.pgidTab[i] == pgid && Jobs.stateTab[i] != EMPTY)
  {
    Jobs.stateTab[i] = EMPTY;
    Jobs.pgidTab[i] = 0;
    printf("Job [%d] terminé\n", i + 1);
    // printf("hi");

    if (sigprocmask(SIG_SETMASK, &oldSet, NULL) < 0)
    {
      perror("supprime_jobs");
      return -1;
    }
    return 0;
  }

  if (sigprocmask(SIG_SETMASK, &oldSet, NULL) < 0)
  {
    perror("Error restoring mask for changeJobsState;");
    return -1;
  }

  return 0;
}
void handlerSIGCHLD(int sig)
{
  pid_t pid;
  while ((pid = waitpid(-1, NULL, WNOHANG)) > 0)
  {

    printf("Handler reaped child %d\n", (int)pid);
    for (int i = 0; i < MAXJOBS; i++)
    {
      if (Jobs.pgidTab[i] == pid)

      {
        supprime_jobs(pid, i);
        break;
      }
    }
  }
  if (pid < 0 && errno != ECHILD)
  {
    unix_error("waitpid error");
  }

  return;
}
void handler_sigint(int sig)
{
  write(1, "\n", 1);
  return;
}
int main()
{

  Signal(SIGCHLD, handlerSIGCHLD);
  Signal(SIGINT, handler_sigint);

  while (1)
  {
    struct cmdline *l;
    int i, j;
    pid_t childPid, childPgid;
    int fdIn, fdOut;
    int stdinCpy = dup(0);
    int stdoutCpy = dup(1);
    unsigned char backgroundProcess = 0;

    if (stdinCpy < 0 || stdoutCpy < 0)
    {
      unix_error("Failed backing up stdin/out");
    }

    int nbCmd = 0; // counting the nb of cmd

    // get working directory
    if (getcwd(cwd, BUFSIZ) == NULL)
    {
      unix_error("Error fetching cwd");
    }
    printf("%s%s>\e[0m ", KBLU, cwd);

    l = readcmd();
    /* If input stream closed, normal termination */
    if (!l)
    {
      printf("exit\n");
      exit(0);
    }

    if (l->err)
    {
      /* Syntax error, read another command */
      printf("error: %s\n", l->err);
      continue;
    }

    if (l->in)
    {
      if ((fdIn = open(l->in, O_RDONLY, S_IRWXU)) < 0)
      {
        fprintf(stderr, "%s: %s\n", l->in, strerror(errno));
        continue;
      }
      else if (dup2(fdIn, 0) < 0)
      {
        perror("Input redirection failed");
        continue;
      }
      //     printf("in: %s\n", l->in);
    }

    if (l->out)
    {
      if ((fdOut = open(l->out, O_WRONLY | O_CREAT, S_IRWXU)) < 0)
      {

        fprintf(stderr, "%s: %s\n", l->out, strerror(errno));
        continue;
      }
      else if (dup2(fdOut, 1) < 0)
      {
        perror("Output redirection failed");
        continue;
      }
      //    printf("out: %s\n", l->out);
    }
    // counting children
    for (i = 0; l->seq[i] != 0; i++)
    {
      nbCmd++;
    }
    // creating pipes
    int pipes[nbCmd - 1][2];
    for (i = 0; i < nbCmd - 1; i++)
    {
      if (pipe(pipes[i]) != 0)
      {
        unix_error("Fix your pipe man");
      };
    }
    // spawning children
    for (i = 0; i < nbCmd; i++)
    {
      wordexp_t eArgs;
      char **cmd = l->seq[i];
      //      printf("seq[%d]: ", i);
      for (j = 0; cmd[j] != 0; j++)
      {
        if (strcmp(cmd[j], "&") == 0)
        {
          if (i == nbCmd - 1 && cmd[j + 1] == 0)
          {
            backgroundProcess = 1;
            printf("backgroundProcess\n");
          }
          else
          {
            fprintf(stderr, "syntax error: & is not placed at the end, "
                            "executing process in the foreground \n");
          }
        }
        else if (wordexp(cmd[j], &eArgs, j == 0 ? 0 : WRDE_APPEND) != 0)
        {
          perror("error expanding argument");
        }
      }

      // if no pipe needed then we can execute builtins
      int exInFunction = 1;
      if (nbCmd <= 1)
      {
        exInFunction = executeBuiltins(eArgs.we_wordv);
      }

      if (exInFunction != 0)
      {
        childPid = Fork();
        if (i == 0)
          childPgid = childPid;

        // CHILD HEREEEEEEEEEEEEEEEEEEEEEEEEEEE
        if (childPid == 0)
        {
          signal(SIGINT, SIG_DFL);
          // setting up pipes for child
          for (int termPipe = 0; termPipe < nbCmd - 1; termPipe++)
          {
            if (termPipe == i)
            {
              // printf("child %d closing pipe %d end 0\n", i, termPipe);
              close(pipes[termPipe][0]);
            }
            else if (termPipe == i - 1)
            {
              // printf("child %d closing pipe %d end 0\n", i, termPipe);
              close(pipes[termPipe][1]);
            }
            else
            {
              close(pipes[termPipe][0]);
              close(pipes[termPipe][1]);
            }
          }

          if (i > 0)
          {
            // printf("child %d connecting to pipe %d %d\n", i, i - 1, 0);
            if (dup2(pipes[i - 1][0], 0) < 0)
              unix_error("child receiving end of pipe error");
          }
          if (i < nbCmd - 1)
          {
            // printf("child %d connecting to pipe %d %d\n", i, i, 1);
            if (dup2(pipes[i][1], 1) < 0)
              unix_error("child sending end of pipe error");
          }

          execvp(cmd[0], eArgs.we_wordv);
          // error in case execvp failed
          unix_error("dvfefa");

          // END CHILDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD
        }
        else
        {
          if (i == 0)
          {
            int newjobnb = emptyJobNb();
            if (newjobnb < 0)
            {
              Kill(childPid, SIGINT);
              fprintf(stderr, "No empty job slot\n");
              break;
            }
            if (backgroundProcess)
            {
              // printf("hi");
              if (changJobsState(newjobnb, childPgid, *l, BACKGROUND) < 0)
              {
                Kill(childPid, SIGINT);
                break;
              }
            }
            else if (changJobsState(newjobnb, childPgid, *l, FOREGROUND) <
                     0)
            {
              Kill(childPid, SIGINT);
              break;
            }
          }

          childPid = -1;
        }
      }

      wordfree(&eArgs);
    }

    // father doing his plumbing
    for (i = 0; i < nbCmd - 1; i++)
    {
      close(pipes[i][1]);
      close(pipes[i][0]);
    }

    if (!backgroundProcess)
    {
      pid_t pidWait;
      while ((pidWait = waitpid(-1, NULL, 0)) > 0)
      {
        // printf("hi");
        for (int i = 0; i < MAXJOBS; i++)
        {
          if (Jobs.pgidTab[i] == pidWait)

          {
            supprime_jobs(pidWait, i);
            break;
          }
        }
      }
      if (pidWait < 0 && errno != ECHILD)
      {
        unix_error("waitpid error");
      }
    }

    if (dup2(stdoutCpy, 1) < 0)
    {
      unix_error("Failed to put back stdout");
    }
    if (dup2(stdinCpy, 0) < 0)
    {
      unix_error("Failed to put back stdin");
    }
    close(stdinCpy);
    close(stdoutCpy);
  }
}
