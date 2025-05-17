#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Parses a line of user input into a command_line struct.
 * Handles arguments, input/output redirection, and background execution.
 *
 * Returns: pointer to a dynamically allocated command_line structure.
 */
struct command_line *parse_input() {
    char input[INPUT_LENGTH];
    struct command_line *curr_command = (struct command_line *) calloc(1, sizeof(struct command_line));

    // Prompt for input
    printf(": ");
    fflush(stdout);
    fgets(input, INPUT_LENGTH, stdin);

    // Tokenize input string by space and newline
    char *token = strtok(input, " \n");
    while (token) {
        if (!strcmp(token, "<")) {
            // Input redirection
            curr_command->input_file = strdup(strtok(NULL, " \n"));
        } else if (!strcmp(token, ">")) {
            // Output redirection
            curr_command->output_file = strdup(strtok(NULL, " \n"));
        } else if (!strcmp(token, "&")) {
            // Background execution flag
            curr_command->is_bg = true;
        } else {
            // Regular argument
            curr_command->argv[curr_command->argc++] = strdup(token);
        }
        token = strtok(NULL, " \n");
    }

    return curr_command;
}
