#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "smallsh.h"

// GLOBAL
bool foreground_mode = false;
bool is_child = false;

// Function used to prompt user for command (:)
char* promptCommand(char* user_input) {
	printf(": ");
	fflush(stdout);
	fgets(user_input, 2049, stdin);
    return user_input;
}

// Function to check if the command is a comment "#"
int checkComment(char* tokenized_input) {
	if (tokenized_input[0] == '#') {
		return 1;
	}
    return 0;
}

// Function to check command string is empty
int checkEmptyCommand(char* tokenized_input) {
    if (tokenized_input == NULL || strcmp(tokenized_input, "") == 0) {
		return 1;
	}
    return 0;
}

// Function to expand any $$ variables in a string
void checkForExpansion(char* tokenized_input, pid_t smallsh_pid) {

    // Convert smallsh PID to a string
	char pid_str[20];
    snprintf(pid_str, sizeof(pid_str), "%d", smallsh_pid); 
    size_t pid_len = strlen(pid_str);

    // Make a copy of entered command to manipulate
	char string_copy[strlen(tokenized_input) + 1]; 
	strcpy(string_copy, tokenized_input);

    // return if there are no expansion to be done
    if (strstr(string_copy, "$$") == NULL) {
        return;
    }
    
	// New command string with proper size to fit expansions (assume no commands are larger than 2048 in instructions)
	char* new_string = (char*) malloc(sizeof(char) * 2048);

    // While there are still $$ chars in string
    while (strstr(string_copy, "$$")) { // cd ./test$$123$$
        int i;
        new_string[0] = '\0'; // reset 
        for (i = 0; i < strlen(string_copy); i++) {
            if (string_copy[i] == '$' && string_copy[i + 1] == '$') {

                // Terminate first found $ sign
                char* first_dollar = strchr(string_copy, '$');
                *first_dollar = '\0';

                char* left_string = string_copy;
                // printf("left_string: %s\n", left_string);

                // Find the second '$'
                char* second_dollar = strchr(first_dollar + 1, '$');

                 // right_string starts after the second '$'
                char* right_string = second_dollar + 1;

                // Build new string
                strcat(new_string, left_string); 
                strcat(new_string, pid_str);
                strcat(new_string, right_string);
                strcpy(string_copy, new_string);
                break;
            }
        }
    }

    strcpy(tokenized_input, new_string);
    free(new_string);
    return;
}

// Function to check if command is ("status", "exit", "cd")
int checkBuiltIn(char* user_input, struct Shell* parent_process) {

    char* tokenized_input = strtok(user_input, "\n");

    // Check if "status"
	if ( (strcmp(tokenized_input, "status\0") == 0) || strcmp(tokenized_input, "status &\0") == 0) {
		printf("exit status: %d\n", parent_process->exit_status);
        return 1;
	}

    // Check if "exit"
    if (strcmp(user_input, "exit\0") == 0 || strcmp(user_input, "exit &\0") == 0) {
        return -1;
    }
    
    // Check if "cd"
	if (tokenized_input[0] == 'c' && tokenized_input[1] == 'd') {

		// tokenize argument given for cd command
		char* cd_arg = strtok(tokenized_input, " ");
		cd_arg = strtok(NULL, "\0");

		// If no arg given, set current working directory as HOME environment variable 
		if (cd_arg == NULL || cd_arg == " ") {
			char* home_env = getenv("HOME");
			chdir(home_env);
			return 1;
		}

		// Change current working directory to argument user entered
		chdir(cd_arg);
        return 1;
	}

    return 0;
}

// Function used to read the command line string entered by the user
void readCommand(char* tokenized_input, struct Command* exec_command) {
    size_t tokenized_input_len = strlen(tokenized_input);

    // check if background process ("&")
    if (tokenized_input[tokenized_input_len - 1] == '&') {
        if (foreground_mode == true) {
            exec_command->foreground = true;
            tokenized_input[tokenized_input_len - 1] = '\0';
        } else {
            exec_command->foreground = false;
            tokenized_input[tokenized_input_len - 1] = '\0';
        }
    } else {
        exec_command->foreground = true;
    }

    // Make sure args is empty
    if (exec_command->args) {
        free(exec_command->args);
    }

    // Make sure input and output file is NULL
    exec_command->input_file = NULL;
    exec_command->output_file = NULL;

    exec_command->args = (char**) malloc(sizeof(char*) * 513);
    exec_command->numArgs = 0;

     // Make a copy of entered command to manipulate
     char* string_copy = strdup(tokenized_input);
     char* token = strtok(string_copy, " ");

    // First token is the command
    exec_command->command = token;
    exec_command->args[exec_command->numArgs++] = token;

    // Go through command and save arguments (separated by spaces)
    while ((token = strtok(NULL, " ")) != NULL) {
        if (strcmp(token, "<") == 0) {
            exec_command->input_file = strtok(NULL, " ");
        } else if (strcmp(token, ">") == 0) {
            exec_command->output_file = strtok(NULL, " ");
        } else {
            exec_command->args[exec_command->numArgs++] = token;
        }
    }
    exec_command->args[exec_command->numArgs] = NULL; // NULL-terminate argument list

    if (exec_command->foreground == false) {
        exec_command->args[exec_command->numArgs] = NULL;
        exec_command->numArgs--;
    }

    exec_command->args[exec_command->numArgs + 1] = NULL;
    exec_command->string_copy = string_copy;
}

