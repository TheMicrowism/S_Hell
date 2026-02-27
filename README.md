# HOMEMADE SHELL IN C

**This shell is built as a noted project for SR6 - L3 INFO GENERALE - UFR IM2AG**

*Duy Minh LE, Anthony ZENG*

*While it might sound a bit artificial, this README is indeed written by a human being*

Compile with 

``` 
make
```

## Core program structure

The block of pseudo code below describes the functionality of the shell 

```
while True:
	backUp_stdinout() 				//keep a copy of stdin and stdout
	printcwd()   					//print the current working directory (nice)
	l = readcmd()					//parsing input
	redirect(l)						//redirection of input, output
	if (!ExecuteBuiltin(l)){		//execute builtins
		ExecuteExternals(l)			//execute eternal commands
	}
	recover_stdinout()				
```

## General command execution

The parsing of commands are done by `readcmd.c` via `readcmd`

### Shell builtins execution

Shell builtins are essentially functions triggered by certain commands. We define structures:

```
typedef struct {
  int (*BuiltinFunc)(char **args);
  char *BuiltinName;
} builtin;

typedef struct {
  int size;
  builtin tab[];
} builtinTab;

builtinTab Builtins {}		//defini dans shell.c
```
La global variable `Builtins` is of type `builtinTab` associates function with strings. If the number of pipe is 0, the parsed command is passed to a function to look for a corresponding builtin function, if matched, said builtin is executed via a simple function call.

List of builtin functions and their command:  
- `Quit` mapped to "q" and "quit". *This will terminate (via `SIGKILL`) any running child processes and exit the shell.*
- `Echo` mapped to "echo". 
- `ChangeDir` mapped to "cd". 
- `ActiveJobsList` mapped to "jobs". 
- `Foreground` mapped to  "fg".
- `Background` mapped to "bg".
- `Terminate` mapped to "term". *Terminates a job or a process group in the job table via `SIGKILL`.* 
- `Stop` mapped to "stop". *Stop a job or a process group in the job table via `SIGSTOP`.*   

### External commands execution

The variable `exInFunction` represented the execution of a shell builtins. If none were executed, **and the number of running jobs has not yet reached MAXJOBS (`shell.h`)** we execute the parsed commands as external commands.

- For each command, we `Fork()` (via `csapp.c`) a child process, the child execute the command via `execvp()` .  
- We take the `pid` of the first child and all children of job to that `pid` as `pgid`.  
- After all child processes have been created and **the launched job is a foreground one**, we pass the terminal to the group using `tcsetpgrp()` the shell waits (`waitpid()`) for all children of the active foreground job to terminate/change state.
- A new job is created in the job table accordingly.

#### Child processes signal handling

The shell, before the main `while` loop, changes its `SIGINT` and `SIGTSTP` handlers to simply displaying a message:

```
/home/microwism/Apps/S_Hell> ^Ccall quit or q to exit shell
```

`tcsetpgrp()` is necessary because the children are put into another group other than that of the shell, so any signal the shell receives will not be forwarded to its children. 

At the beginning of each child, we switched back the handlers to their default ones. As an extra precaution, any error handling from the shell invokes `SIGKILL` to the children instead of `SIGINT`. 

 #### Input/Output redirection

The function `readcmd()` gives us a struct that contains parsed file names for In/Output redirection. We simply open the files using `open()` then using the new file descriptors return to override`stdin` and `stdout` using `dup2()`. We keep a backup of `stdin` and `stdout` to recover them after all foreground executions is done.

#### Pipes

We count the number of command parsed with `nbCmd` ,then we create an array `int pipes[nbCmd -1][2]` to serve as a pipe array.

For each child process of index `i` :

- If `i > 0` ,  its input file descriptor is replaced with the read end of the `i-1`th pipe.

- If `i < nbCmd -1`, its output file descriptor is replaced with the read end of the `i`th pipe.   

This is completed using `dup2()`.

The shell then closes every end of every pipe in the array, and we close every unused end of every pipe in every child processes.

#### Background job execution 

The struct given by `readcmd()` now has `unsigned char isBackground` that indicates if the commands parsed should be executed in the background. As described before, when a background job is launched the shell does not wait for its termination and all is handled by a `SIGCHLD` handler.

