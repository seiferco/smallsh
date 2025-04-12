#ifndef SMALLSH_H  
#define SMALLSH_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

struct Command {
    char* command;
    int numArgs;
    char** args;
    bool foreground;
    char* string_copy;
    char* input_file;
    char* output_file;
};

struct Shell {
    size_t* bg_pids;
    int num_bg_pids;
    int bg_capacity;
    int exit_status;
};

// Function prototypes
char* promptCommand(char* user_input);
int checkComment(char* tokenized_input);
int checkEmptyCommand(char* tokenized_input);
void checkForExpansion(char* tokenized_input, pid_t smallsh_pid);
int checkBuiltIn(char* user_input, struct Shell* parent_process);
void readCommand(char* tokenized_input, struct Command* exec_command);
void stdinDirect(struct Command* exec_command, struct Shell* parent_process);
void stdoutDirect(struct Command* exec_command, struct Shell* parent_process);
void catch_sigint(int signal_number);
void catch_sigtstp(int signal_number);
void forkNewProcess(char* tokenized_input, struct Command* exec_command, struct Shell* parent_process);


#endif