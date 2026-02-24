#include "jobs.h"

extern jobsTab Jobs;

int emptyJobNb() {
  for (int i = 0; i < MAXJOBS; i++) {
    if (Jobs.stateTab[i] == EMPTY)
      return i;
  }
  return -1;
}
int findJobNb(pid_t pgid) {

  for (int i = 0; i < MAXJOBS; i++) {
    if (pgid == Jobs.pgidTab[i]) {
      return i;
    }
  }
  return -1;
}

int ActiveJobsList(char **args) {
  printf("Foreground job: %d\n", Jobs.fgNb + 1);
  for (int i = 0; i < MAXJOBS; i++) {
    if (Jobs.stateTab[i] != EMPTY) {
      printf("[%d]   %7d    ", i + 1, Jobs.pgidTab[i]);
      switch (Jobs.stateTab[i]) {
      case FOREGROUND:
        printf("FOREGROUND ");
        break;
      case BACKGROUND:
        printf("BACKGROUND ");
        break;
      case STOPPED:
        printf("STOPPED    ");
        break;
      default:
        printf("           ");
      }
      printf("%s\n", Jobs.commandTab[i]);
    }
  }
  return 0;
}

int changeJobsAll(int jobNb, pid_t pgid, char *lineRead, enum jobState state) {
  // blocking all signals
  sigset_t newSet, oldSet;
  char jobChanged = 1;
  if (sigfillset(&newSet) < 0) {
    perror("Error setting up sig mask for changeJobsAll");
    return -1;
  }

  if (sigprocmask(SIG_BLOCK, &newSet, &oldSet) < 0) {
    perror("Error blocking signals for changeJobsAll");
    return -1;
  }

  // deleting the jobs
  if (state == EMPTY) {
    for (int i = 0; i < MAXCHILD; i++) {
      Jobs.childrenPids[jobNb].pidStateTab[i] = PEMPTY;
      Jobs.childrenPids[jobNb].pid[i] = 0;
    }
    Jobs.stateTab[jobNb] = EMPTY;
    if (jobNb == Jobs.fgNb) {
      Jobs.fgNb = -1;
    }
  }

  else if (Jobs.stateTab[jobNb] == EMPTY) {
    Jobs.stateTab[jobNb] = state;
    strcpy(Jobs.commandTab[jobNb], lineRead);
    Jobs.pgidTab[jobNb] = pgid;
    if (state == FOREGROUND)
      Jobs.fgNb = jobNb;
  } else {
    jobChanged = 0;
  }

  // restoring mask
  if (sigprocmask(SIG_SETMASK, &oldSet, NULL) < 0) {
    perror("Error restoring mask for changeJobsAll;");
    return -1;
  }

  // Use this line for debugging job state changes
  // printf("Jobs: [%d] %d\n", jobNb + 1, state);
  if (jobChanged)
    return 0;
  else {
    fprintf(stderr, "job %d was not changed (changeJobsAll)\n", jobNb);
    return -1;
  }
}
int changeJobsState(int jobNb, enum jobState state) {
  // blocking all signals
  sigset_t newSet, oldSet;
  if (sigfillset(&newSet) < 0) {
    perror("Error setting up sig mask for changeJobsState");
    return -1;
  }

  if (sigprocmask(SIG_BLOCK, &newSet, &oldSet) < 0) {
    perror("Error blocking signals for changeJobsState");
    return -1;
  }
  // code commences
  Jobs.stateTab[jobNb] = state;
  if (state == FOREGROUND)
    Jobs.fgNb = jobNb;

  // restoring mask
  if (sigprocmask(SIG_SETMASK, &oldSet, NULL) < 0) {
    perror("Error restoring mask for changeJobsState\n WARNING program is now "
           "broken, exit now\n");
    return -1;
  }
  return 0;
}

int newProcess(pid_t pid, int jobNb, enum processState state) {
  sigset_t newSet, oldSet;
  if (sigfillset(&newSet) < 0) {
    perror("Error setting up sig mask for newProcess");
    return -1;
  }

  if (sigprocmask(SIG_BLOCK, &newSet, &oldSet) < 0) {
    perror("Error blocking signals for newProcess");
    return -1;
  }

  char processAdded = 0;
  for (int i = 0; i < MAXCHILD; i++) {
    if (Jobs.childrenPids[jobNb].pidStateTab[i] == PEMPTY) {
      Jobs.childrenPids[jobNb].pid[i] = pid;
      Jobs.childrenPids[jobNb].pidStateTab[i] = state;
      processAdded = 1;
      break;
    }
  }

  // restoring mask
  if (sigprocmask(SIG_SETMASK, &oldSet, NULL) < 0) {
    perror("Error restoring mask for newProcess;");
    return -1;
  }

  if (!processAdded) {
    fprintf(stderr, "process %d was not added to job %d\n", pid, jobNb);
    return -1;
  }
  return 0;
}

