/**
 * Shelldone
 *
 * Copyright (c) 2011, Ziirish <mr.ziirish@gmail.com>
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by Ziirish.
 * 4. Neither the name of the author nor the names of its contributors 
 *    may be used to endorse or promote products derived from this software 
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY Ziirish ''AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL Ziirish BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */
#ifndef _COMMAND_H_
#define _COMMAND_H_

/* Structure that represents a command */
typedef struct _command_line command;

/** 
 * Structure that represents a command-line
 * A command-line can contains multiple command linked by operators or not
 * Thus the data-structure used for the command-line is a double liked-list of 
 * commands (cf. previous struct)
 */
typedef struct _line input_line;

/* Different flags that describe in which context the command should be run */
typedef enum {
    /* the command is followed by a '|' */
    PIPE = 1,
    /* the command is followed by a '&' */
    BG,
    /* the command is followed by a '||' */
    OR,
    /* the command is followed by a '&&' */
    AND,
    /* the command is followed by ';' or nothing (default flag) */
    END
} CmdFlag;

struct _command_line {
    /* command */
    char *cmd;
    /* arguments */
    char **argv;
    /* nb arguments */
    int argc;
    /* cmd flag */
    CmdFlag flag;
    /* stdout */
    int out;
    /* stdin */
    int in;
    /* stderr */
    int err;
    /* file descriptor flag */
    int oflag;
    /* is it a builtin command */
    unsigned int builtin;
    /* next cmd */
    command *next;
    /* prev cmd */
    command *prev;
};

/* a double linked list */
struct _line {
    /* head command */
    command *head;
    /* tail command */
    command *tail;
    /* nb commands */
    int nb;
};

/* Allocate memory for a command structure */
command *new_cmd (void);

/** 
 * Free memory used by the given command
 * @param ptr Command that must be free'ed
 */
void free_cmd (command *ptr);

/**
 * Free memory used by the given line
 * @param ptr Line that must be free'ed
 */
void free_line (input_line *ptr);

/**
 * A debug function used to display the content of a command
 * @param ptr Command to display
 */
void dump_cmd (command *ptr);

/**
 * A debug function used to display the content of a command-line
 * @param ptr Command-line to display
 */
void dump_line (input_line *ptr);

/**
 * Parse the given string into an input_line structure
 * @param line Input string to parse
 * @return An input_line structure corresponding to the given string, or NULL
 */
input_line *parse_line (const char *line);

/**
 * Read a command-line on the standard-input using the given prompt
 * A line is terminated by the '\n' character unless the lines ends with an \
 * @param prompt Prompt to display
 * @return An allocated string corresponding to the user's input, or NULL
 */
char *read_line (const char *prompt);

/**
 * Parse the given command to separate the builtins from the rest and replace
 * the wildcards/variables/etc in order to execute in a subprocess for the
 * non-builtin commands.
 * @param ptr The command to parse
 */
void parse_command (command *ptr);

/**
 * Execute the given input_line evaluating the command returns to set the
 * apropriate viariables
 * @param ptr Input-line to run
 */
void run_line (input_line *ptr);

#endif