// Function used to handle standard input redirection
void stdinDirect(struct Command* exec_command, struct Shell* parent_process) {
    // Background
    if (!exec_command->foreground) {
        if (exec_command->input_file != NULL) {
            // Background w/ input file (direct to input_file)
            int fd = open(exec_command->input_file, O_RDONLY, 0644);
            if (fd == -1) {
                parent_process->exit_status = 1;
                exit(1);
            }
            dup2(fd, STDIN_FILENO); // redirect stdin to input_file
            close(fd);
        } else {
            // Background no input file (direct to /dev/null)
            int fd = open("/dev/null", O_RDONLY);
            dup2(fd, STDIN_FILENO); // redirect stdin to /dev/null
            close(fd);
        }
    } else {
        // Foreground
        if (exec_command->input_file != NULL) {
            // Foreground w/ input file
            int fd = open(exec_command->input_file, O_RDONLY, 0644);
            if (fd == -1) {
                printf("%s: no such file or directory\n", exec_command->input_file);
                parent_process->exit_status = 1;
                close(fd);
                exit(1);
            }
            dup2(fd, STDIN_FILENO); // Redirect stdin to input_file
            close(fd);

        } else {
            // Foreground no input file
            return;
        }
    }
}

// Function used to handle standard output redirection
void stdoutDirect(struct Command* exec_command, struct Shell* parent_process) {
    // Background
    if (!exec_command->foreground) {
        if (exec_command->output_file != NULL) {
            // Background w/ output file (direct to output_file)
            int fd = open(exec_command->output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd == -1) {
                parent_process->exit_status = 1;
                exit(1);
            }
            dup2(fd, STDOUT_FILENO); // redirect stdout to output_file
            close(fd);
        } else {
            // Background no output file (direct to /dev/null)
            int fd = open("/dev/null", O_WRONLY | O_TRUNC);
            dup2(fd, STDOUT_FILENO); // redirect stdout to /dev/null
            close(fd);
        }
    } else {
        // Foreground
        if (exec_command->output_file != NULL) {
            // Foreground w/ output file
            int fd = open(exec_command->output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd == -1) {
                parent_process->exit_status = 1;
                close(fd);
                exit(1);
            }
            dup2(fd, STDOUT_FILENO); // redirect stdout to output_file
            close(fd);

        } else {
            // Foreground no output file
            return;
        }
    }
}

void catch_sigint(int signal_number) {
    
}

void catch_sigtstp(int signal_number) {
    if (is_child == true) {
        printf("Child");
        return;
    }
    if (foreground_mode == true) {
        // Deactivate forground mode
        foreground_mode = false;
        write(STDOUT_FILENO, "\nExiting foreground-only mode\n", 30);
        write(STDOUT_FILENO, ": ", 2);

    } else {
        // Activate foreground mode
        foreground_mode = true;
        write(STDOUT_FILENO, "\nEntering foreground-only mode (& is now ignored)\n", 51);
        write(STDOUT_FILENO, ": ", 2);
    }
}

// Function that forks over a new child process
void forkNewProcess(char* tokenized_inptut, struct Command* exec_command, struct Shell* parent_process) { 
    readCommand(tokenized_inptut, exec_command);

    // create/fork new child process
	pid_t fork_result = fork();
    
    if (fork_result == 0) {
        // Child process
        if (exec_command->foreground == true) {
            // Set SIGINT to default behavior so it can terminate child foreground processes
            struct sigaction sa_sigint = {0};
            sigfillset(&sa_sigint.sa_mask); // Block all signals while handling SIGINT
            sa_sigint.sa_handler = SIG_DFL; // Ignore SIGTSTP
            sigaction(SIGINT, &sa_sigint, NULL);
        }

        is_child = true;
        //Ignore SIGTSTP for both foreground and background child processes
        struct sigaction sa_sigstp = {0};
        sigfillset(&sa_sigstp.sa_mask); // Block all signals while handling SIGTSTP
        // sa_sigstp.sa_handler = SIG_IGN; // Ignore SIGTSTP
        sigaction(SIGTSTP, &sa_sigstp, NULL);

        // Redirect standard input and output if needed
        stdinDirect(exec_command, parent_process);
        stdoutDirect(exec_command, parent_process);

        // execute command entered by user
        execvp(exec_command->command, exec_command->args);
        printf("%s: command not found\n", exec_command->command); // Code should not get here unless exec fails
        parent_process->exit_status = 1;
        exit(1);

    } else {
        // Parent process
        if (exec_command->foreground == true) {
            // Wait for child process to complete before continuing with waitpid(...,0)
            if (strcmp(exec_command->command, "kill") == 0) {
                is_child = false;
            } else {
                is_child = true;
            }
            int exit_status;
            waitpid(fork_result, &exit_status, 0);
            is_child = false;
            // Save exit status
            if (WIFSIGNALED(exit_status)) {
                // Child was terminated by a signal
                int signal_number = WTERMSIG(exit_status);
                printf("terminated by signal: %d\n", signal_number);
                parent_process->exit_status = signal_number;
            } else {
                parent_process->exit_status = WEXITSTATUS(exit_status);
            }

        } else {
            // Run child in background (i.e parent does not wait on child process)
            printf("Background pid is %d\n", fork_result);
            // Track background child process PID for clean up
            if (parent_process->num_bg_pids == parent_process->bg_capacity) {
                parent_process->bg_capacity = (parent_process->bg_capacity == 0) ? 1 : parent_process->bg_capacity * 2;
                parent_process->bg_pids = realloc(parent_process->bg_pids, parent_process->bg_capacity * sizeof(size_t));
                if (!parent_process->bg_pids) {
                    perror("Error resizing PIDS array\n");
                    exit(EXIT_FAILURE);
                }
            }
            parent_process->bg_pids[parent_process->num_bg_pids++] = fork_result;
        }
    }
}

