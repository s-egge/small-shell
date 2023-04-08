/****************************************************************************************
 * Description: Creates a "small shell" that accepts command line inputs from the user in 
 *              the form of [arg1] [arg2]...[<] [inputfile] [>] [outputfile] [&]
 *              where < and > signal input and output (and are optional), & signals to run
 *              in the background (also optional). Any instance of "$$" will be replaced
 *              with the PID of the shell process. 
 *              
 * Compile:  gcc --std=gnu99 smallsh.c -o smallsh
 * Run:      ./smallsh
 * **************************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> //fork, getpid, chdir
#include <sys/types.h> //pid_t
#include <sys/wait.h> //waitpid
#include <fcntl.h> //file functions
#include <signal.h> //for sigaction

#define MAX_INPUT 2048
#define MAX_ARGS 512

// the sig handler cannot be passed variables, so foreground flag must be global
int fgFlag = 0;

// shell struct holds all variables for the small shell
struct shell{
    int pid;
    int exitShell;
    int exitStatus;
    pid_t childPID;
    int bgProcesses[50];
    int bgFlag;
    char* cmdLineArgs[MAX_ARGS];
    char* inputFile;
    char* outputFile;
};

/* 
 * Sets up a small shell struct, assigns memory, then returns pointer to the new shell
 */
struct shell* shellSetUp(){
    struct shell* shell = calloc(1, sizeof(struct shell));//malloc(sizeof(struct shell));
    shell->pid = getpid();
    shell->childPID = -5;
    shell->exitShell = 0;
    shell->exitStatus = 0;
    shell->bgFlag = 0;
    
    //bgProcesses will keep track of bgProcesses that haven't terminated yet
    memset(shell->bgProcesses, 0, 50);

    //cmdLineArgs is an array of all arguments user put in
    memset(shell->cmdLineArgs, '\0', MAX_ARGS);

    //if input/output processes are requested, these will keep track of the files
    //to input/output to
    shell->inputFile = NULL;
    shell->outputFile = NULL;

    return shell;
}


/*
 * Resets all of the command line arguments, input/output file names, and resets
 * background flag in the shell struct that's passed in. This "cleans" the arguments
 * to prepare for getting more commands from the user
 */
void commandLineArgumentReset(struct shell* shell){
    //free memory used by arguments
    for(int x = 0; x < MAX_ARGS; x++){
        if(shell->cmdLineArgs[x] != '\0')
            free(shell->cmdLineArgs[x]);
        else
            break;
    }

    //reset cmdLineArgs
    memset(shell->cmdLineArgs, '\0', MAX_ARGS);

    //free and reset input/output
    free(shell->inputFile);
    shell->inputFile = NULL;
    free(shell->outputFile);
    shell->outputFile = NULL;

    //reset bgFlag to 0
    shell->bgFlag = 0;
}


/*
 * Looks through given string and replaces any instance of "$$" with the processes PID
 */
void pidReplacement(char* command){
    int pid = getpid();

    for (int x = 0; x < strlen(command); x++) {
        // swapping $$ for %d so we can sprintf pid in
        if(command[x] == '$' && x + 1 < strlen(command) &&
            command[x + 1] == '$'){
            //create a temporary command to use with sprintf
            char* tempInput = malloc(strlen(command) + 1);
            strcpy(tempInput, command);

            //replace $$ with %d to use with sprintf
            tempInput[x] = '%';
            tempInput[x + 1] = 'd';

            //replace original $$ with pid
            sprintf(command, tempInput, pid);

            //free tempInput memory
            free(tempInput);
        }
    }
}


/*
 * Loops through the background process aray, for each index that holds a PID instead of 
 * a '0', checks if that process has finished running or not. If it has, it prints the
 * exit status of the process and clears the PID from the array. It does not wait for any
 * processes to finish, merely checks IF they are finished. 
 */
