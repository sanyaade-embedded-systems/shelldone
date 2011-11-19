/**
 * Shelldone
 *
 * vim:ts=4:sw=4:expandtab
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
#ifndef _GNU_SOURCE
    #define _GNU_SOURCE
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

#include "xutils.h"
#include "parser.h"
#include "command.h"

/**
 * XXX: is there a better way to store these variables?
 */
static char *prompt = NULL;
static char *host = NULL;
static const char *fpwd = NULL;
static input_line *l = NULL;
static char *li = NULL;
unsigned int interrupted = FALSE;

static void shelldone_init (void);
static void shelldone_clean (void);
static void handler (int signal);
static const char *get_prompt (void);
static void shelldone_loop (void);

int ret_code;

/* Initialization function */
static void
shelldone_init (void)
{
    /* get the hostname */
    host = xmalloc (30);
    gethostname (host, 30);
    /* load the commands list */
    init_command_list ();
    /* load the history */
    init_history ();
    /* register the cleanup function */
    atexit (shelldone_clean);
    /* ignoring SIGINT */
    signal (SIGINT, handler);
}

/* Cleanup function */
static void
shelldone_clean (void)
{
    xfree (li);
    xfree (host);
    xfree (prompt);
    free_line (l);
    li = NULL;
    host = NULL;
    prompt = NULL;
    l = NULL;
    clear_command_list ();
    clear_history ();
}

/**
 * Signal handler for the signal 2 (SIGINT (^C))
 */
static void
handler (int sig)
{
    (void) sig;
    interrupted = TRUE;
    fprintf (stdout, "^C\n");
/*    signal (SIGINT, handler);*/
/*    shelldone_loop ();*/
/*    exit (0);*/
}

/**
 * Gets the prompt in a hard-coded pattern like:
 * user@hostname:pwd$
 * We can imagine make it configurable in the future
 * @return The prompt
 */
static const char *
get_prompt (void)
{
    const char *user = getenv ("USER");
    const char *full_pwd = getenv ("PWD");
    const char *home = getenv ("HOME");
    char *pwd = NULL;

    size_t s_home = xstrlen (home);
    size_t s_pwd = xstrlen (full_pwd);
    size_t full;

    if (fpwd != NULL && xstrcmp (full_pwd, fpwd) == 0)
    {
        return prompt;
    }

    if (strncmp (full_pwd, home, s_home) == 0)
    {
        size_t size = s_pwd - s_home;
        int cpt = 1;
        pwd = xmalloc (size + 2);
        *pwd = '~';
        while (cpt < (int) size + 2)
        {
            pwd[cpt] = full_pwd[s_home+cpt-1];
            cpt++;
        }
        pwd[cpt-1] = '\0';
    }
    else
        pwd = xstrdup (full_pwd);

    xfree (prompt);
    full = xstrlen (user) + xstrlen (host) + xstrlen (pwd) + 4;
    prompt = xmalloc (full + 1);
    snprintf (prompt, full + 1, "%s@%s:%s$ ", user, host, pwd);
    fpwd = full_pwd;

    xfree (pwd);
    return prompt;
}

static void
shelldone_loop (void)
{
    while (1)
    {
        interrupted = FALSE;
        li = NULL;
        l = NULL;
        const char *pt = get_prompt ();
        /* read the input line */
        li = read_line (pt);
        if (xstrcmp ("quit", li) == 0)
            break;
        if (xstrlen (li) > 0)
            insert_history (li);
        /* parsing the input line into a command-line structure */
        l = parse_line (li);
/*        dump_line (l); */
        /* execute the command-line */
        run_line (l);

        free_line (l);
        xfree (li);
    }
}

int
main (int argc, char **argv)
{
    /* initializing shelldone */
    shelldone_init ();

    /* infinite loop waiting for commands to launch */
    shelldone_loop ();

/* avoid the 'unused variables' warning */
    (void) argc;
    (void) argv;

/* we don't need to cleanup anything since we registered the cleanup function */
    return 0;
}