int main() {
    pid_t smallsh_pid = getpid(); // Save PID of smallsh parent process 
	char* user_input = (char*) malloc(sizeof(char) * 2049);
    struct Shell* parent_process = malloc(sizeof(struct Shell));
    parent_process->bg_pids = NULL;
    parent_process->num_bg_pids = 0;
    parent_process->bg_capacity = 0;
    parent_process->exit_status = 0;

    struct Command* exec_command = malloc(sizeof(struct Command));
    exec_command->args = (char**) malloc(sizeof(char*) * 513);
    exec_command->numArgs = 0;
    exec_command->string_copy = NULL;
    exec_command->input_file = NULL;
    exec_command->output_file = NULL;

    // smallsh ignores CTRL+C (aka SIGINT)
    struct sigaction sa_sigint = {0}; 
    sigfillset(&sa_sigint.sa_mask);
    sa_sigint.sa_handler = SIG_IGN; // Ignore SIGINT (CTRL+C)
    sigaction(SIGINT, &sa_sigint, NULL);

    // Ignore SIGTSTP (CTRL+Z) in the parent process
    struct sigaction sa_sigstp = {0};
    sigfillset(&sa_sigstp.sa_mask);
    sa_sigstp.sa_handler = catch_sigtstp;
    sa_sigstp.sa_flags = SA_RESTART;
    sigaction(SIGTSTP, &sa_sigstp, NULL);
    

    // Check background child PIDs for termination with waitpid(WNOHANG) in a loop
    while(1) {
        pid_t wait_result;
        int exit_status;
        int p;
        for (p = 0; p < parent_process->num_bg_pids; p++) {
            if (parent_process->bg_pids[p] != 0) {
                wait_result = waitpid(parent_process->bg_pids[p], &exit_status, WNOHANG);
                if (wait_result > 0) {
                    printf("background pid %d is done: exit value %d\n", parent_process->bg_pids[p], exit_status / 256);
                    
                    // Remove finished process from array
                    int j;
                    for (j = p; j < parent_process->num_bg_pids - 1; j++) {
                        parent_process->bg_pids[j] = parent_process->bg_pids[j + 1];
                    }
                    parent_process->num_bg_pids--;
                    p--;
                }
            }
        }

        // Get user input
        user_input = promptCommand(user_input);

        // Tokenize input string
        char* tokenized_input = strtok(user_input, "\n");

        // Check for empty command
        if (checkEmptyCommand(tokenized_input) == 1) {
            continue;
        }

        // Check for comment
        if (checkComment(tokenized_input) == 1) {
            continue;
        }

        // Check command for $$ expansion
        checkForExpansion(tokenized_input, smallsh_pid);

        int ranBuiltIn = checkBuiltIn(user_input, parent_process);

        // exit (-1 means user input "exit")
        if (ranBuiltIn == -1) { 
            break;
        }

        // built-in command was run, else fork() new process
        if (ranBuiltIn == 1) {
            continue;
        } else {
			forkNewProcess(tokenized_input, exec_command, parent_process);
        }
    }

    // Free up any memory used
    if (exec_command->args != NULL) {
        free(exec_command->args);
    }
    if (exec_command->string_copy) {
        free(exec_command->string_copy);
    }
    if (exec_command != NULL) {
        free(exec_command);
    }
    if (user_input != NULL) {
        free(user_input);
    }
    if (parent_process->bg_pids != NULL) {
        free(parent_process->bg_pids);
    }
    if (parent_process != NULL) {
        free(parent_process);
    }
    
    return 0;
}