void checkBackgroundProcessTermination(struct shell* shell){
    int childStatus;

    //check if each background process has terminated
    for(int x = 0; x < 50; x++){
        if(shell->bgProcesses[x] != 0){
            if(waitpid(shell->bgProcesses[x], &childStatus, WNOHANG) != 0){

                //check how background process exited and print exit/termination status
                //code altered from module 4: monitoring child processes
                if(WIFEXITED(childStatus)){
                    //child terminated normally
                    printf("Background pid %d is done: Exit value %d\n", shell->bgProcesses[x], WEXITSTATUS(childStatus));
                } 
                else{
                    //child terminated abnormally
                    printf("Background pid %d is done: Terminated by signal %d\n", shell->bgProcesses[x], WTERMSIG(childStatus));
                }

                shell->bgProcesses[x] = 0;
            }
        }
    }
}


/*
 * Gets command line arguments from the user. Does not error handle correct command line
 * syntax. Parses entered commands and puts them in to an array of commands, which are then
 * checked for "$$" replacement, I/O redirects, and background command '&'. Clears IO 
 * and background commands from command line array as they will not be passed to execvp
 * later.
 */
void getCommandLineArguments(struct shell* shell, int maxArgs){
    int invalidInput = 1;
    char userInput[MAX_INPUT];


    //loop until the user provides one or more valid commands
    while(invalidInput){
        checkBackgroundProcessTermination(shell);
        printf(": ");
        fflush(stdout);
        fgets(userInput, MAX_INPUT, stdin);
        
        //check for comments/empty input
        if(strlen(userInput) > 1 && userInput[0] != '#')
            invalidInput = 0;
    }

    //---separate input in to individual arguments

    //first get rid of extraneous newline character that fgets takes in
    strtok(userInput, "\n");

    //variables for tokenizing
    char *saveptr;
    int cmdNumber = 0;

    //grab and process the first command
    char* token = strtok_r(userInput, " ", &saveptr);
    shell->cmdLineArgs[cmdNumber] = malloc(strlen(token) + 1);
    strcpy(shell->cmdLineArgs[cmdNumber], token);
    cmdNumber++;

    //continue grabbing and processing commands until there aren't any left
    while(token = strtok_r(NULL, " ", &saveptr)){
        //assign memory and copy over token
        shell->cmdLineArgs[cmdNumber] = malloc(strlen(token) + 1);
        strcpy(shell->cmdLineArgs[cmdNumber], token);

        //variable expansion
        pidReplacement(shell->cmdLineArgs[cmdNumber]);
        
        cmdNumber++;
    }

    int IOstart = 0;

    //check for input/output and set variables if necessary
    for(int x = 0; x < cmdNumber; x++){
        //check for input file
        if(strcmp(shell->cmdLineArgs[x], "<") == 0){

            //keep track of where IO starts in order to clear command line arguments later
            if(IOstart == 0 || IOstart > x)
                IOstart = x;

            //assign memory for inputFile, then copy string over
            shell->inputFile = malloc(strlen(shell->cmdLineArgs[x + 1]) + 1);
            strcpy(shell->inputFile, shell->cmdLineArgs[x + 1]);
        }

        //check for output file
        else if(strcmp(shell->cmdLineArgs[x], ">") == 0){

            //keep track of where IO starts in order to clear command line arguments later
            if(IOstart == 0 || IOstart > x)
                IOstart = x;

            //assign memory for outputFile, then copy string over
            shell->outputFile = malloc(strlen(shell->cmdLineArgs[x + 1]) + 1);
            strcpy(shell->outputFile, shell->cmdLineArgs[x + 1]);
        }

        //check if user wants to run in background
        else if(strcmp(shell->cmdLineArgs[x], "&") == 0 && shell->cmdLineArgs[x + 1] ==  '\0'){
            shell->bgFlag = 1;

            //clear & from the command arguments
            free(shell->cmdLineArgs[x]);
            shell->cmdLineArgs[x] = '\0';
        }
    }

    //if there is IO, free and reset all commands after IOstart to '\0' for execvp later
    if(IOstart != 0){
        for(int x = IOstart; x < MAX_ARGS; x++){
            if(shell->cmdLineArgs[x] != '\0'){
                free(shell->cmdLineArgs[x]);
                shell->cmdLineArgs[x] = '\0';
            }
        }
    }
}


