/*******************************************************************************
 * Amy Salley
 * 
 * Description: C shell, smallsh
 * This program provides a prompt for running commands, handles blank lines and 
 * comments (lines beginning with the # character), provids expansion for the
 * variable $$, executes exit, cd, and status commands via code built into the
 * shell, executes other commands by creating new processes using a function
 * from the exec family of functions, supports input and output redirection,
 * supports running commands in foreground and background processes, and 
 * implements custom handlers for SIGINT and SIGTSTP.
 *******************************************************************************/
 
 	
#include <stdio.h>    	
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>   	
#include <unistd.h>   	
#include <string.h>		
#include <fcntl.h>
#include <signal.h>

int forgroundOnly = 0;	// Global variable to keep track of foreground-only mode
int controlCZUsed = 0;	// Global variable to keep track if ^C or ^Z was used
pid_t childBGPid = 0;	// Global variable to keep track of background pid
pid_t childFGPid = 0;	// Global variable to terminate the foreground child
						// process when SIGINT (^C) is sent	


/*******************************************************************************
handle_SIGTSTP funcion
Signal handler for SIGTSTP (^Z)
Lets user enter and exit foreground-only mode by typing ^Z

** Modified from Module 5 Exploration, Example: Custom Handler for SIGINT **
*******************************************************************************/
void handle_SIGTSTP(int signo)
{
	fflush(stdout);

	// Exit foreground-only mode
	if (forgroundOnly)
	{
		char* message = "\nExiting foreground-only mode\n";
		write(STDOUT_FILENO, message, 30);
		forgroundOnly = 0;
	}

	// Enter foreground-only mode
	else
	{
		char* message = "\nEntering foreground-only mode (& is now ignored)\n";
		write(STDOUT_FILENO, message, 50);
		forgroundOnly = 1;
	}

	fflush(stdout);
	controlCZUsed = 1;			// Set boolean value for ^Z or ^C used
}


/*******************************************************************************
handle_SIGINT funcion
Signal handler for SIGTSTP (^C)
Terminates child running as a foreground process by typing ^C

** Modified from Module 5 Exploration, Example: Custom Handler for SIGINT **
*******************************************************************************/
void handle_SIGINT(int signo)
{
	// Only terminate the foreground child process if there is one
	if (getpgid(childFGPid) == getpid())
	{
		kill(childFGPid, 0);
	}
	char* message = "\n";				// Moves prompt to new line
	write(STDOUT_FILENO, message, 1);
	fflush(stdout);
	controlCZUsed = 1;					// Set boolean value for ^Z or ^C used

} 


