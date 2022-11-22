/*
 * Supporting functions for Xdialog.
 */

#include "common.h"

#include <errno.h> // my_scanf
#include <fcntl.h> // my_scanf

#include "interface.h"

/* Global structures and variables */
extern Xdialog_data Xdialog;
extern gboolean dialog_compat;

/* Two useful functions to avoid buffer overflows... */

/* replace scanf() with a non-blocking function based on read() calls...
   It may not compile on systems lacking lacking errno.h / fcntl.h headers...
   Code adapted from a patch by Rolland Dudemaine.
 */
int my_scanf(char *buffer)
{
    static int input_buffer_pos = 0;
    static char input_buffer[256];
    char *p;
    int ret;

    fcntl(0, F_SETFL, O_NONBLOCK);

    ret = read(0, (input_buffer + input_buffer_pos), 256 - input_buffer_pos);
    if ( ret > 0 )
        input_buffer_pos += ret;

    if ((p = memchr(input_buffer, '\n', input_buffer_pos)) != NULL) {
        strncpy(buffer, input_buffer, p - input_buffer);
        buffer[p - input_buffer] = '\0';
        memmove(input_buffer, p+1, input_buffer_pos - (p - input_buffer));
        input_buffer_pos -= p - input_buffer + 1;
    } else {
        if (ret == 0)
            return EOF;
        if (ret == -1) {
            switch (errno) {
                /* nothing to read */
                case EINTR:
                case EAGAIN:
                    return 0;
                /* like EOF */
                case EINVAL:
                case EBADF:
                    return EOF;
            }
            }
    }
    return 1;
}

/* "\n" to linefeed translation */

void backslash_n_to_linefeed(char *s0, char *s, int max_len)
{
    char *tmp;

    strncpy(s, s0, max_len);

    while (strstr(s, "\\n") != NULL) {
        tmp = strstr(s, "\\n");
        strcpy(tmp, tmp+1);
        *tmp = '\n';
    }

}

/*
 * This function was borrowed from cdialog-0.9a "util.c" (and sligthly changed)...
 *
 * Change embedded "\n" substrings to '\n' characters and tabs to single
 * spaces.  If there are no "\n"s, it will strip all extra spaces, for
 * justification.  If it has "\n"'s, it will preserve extra spaces.  If cr_wrap
 * is set, it will preserve '\n's.
 */
void trim_string(char *s0, char *s, int max_len)
{
    char *p1;
    char *p = s;
    int has_newlines = 0;

    strncpy(s, s0, max_len);

    if (strstr(s, "\\n")) {
        has_newlines = 1;
    }

    while (*p != '\0')
    {
        if (*p == '\t') {
            *p = ' ';
        }
        if (*p == '\\' && *(p + 1) == 'n') {
            *s++ = '\n';
            p += 2;
            p1 = p;
            /*
             * Handle end of lines intelligently.  If '\n' follows "\n" then
             * ignore the '\n'.  This eliminates the need escape the '\n'
             * character (no need to use "\n\").
             */
            while (*p1 == ' ')
                p1++;
            if (*p1 == '\n')
                p = p1 + 1;
        } else {
            if (has_newlines) {    /* If prompt contains "\n" strings */
                if (*p == '\n') {
                    if (Xdialog.cr_wrap)
                        *s++ = *p++;
                    else {
                        /* Replace the '\n' with a space if cr_wrap is not set */
                        if (*(s - 1) != ' ')
                            *s++ = ' ';
                        p++;
                    }
                } else        /* If *p != '\n' */
                    *s++ = *p++;
            } else {        /* If there are no "\n" strings */
                if (*p == ' ') {
                    if (*(s - 1) != ' ') {
                        *s++ = ' ';
                        p++;
                    } else
                        p++;
                } else if (*p == '\n') {
                    if (Xdialog.cr_wrap)
                        *s++ = *p++;
                    else if (*(s - 1) != ' ') {
                        /* Strip '\n's if cr_wrap is not set. */
                        *s++ = ' ';
                        p++;
                    } else
                        p++;
                } else
                    *s++ = *p++;
            }
        }
    }

    *s = '\0';
}

/* Array allocation function */

void Xdialog_array(gint elements)
{
    Xdialog.array = g_malloc0((elements+1)*sizeof(listname));
    if (Xdialog.array == NULL) {
        fprintf(stderr,
             XDIALOG": problem while allocating memory, exiting !\n");
            exit(255);
    }
    Xdialog.array[elements].state = -1;
}
