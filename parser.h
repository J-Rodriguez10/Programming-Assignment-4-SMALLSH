#ifndef PARSER_H
#define PARSER_H

#define INPUT_LENGTH 2048
#define MAX_ARGS 512

#include <stdbool.h>

struct command_line {
    char *argv[MAX_ARGS + 1]; // Array of command arguments, (with an extra cell for the NULL terminator)
    int argc; // Arguments count from the argv array
    char *input_file; // Name of the file for input redirection ("< input.txt").
    char *output_file; // Name of the file for output redirection ("> output.txt").
    bool is_bg; // Flag indicating if the command should run in the background (true if '&' is present).
};

// Function Declaration
struct command_line *parse_input();

#endif