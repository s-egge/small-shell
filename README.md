# small-shell

This program runs a "miniature shell" in the terminal. 

**Compile:**
   
  gcc --std=gnu99 smallsh.c -o smallsh

**Run executable:**

  ./smallsh

## Shell features:

* Prompts with the colon symbol

  :
  
* Can handle blank lines and comments beginning with a pound sign **#**
   
   #this is an example of comment

* Variable expansion: Any instance of **$$** will expand into the process ID for the shell

  : cat my PID is $$
  
  prints
  
  my PID is 45678
  
  
* **exit**, **CD**, and **status** commands are built in to the shell, while all other commands will fork a new process and run through **execvp**. 

* Runs processes in the foreground unless last argument is **&**, in which case the process will run in the background. If run in the background, the shell will notify the user when the background process terminates. Example:

  : sleep 5 &

  background pid is 4589

  : # the background process is sleeping for five seconds now

  : # still sleeping

  background pid 4589 is done: exit value 0
  
  :

* Handles input and output redirection through **>** and **<** symbols
  
  : cat < inputfile.txt > outputfile.txt
  
* Implements two custom signal handlers, SIGINT and SIGSTP
  
  * The shell itself ignores the CTRL-C command for SIGINT, as do all children processes that are running in the background. Any child process that is running in the foreground will not ignore the command, and will terminate itself as normal, returning control back to the parent process (the shell). 
  
  * CTRL-Z sends a SIGTSTOP signal to the shell, and it will no longer allow new background processes to run. Any currently running background processes will continue as normal. If the user requests to start a new process in the background, it will be run in the foreground. A second CTRL-Z signal will return the shell back to normal, allowing background processes to be run.