int changeProccessState(pid_t pid, enum processState state) {
  sigset_t newSet, oldSet;
  if (sigfillset(&newSet) < 0) {
    perror("Error setting up sig mask for changeProccessState");
    return -1;
  }

  if (sigprocmask(SIG_BLOCK, &newSet, &oldSet) < 0) {
    perror("Error blocking signals for changeProccessState");
    return -1;
  }
  char found = 0;

  for (int i = 0; i < MAXJOBS; i++) {
    if (Jobs.stateTab[i] != EMPTY) {
      for (int j = 0; j < MAXCHILD; j++) {
        if (Jobs.childrenPids[i].pid[j] == pid) {
          Jobs.childrenPids[i].pidStateTab[j] = state;
          found = 1;
        }
      }
    }
  }
  // restoring mask
  if (sigprocmask(SIG_SETMASK, &oldSet, NULL) < 0) {
    perror("Error restoring mask for changeProccessState;");
    return -1;
  }
  if (!found) {
    fprintf(stderr, " process %d is not in the Jobs table\n", pid);
    return -1;
  }
  return 0;
}

int updateJobsList() {

  sigset_t newSet, oldSet;
  if (sigfillset(&newSet) < 0) {
    perror("Error setting up sig mask for changeProccessState");
    return -1;
  }

  if (sigprocmask(SIG_BLOCK, &newSet, &oldSet) < 0) {
    perror("Error blocking signals for changeProccessState");
    return -1;
  }

  unsigned char isStopped, isDone, isFg;
  int res = 0;

  for (int i = 0; i < MAXJOBS; i++) {
    isStopped = 0;
    isDone = 0;
    isFg = 0;
    if (Jobs.stateTab[i] == EMPTY)
      continue;
    if (i == Jobs.fgNb)
      isFg = 1;
    for (int j = 0; j < MAXCHILD; j++) {
      if (Jobs.childrenPids[i].pidStateTab[j] == PRUNNING) {
        isDone = 0;
        isStopped = 0;
        break;
      }
      switch (Jobs.childrenPids[i].pidStateTab[j]) {
      case PSTOPPED:
        isStopped = 1;
        break;
      case PDONE:
        isDone = 1;
        break;
      default:
        break;
      }
    }
    if (isStopped) {

      if (isFg)
        Jobs.fgNb = -1;
      if (Jobs.stateTab[i] != STOPPED) {
        if (changeJobsState(i, STOPPED) < 0) {
          res = -1;
          break;
        }
        printf("\nJob [%d] STOPPED      %s\n", i + 1, Jobs.commandTab[i]);
        res = 1;
      }

    } else if (isDone) {
      if (!isFg) {
        printf("\nJob [%d] DONE         %s\n", i + 1, Jobs.commandTab[i]);
        res = 1;
      } else {
        Jobs.fgNb = -1;
      }
      if (changeJobsAll(i, 0, "", EMPTY) < 0) {
        res = -1;
        break;
      };
    }
  }

  // restoring mask
  if (sigprocmask(SIG_SETMASK, &oldSet, NULL) < 0) {
    perror("Error restoring mask for changeProccessState;");
    return -1;
  }
  if (res < 0) {
    fprintf(stderr, " Error updating job list\n");
    return res;
  }
  return res;
}