/*
 * Custom handler for SIGTSTP. Enters foreground-only mode if not already in it, otherwise
 * exits forground-only mode. 
 */
void handle_SIGTSTP(int signo){
    char* enterMessage = "\nEntering foreground-only mode (& is now ignored)\n";
    int enterMessageLen = strlen(enterMessage);

    char* exitMessage = "\nExiting foreground-only mode\n";
    int exitMessageLen = strlen(exitMessage);

    //toggle forground f
    if(fgFlag == 0){
        write(STDOUT_FILENO, enterMessage, enterMessageLen);
        fgFlag = 1;
    }
    else{
        write(STDOUT_FILENO, exitMessage, exitMessageLen);
        fgFlag = 0;
    } 
}


/*
 * Kills any running child processes and sets variable to exit the shell in main loop
 */
void exitSmallShell(struct shell* shell){
    //kill any running child processes
    for(int x = 0; x < 50; x++){
        if(shell->bgProcesses[x] != 0)
            kill(shell->bgProcesses[x], SIGKILL);
    }
    //set variable to terminate while loop
    shell->exitShell = 1;
}


/*
 * Prints the exit status of either the most recently ran foreground process, or the 
 * current exit status of the shell if no foreground processes have been run
 */
void printShellStatus(struct shell* shell){
    //if exitStatus is 0, that means last exit was normal
    if(shell->exitStatus == 0)
        printf("exit value %d\n", 0);

    //if last exit wasn't 0, check if last child exited normally or abnormally
    else if(WIFEXITED(shell->exitStatus)){
        //child terminated normally
        printf("exit value %d\n", WEXITSTATUS(shell->exitStatus));
    } 
    else{
        //child terminated abnormally
        printf("terminated by signal %d\n", WTERMSIG(shell->exitStatus));
    }

    fflush(stdout);
}


/*
 * Changes the directory of the shell based on user input. If no path was given, changes
 * directory to home directory
 */
void changeShellDirectory(struct shell* shell){
    //if user entered a path, go there
    if(shell->cmdLineArgs[1] != '\0'){
        chdir(shell->cmdLineArgs[1]);
    }
    //otherwise go to the home directory
    else {
        chdir(getenv("HOME"));
    }
}


/*
 * Forks and creates a new process in either the foreground or background, depending on
 * command line inputs. Redirects input/output if requested by user. 
 */
