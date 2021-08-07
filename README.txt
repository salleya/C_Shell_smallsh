# Small Shell

Description: C shell, smallsh
This program provides a prompt for running commands, handles blank lines and comments (lines beginning with the # character), 
provides expansion for the variable $$, executes exit, cd, and status commands via code built into the shell, executes other 
commands by creating new processes using a function from the exec family of functions, supports input and output redirection, 
supports running commands in foreground and background processes, and implements custom handlers for SIGINT and SIGTSTP.

To compile the program:
gcc --std=gnu99 -o smallsh main.c
To run the program:
./smallsh