int Foreground(char **args) {
  long val = 0;
  pid_t pgid;
  int jobNb;
  if (args[0] == NULL) {
    fprintf(stderr, "needs an argument\n");
    return -1;
  }
  if (args[0][0] == '%') {
    val = strtol(args[0] + 1, NULL, 0);
    if (val <= 0 || val > MAXJOBS) {
      fprintf(stderr, "Invalid job number\n");
      return -1;
    }
    // printf("Detected argument %ld\n", val);
    if (Jobs.stateTab[val - 1] == EMPTY) {
      fprintf(stderr, "%%%ld: no such job\n", val);
      return -1;
    }
    pgid = Jobs.pgidTab[val - 1];
    jobNb = val - 1;
  } else {
    if ((pgid = strtol(args[0], NULL, 0)) == 0) {
      fprintf(stderr, "invalid pgid\n");
      return -1;
    }
    //  printf("detected pgid %d\n", pgid);
    if ((jobNb = findJobNb(pgid)) < 0) {
      fprintf(stderr, "pgid %d not found\n", pgid);
      return -1;
    }
  }

  Kill(-pgid, SIGCONT);
  pid_t pidWait;
  int status;
  if (Jobs.stateTab[jobNb] != BACKGROUND) {
    while ((pidWait = waitpid(-pgid, &status, WCONTINUED)) > 0) {
      if (WIFCONTINUED(status)) {
        // printf("children group stopped via signal");
        changeProccessState(pidWait, PRUNNING);
        break;
      }
    }
  }
  changeJobsState(jobNb, FOREGROUND);

  if (tcsetpgrp(0, pgid) < 0) {
    fprintf(stderr, "Error setting up tcsetpgrp (shell)\n");
    Kill(-pgid, SIGKILL);
  }
  // ignoring SIGTTOU
  Signal(SIGTTOU, SIG_IGN);
  while ((pidWait = waitpid(-pgid, &status, WUNTRACED)) > 0) {
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
  if (tcsetpgrp(0, getpgid(0)) < 0) {
    fprintf(stderr, "Error recovering tcsetpgrp (shell)\n");
    Kill(-pgid, SIGKILL);
    exit(1);
  }
  Signal(SIGTTOU, SIG_DFL);

  return 0;
}

int Background(char **args) {
  long val = 0;
  pid_t pgid;
  int jobNb;
  if (args[0] == NULL) {
    fprintf(stderr, "needs an argument\n");
    return -1;
  }
  if (args[0][0] == '%') {
    val = strtol(args[0] + 1, NULL, 0);
    if (val <= 0 || val > MAXJOBS) {
      fprintf(stderr, "Invalid job number\n");
      return -1;
    }
    //  printf("Detected argument %ld\n", val);
    if (Jobs.stateTab[val - 1] == EMPTY) {
      fprintf(stderr, "%%%ld: no such job\n", val);
      return -1;
    }
    pgid = Jobs.pgidTab[val - 1];
    jobNb = val - 1;
  } else {
    if ((pgid = strtol(args[0], NULL, 0)) == 0) {
      fprintf(stderr, "invalid pgid\n");
      return -1;
    }
    //    printf("detected pgid %d\n", pgid);
    if ((jobNb = findJobNb(pgid)) < 0) {
      fprintf(stderr, "pgid %d not found\n", pgid);
      return -1;
    }
  }

  Kill(-pgid, SIGCONT);
  pid_t pidWait;
  int status;
  if (Jobs.stateTab[jobNb] != BACKGROUND) {
    while ((pidWait = waitpid(-pgid, &status, WCONTINUED)) > 0) {
      if (WIFCONTINUED(status)) {
        // printf("children group stopped via signal");
        changeProccessState(pidWait, PRUNNING);
        break;
      }
    }
  }
  int res = changeJobsState(jobNb, BACKGROUND);
  if (res == 0) {

    printf("Job [%d] %d     BACKGROUND     %s\n", jobNb + 1, pgid,
           Jobs.commandTab[jobNb]);

  } else {
    fprintf(stderr, "Error setting up background job\n");
  }

  return res;
}

int Terminate(char **args) {

  long val = 0;
  pid_t pgid;
  int jobNb;
  if (args[0] == NULL) {
    fprintf(stderr, "needs an argument\n");
    return -1;
  }
  if (args[0][0] == '%') {
    val = strtol(args[0] + 1, NULL, 0);
    if (val <= 0 || val > MAXJOBS) {
      fprintf(stderr, "Invalid job number\n");
      return -1;
    }
    //  printf("Detected argument %ld\n", val);
    if (Jobs.stateTab[val - 1] == EMPTY) {
      fprintf(stderr, "%%%ld: no such job\n", val);
      return -1;
    }
    pgid = Jobs.pgidTab[val - 1];
    jobNb = val - 1;
  } else {
    if ((pgid = strtol(args[0], NULL, 0)) == 0) {
      fprintf(stderr, "invalid pgid\n");
      return -1;
    }
    //    printf("detected pgid %d\n", pgid);
    if ((jobNb = findJobNb(pgid)) < 0) {
      fprintf(stderr, "pgid %d not found\n", pgid);
      return -1;
    }
  }

  Kill(-pgid, SIGKILL);
  return 0;
}
int Stop(char **args) {

  long val = 0;
  pid_t pgid;
  int jobNb;
  if (args[0] == NULL) {
    fprintf(stderr, "needs an argument\n");
    return -1;
  }
  if (args[0][0] == '%') {
    val = strtol(args[0] + 1, NULL, 0);
    if (val <= 0 || val > MAXJOBS) {
      fprintf(stderr, "Invalid job number\n");
      return -1;
    }
    //  printf("Detected argument %ld\n", val);
    if (Jobs.stateTab[val - 1] == EMPTY) {
      fprintf(stderr, "%%%ld: no such job\n", val);
      return -1;
    }
    pgid = Jobs.pgidTab[val - 1];
    jobNb = val - 1;
  } else {
    if ((pgid = strtol(args[0], NULL, 0)) == 0) {
      fprintf(stderr, "invalid pgid\n");
      return -1;
    }
    //    printf("detected pgid %d\n", pgid);
    if ((jobNb = findJobNb(pgid)) < 0) {
      fprintf(stderr, "pgid %d not found\n", pgid);
      return -1;
    }
  }

  Kill(-pgid, SIGSTOP);
  return 0;
}