/*******************************************************************************
runCommand function
This function forks a child process.  The child process will redirect the 
input and output if specified by the user.  The child process runs the 
command specified by the user.  The parent function will wait for the child
to finish if the child is running in the foreground.  Parent will not wait
for the child to finish if the child process is to be run in the background.
Periodically checks for background child processes to finish. 

** Modified from Module 4 Exploration examples included in
   Process API - Monitoring Child Processes **
*******************************************************************************/
int runCommand(char* command[], char* inputFile, char* outputFile, int runBG)
{	
	int childStatus;
	int exitStatus = 1;
	int sourceFD;				// File descriptor for input
	int targetFD;				// File descriptor for output
	
	// Arguments for exec() function
	char *newargv[] = {command[0], command[1], command[2]};

	// struct for handling SIGINT (^C)
	struct sigaction SIGINT_action = {0};

	// Fork a child process
	pid_t spawnpid = fork();

	switch(spawnpid)
	{
		// Fork failed
		case -1:
			perror("fork()\n");
			fflush(stdout);
			exitStatus = 1;
			exit(1);

		// Child process
		case 0:

			// Handle ^C for foreground child process
			SIGINT_action.sa_handler = handle_SIGINT;
			sigfillset(&SIGINT_action.sa_mask);
			SIGINT_action.sa_flags = 0;
			sigaction(SIGINT, &SIGINT_action, NULL);

			// Input file: using example from Module 5 Redirecting Stdin
			if (inputFile)
			{
				sourceFD = open(inputFile, O_RDONLY);

				// Error in opening input file
				if (sourceFD == -1)
    			{
					perror("source open()");
					fflush(stdout);
					exitStatus = 1;
					exit(1);
    			}	

				// Redirect input
				int result = dup2(sourceFD, 0);

				// Error in input redirection
				if (result == -1)
				{
					perror("source dup2()");
					fflush(stdout);
					exitStatus = 1;
					exit(1);
				}
			}

			// Output file: using example from Module 5 Redirecting Stdout
			if (outputFile)
			{
				targetFD = open(outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0777);

				// Error in opening output file
				if (targetFD == -1)
    			{
        			perror("target open()");
					fflush(stdout);
					exitStatus = 1;
					exit(1);
    			}

				// Redirect output
				int result = dup2(targetFD, 1);

				// Error in output redirection
				if (result == -1)
				{
					perror("target dup2()");
					fflush(stdout);
					exitStatus = 1;
					exit(1);
				}
			}
		
			// Run the command in the child process
			execvp(newargv[0], newargv);

			// exec only returns if there is an error
			perror("Error");
			fflush(stdout);
			exitStatus = 1;
			exit(1);
		
		// Parent process
		default:

			// Run the child process in the background
			if (runBG && !forgroundOnly)
			{
				// Process will ignore ^C
				childFGPid = 0;	
				childBGPid = spawnpid;

				// Do not wait for child process to finish
				pid_t childPid = waitpid(spawnpid, &childStatus, WNOHANG);
				printf("background pid is %i\n", spawnpid);
				fflush(stdout);
			}

			// Run the child process in the foreground
			else
			{
				// Process to teminate with ^C
				childFGPid = spawnpid;		

				// Wait for child process to finish
				pid_t childPid = waitpid(spawnpid, &childStatus, 0);		
			}
			
			// Modified from Interpreting the Termination Status example, Module 4
			// The child process terminated normally
			if (WIFEXITED(childStatus))
			{
				exitStatus = (WEXITSTATUS(childStatus));
			}

			// The child process was terminated abnormally
			else
			{
				printf("terminated by signal %i\n", WTERMSIG(childStatus));
				fflush(stdout);
				exitStatus = (WTERMSIG(childStatus));
			}

			// Reset child foreground pid for use with ^C
			childFGPid = 0;

			// Check for background child process finishing
			pid_t checkChildinBG = waitpid(-1, &childStatus, WNOHANG);
			while (checkChildinBG > 0)
			{
				// Modified from Interpreting the Termination Status example, Module 4
				printf("background pid %i is done: ", checkChildinBG);
				if (WIFEXITED(childStatus))
				{
					printf("exit status %i\n", WEXITSTATUS(childStatus));
				}
				else
				{
					printf("terminated by signal %i\n", WTERMSIG(childStatus));
				}
				fflush(stdout);

				// Reset child background pid
				childBGPid = 0;

				// Update checkCildinBG
				checkChildinBG = waitpid(-1, &childStatus, WNOHANG);
			}

			// Close input and output files if necessary
			if (sourceFD)
			{
				close(sourceFD);
			}
			if (targetFD)
			{
				close(targetFD);
			}
			
	}
	return exitStatus;
}


/*******************************************************************************
parseInput function
Parse the user input line.  Assigns input and output files if specified.
Replaces "$$" with the process pid, and determines if a process will run
in the background. Calls runCommand to execute the command.
*******************************************************************************/
int parseInput(char* input)
{
	char* command;					// first command entered by user
	char* command2 = NULL;			// used to build command array
	char* inputFile = NULL;			// file for input redirect	
	char* outputFile = NULL;		// file for output redirect
	char* saveptr;					// pointer used with strtok_r
	char *commandArray[512];		// build array of command arguments
	int runBG = 0;					// boolean value for background process
	int i = 1;						// counter for command array
	int exitStatus = 1;

	// Remove the new line character at the end of the input
	for(int i = 0; i < strlen(input); i++)
	{
		if (input[i] == '\n')
		{
			input[i] = '\0';
		}
	}

	// Initialize the command array
	for (int j=0; j<512; j++)
	{
		commandArray[j] = NULL;
	}

	// Get the first command
	char* token = strtok_r(input, " ", &saveptr);
	command = calloc(strlen(token) + 1, sizeof(char));
	strcpy(command, token);

	commandArray[0] = command;

	// Parse the rest of the user input
	token = strtok_r(NULL, " ", &saveptr);
	while (token)
	{
		// Input file detected
		if (strcmp(token, "<") == 0)
		{
			token = strtok_r(NULL, " ", &saveptr);
			inputFile = calloc(strlen(token) + 1, sizeof(char));
    		strcpy(inputFile, token);
		}

		// Output file detected
		else if (strcmp(token, ">") == 0)
		{
			token = strtok_r(NULL, " ", &saveptr);
			outputFile = calloc(strlen(token) + 1, sizeof(char));
    		strcpy(outputFile, token);
		}

		// Run process in the background
		else if (strcmp(token, "&") == 0)
		{
			runBG = 1;
		}

		// Add argument to command array
		else
		{
			command2 = calloc(strlen(token) + 1, sizeof(char));
			strcpy(command2, token);

			// Replace "$$" with pid
			if (strstr(command2, "$$"))
			{
				// Get pid
				int pid = getpid();

				// Create a temporary name without $$
				char* tempName = calloc(strlen(command2)-2, sizeof(char));
				strncpy(tempName, command2, strlen(command2)-2); 
				
				// Append the pid to the temporary name
				sprintf(command2, "%i", pid);
				strcat(tempName, command2);

				// Copy the temporary name to the next command
				strcpy(command2, tempName);
				free(tempName);
			}

			// Add the command to the command array
			commandArray[i] = command2;
			i = i + 1;
		}
		
		token = strtok_r(NULL, " ", &saveptr);
	}
    
	// Call the function to run the command
	exitStatus = runCommand(commandArray, inputFile, outputFile, runBG);
	
	// Free dynamically allocated memory
	free(inputFile);
	free(outputFile);
	free(command);
	free(command2);

	return exitStatus;
}

