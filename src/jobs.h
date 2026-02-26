#include "shell.h"

enum processState { PEMPTY, PRUNNING, PSTOPPED, PDONE };
typedef struct {
  pid_t pid[MAXCHILD];
  enum processState pidStateTab[MAXCHILD];
} jobPidsTab;

enum jobState { EMPTY, FOREGROUND, BACKGROUND, STOPPED };

typedef struct {
  int fgNb;
  pid_t pgidTab[MAXJOBS];
  enum jobState stateTab[MAXJOBS];
  char commandTab[MAXJOBS][BUFSIZ];
  jobPidsTab childrenPids[MAXJOBS];
} jobsTab;

extern jobsTab Jobs;

// find an empty index in the global variable Jobs
int emptyJobNb();

// find the according jobs index of a pgid in the global table Jobs
int findJobNb(pid_t pgid);

// Display the job list (builtin)
int ActiveJobsList(char **args);

// Change everything about a job in the global variable Jobs, with index jobNb
// group pid pgid, command line lineRead, state state.
int changeJobsAll(int jobNb, pid_t pgid, char *lineRead, enum jobState state);

// Change the state of a job in the global variable Jobs
int changeJobsState(int jobNb, enum jobState state);

// create a new process for a Job, in the global varaible jobNb
int newProcess(pid_t pid, int jobNb, enum processState state);

// Change state of a process, will automatically find it in the global varirable
// Jobs
int changeProccessState(pid_t pid, enum processState state);

// update the global variable Jobs, butcher done jobs, return 1 if there is a
// change in any job state
int updateJobsList();

// Built in function, brings to foreground job %i or pgid
int Foreground(char **args);

// Built in function, brings to foreground job %i or pgid
int Background(char **args);

// Terminate a job
int Terminate(char **args);

// Force stop a job
int Stop(char **args);
