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
#ifndef _BSD_SOURCE
    #define _BSD_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <ctype.h>
#include <errno.h>
#include <termios.h>

#include "command.h"
#include "parser.h"
#include "xutils.h"
#include "structs.h"
#include "caps.h"

static char *history[HISTORY];
static int last_history = 0, curr_history;
static char **command_list = NULL;
static int nb_commands = 0;
static struct termios in_save;

/* static functions */
static void init_ioctl (void);
static void clear_ioctl (void);
static char *completion (const char *prompt, char *buf, int ind);
static int cmpsort (const void *p1, const void *p2);
static int reg_filter (const struct dirent *p);
static void line_append (input_line **ptr, command *data);
static command *copy_cmd (const command *src);
static char get_char (const char input[5],
                      const char *prompt,
                      char **ret,
                      int *cpt,
                      int *n);

static int
cmpsort (const void *p1, const void *p2)
{
    return xstrcmp (* (char * const *) p1, * (char * const *) p2);
}

static int
reg_filter (const struct dirent *p)
{
    return S_ISREG(DTTOIF(p->d_type));
}

void
init_history (void)
{
    int i;
    for (i = 0; i < HISTORY; i++)
        history[i] = NULL;
}

void 
init_command_list (void)
{
    char *path = getenv ("PATH");
    size_t size;
    char **paths = xstrsplit (path, ":", &size);
    int i, j, k = 0, l, n = 1;
    char **tmp_command_list = xcalloc (n * 500, sizeof (char *));
    /* 
     * first of all, we store each executables files in each directories of the
     * PATH variable
     */
    for (i = 0; i < (int) size; i++)
    {
        struct dirent **files = NULL;
        int nbfiles = scandir (paths[i], &files, reg_filter, alphasort);
        for (j = 0; j < nbfiles; j++)
        {
            if (k >= n * 500)
            {
                n++;
                tmp_command_list = xrealloc (tmp_command_list, 
                                         n * 500 * sizeof (char *));
            }
            tmp_command_list[k] = xstrdup (files[j]->d_name);
            k++;
            xfree (files[j]);
        }
        xfree (files);
    }
    xfree_list (paths, size);
    /* then we sort the list */
    qsort (tmp_command_list, k, sizeof (char *), cmpsort);
    n = 1;
    command_list = xcalloc (n * 500, sizeof (char *));
    /* finally we remove each duplicates files */ 
    for (i = 0, j = 1, l = 0; j < k; i++)
    {
        if (i >= n * 500)
        {
            n++;
            command_list = xrealloc (command_list, n * 500 * sizeof (char *));
        }
        command_list[i] = xstrdup (tmp_command_list[l]);
        while (j <= k - 1 && 
               xstrcmp (command_list[i], tmp_command_list[j]) == 0)
            j++;
        l = (j <= k - 1) ? j : k - 1;
        j++;
    }
    nb_commands = i;
    xfree_list (tmp_command_list, k);
}

void
clear_history (void)
{
    int i;
    for (i = 0; i < HISTORY && history[i] != NULL; i++)
    {
/*        fprintf (stdout, "history[%d] = %s\n", i, history[i]); */
        xfree (history[i]);
    }
}

void
clear_command_list (void)
{
    xfree_list (command_list, nb_commands);
    nb_commands = 0;
    command_list = NULL;
}

/* Input initialization function */
static void
init_ioctl (void)
{
    struct termios term;

    if (ioctl (STDIN_FILENO, TCGETS, &term) != 0)
        fprintf (stderr, "ioctl G prob\n");
    in_save = term;
    term.c_lflag &= ~(ECHO | ICANON);
    term.c_cc[VMIN] = 1;
    term.c_cc[VTIME] = 0;
    if (ioctl (STDIN_FILENO, TCSETS, &term) != 0)
        fprintf (stderr, "ioctl S prob\n");
}

static void
clear_ioctl (void)
{
    if (ioctl (STDIN_FILENO, TCSETS, &in_save) != 0)
        fprintf (stderr, "ioctl S prob\n");
}