The shell announces the job number,  `pgid` and the job's commands when a job is launched in the background.

```
/home/microwism/Apps/S_Hell> sleep 2 &
Job [1] 7534     BACKGROUND     sleep 2 &
/home/microwism/Apps/S_Hell>
```

**The parser is not modified to recognize a chain of multiple jobs**. The following input is **NOT **excepted and will return a misplaced & error.

```
sleep 100 & sleep 100 & 
```

### Zombies killer 

The changing of state of child processes are handled at two different places. One in `handlerSIGCHLD` and another at the end of the creation of the children in the main loop.

In the handler: 

- We acknowledge the termination/state change of any child using `waitpid(-1, &status, WNOHANG|WUNTRACED|WCONTINUED)`. 
- After all pending children is acknowledged, we update the job table to reflect any change of state of any job.

In the loop:

- We wait for the foreground processes group using `waitpid(-pgid, &status, WUNTRACED)`
- If a process is caught being stopped, we stop the wait and update its state to `PSTOPPED`, the rest of the processes group is handled by the handler. 
- If a process is done, we change its state in the job table to `PDONE`.
- After waiting, we update the job table for any change in job state.

## Management of jobs 

The job table mentioned in all previous section is of structure: 

```
typedef struct {
  int fgNb;
  pid_t pgidTab[MAXJOBS];
  enum jobState stateTab[MAXJOBS];
  char commandTab[MAXJOBS][BUFSIZ];
  jobPidsTab childrenPids[MAXJOBS];
} jobsTab;

jobsTab Jobs = {.fgNb = -1};
```

Inside the global `Jobs` are `pgid` and state of every ongoing jobs, and `pid` of all the child processes executing any job.

We constructed a variety of functions that serves as an interface with the global job table `Jobs` (see `jobs.h`). *It is imperative that any function that touches `Jobs` needs to block any signal that can provoke another update to job table (we ended up blocking all) before beginning its modifications.* 

`Foreground`(), `Background()`, `Terminate()`, `Stop()` are essentially wrappers for `kill()` (`Kill()` exit the shell if failed)  to send signals to jobs, with the exception of `Foreground` having to handle the passing of the terminal to the foreground job. 

**ALL** mention of number of job in the code is an **INDEX** and starts from 0. The job number that starts from 1 is for the "front end" only. 

### Jobs states 

```
enum jobState { EMPTY, FOREGROUND, BACKGROUND, STOPPED };
```

`EMPTY` signifies this job index is unoccupied. `FOREGROUND`, `BACKGROUND`, `STOPPED` are explicit. 

A job that has done its execution in the background will be announced as `DONE` whenever the shell detects all of its processes are done. See example :

```
/home/microwism/Apps/S_Hell> sleep 1 &
Job [1] 10543     BACKGROUND     sleep 1 &
/home/microwism/Apps/S_Hell>
Job [1] DONE         sleep 1 &

/home/microwism/Apps/S_Hell>
```

Similarly, the shell will announce the stopping of a job when it detects all of its processes has been stopped. 

### Processes states 

```
enum processState { PEMPTY, PRUNNING, PSTOPPED, PDONE };
```

Processes state are keep to date mainly using the `SIGCHLD` handler. A process of state `PRUNNING` prevents its job from turning `STOPPED` or being announced as `DONE` .

## Shell-like expansion

We use `wordexp()` to expand every word parsed with a shell-like behavior. We create `wordexp_t eArgs` and then iterate through commands, append the new expanded words to `eArgs` and simply pass it to any builtin function or `execvp` as the argument vector. 

*Any error caught set `successWordexp` to 0, and we halt the execution of commands* 

Example of shell-like expansion:

```
/home/microwism/Apps/S_Hell> echo src/*
src/builtins.c src/builtins.h src/csapp.c src/csapp.h src/jobs.c src/jobs.h src/readcmd.c src/readcmd.h src/shell.c src/shell.h
```

## Non - interactive mode

This shell is fitted with a non-interactive mode that can be triggered by any instructions injecting driver. The shell detects if it is connected to a terminal, if not, it does not call `tcsetpgrp()` to prevent error.  

When using the `perl` shell driver, inputting `cat` will simply close the "stdin" right after the `cat` is launched.