void createNewProcess(struct shell* shell, struct sigaction SIGINT_action){
    int childStatus;
    //fork a child process
    shell->childPID = fork();

    switch(shell->childPID){
        case -1: 
            //fork and the creation of child process failed
            perror("fork() failed!\n");
            fflush(stdout);
            exit(1);
            break;
        case 0:
            //child will execute code in this branch

            //if child is running in forground, don't ignore ^C
            if(shell->bgFlag == 0 || fgFlag == 1){
                SIGINT_action.sa_handler = SIG_DFL;
                sigaction(SIGINT, &SIGINT_action, NULL);
            }
            
            //check if input redirect is needed (code adapted from module 4: processes and I/O)
            if(shell->inputFile != NULL || shell->bgFlag == 1){

                //open source file
                int sourceFD;  

                //open file if user entered it, otherwise redirect to /dev/null 
                //in case of background process
                if(shell->inputFile != NULL)
                    sourceFD = open(shell->inputFile, O_RDONLY);
                else
                    sourceFD = open("/dev/null", O_RDONLY);

                if (sourceFD == -1) {
                    printf("Cannot open %s for input\n", shell->inputFile);
                    exit(1); 
                }

                //redirect stdin to source file
                int result = dup2(sourceFD, 0);
                if (result == -1) { 
                    perror("Unable to reroute stdin to requested file"); 
                    exit(2); 
                }

                //set trigger to close file when child exits
                fcntl(sourceFD, F_SETFD, FD_CLOEXEC);
            }

            //check if output redirect is needed (code adapted from module 4: processes and I/O)
            if(shell->outputFile != NULL || shell->bgFlag == 1){
                //open the output file
                int outputFD;
                
                //open file if user entered it, otherwise redirect to /dev/null
                //in case of background process
                if(shell->outputFile != NULL)
                    outputFD = open(shell->outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                else
                    outputFD = open("/dev/null", O_WRONLY | O_CREAT | O_TRUNC, 0644);

                if (outputFD == -1) { 
                    printf("Cannot open %s for input\n", shell->outputFile);
                    exit(1); 
                }

                // Redirect stdout to target file
                int result = dup2(outputFD, 1);
                if (result == -1) { 
                    perror("Unable to reroute stdout to requested file"); 
                    exit(2); 
                }

                //set trigger to close file when child exits
                fcntl(outputFD, F_SETFD, FD_CLOEXEC);
            }

            //run commands through execvp, printing if an error occurred
            if(execvp(shell->cmdLineArgs[0], shell->cmdLineArgs)){
                perror(shell->cmdLineArgs[0]); 
                exit(EXIT_FAILURE);
            }
            break;

        default:
            //parent will execeute code in this branch

            //print process PID if running in background and add to bg array
            if(shell->bgFlag == 1 && fgFlag == 0){

                //loop for an "empty" index to add PID to
                for(int x = 0; x < 50; x++){
                    if(shell->bgProcesses[x] == 0){
                        shell->bgProcesses[x] = shell->childPID;
                        break;
                    }
                }

                printf("Background PID is %d\n", shell->childPID);
                fflush(stdout);
            }


            //wait for child to terminate
            else{
                waitpid(shell->childPID, &(shell->exitStatus), 0);

                //if child terminated abnormally, print out signal
                if(WIFSIGNALED(shell->exitStatus)){
                    printf("terminated by signal %d\n", WTERMSIG(shell->exitStatus));
                }
            }
    }
}


int main(void){
    //set up our small shell struct
    struct shell* shell = shellSetUp();

    //ignore ^C signals in main
    struct sigaction SIGINT_action = {0};
    SIGINT_action.sa_handler = SIG_IGN;
    sigfillset(&SIGINT_action.sa_mask);
    SIGINT_action.sa_flags = 0;
    sigaction(SIGINT, &SIGINT_action, NULL);

    //set up ^Z foreground mode signal
    struct sigaction SIGTSTP_action = {0};
    SIGTSTP_action.sa_handler = handle_SIGTSTP;
    sigfillset(&SIGTSTP_action.sa_mask);
    SIGTSTP_action.sa_flags = 0;
    sigaction(SIGTSTP, &SIGTSTP_action, NULL);


    //this while loop runs the shell until user decides to exit
    while(shell->exitShell != 1){
        getCommandLineArguments(shell, MAX_ARGS);

        //if command 'exit', kill all child processes and exit
        if(strcmp(shell->cmdLineArgs[0], "exit") == 0){
            exitSmallShell(shell);
        }
        
        //print exit status
        else if(strcmp(shell->cmdLineArgs[0], "status") == 0){
            printShellStatus(shell);
        }

        //change directory
        else if(strcmp(shell->cmdLineArgs[0], "cd") == 0){
            changeShellDirectory(shell);
        }

        // all other commands create a new process
        else{
            createNewProcess(shell, SIGINT_action);

            //clear any output in the buffer, otherwise ^Z will print previous message
            fflush(stdout);
        }

        commandLineArgumentReset(shell);
    }

    //free up remaining memory in shell
    free(shell);

    return EXIT_SUCCESS;
}