static char *
completion (const char *prompt, char *buf, int ind)
{
    /* in order to avoid multiple [TAB] hits we store the last index */
    static int save = 0;
    if (ind < 1 || save == ind)
        return NULL;
    save = ind;
    char *tmp = xmalloc (ind + 1), *ret = NULL;
    char **split, **list;
    size_t s_split, s_in;
    int i, j, n = 1;
    unsigned int found = FALSE;
    for (i = 0; i < ind; i++)
        tmp[i] = buf[i];
    tmp[i] = '\0';
    split = xstrsplit (tmp, " ", &s_split);
    if (s_split > 1)
    {
        xfree_list (split, s_split);
        xfree (tmp);
        return NULL;
    }

    list = xcalloc (n * 10, sizeof (char *));
    s_in = xstrlen (split[0]);
    for (i = 0, j = 0; i < nb_commands; i++)
    {
        int ok = strncmp (split[0], command_list[i], s_in);
        /* 
         * our list is sorted so once a command dismatch we are sure the rest
         * won't match
         */
        if (found && ok != 0)
            break;
        if (ok == 0)
        {
            if (j >= n * 10)
            {
                n++;
                list = xrealloc (list, n * 10 * sizeof (char *));
            }
            list[j] = command_list[i];
            j++;
            found = TRUE;
        }
    }
    if (j == 1)
    {
        size_t s = xstrlen (list[0]);
        ret = xmalloc (s + 2);
        snprintf (ret, s+2, "%s ", list[0]);
        ret[s+1] = '\0';
        for (i = 0; i < (int) s_in; i++)
            fprintf (stdout, "\b");
        fprintf (stdout, "%s", ret);
        fflush (stdout);
    }
    else if (j > 1)
    {
        size_t s_min = 10000, len;
        int ind_min = -1;
        char *t = NULL;
        fprintf (stdout, "\n");
        for (i = 0; i < j; i++)
        {
            size_t s_tmp = xstrlen (list[i]);
            if (s_tmp < s_min)
            {
                s_min = s_tmp;
                ind_min = i;
                len = s_tmp;
            } 
            fprintf (stdout, "%s\t", list[i]);
        }
        for (i = 0; i < j; i++)
        {
            while (strncmp (list[ind_min], list[i], len) != 0 && len > 0)
                len--;
        }
        t = (len > xstrlen (tmp) && ind_min > -1) ?
                            xstrsub (list[ind_min], 0, len) :
                            NULL;
        if (t != NULL)
            ret = xstrdup (t);

        xfree (t);

        fprintf (stdout, 
                 "\n%s%s", 
                 prompt,
                 ret != NULL ? ret : tmp);
        fflush (stdout);

    }
    xfree (list);
    xfree_list (split, s_split);
    xfree (tmp);
    if (ret != NULL)
        save = xstrlen (ret);
    return ret;
}

void
free_line (input_line *ptr)
{
    if (ptr != NULL)
    {
        int cpt = 0;
        command *tmp = ptr->head;
        while (cpt < ptr->nb && tmp != NULL)
        {
            command *tmp2 = tmp->next;
            free_cmd (tmp);
            tmp = tmp2;
            cpt++;
        }
        xfree (ptr);
    }
}

static void
line_append (input_line **ptr, command *data)
{
    if (*ptr != NULL && data != NULL)
    {
        if ((*ptr)->tail == NULL)
        {
            (*ptr)->head = data;
            (*ptr)->tail = data;
            data->next = NULL;
            data->prev = NULL;
        }
        else
        {
            (*ptr)->tail->next = data;
            data->prev = (*ptr)->tail;
            (*ptr)->tail = data;
            data->next = NULL;
        }
        (*ptr)->nb++;
    }
}

static command *
copy_cmd (const command *src)
{
    command *ret = new_cmd ();
    if (ret == NULL)
        return NULL;
    ret->cmd = xstrdup (src->cmd);
    ret->flag = src->flag;
    ret->in = src->in;
    ret->out = src->out;
    ret->err = src->err;
    ret->prev = src->prev;
    ret->next = src->next;
    ret->builtin = src->builtin;
    ret->pid = src->pid;
    if (src->argc > 0)
    {
        ret->argv = xcalloc (src->argc, sizeof (char *));
        if (ret->argv != NULL)
        {
            int i;
            for (i = 0; i < src->argc; i++)
                ret->argv[i] = xstrdup (src->argv[i]);
            ret->argc = src->argc;
        }
        else
        {
            free_cmd (ret);
            return NULL;
        }
    }
    else
    {
        ret->argv = NULL;
    }
    return ret;
}

