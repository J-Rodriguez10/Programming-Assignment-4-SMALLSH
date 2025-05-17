#ifndef PARSER_H
#define PARSER_H

/**
 * parser.h
 * 
 * Defines the structure and function prototype used to parse shell command-line input.
 */

#define INPUT_LENGTH 2048  // Max length for a line of user input
#define MAX_ARGS 512       // Max number of arguments a command can accept

#include <stdbool.h>

/**
 * Represents a parsed command line with arguments, redirection, and background execution info.
 */
struct command_line {
    char *argv[MAX_ARGS + 1];  // Array of argument strings (NULL-terminated)
    int argc;                  // Number of arguments in argv
    char *input_file;          // Input redirection file (e.g., "< input.txt")
    char *output_file;         // Output redirection file (e.g., "> output.txt")
    bool is_bg;                // True if command ends with '&' (background execution)
};

// Parses a line of user input into a command_line structure
struct command_line *parse_input();

#endif
