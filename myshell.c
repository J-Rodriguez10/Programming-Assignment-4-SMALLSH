#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h> 
#include <signal.h>

#include "parser.h"

// Function Declarations:
void handle_builtin(struct command_line *cmd);
void handle_external(struct command_line *cmd);
int is_builtin(struct command_line *cmd);
void check_background_processes();

// Global Variables:
int last_foreground_status = 0; // Keeps track of the last foreground status
int bg_pid_count = 0;  // Keeps track of the number of background processes
struct command_line *curr_command; // Structure that holds various info on user's command

// Constraints:
#define MAX_BG_PROCESSES 100  // Max number of background processes to track
pid_t bg_pids[MAX_BG_PROCESSES];  // Array to store background process PIDs

// Global flag to track foreground-only mode
volatile sig_atomic_t foreground_only_mode = 0; 

// Checks and prints for any 
void check_background_processes() {
    int status;
    pid_t finished_pid;

    // Check each stored background process
    for (int i = 0; i < bg_pid_count; i++) {

        // Non-blocking check
        finished_pid = waitpid(bg_pids[i], &status, WNOHANG); 

        // Process has terminated
        if (finished_pid > 0) {  
            if (WIFEXITED(status)) {
                printf("Background process %d terminated with exit status %d.\n", finished_pid, WEXITSTATUS(status));
            } else if (WIFSIGNALED(status)) {
                printf("Background process %d terminated by signal %d.\n", finished_pid, WTERMSIG(status));
            }
            // Prints immediately
            fflush(stdout); 

            // Remove the finished process from the array
            for (int j = i; j < bg_pid_count - 1; j++) {
                bg_pids[j] = bg_pids[j + 1];
            }
            bg_pid_count--;  
            i--;  
        }
    }
}

// SIGTSTP Handler for Ctrl+Z (Foreground mode)
void handle_sigtstp(int signo) {
    if (foreground_only_mode == 0) {
        // Switch to foreground-only mode
        foreground_only_mode = 1;
        write(STDOUT_FILENO, "\nEntering foreground-only mode (& is now ignored)\n: ", 50);
    } else {
        // Exit foreground-only mode
        foreground_only_mode = 0;
        write(STDOUT_FILENO, "\nExiting foreground-only mode\n: ", 30);
    }
    fflush(stdout); 
}

// SIGINT Handler for Ctrl+C (Interrupt)
void handle_sigint(int signo) {
    // Only print a newline if no foreground process is running
    if (curr_command == NULL) {
        write(STDOUT_FILENO, "\n", 1); 
        write(STDOUT_FILENO, ": ", 2);  
        fflush(stdout);  
    }
}

// Main loop
int main() {
    struct sigaction sa_int, sa_tstp;

    // SIGINT (Ctrl+C) handler
    sa_int.sa_handler = handle_sigint;
    sigfillset(&sa_int.sa_mask); // Block all signals while in handler
    sa_int.sa_flags = SA_RESTART; // Restart interrupted syscalls
    sigaction(SIGINT, &sa_int, NULL);

    // SIGTSTP (Ctrl+Z) handler
    sa_tstp.sa_handler = handle_sigtstp;
    sigfillset(&sa_tstp.sa_mask);
    sa_tstp.sa_flags = SA_RESTART;
    sigaction(SIGTSTP, &sa_tstp, NULL);

    while (1) {  // Infinite shell loop

        // Reset curr_command to NULL to signify no active command
        curr_command = NULL;

        // Check for finished background processes
        check_background_processes();

        // Parse new input
        curr_command = parse_input();

        
        // If the command is NULL (blank line or comment), just continue the loop
        if (curr_command == NULL || (curr_command->argc == 0 || curr_command->argv[0][0] == '#')) {
            free(curr_command);  // Free memory allocated by parse_input (if any)
            continue;  // Skip to the next iteration
        }


        // If foreground-only mode is enabled, override background execution
        if (foreground_only_mode && curr_command->is_bg) {
            curr_command->is_bg = 0;  // Force foreground execution
        }

        // If the command is a built-in, handle it and skip external execution
        if (is_builtin(curr_command)) {
            handle_builtin(curr_command);
        } else {
            handle_external(curr_command);
        }

        // Free allocated memory for curr_command after handling
        free(curr_command);
    }

    return EXIT_SUCCESS;
}

