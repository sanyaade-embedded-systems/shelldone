#include <stdio.h>
#include <stdlib.h>

#include "callbacks.h"
#include "command.h"
#include "xutils.h"

#define BUF 128
#define ARGC 10

static callback calls[] = {{"cd", (cmd_callback) sd_cd},
                           {NULL, NULL}};

command *
new_cmd (void)
{
    command *ret = xmalloc (sizeof (*ret));
    if (ret != NULL)
    {
        ret->cmd = NULL;
        ret->argv = NULL;
        ret->argc = 0;
        ret->next = NULL;
        ret->prev = NULL;
    }
    return ret;
}

void
free_cmd (command *ptr)
{
    if (ptr != NULL)
    {
        int cpt;
        for (cpt = 0; cpt < ptr->argc; cpt++)
            xfree (ptr->argv[cpt]);
        xfree (ptr->argv);
        xfree (ptr->cmd);
        xfree (ptr);
    }
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
    command *curr = NULL;
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
    while (cpt < size)
    {
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
        if (l[cpt] == ' ' && !(squote || dquote))
        {
            cpt++;
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
        if (l[cpt] == '|' && !(squote || dquote))
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
    }  
    if (curr->cmd != NULL)
        line_append (&ret, curr);
    else
        free_cmd (curr);
    return ret;
}

char *
read_line (const char *prompt)
{
    char *ret = xmalloc (BUF * sizeof (char));
    int cpt = 0, ind = 1, nb_lines = 0, antislashes = 0, read_tmp = 0;
    char c, tmp;
    if (ret == NULL)
        return NULL;
    if (prompt != NULL && xstrlen (prompt) > 0)
    {
        fprintf (stdout, "%s", prompt);
        fflush (stdout);
    }
    c = getchar ();
    while (c != '\n' || nb_lines != antislashes)
    {
        if (c == '\\')
        {
            tmp = getchar ();
            if (tmp == '\n')
            {
                c = tmp;
                antislashes++;
                continue;
            }
            read_tmp = 1;
        }
        if (c == '\n')
        {
            c = getchar ();
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
            c = tmp;
            cpt++;
            goto replay;
        }
        ret[cpt] = c;
        cpt++;
        c = getchar ();
    }
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
    return ret;
}

void
parse_command (command *ptr)
{
    if (ptr != NULL)
    {
        cmd_callback call = NULL;
        int i = 0;
        while (calls[i].key != NULL)
        {
            if (xstrcmp (ptr->cmd, calls[i].key) == 0)
            {
                call = calls[i].func;
                break;
            }
            i++;
        }
        if (call != NULL)
        {
            call (ptr->argc, ptr->argv);
        }
    }
}

void
run_line (input_line *ptr)
{
    if (ptr != NULL)
    {
        parse_command (ptr->head);
    }
}