/*******************************************************************************
  main function
  The following program prompts for user input.  Runs the commands exit, cd, and
  status. Ignores blank lines and comments, and calls the parseInput function
  to execute other commands.  Initializes the custom handlers for SIGINT and 
  SIGTSTP.
*******************************************************************************/
int main(){
	int continueShell = 1;	// Boolean value to continue shell
	char input[2048];		// User input
	char* changeDir;		// Used when "cd" is entered
	char* dirName;			// Directory name when "cd" is entered
	char* saveptr;			// Used for strtok_r
	char* echoText;			// Text to print to screen when "echo" is entered
	int exitStatus = 0;

	// Initialize SIGINT handling struct
	// Ignore ^C in main/parent
	// ** Modified from Module 5 Exploration, Example: Custom Handlers for 
	//    SIGINT, SIGUSR2, and Ignoring SIGTERM, etc. **
	struct sigaction SIGINT_action = {0};
	SIGINT_action.sa_handler = SIG_IGN;			// Ignore ^C in parent
	sigfillset(&SIGINT_action.sa_mask);
	SIGINT_action.sa_flags = 0;
	sigaction(SIGINT, &SIGINT_action, NULL);


	// Initialize SIGTSTP handling struct
	// ** Modified from Module 5 Exploration, Example: Custom Handlers for 
	//    SIGINT, SIGUSR2, and Ignoring SIGTERM, etc. **
	struct sigaction SIGTSTP_action = {0};
	SIGTSTP_action.sa_handler = handle_SIGTSTP;
	sigfillset(&SIGTSTP_action.sa_mask);
	SIGTSTP_action.sa_flags = 0;
	sigaction(SIGTSTP, &SIGTSTP_action, NULL);

	fflush(stdout);
	while (continueShell)
	{
		// Get user input
		printf(": ");
		fflush(stdout);
		fgets(input, 2048, stdin);

		// Exit the program
		if (strncmp(input, "exit", 4) == 0)
		{
			// Kill background process before exiting
			if (childBGPid != 0)
			{
				kill(childFGPid, 9);
			}
			continueShell = 0;
		}

		// Change directory
		else if (strncmp(input, "cd", 2) == 0 && !controlCZUsed)
		{
			// "cd"
			char* token = strtok_r(input, " ", &saveptr);
			changeDir = calloc(strlen(token) + 1, sizeof(char));
			strcpy(changeDir, token);

			// Get the directory name
			token = strtok_r(NULL, " ", &saveptr);
			if (token)
			{
				dirName = calloc(strlen(token), sizeof(char));
				strncpy(dirName, token, strlen(token)-1);

				// Replace "$$" with pid
				if (strstr(dirName, "$$"))
				{
					// Get pid
					int pid = getpid();

					// Create a temporary name without $$
					char* tempName = calloc(strlen(dirName)-2, sizeof(char));
					strncpy(tempName, dirName, strlen(dirName)-2); 
					
					// Append the pid to the temporary name
					sprintf(dirName, "%i", pid);
					strcat(tempName, dirName);

					// Copy the temporary name to the directory name
					strcpy(dirName, tempName);
					free(tempName);
				}

				// Error in changing directory
				if (chdir(dirName) != 0)
				{
					perror(dirName);
					memset(dirName, '\0', sizeof(dirName));
					fflush(stdout);
				}
			}

			// Go to the home directory if no directory name is specified
			else
			{
				chdir(getenv("HOME"));
			}	
		}

		// Print status
		else if (strncmp(input, "status", 6) == 0 && !controlCZUsed)
		{
			if (exitStatus == 0 || exitStatus == 1)
			{
				printf("exit value %i\n", exitStatus);
			}
			else
			{
				printf("terminated by signal %i\n", exitStatus);
			}
			
			fflush(stdout);
		}

		// Ignore comments and blank lines
		else if ((strncmp(input, "#", 1) == 0) || (strncmp(input, "\n", 1) == 0))
		{
			continue;
		}

		// Print text after echo
		else if (strncmp(input, "echo", 4) == 0 && !controlCZUsed)
		{
			// Point to the character in the string after "echo"
			echoText = input + 4;
			printf("%s", echoText);
			fflush(stdout);
		}

		// Send input to be parsed
		else 
		{
			if (!controlCZUsed)
			{
				exitStatus = parseInput(input);
			}	
		}
		controlCZUsed = 0;			// Reset boolean value for ^C or ^Z used
	}
}
