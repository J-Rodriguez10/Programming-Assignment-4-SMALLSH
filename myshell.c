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
int last_foreground_status = 0; // Last foreground command exit/signal status
int bg_pid_count = 0; // Number of active background processes
struct command_line *curr_command; // Holds parsed command data

// Background process tracking
#define MAX_BG_PROCESSES 100
pid_t bg_pids[MAX_BG_PROCESSES];

// Foreground-only mode toggle
volatile sig_atomic_t foreground_only_mode = 0;

/**
 * Checks for completed background processes and reports their status.
 */
void check_background_processes() {
    int status;
    pid_t finished_pid;

    for (int i = 0; i < bg_pid_count; i++) {
        finished_pid = waitpid(bg_pids[i], &status, WNOHANG);

        if (finished_pid > 0) {
            if (WIFEXITED(status)) {
                printf("Background process %d terminated with exit status %d.\n", finished_pid, WEXITSTATUS(status));
            } else if (WIFSIGNALED(status)) {
                printf("Background process %d terminated by signal %d.\n", finished_pid, WTERMSIG(status));
            }
            fflush(stdout);

            // Remove terminated process from array
            for (int j = i; j < bg_pid_count - 1; j++) {
                bg_pids[j] = bg_pids[j + 1];
            }
            bg_pid_count--;
            i--;
        }
    }
}

/**
 * Handles Ctrl+Z (SIGTSTP): Toggles foreground-only mode.
 */
void handle_sigtstp(int signo) {
    if (foreground_only_mode == 0) {
        foreground_only_mode = 1;
        write(STDOUT_FILENO, "\nEntering foreground-only mode (& is now ignored)\n: ", 50);
    } else {
        foreground_only_mode = 0;
        write(STDOUT_FILENO, "\nExiting foreground-only mode\n: ", 30);
    }
    fflush(stdout);
}

/**
 * Handles Ctrl+C (SIGINT): Prints prompt if no foreground process.
 */
void handle_sigint(int signo) {
    if (curr_command == NULL) {
        write(STDOUT_FILENO, "\n", 1); 
        write(STDOUT_FILENO, ": ", 2);
        fflush(stdout);
    }
}

/**
 * Main loop: Parses and executes commands, handles signals and background processes.
 */
int main() {
    struct sigaction sa_int, sa_tstp;

    // Setup SIGINT handler
    sa_int.sa_handler = handle_sigint;
    sigfillset(&sa_int.sa_mask);
    sa_int.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sa_int, NULL);

    // Setup SIGTSTP handler
    sa_tstp.sa_handler = handle_sigtstp;
    sigfillset(&sa_tstp.sa_mask);
    sa_tstp.sa_flags = SA_RESTART;
    sigaction(SIGTSTP, &sa_tstp, NULL);

    while (1) {
        curr_command = NULL;

        check_background_processes(); // Check for finished background jobs

        curr_command = parse_input(); // Get new command

        // Skip empty or comment lines
        if (curr_command == NULL || (curr_command->argc == 0 || curr_command->argv[0][0] == '#')) {
            free(curr_command);
            continue;
        }

        // Enforce foreground mode if set
        if (foreground_only_mode && curr_command->is_bg) {
            curr_command->is_bg = 0;
        }

        if (is_builtin(curr_command)) {
            handle_builtin(curr_command);
        } else {
            handle_external(curr_command);
        }

        free(curr_command);
    }

    return EXIT_SUCCESS;
}

/**
 * Executes non-builtin commands, handles redirection and background execution.
 */
void handle_external(struct command_line *cmd) {
    pid_t pid = fork();

    if (pid == -1) {
        perror("fork");
        exit(1);

    } else if (pid == 0) { // Child process
        if (cmd->is_bg) {
            signal(SIGINT, SIG_IGN); // Ignore SIGINT in background
        } else {
            signal(SIGINT, SIG_DFL); // Terminate on SIGINT in foreground
        }

        // Input redirection
        if (cmd->input_file) {
            int fd = open(cmd->input_file, O_RDONLY);
            if (fd == -1) {
                perror("open");
                exit(1);
            }
            dup2(fd, STDIN_FILENO);
            close(fd);
        } else if (cmd->is_bg) {
            int dev_null = open("/dev/null", O_RDONLY);
            if (dev_null == -1) {
                perror("open");
                exit(1);
            }
            dup2(dev_null, STDIN_FILENO);
            close(dev_null);
        }

        // Output redirection
        if (cmd->output_file) {
            int fd = open(cmd->output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd == -1) {
                perror("open");
                exit(1);
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
        } else if (cmd->is_bg) {
            int dev_null = open("/dev/null", O_WRONLY);
            if (dev_null == -1) {
                perror("open");
                exit(1);
            }
            dup2(dev_null, STDOUT_FILENO);
            close(dev_null);
        }

        // Run the command
        execvp(cmd->argv[0], cmd->argv);
        perror("execvp");
        exit(1);

    } else { // Parent process
        if (cmd->is_bg) {
            if (bg_pid_count < MAX_BG_PROCESSES) {
                bg_pids[bg_pid_count++] = pid;
            }
            printf("background pid %d\n", pid);
            fflush(stdout);
        } else {
            int status;
            pid_t child_pid = waitpid(pid, &status, 0);

            if (child_pid > 0) {
                if (WIFEXITED(status)) {
                    last_foreground_status = WEXITSTATUS(status);
                } else if (WIFSIGNALED(status)) {
                    last_foreground_status = WTERMSIG(status);
                    char msg[64];
                    int len = snprintf(msg, sizeof(msg), "\nterminated by signal %d\n", last_foreground_status);
                    write(STDOUT_FILENO, msg, len);
                }
            }
        }
    }
}

/**
 * Returns 1 if the command is a built-in ("exit", "cd", "status").
 */
int is_builtin(struct command_line *cmd) {
    if (cmd->argc == 0) {
        return 0;
    }

    return (strcmp(cmd->argv[0], "exit") == 0 ||
            strcmp(cmd->argv[0], "cd") == 0 ||
            strcmp(cmd->argv[0], "status") == 0);
}

/**
 * Executes built-in commands: "exit", "cd", "status".
 */
void handle_builtin(struct command_line *cmd) {
    if (cmd->argc == 0) return;

    if (strcmp(cmd->argv[0], "exit") == 0) {
        for (int i = 0; i < bg_pid_count; i++) {
            kill(bg_pids[i], SIGTERM); // Terminate background processes
        }
        exit(0);
    }

    if (strcmp(cmd->argv[0], "cd") == 0) {
        char *target = cmd->argv[1];
        if (target == NULL) {
            target = getenv("HOME");
        }
        if (chdir(target) != 0) {
            perror("cd");
        }
        return;
    }

    if (strcmp(cmd->argv[0], "status") == 0) {
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