void handle_external(struct command_line *cmd) {

    pid_t pid = fork();

    // FAILED FORK:
    if (pid == -1) {
        perror("fork");
        exit(1);

    // CHILD PROCESS:
    } else if (pid == 0) { 
        if (cmd->is_bg) {
            // Background processes ignores SIGINT
            signal(SIGINT, SIG_IGN); 
        } else {
            // Foreground processes terminates on SIGINT
            signal(SIGINT, SIG_DFL);
        }

        // Handle input redirection
        if (cmd->input_file) {
            int fd = open(cmd->input_file, O_RDONLY);
            if (fd == -1) {
                perror("open");
                exit(1);
            }
            dup2(fd, STDIN_FILENO);
            close(fd);
        } else if (cmd->is_bg) {
            // If background process and no input file, redirect stdin to /dev/null
            int dev_null = open("/dev/null", O_RDONLY);
            if (dev_null == -1) {
                perror("open");
                exit(1);
            }
            dup2(dev_null, STDIN_FILENO);
            close(dev_null);
        }

        // Handle output redirection
        if (cmd->output_file) {
            int fd = open(cmd->output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd == -1) {
                perror("open");
                exit(1);
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
        } else if (cmd->is_bg) {
            // If background process and no output file, redirect stdout to /dev/null
            int dev_null = open("/dev/null", O_WRONLY);
            if (dev_null == -1) {
                perror("open");
                exit(1);
            }
            dup2(dev_null, STDOUT_FILENO);
            close(dev_null);
        }

        // Execute the command after updating input/output redirection
        execvp(cmd->argv[0], cmd->argv);
        perror("execvp"); // If execvp fails, print error
        exit(1);

    // PARENT PROCESS:
    } else {

        if (cmd->is_bg) {  
            // Store the background process PID
            // No waitpid(), continue on
            if (bg_pid_count < MAX_BG_PROCESSES) {
                bg_pids[bg_pid_count++] = pid;
            }

            // Background process: Print PID
            printf("Background PID: %d\n", pid);
            fflush(stdout);
        } else {  
            // Foreground process: Wait for completion
            int status;
            pid_t child_pid = waitpid(pid, &status, 0);

            if (child_pid > 0) {
                if (WIFEXITED(status)) {
                    last_foreground_status = WEXITSTATUS(status);
                } else if (WIFSIGNALED(status)) {
                    last_foreground_status = WTERMSIG(status);

                    // Print the signal number if the foreground process is terminated by a signal
                    char msg[64];
                    int len = snprintf(msg, sizeof(msg), "\nterminated by signal %d\n", last_foreground_status);
                    write(STDOUT_FILENO, msg, len);
                }
            }
        }
    }
}

int is_builtin(struct command_line *cmd) {
    // No command entered
    if (cmd->argc == 0) {
        return 0;
    }

    return (strcmp(cmd->argv[0], "exit") == 0 ||
            strcmp(cmd->argv[0], "cd") == 0 ||
            strcmp(cmd->argv[0], "status") == 0);
}

void handle_builtin(struct command_line *cmd) { 
    if (cmd->argc == 0) {
        return; // No command entered
    }

    if (strcmp(cmd->argv[0], "exit") == 0) {
        // Kill any background processes before exiting
        for (int i = 0; i < bg_pid_count; i++) {
            kill(bg_pids[i], SIGTERM);  
        }
        // Exit the shell
        exit(0); 
    }

    // 2️⃣ Change directory command
    if (strcmp(cmd->argv[0], "cd") == 0) {
        // Get the directory argument
        char *target = cmd->argv[1]; 

        // Default to HOME (if no path is given)
        if (target == NULL) {
            target = getenv("HOME");
        }

        if (chdir(target) != 0) {
            perror("cd"); 
        }
        return;
    }

    // 3️⃣ Status command
    if (strcmp(cmd->argv[0], "status") == 0) {
        // Print the last foreground process status
        if (last_foreground_status == 0) {
            printf("exit value 0\n");
        } else if (WIFSIGNALED(last_foreground_status)) {
            printf("terminated by signal %d\n", last_foreground_status);
        } else {
            printf("exit value %d\n", last_foreground_status);
        }
        fflush(stdout); 
        return;
    }  
}