void
dump_cmd (command *ptr)
{
    if (ptr != NULL)
    {
        int i;
        fprintf (stdout, "cmd: %s\n", ptr->cmd);
        fprintf (stdout, "argc: %d\n", ptr->argc);
        for (i = 0; i < ptr->argc; i++)
            fprintf (stdout, "argv[%d]: %s\n", i, ptr->argv[i]);
    }
}

void
dump_line (input_line *ptr)
{
    if (ptr != NULL)
    {
        command *tmp = ptr->head;
        int cpt = 0;
        if (ptr->nb > 0)
            fprintf (stdout, "nb commands: %d\n", ptr->nb);
        while (tmp != NULL)
        {
            fprintf (stdout, "=== Dump cmd n°%d ===\n", ++cpt);
            dump_cmd (tmp);
            tmp = tmp->next;
        }
    }
}

input_line *
parse_line (const char *l)
{
    input_line *ret;
    size_t cpt = 0;
    size_t size = xstrlen (l);
    int new_word = 0, first = 1, new_command = 0, begin = 1, i = 0, factor = 1,
        factor2 = 1, arg = 0, squote = 0, dquote = 0;
    unsigned int found = FALSE;
    command *curr = NULL;
    /* do we save the command in the history? */
    /*
    for (i = 0; i < HISTORY && history[i] != NULL; i++)
    {
        if (xstrcmp (history[i], l) == 0)
        {
            found = TRUE;
            break;
        }
    }
    */
    if (!found && xstrlen (l) > 0)
    {
        if (last_history == 0 || xstrcmp (l, history[last_history-1]) != 0)
        {
            last_history = last_history < HISTORY ? last_history : 0;
            xfree (history[last_history]);
            history[last_history] = xstrdup (l);
            last_history++;
        }
    }
    i = 0;
    /* let's create the line container */
    ret = xmalloc (sizeof (*ret));
    if (ret == NULL)
        return NULL;
    ret->nb = 0;
    ret->head = NULL;
    ret->tail = NULL;
    curr = new_cmd ();
    if (curr == NULL)
    {
        free_line (ret);
        return NULL;
    }
    /* the parsing begin here */
    while (cpt < size)
    {
        /* handle the quotes to protect some characters */
        if (l[cpt] == '\'' && !dquote)
        {
            cpt++;
            squote = !squote;
            continue;
        }
        if (l[cpt] == '"' && !squote)
        {
            cpt++;
            dquote = !dquote;
            continue;
        }
        /* 
         * a 'space' found outside a quotation means we have done the current
         * command/argument
         */
        if (l[cpt] == ' ' && !(squote || dquote))
        {
            cpt++;
            /* 
             * of course if there are no non-space character before the space we
             * don't close any word
             */
            if (!first)
            {
                if (i != 0 && begin)
                {
                    curr->cmd[i] = '\0';
                }
                else if (i != 0)
                {
                    curr->argv[curr->argc][i] = '\0';
                    curr->argc++;
                }
                new_word = 1;
                factor = 1;
                factor2 = 1;
                arg = 0;
                i = 0;
                begin = 0;
            }
            continue;
        }
        /* we found a comment */
        if (!(squote || dquote) && l[cpt] == '#')
            break;
        /* handle the redirections (ie. cmd 2>&1 >/tmp/blah) */
        if (!(squote || dquote) &&
            (l[cpt] == '<' || l[cpt] == '>'))
        {
            unsigned int read;
            int f = 1, zi = 0, fd = -1, flags;
            char *file;
            if (curr == NULL)
            {
                syntax_error (l, size, cpt);
                free_line (ret);
                return NULL;
            }
            switch (l[cpt])
            {
            /* redirecting the standard input is pretty simple */
            case '<':
                read = TRUE;
                flags = O_RDONLY;
                break;
            /* it's a bit more complicated for the standard/error output */
            case '>':
            {
                read = FALSE;
                flags = O_CREAT|O_WRONLY;
                if (cpt + 1 < size && l[cpt+1] != '>')
                {
                    flags |= O_TRUNC;
                }
                else
                {
                    cpt++;
                    flags |= O_APPEND;
                }

                /* 
                 * we can specify which file descriptor to redirect within the
                 * command-line (ie. cmd 2>/tmp/errors 1>/tmp/output)
                 */
                if ((cpt > 2 && isdigit (l[cpt-1]) && !isdigit (l[cpt-2])) ||
                    (cpt > 1 && isdigit (l[cpt-1])))
                {
                    char buf[10];
                    snprintf (buf, 10, "%c", l[cpt-1]);
                    fd = strtol (buf, NULL, 10);
                    i--;
                }

                if (fd != -1)
                {
                    if (cpt + 2 > size)
                    {
                        syntax_error (l, size, cpt);
                        free_cmd (curr);
                        free_line (ret);
                        return NULL;
                    }
                    /* do we redirect a descriptor to another one? */
                    if (l[cpt+1] == '&' && isdigit (l[cpt+2]))
                    {
                        if (cpt + 3 < size && isdigit (l[cpt+3]))
                        {
                            syntax_error (l, size, cpt+3);
                            free_cmd (curr);
                            free_line (ret);
                            return NULL;
                        }
                        char buf[10];
                        snprintf (buf, 10, "%c", l[cpt+2]);
                        if (fd == STDERR_FILENO)
                            curr->err = strtol (buf, NULL, 10);
                        else if (fd == STDOUT_FILENO)
                            curr->out = strtol (buf, NULL, 10);
                        cpt += 3;
                        continue;
                    }
                }
                break;
            }
            }
            /* get the filename */
            file = xmalloc (f * BUF * sizeof (char));
            cpt++;
            while (cpt < size && 
                   (isalnum (l[cpt]) || 
                    l[cpt] == '.' || 
                    l[cpt] == '-' || 
                    l[cpt] == '_' ||
                    l[cpt] == '/'))
            {
                if (zi >= f * BUF)
                {
                    f++;
                    file = xrealloc (file, f * BUF * sizeof (char));
                }
                file[zi] = l[cpt];
                zi++;
                cpt++;
            }
            if (zi == 0)
            {
                xfree (file);
                syntax_error (l, size, cpt);
                free_cmd (curr);
                free_line (ret);
                return NULL;
            }
            if (zi >= f * BUF)
            {
                file = xrealloc (file, (f * BUF + 1) * sizeof (char));
            }
            file[zi] = '\0';
            int desc;
            if (read)
                desc = open (file, flags);
            else
                desc = open (file, flags, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP);
            if (desc < 0)
            {
                fprintf (stderr, "%s: %s\n", file, strerror (errno));
                xfree (file);
                free_cmd (curr);
                free_line (ret);
                return NULL;
            }
            if (read)
                curr->in = desc;
            else
            {
                switch (fd)
                {
                case STDOUT_FILENO:
                    curr->out = desc;
                    break;
                case STDERR_FILENO:
                    curr->err = desc;
                    break;
                default:
                    curr->out = desc;
                }
            }
            xfree (file);
            continue;
        }
        /* end of command */
        if (!(squote || dquote) &&
            (l[cpt] == '|' || l[cpt] == ';' || l[cpt] == '&'))
        {
            cpt++;
            if (i != 0 && begin)
            {
                curr->cmd[i] = '\0';
            }
            else if (i != 0)
            {
                curr->argv[curr->argc][i] = '\0';
                curr->argc++;
            }
            /* let's set the flag to know how to run the command */
            switch (l[cpt-1]) 
            {
            case '|':
                if (cpt < size && l[cpt] == '|')
                {
                    cpt++;
                    curr->flag = OR;
                }
                else
                    curr->flag = PIPE;
                break;
            case ';':
                curr->flag = END;
                break;
            case '&':
                if (cpt < size && l[cpt] == '&')
                {
                    cpt++;
                    curr->flag = AND;
                }
                else
                    curr->flag = BG;
                break;
            }
            new_command = 1;
            new_word = 0;
            begin = 1;
            factor = 1;
            factor2 = 1;
            arg = 0;
            first = 1;
            i = 0;
            continue;
        }
        first = 0;
        if (new_command)
        {
            new_command = 0;
            command *tmp = copy_cmd (curr);
            line_append (&ret, tmp);
            free_cmd (curr);
            curr = new_cmd ();
            if (curr == NULL)
            {
                free_line (ret);
                return NULL;
            }
        }
        if (begin && !new_word)
        {
            if (curr->cmd == NULL)
            {
                curr->cmd = xmalloc (factor * BUF * sizeof (char));
                if (curr->cmd == NULL)
                {
                    free_cmd (curr);
                    free_line (ret);
                    return NULL;
                }
            }
            if (i >= factor * BUF)
            {
                factor++;
                char *tmp = xrealloc (curr->cmd, factor * BUF * sizeof (char));
                if (tmp == NULL)
                {
                    free_cmd (curr);
                    free_line (ret);
                    return NULL;
                }
                curr->cmd = tmp;
            }
            curr->cmd[i] = l[cpt];
        }
        else if (new_word)
        {
            if (curr->argc == 0 && !arg)
            {
                curr->argv = xcalloc (factor * ARGC, sizeof (char *));
                if (curr->argv == NULL)
                {
                    free_cmd (curr);
                    free_line (ret);
                    return NULL;
                }
                arg = 1;
            }
            else if (curr->argc >= factor * ARGC)
            {
                factor++;
                char **tmp = xrealloc (curr->argv, 
                                       factor * ARGC * sizeof (char *));
                if (tmp == NULL)
                {
                    free_cmd (curr);
                    free_line (ret);
                    return NULL;
                }
                curr->argv = tmp;
            }
            if (i == 0)
            {
                curr->argv[curr->argc] = xmalloc (BUF * sizeof (char));
                if (curr->argv[curr->argc] == NULL)
                {
                    free_cmd (curr);
                    free_line (ret);
                    return NULL;
                }
            }
            else if (i >= factor2 * BUF)
            {
                factor2++;
                char *tmp = xrealloc (curr->argv[curr->argc], 
                                      factor2 * BUF * sizeof (char));
                if (tmp == NULL)
                {
                    free_cmd (curr);
                    free_line (ret);
                    return NULL;
                }
                curr->argv[curr->argc] = tmp;
            }
            curr->argv[curr->argc][i] = l[cpt];
        }
        i++;
        cpt++;
    }
    if (i != 0 && begin)
    {   
        curr->cmd[i] = '\0';
    }   
    else if (i != 0)
    {   
        curr->argv[curr->argc][i] = '\0';
        curr->argc++;
        if (curr->argc >= factor * ARGC)
        {
            factor++;
            char **tmp = xrealloc (curr->argv, factor * ARGC * sizeof (char *));
            if (tmp == NULL)
            {
                free_cmd (curr);
                free_line (ret);
                return NULL;
            }
            curr->argv = tmp;
        }
        curr->argv[curr->argc] = NULL;
    }  
    if (curr->cmd != NULL)
        line_append (&ret, curr);
    else
        free_cmd (curr);
    return ret;
}

static char
get_char (const char input[5], 
          const char *prompt, 
          char **ret, 
          int *cpt, 
          int *n)
{
    size_t len;
    (void) prompt;
    if (input[4] == 0)
        len = xstrlen (input);
    else
        len = 5;
    if (len == 1)
    {
/*        fprintf (stdout, "\nc: %c d: %d\n", input[0], input[0]);*/
        switch (input[0])
        {
        case 127:
            if (*cpt > 0)
            {
                (*cpt)--;
                fprintf (stdout, "\b \b");
                fflush (stdout);
            }
            return -1;
            break;
        case CTRL_D:
        case CTRL_L:
        case CTRL_R:
            return -1;
            break;
        default:
            return input[0];
        }
    }
    if (len >= 3 && input[0] == 27 && input[1] == '[')
    {
        switch (input[2])
        {
        /* arrow UP */
        case 'A':
            if (curr_history > 0 && history[curr_history-1] != NULL)
            {
                for (; *cpt > 0; (*cpt)--)
                    fprintf (stdout, "\b \b");
                fprintf (stdout, "%s", history[curr_history-1]);
                fflush (stdout);
                curr_history = xmax (curr_history - 1, 0);
                for (; *cpt < (int) xstrlen (history[curr_history]); (*cpt)++)
                {
                    if (*cpt >= *n * BUF)
                    {
                        (*n)++;
                        *ret = xrealloc (*ret, *n * BUF * sizeof (char));
                    }
                    (*ret)[*cpt] = history[curr_history][*cpt];
                }
            }
            break;
        /* arrow DOWN */
        case 'B':
            for (; *cpt > 0; (*cpt)--)
                fprintf (stdout, "\b \b");
            if (curr_history <= last_history && history[curr_history] != NULL)
            {
                if (history[curr_history+1] != NULL)
                {
                    fprintf (stdout, "%s", history[curr_history+1]);
                    curr_history = xmin (curr_history + 1, HISTORY);
                    for (; *cpt < (int) xstrlen (history[curr_history]); (*cpt)++)
                    {
                        if (*cpt >= *n * BUF)
                        {
                            (*n)++;
                            *ret = xrealloc (*ret, *n * BUF * sizeof (char));
                        }
                        (*ret)[*cpt] = history[curr_history][*cpt];
                    }
                }
            }
            fflush (stdout);
            break;
        }
    } 
    else
    {
        int max = *cpt + len, i = 0;
        for (; *cpt < max; (*cpt)++)
        {
            if (*cpt >= *n * BUF)
            {
                (*n)++;
                *ret = xrealloc (*ret, *n * BUF * sizeof (char));
            }
            (*ret)[*cpt] = input[i];
            fprintf (stdout, "%c", input[i]);
            i++;
        }
        fflush (stdout);
    }
    return -1;
}

char *
read_line (const char *prompt)
{
    char *ret = xmalloc (BUF * sizeof (char));
    int cpt = 0, ind = 1, nb_lines = 0, antislashes = 0, read_tmp = 0,
        squote = 0, dquote = 0;
    char in[5], tmp[5], c = -1;
    curr_history = last_history;
    if (ret == NULL)
        return NULL;
    if (prompt != NULL && xstrlen (prompt) > 0)
    {
        fprintf (stdout, "%s", prompt);
        fflush (stdout);
    }
    init_ioctl ();
    do
    {
        memset (in, 0, 5);
        read (STDIN_FILENO, &in, 5);
        c = get_char (in, prompt, &ret, &cpt, &ind);
    } while (c == -1);
    while (c != '\n' || (c == '\n' && (squote || dquote)) || nb_lines != antislashes)
    {
        if (c == '\t')
        {
            char *comp = completion (prompt, ret, cpt);
            if (comp != NULL)
            {
                size_t s_comp = xstrlen (comp);
                int iz;
                for (iz = cpt; iz < (int) s_comp; iz++, cpt++)
                {
                    if (cpt >= ind * BUF)
                    {
                        ind++;
                        ret = xrealloc (ret, ind * BUF * sizeof (char));
                    }
                    ret[cpt] = comp[iz];
                }
                xfree (comp);
            }
            do
            {
                memset (in, 0, 5);
                read (STDIN_FILENO, &in, 5);
                c = get_char (in, prompt, &ret, &cpt, &ind);
            } while (c == -1);
            continue;
        }
        else if (c != '\n')
        {
            fprintf (stdout, "%c", c);
            fflush (stdout);
        }
        else if (c == '\n' && (squote || dquote))
        {
            fprintf (stdout, "\n> ");
            fflush (stdout);
        }
        if (c == '\'' && !dquote)
            squote = !squote;
        if (c == '"' && !squote)
            dquote = !dquote;
        if (c == '\\')
        {
            char ctmp = -1;
            do
            {
                memset (tmp, 0, 5);
                read (STDIN_FILENO, &tmp, 5);
                ctmp = get_char (tmp, prompt, &ret, &cpt, &ind);
            } while (ctmp == -1);
            if (ctmp == '\n')
            {
                c = ctmp;
                antislashes++;
                fprintf (stdout, "\n> ");
                fflush (stdout);
                continue;
            }
            read_tmp = 1;
        }
        if (c == '\n' && !(squote || dquote))
        {
            do
            {
                memset (in, 0, 5);
                read (STDIN_FILENO, &in, 5);
                c = get_char (in, prompt, &ret, &cpt, &ind);
            } while (c == -1);
            nb_lines++;
            continue;
        }
replay:
        if (cpt >= ind * BUF)
        {
            ind++;
            char *new = xrealloc (ret, ind * BUF * sizeof (char));
            if (new == NULL)
            {
                xfree (ret);
                return NULL;
            }
            ret = new;
        }
        if (read_tmp)
        {
            read_tmp = 0;
            ret[cpt] = c;
            c = get_char (tmp, prompt, &ret, &cpt, &ind);
            cpt++;
            fprintf (stdout, "%c", c);
            fflush (stdout);
            goto replay;
        }
        ret[cpt] = c;
        cpt++;
        do
        {
            memset (in, 0, 5);
            read (STDIN_FILENO, &in, 5);
            c = get_char (in, prompt, &ret, &cpt, &ind);
        } while (c == -1);
    }
    fprintf (stdout, "\n");
    if (cpt >= ind * BUF)
    {
        char *new = xrealloc (ret, (ind * BUF * sizeof (char)) + 1);
        if (new == NULL)
        {
            xfree (ret);
            return NULL;
        }
        ret = new;
    }
    ret[cpt] = '\0';
    clear_ioctl ();
    return ret;
}
