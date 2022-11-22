/*
 * Callback functions for Xdialog.
 */

#include "common.h"

#include <time.h>

#include "callbacks.h"
#include "interface.h"
#include "support.h"

extern Xdialog_data Xdialog;

/* This function is called when a "delete_event" is received from the window
 * manager. It is used to trigger a "destroy" event by returning FALSE
 * (provided the "--no-close" option was not given).
 */
gboolean delete_event(gpointer object, GdkEventAny *event, gpointer data)
{
    return Xdialog.no_close;
}

/* This function is called when a "destroy" event is received 
 * by the top level window.
 */
gboolean destroy_event(gpointer object, GdkEventAny *event, gpointer data)
{
    if (Xdialog.timer != 0) {
        g_source_remove(Xdialog.timer);
        Xdialog.timer = 0;
    }
    if (Xdialog.timer2 != 0) {
        g_source_remove(Xdialog.timer2);
        Xdialog.timer2 = 0;
    }
    gtk_main_quit();
    Xdialog.window = Xdialog.widget1 = Xdialog.widget2 = Xdialog.widget3 = NULL;

    if (Xdialog.file != NULL) {
        if (Xdialog.file != stdin)
            fclose(Xdialog.file);
        Xdialog.file = NULL;
    }
    if (Xdialog.array != NULL) {
        g_free(Xdialog.array);
        Xdialog.array = NULL;
    }

    if (Xdialog.beep & BEEP_AFTER && Xdialog.exit_code != 2) {
        gdk_beep();
    }
    return FALSE;
}

/* Double-click event is processed as a button click in radiolist and
 * checklist... The button widget is to be passed as "data".
 */
gint double_click_event (GtkWidget *object, GdkEventButton *event, gpointer data)
{
    if (event->type == GDK_2BUTTON_PRESS || event->type == GDK_3BUTTON_PRESS) {
        g_signal_emit_by_name (G_OBJECT(data), "clicked");
    }
    return FALSE;
}

/* These are the normal termination callback routines that are used by the
 * OK, Cancel and Help buttons. They set the exit_code global variable (for
 * use as the exit() parameter by main.c), and then call the destroy_event
 * function that will cleanup everything, destroy the top level window and
 * set the gtk main loop exit flag. They are used "as is" by most Xdialog
 * widgets but care must be taken that they are connected with the
 * g_signal_connect_after() function so that any other signal callbacks
 * are executed BEFORE them (after them, the widget does not exists anymore).
 */
gboolean exit_ok(gpointer object, gpointer data)
{
    if (Xdialog.check) {
        if (Xdialog.checked)
            fprintf(Xdialog.output, "checked\n");
        else
            fprintf(Xdialog.output, "unchecked\n");
    }
    gtk_widget_destroy(Xdialog.window);
    Xdialog.exit_code = 0;
    return FALSE;
}

gboolean exit_extra(gpointer object, gpointer data)
{
    if (Xdialog.check) {
        if (Xdialog.checked)
            fprintf(Xdialog.output, "checked\n");
        else
            fprintf(Xdialog.output, "unchecked\n");
    }
    gtk_widget_destroy(Xdialog.window);
    Xdialog.exit_code = 3;
    return FALSE;
}

gboolean exit_cancel(gpointer object, gpointer data)
{
    Xdialog.exit_code = 1;
    gtk_widget_destroy(Xdialog.window);
    return FALSE;
}

gint exit_keypress(gpointer object, GdkEventKey *event, gpointer data)
{
    if (event->type == GDK_KEY_PRESS && (event->keyval == GDK_KEY(Escape))) {
        return exit_cancel(object, data);
    }
    return TRUE;
}

gboolean exit_help(gpointer object, gpointer data)
{
    Xdialog.exit_code = 2;
    gtk_widget_destroy(Xdialog.window);
    return FALSE;
}

gboolean exit_previous(gpointer object, gpointer data)
{
    Xdialog.exit_code = 3;
    gtk_widget_destroy(Xdialog.window);
    return FALSE;
}

gboolean checked(GtkWidget *button, gpointer data)
{
    Xdialog.checked = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));
    return TRUE;
}

gboolean timeout_exit(gpointer data)
{
    Xdialog.exit_code = 255;
    gtk_widget_destroy(Xdialog.window);
    return FALSE;
}

/* This function is called within the timeout functions so to force
 * the processing of all queued GTK events.
 */
gboolean empty_gtk_queue(void)
{
    while(gtk_events_pending()) {
        gtk_main_iteration();
        /* If the timeout function has been removed, return immediately */
        if (Xdialog.timer == 0)
            return FALSE;
    }
    /* All events have been taken into account... */
    return TRUE;
}

/* infobox callbacks: the infobox_timeout_exit() is responsible for
 * closing the infobox once the timeout is over (it therefore calls
 * exit_ok()). The infobox_timeout() function is responsible for
 * reading the stdin and changing the infobox label and/or exiting the
 * infobox by calling exit_ok().
 */

gboolean infobox_timeout_exit(gpointer data)
{
    return exit_ok(NULL, NULL);
}

gboolean infobox_timeout(gpointer data)
{
    char temp[256];
    int ret;

    if (!empty_gtk_queue())
        return FALSE;

    /* Read the the data from stdin */
    ///ret = scanf("%255s", temp); // blocking function, breaks stuff
    ret = my_scanf(temp);
    if ((ret == EOF && !Xdialog.ignore_eof) || (strcmp(temp, "XXXX") == 0))
        return exit_ok(NULL, NULL);
    if (ret != 1)
        return TRUE;

    if (strcmp(temp, "XXX") == 0) {
        /* If this is a new label delimiter, then check to see if it's the
         * start or the end of the label. */
        if (Xdialog.new_label) {
            gtk_label_set_text(GTK_LABEL(Xdialog.widget1),
                       Xdialog.label_text);
            Xdialog.label_text[0] = 0 ;
            Xdialog.new_label = FALSE;
        } else {
            Xdialog.new_label = TRUE;
        }
    } else {
        /* Add this text to the new label text */
        if (strlen(Xdialog.label_text)+strlen(temp)+2 < MAX_LABEL_LENGTH) {
            if (strcmp(temp, "\\n") == 0) {
                strcat(Xdialog.label_text, "\n");
            } else {
                strcat(Xdialog.label_text, " ");
                strcat(Xdialog.label_text, temp);
            }
        }
    }

    /* As this is a timeout function, return TRUE so that it
     * continues to get called */
    return TRUE;
}

 
// ------------------------------------------------------------------------------------------
//                          gauge timeout callback
// ------------------------------------------------------------------------------------------

gboolean gauge_timeout(gpointer data)
{
    gdouble new_val;
    char temp[256];
    int ret;
#if GTK_MAJOR_VERSION == 1 // -- GTK1 --
    GtkAdjustment *adj = GTK_PROGRESS(Xdialog.widget1)->adjustment;
#endif

    if (!empty_gtk_queue())
        return FALSE;

    /* Read the new progress bar value or the new label from stdin */
    ///ret = scanf("%255s", temp); // blocking function, breaks stuff
    ret = my_scanf(temp);
    if (ret == EOF && !Xdialog.ignore_eof)
        return exit_ok(NULL, NULL);
    if (ret != 1)
        return TRUE;

    if (!Xdialog.new_label && strcmp(temp, "XXX")) {
        /* Try to convert the string into an integer for use as the new
          * progress bar value... */
        new_val = (gdouble) atoi(temp);
#if GTK_CHECK_VERSION(2,0,0)
        char txt[20];
        snprintf(txt, sizeof(txt), "%g%%", new_val); // 50%
        gtk_progress_bar_set_text (GTK_PROGRESS_BAR (Xdialog.widget1), txt);
        new_val = new_val / 100;
        //printf ("x: %g\n", new_val);
        if (new_val < 0.0 || new_val > 1.0) {
            return exit_ok(NULL, NULL);
        }
        /* Set the new value */
        gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (Xdialog.widget1), new_val);
#else // -- GTK1 --
        if ((new_val > adj->upper) || (new_val < adj->lower)) {
            return exit_ok(NULL, NULL);
        }
        /* Set the new value */
        gtk_progress_set_value (GTK_PROGRESS(Xdialog.widget1), new_val);
#endif
    }
    else
    {
        if (strcmp(temp, "XXX") == 0) {
            /* If this is a new label delimiter, then check to see if it's the
             * start or the end of the label. */
            if (Xdialog.new_label) {
                gtk_label_set_text(GTK_LABEL(Xdialog.widget2),
                           Xdialog.label_text);
                Xdialog.label_text[0] = 0 ;
                Xdialog.new_label = FALSE;
            } else {
                Xdialog.new_label = TRUE;
            }
        } else {
            /* Add this text to the new label text */
            if (strlen(Xdialog.label_text)+strlen(temp)+2 < MAX_LABEL_LENGTH) {
                if (strcmp(temp, "\\n") == 0) {
                    strcat(Xdialog.label_text, "\n");
                } else {
                    strcat(Xdialog.label_text, " ");
                    strcat(Xdialog.label_text, temp);
                }
            }
        }
    }
    /* As this is a timeout function, return TRUE so that it
     * continues to get called */
    return TRUE;
}


// ------------------------------------------------------------------------------------------
//                        progress timeout callback
// ------------------------------------------------------------------------------------------

gboolean progress_timeout(gpointer data)
{
    gdouble new_val;
    char temp[256];
    int ret;
#if GTK_MAJOR_VERSION == 1 // -- GTK1 --
    GtkAdjustment *adj = GTK_PROGRESS(Xdialog.widget1)->adjustment;
#endif

    if (!empty_gtk_queue())
        return FALSE;

    temp[255] = '\0';

    /* Read the new progress bar value or the new "dot" from stdin,
     * skipping any control character.
     */
    do {
        ret = fgetc(stdin);
        if (ret == EOF)
            return exit_ok(NULL, NULL);
        temp[0] = (char) ret;
    } while (temp[0] <= ' ');

    if (temp[0] >= '0' && temp[0] <= '9')
    {
        /* Get and convert a string into an integer for use as
         * the new progress bar value... 1-100 */
        ret = scanf("%254s", temp + 1);
        if (ret == EOF)
            return exit_ok(NULL, NULL);
        if (ret != 1)
            return TRUE;
#if GTK_CHECK_VERSION(2,0,0)
        new_val = strtod (temp, NULL) / 100.0;
#else
        new_val = (gdouble) atoi(temp);
#endif
    }
    else
    {
#if GTK_CHECK_VERSION(2,0,0)
        /* Increment the number of "dots" */
        new_val = gtk_progress_bar_get_fraction (GTK_PROGRESS_BAR (Xdialog.widget1));
        new_val = new_val + Xdialog.progress_step;
#else // -- GTK1 --
        new_val = adj->value + 1;
#endif
    }

#if GTK_CHECK_VERSION(2,0,0)
    ///printf("%g\n", new_val);
    if (new_val < 0.0 || new_val > 1.0) {
        return exit_ok(NULL, NULL);
    }
    /* https://www.mathsisfun.com/converting-fractions-percents.html */
    int percent = (int) ((new_val / 1.0) * 100.0);
    /* set pg txt */
    char txt[20];
    snprintf(txt, sizeof(txt), "%d%%", percent);
    gtk_progress_bar_set_text (GTK_PROGRESS_BAR (Xdialog.widget1), txt);
    /* Set the new value */
    gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (Xdialog.widget1), new_val);
#else // -- GTK1 --
    if ((new_val > adj->upper) || (new_val < adj->lower)) {
        return exit_ok(NULL, NULL);
    }
    /* Set the new value */
    gtk_progress_set_value (GTK_PROGRESS(Xdialog.widget1), new_val);
#endif

    /* As this is a timeout function, return TRUE so that it
     * continues to get called */
    return TRUE;
}


// ------------------------------------------------------------------------------------------
//                           tailbox callbacks
// ------------------------------------------------------------------------------------------

gboolean tailbox_timeout(gpointer data)
{
    gchar buffer[1024];
    int nchars;
#if GTK_CHECK_VERSION(2,0,0)
    GtkTextIter end_iter;
    GtkTextBuffer *text_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(Xdialog.widget1));

    do
    {
        if (!empty_gtk_queue() && (Xdialog.file_init_size <= 0))
            return FALSE;

        nchars = fread(buffer, sizeof(gchar), 1024, Xdialog.file);
        if (nchars == 0)
            break;

        gtk_text_buffer_get_end_iter(text_buffer, &end_iter);
        gtk_text_buffer_insert(text_buffer, &end_iter, buffer, nchars);

        if (Xdialog.file_init_size > 0) 
            Xdialog.file_init_size -= nchars;
    } while (nchars == 1024);

    if (nchars > 0) {
        GtkTextMark *mark;
        gtk_text_buffer_get_end_iter(text_buffer, &end_iter);

        mark = gtk_text_buffer_create_mark(text_buffer, "end", &end_iter, TRUE);

        gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(Xdialog.widget1),
            mark, 0, FALSE, 0, 0);
    }
#else // -- GTK1 --
    GtkAdjustment *adj;
    gboolean flag = FALSE;

    adj = GTK_TEXT(Xdialog.widget1)->vadj;

    if (Xdialog.file_init_size > 0) {
        gtk_text_freeze(GTK_TEXT(Xdialog.widget1));
    }
    do
    {
        if (!empty_gtk_queue() && (Xdialog.file_init_size <= 0))
            return FALSE;

        nchars = fread(buffer, sizeof(gchar), 1024, Xdialog.file);

        if (nchars == 0)
            break;

        if (Xdialog.file_init_size > 0) {
            Xdialog.file_init_size -= nchars;
            if (Xdialog.file_init_size <= 0)
                flag = TRUE;
        } else {
            if (!Xdialog.smooth)
                gtk_text_freeze(GTK_TEXT(Xdialog.widget1));
        }

        gtk_text_insert (GTK_TEXT(Xdialog.widget1), NULL, NULL,
                         NULL, buffer, nchars);

        if ((!Xdialog.smooth || flag) && Xdialog.file_init_size <= 0) {
            gtk_text_thaw (GTK_TEXT(Xdialog.widget1));
            flag = FALSE;
        }

        gtk_adjustment_set_value(adj, adj->upper);

    } while (nchars == 1024);
#endif
    return TRUE;
}

gint tailbox_keypress(GtkWidget *text, GdkEventKey *event,
                      gpointer data)
{
    if (event->type == GDK_KEY_PRESS &&
    (event->keyval == GDK_KEY(Return) || event->keyval == GDK_KEY(KP_Enter))) {
        if (Xdialog.default_no)
            Xdialog.exit_code = 1;
        else
            Xdialog.exit_code = 0;

        return(FALSE);
    }
    return(TRUE);
}


// ------------------------------------------------------------------------------------------
//                           logbox callbacks
// ------------------------------------------------------------------------------------------

static void vt_to_gdk_color(gint color, GdkColor **fgcolor, GdkColor **bgcolor)
{
    static const GdkColor BLACK     = { 0, 0x0000, 0x0000, 0x0000 };
    static const GdkColor RED       = { 0, 0xffff, 0x0000, 0x0000 };
    static const GdkColor GREEN     = { 0, 0x0000, 0xffff, 0x0000 };
    static const GdkColor BLUE      = { 0, 0x0000, 0x0000, 0xffff };
    static const GdkColor MAGENTA   = { 0, 0xffff, 0x0000, 0xffff };
    static const GdkColor YELLOW    = { 0, 0xffff, 0xffff, 0x0000 };
    static const GdkColor CYAN      = { 0, 0x0000, 0xffff, 0xffff };
    static const GdkColor WHITE     = { 0, 0xffff, 0xffff, 0xffff };
    switch (color)
    {
        case 30: *fgcolor = (GdkColor *) &BLACK;  break;
        case 31: *fgcolor = (GdkColor *) &RED;    break;
        case 32: *fgcolor = (GdkColor *) &GREEN;  break;
        case 33: *fgcolor = (GdkColor *) &YELLOW; break;
        case 34: *fgcolor = (GdkColor *) &BLUE;   break;
        case 35: *fgcolor = (GdkColor *) &MAGENTA; break;
        case 36: *fgcolor = (GdkColor *) &CYAN;  break;
        case 37: *fgcolor = (GdkColor *) &WHITE; break;
        case 38: *fgcolor = NULL; break;
        //--
        case 40: *bgcolor = (GdkColor *) &BLACK; break;
        case 41: *bgcolor = (GdkColor *) &RED;   break;
        case 42: *bgcolor = (GdkColor *) &GREEN; break;
        case 43: *bgcolor = (GdkColor *) &YELLOW; break;
        case 44: *bgcolor = (GdkColor *) &BLUE;   break;
        case 45: *bgcolor = (GdkColor *) &MAGENTA;break;
        case 46: *bgcolor = (GdkColor *) &CYAN;  break;
        case 47: *bgcolor = (GdkColor *) &WHITE; break;
        case 48: *bgcolor = NULL; break;
    }
}

static void remove_vt_sequences(char *str)
{
    char *p, *q;
    while ((p = strstr(str, "\033[")) != NULL) {
        q = p;
        while (++p < str + strlen(str)) {
            if (*p == 'm')
                break;
        }
        strcpy(q, ++p);
    }
}


gboolean logbox_timeout(gpointer data)
{
    static GdkColor *old_fgcolor = NULL;
    static GdkColor *old_bgcolor = NULL;
#if GTK_CHECK_VERSION(2,0,0)
    GtkTreeView  * tree  = GTK_TREE_VIEW (Xdialog.widget1);
    GtkTreeModel * model = gtk_tree_view_get_model (tree);
    GtkListStore * store = GTK_LIST_STORE (model);
    GtkTreeIter iter;
#else // -- GTK1 --
    GtkCList *clist = GTK_CLIST(Xdialog.widget1);
    // the list may contain up to 2 rows
    static gchar *null_row[] = { NULL, NULL };
    int rownum;
#endif

    GdkColor *fgcolor, *bgcolor;
    gchar buffer[MAX_LABEL_LENGTH], *p;
    int color, len;
    struct tm *localdate = NULL;
    time_t curr_time;

    if (Xdialog.file_init_size <= 0)
        if (!empty_gtk_queue())
            return FALSE;

    while (fgets(buffer, MAX_LABEL_LENGTH, Xdialog.file) != NULL)
    {
        len = strlen(buffer);

        if (Xdialog.file_init_size > 0) {
            Xdialog.file_init_size -= len;
        }
        if ((len > 0) && (buffer[len - 1] == '\n')) {
            buffer[--len] = 0;
        }
        if (Xdialog.time_stamp) {
            time(&curr_time);
            localdate = localtime(&curr_time);
        }
        if (Xdialog.keep_colors) {
            fgcolor = old_fgcolor;
            bgcolor = old_bgcolor;
        } else {
            fgcolor = bgcolor = NULL;
        }
        if ((p = strstr(buffer, "\033[1;")) != NULL) {
            p += 4;
            color = atoi(p);
            vt_to_gdk_color(color, &fgcolor, &bgcolor);

            while (++p < buffer + len) {
                if (*p == ';') {
                    color = atoi(++p);
                    vt_to_gdk_color(color, &fgcolor, &bgcolor);
                    p += 2;
                }
                if (*p == 'm')
                    break;
            }
            if (Xdialog.keep_colors) {
                old_fgcolor = fgcolor;
                old_bgcolor = bgcolor;
            }
            remove_vt_sequences(buffer);
        }

#if GTK_CHECK_VERSION(2,0,0)
        if (Xdialog.reverse) {
            gtk_list_store_prepend (store, &iter);
        } else {
            gtk_list_store_append (store, &iter);
        }
        if (fgcolor) {
            gtk_list_store_set (store, &iter, LOGBOX_COL_FGCOLOR, fgcolor, -1);
        }
        if (bgcolor) {
            gtk_list_store_set (store, &iter, LOGBOX_COL_BGCOLOR, bgcolor, -1);
        }
        gtk_list_store_set (store, &iter, LOGBOX_COL_TEXT, buffer, -1);

#else // -- GTK1 --
        if (Xdialog.reverse) {
            rownum = gtk_clist_prepend (clist, null_row);
        } else {
            rownum = gtk_clist_append (clist, null_row);
        }
        gtk_clist_set_selectable (clist, rownum, FALSE);
        if (fgcolor) {
            gtk_clist_set_foreground (clist, rownum, fgcolor);
        }
        if (bgcolor) {
            gtk_clist_set_background (clist, rownum, bgcolor);
        }
        if (!Xdialog.time_stamp) {
            gtk_clist_set_text (clist, rownum, 0, buffer);
        }
#endif

        if (Xdialog.time_stamp)
        {
#if GTK_MAJOR_VERSION == 1 // -- GTK1 --
            gtk_clist_set_text (clist, rownum, 1, buffer);
#endif
            if (Xdialog.date_stamp) {
                sprintf(buffer, "%02d/%02d/%d %02d:%02d:%02d ",
                        localdate->tm_mday, localdate->tm_mon+1, localdate->tm_year+1900,
                        localdate->tm_hour, localdate->tm_min, localdate->tm_sec);
            } else {
                sprintf(buffer, "%02d:%02d:%02d",
                        localdate->tm_hour, localdate->tm_min, localdate->tm_sec);
            }
#if GTK_CHECK_VERSION(2,0,0)
            gtk_list_store_set (store, &iter, LOGBOX_COL_DATE, buffer, -1);
#else // -- GTK1 --
            gtk_clist_set_text (clist, rownum, 0, buffer);
#endif
        }

#if GTK_MAJOR_VERSION == 1 // -- GTK1 --
        gtk_clist_columns_autosize(clist);
#endif

        if (!empty_gtk_queue()) {
            return FALSE;
        }
    }

    return TRUE;
}


// ------------------------------------------------------------------------------------------
//                   inputboxes and combobox callbacks
// ------------------------------------------------------------------------------------------

gint inputbox_ok(gpointer object, gpointer data)
{
    fprintf(Xdialog.output, "%s",
        gtk_entry_get_text(GTK_ENTRY(Xdialog.widget1)));
    if (Xdialog.widget2 != NULL)
        fprintf(Xdialog.output, "%s%s", Xdialog.separator,
            gtk_entry_get_text(GTK_ENTRY(Xdialog.widget2)));
    if (Xdialog.widget3 != NULL)
        fprintf(Xdialog.output, "%s%s", Xdialog.separator,
            gtk_entry_get_text(GTK_ENTRY(Xdialog.widget3)));
    fprintf(Xdialog.output, "\n");
        
    return TRUE;
}


gboolean inputbox_timeout(gpointer data)
{
    return inputbox_ok(NULL, NULL);
}


gint input_keypress(GtkWidget *entry, GdkEventKey *event, gpointer data)
{
    if (event->type == GDK_KEY_PRESS
        && (event->keyval == GDK_KEY(Return) || event->keyval == GDK_KEY(KP_Enter))) {
        if (Xdialog.default_no) {
            Xdialog.exit_code = 1;
        } else {
            inputbox_ok(NULL, NULL);
            Xdialog.exit_code = 0;
        }
        if (Xdialog.check) {
            if (Xdialog.checked)
                fprintf(Xdialog.output, "checked\n");
            else
                fprintf(Xdialog.output, "unchecked\n");
        }
        gtk_widget_destroy(Xdialog.window);
        return FALSE;
    }
    return TRUE;
}


gboolean hide_passwords(GtkWidget *button, gpointer data)
{
    gint entries;
    gboolean visible;

    entries = 1 + (Xdialog.widget2 != NULL ? 1 : 0) + (Xdialog.widget3 != NULL ? 1 : 0);

    visible = (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)) == FALSE);

    if ((Xdialog.passwd < 10 && Xdialog.passwd >= entries) || Xdialog.passwd == 11)
        gtk_entry_set_visibility(GTK_ENTRY(Xdialog.widget1), visible);
    if (entries > 1 && ((Xdialog.passwd < 10 && Xdialog.passwd >= entries-1) || Xdialog.passwd == 12))
        gtk_entry_set_visibility(GTK_ENTRY(Xdialog.widget2), visible);
    if (entries > 2 && ((Xdialog.passwd < 10 && Xdialog.passwd > 0) || Xdialog.passwd == 13))
        gtk_entry_set_visibility(GTK_ENTRY(Xdialog.widget3), visible);

    return TRUE;
}


// ------------------------------------------------------------------------------------------
//                             editbox callback
// ------------------------------------------------------------------------------------------

gboolean editbox_ok(gpointer object, gpointer data)
{
#if GTK_CHECK_VERSION(2,0,0)
    GtkTextIter start_iter, end_iter;
    GtkTextBuffer *text_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(Xdialog.widget1));
    gtk_text_buffer_get_bounds(text_buffer, &start_iter, &end_iter);
    fputs (gtk_text_buffer_get_text(text_buffer, &start_iter, &end_iter, FALSE), Xdialog.output);
#else // -- GTK1 --
    int length, i;
    length = gtk_text_get_length(GTK_TEXT(Xdialog.widget1));
    for (i = 0; i < length; i++)
        fputc (GTK_TEXT_INDEX(GTK_TEXT(Xdialog.widget1), i),
               Xdialog.output);
#endif
    return TRUE;
}

/* The print button callback (used by editbox, textbox and tailbox) */

gboolean print_text(gpointer object, gpointer data)
{
    int length;
    char cmd[MAX_PRTCMD_LENGTH];
    FILE * temp;
    char *buffer;
    
    strncpy(cmd, PRINTER_CMD, sizeof(cmd));
    if (strlen(Xdialog.printer) != 0) {
        strncat(cmd, " "PRINTER_CMD_OPTION, sizeof(cmd));
        strncat(cmd, Xdialog.printer, sizeof(cmd));
    }
#if GTK_CHECK_VERSION(2,0,0)
    GtkTextIter start_iter;
    GtkTextIter end_iter;
    GtkTextBuffer *text_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(Xdialog.widget1));

    gtk_text_buffer_get_bounds (text_buffer, &start_iter, &end_iter);

    length = gtk_text_buffer_get_char_count (text_buffer);
    buffer = gtk_text_buffer_get_text (text_buffer, &start_iter, &end_iter, FALSE);
#else
    int i;
    length = gtk_text_get_length (GTK_TEXT(Xdialog.widget1));
    buffer = g_malloc ((length+1)*sizeof(gchar));
    for (i = 0; i < length; i++)
    {
        buffer[i] = GTK_TEXT_INDEX(GTK_TEXT(Xdialog.widget1), i);
    }
#endif
    temp = popen(cmd, "w");
    if (temp != NULL) {
        fwrite (buffer, sizeof(gchar), length, temp);
        pclose (temp);
    }
    g_free(buffer);

    return TRUE;
}


// ------------------------------------------------------------------------------------------
//                           rangebox callbacks
// ------------------------------------------------------------------------------------------

gboolean rangebox_exit(GtkButton *button, gpointer data)
{
    GtkAdjustment *adj;
    gdouble value;

    adj = GTK_ADJUSTMENT(Xdialog.widget1);
    value = gtk_adjustment_get_value (adj);
    fprintf(Xdialog.output, "%d", (gint) value);
    if (Xdialog.widget2 != NULL) {
        adj = GTK_ADJUSTMENT(Xdialog.widget2);
        fprintf(Xdialog.output, "%s%d", Xdialog.separator,
        (gint) value);
    }
    if (Xdialog.widget3 != NULL) {
        adj = GTK_ADJUSTMENT(Xdialog.widget3);
        fprintf(Xdialog.output, "%s%d", Xdialog.separator,
            (gint) value);
    }
    fprintf(Xdialog.output, "\n");

    return TRUE;
}


gboolean rangebox_timeout(gpointer data)
{
    return rangebox_exit(NULL, NULL);
}


// ------------------------------------------------------------------------------------------
//                          spin boxes callbacks
// ------------------------------------------------------------------------------------------

gboolean spinbox_exit(GtkButton *button, gpointer data)
{
    fprintf(Xdialog.output, "%d",
        gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(Xdialog.widget1)));

    if (Xdialog.widget2 != NULL)
        fprintf(Xdialog.output, "%s%d", Xdialog.separator,
            gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(Xdialog.widget2)));

    if (Xdialog.widget3 != NULL)
        fprintf(Xdialog.output, "%s%d", Xdialog.separator,
            gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(Xdialog.widget3)));

    fprintf(Xdialog.output, "\n");

    return TRUE;
}

gboolean spinbox_timeout(gpointer data)
{
    return spinbox_exit(NULL, NULL);
}


// ------------------------------------------------------------------------------------------
//                     radiolist and checklist callbacks
// ------------------------------------------------------------------------------------------

void item_toggle(GtkWidget *item, int i)
{
    gboolean active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (item));
    if (active) {
        Xdialog.array[i].state = 1;
    } else
        Xdialog.array[i].state = 0;
}

gboolean print_items(GtkButton *button, gpointer data)
{
    int i;
    gboolean flag = FALSE;

    for (i = 0 ; Xdialog.array[i].state != -1 ; i++) {
        if (Xdialog.array[i].state) {
            if (flag)
                fprintf(Xdialog.output, "%s", Xdialog.separator);
            fprintf(Xdialog.output, "%s", Xdialog.array[i].tag);
            flag = TRUE;
        }
    }
    if (flag)
        fprintf(Xdialog.output, "\n");

    return TRUE;
}

gboolean itemlist_timeout(gpointer data)
{
    return print_items(NULL, NULL);
}


// ------------------------------------------------------------------------------------------
//                           menubox callbacks
// ------------------------------------------------------------------------------------------

static void menubox_print_selected (GtkWidget *list)
{
#if GTK_CHECK_VERSION(2,0,0)
    GtkTreeSelection *selection;
    GtkTreeModel *model;
    GtkTreeIter iter;
    gchar *tag;
    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW(list));
    gtk_tree_selection_get_selected (selection, &model, &iter);
    gtk_tree_model_get (model, &iter, 0, &tag, -1);
    if (tag) {
        fprintf(Xdialog.output, "%s\n", tag);
        g_free(tag);
    }

#else // -- GTK1 --
    GList *selection = GTK_CLIST(list)->selection;
    int      sel_row = GPOINTER_TO_INT(selection->data);
# if 0
    // -- GTK1 --.2 this segfaults if there are less then 6 rows !!?
    char *tag;
    gtk_clist_get_text (GTK_CLIST(list), sel_row, 0, &tag);
    if (tag) {
        fprintf(Xdialog.output, "%s\n", tag);
        g_free(tag);
    }
# else
    listname *rowdata;
    rowdata = (listname*) gtk_clist_get_row_data (GTK_CLIST(list), sel_row);
    if (rowdata) {
        fprintf(Xdialog.output, "%s\n", rowdata->tag);
    }
# endif
#endif
}

void on_menubox_ok_click (GtkButton *button, gpointer data)
{
    menubox_print_selected (GTK_WIDGET(data));
    exit_ok (NULL, NULL);
}


#if GTK_CHECK_VERSION(2,0,0)

void on_menubox_treeview_row_activated_cb (GtkTreeView *tree_view, GtkTreePath *path,
                                           GtkTreeViewColumn *column, gpointer data)
{
    menubox_print_selected (GTK_WIDGET(tree_view));
    exit_ok (NULL, NULL);
}

void on_menubox_tip_treeview_changed (GtkTreeSelection *selection, gpointer data)
{
    if (Xdialog.tips != 1) {
        return;
    }
    GtkTreeModel *model;
    GtkTreeIter iter;
    listname *rowdata;
    gtk_tree_selection_get_selected (selection, &model, &iter);
    gtk_tree_model_get (model, &iter, 2, &rowdata, -1);
    gtk_statusbar_pop  (GTK_STATUSBAR(data), Xdialog.status_id);
    gtk_statusbar_push (GTK_STATUSBAR(data), Xdialog.status_id, rowdata->tips);
}

#else // GTK 1

void on_menubox_item_select (GtkWidget *clist, gint row, gint column,
                             GdkEventButton *event, gpointer data)
{
    // If the tag is empty, then this is an unavailable item:
    // select back the last selected row and exit.
    if (strlen(Xdialog.array[row].tag) == 0) {
        gtk_clist_select_row(GTK_CLIST(clist), Xdialog.array[0].state, 0);
        return;
    }
    // remember which row was last selected
    // (store it in the first element state of the array)
    Xdialog.array[0].state = row;

    if (Xdialog.tips == 1)
    {
        gtk_statusbar_pop  (GTK_STATUSBAR(Xdialog.widget1), Xdialog.status_id);
        gtk_statusbar_push (GTK_STATUSBAR(Xdialog.widget1), Xdialog.status_id,
                            Xdialog.array[row].tips);
    }
}

gboolean move_to_row_timeout(gpointer data)
{
    gtk_clist_moveto (GTK_CLIST(data),
                      Xdialog.array[0].state, 0, 0.5, 0.0);
    return FALSE; /* run only once */
}

#endif


// ------------------------------------------------------------------------------------------
//                           treeview callbacks
// ------------------------------------------------------------------------------------------

void print_tree_selection (GtkButton *button, gpointer data)
{
    listname *rowdata;

#if GTK_CHECK_VERSION(2,0,0)
    GtkTreeView *treeview = GTK_TREE_VIEW(Xdialog.widget1);
    GtkTreeModel *model   = gtk_tree_view_get_model (treeview);
    GtkTreeSelection *sel = gtk_tree_view_get_selection (treeview);
    GtkTreeIter tree_iter;
    if (gtk_tree_selection_get_selected (sel, &model, &tree_iter)) {
        gtk_tree_model_get (model, &tree_iter, 1, &rowdata, -1);
    }
#else // -- GTK1 --
    GtkTree *tree = GTK_TREE(Xdialog.widget1);
    GList *list   = tree->selection;
    // list->data = GtkTreeItem
    rowdata = (listname*) g_object_get_data (G_OBJECT(list->data), "listitem");
#endif

    g_return_if_fail (rowdata != NULL);
    fprintf(Xdialog.output, "%s\n", rowdata->tag);
}


void tree_selection_changed (GtkWidget *tree)
{
#if 0
    // this is for debugging purposes, and to see how
    // the handle the changed / selection_changed signal...
    listname *rowdata;
#if GTK_CHECK_VERSION(2,0,0)
    GtkTreeSelection *sel = GTK_TREE_SELECTION(tree);
    GtkTreeModel *model;
    GtkTreeIter tree_iter;
    if (gtk_tree_selection_get_selected (sel, &model, &tree_iter)) {
        gtk_tree_model_get (model, &tree_iter, 1, &rowdata, -1);
    }
#else // -- GTK1 --
    GList *list   = GTK_TREE(tree)->selection;
    // list->data = GtkTreeItem
    rowdata = (listname*) g_object_get_data (G_OBJECT(list->data), "listitem");
#endif
    if (rowdata) {
        fprintf (stderr, "%s\n", rowdata->name);
    }
#endif
}


// ------------------------------------------------------------------------------------------
//                           buildlist callbacks
// ------------------------------------------------------------------------------------------

void buildlist_sensitive_buttons (void)
{
    // this is called after each glist update so to
    // set the proper (in)sensitive status onto the Add/Remove buttons.
    gboolean enabled1, enabled2;
#if GTK_CHECK_VERSION(2,0,0)
    GtkTreeModel *model1, *model2;
    GtkTreeIter iter1, iter2;
    model1 = gtk_tree_view_get_model (GTK_TREE_VIEW(Xdialog.widget1));
    model2 = gtk_tree_view_get_model (GTK_TREE_VIEW(Xdialog.widget2));

    enabled1 = gtk_tree_model_get_iter_first (model1, &iter1);
    enabled2 = gtk_tree_model_get_iter_first (model2, &iter2);

    gtk_widget_set_sensitive (Xdialog.widget3, enabled1);
    gtk_widget_set_sensitive (Xdialog.widget4, enabled2);
#else // -- GTK1 --
    enabled1 = g_list_length(GTK_LIST(Xdialog.widget1)->children) != 0;
    enabled2 = g_list_length(GTK_LIST(Xdialog.widget2)->children) != 0;
    gtk_widget_set_sensitive (Xdialog.widget3, enabled1);
    gtk_widget_set_sensitive (Xdialog.widget4, enabled2);
#endif
}


void buildlist_add_or_remove (GtkButton *button, gpointer data)
{
    intptr_t remove = GPOINTER_TO_INT(data);
    GtkWidget *listsrc;
    GtkWidget *listdest;
    if (remove) {
        listsrc  = Xdialog.widget2;
        listdest = Xdialog.widget1;
    } else {
        listsrc  = Xdialog.widget1;
        listdest = Xdialog.widget2;
    }
#if GTK_CHECK_VERSION(2,0,0)
    GtkTreeIter itersrc, iterdest;
    GtkTreeModel *modelsrc, *modeldest;
    GtkTreeSelection* selection;
    listname *rowdata;

    modelsrc  = gtk_tree_view_get_model (GTK_TREE_VIEW(listsrc));
    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW(listsrc));
    modeldest = gtk_tree_view_get_model (GTK_TREE_VIEW(listdest));

    if (gtk_tree_selection_get_selected(selection, &modelsrc, &itersrc))
    {
        gtk_tree_model_get (modelsrc, &itersrc,
                            1, &rowdata, -1);
        gtk_list_store_remove (GTK_LIST_STORE(modelsrc), &itersrc);
        //--
        gtk_list_store_append (GTK_LIST_STORE(modeldest), &iterdest);
        gtk_list_store_set (GTK_LIST_STORE(modeldest), &iterdest,
                            0, rowdata->name,
                            1, rowdata,
                            -1);
    }
#else // -- GTK1 --
    GList *selected;
    selected = g_list_copy (GTK_LIST(listsrc)->selection);
    gtk_list_remove_items_no_unref (GTK_LIST(listsrc), selected);
    gtk_list_append_items (GTK_LIST(listdest), selected);
#endif
    buildlist_sensitive_buttons(); 
}


void buildlist_print_list (GtkButton *button, gpointer data)
{
    gboolean flag = FALSE;
    listname *rowdata;
#if GTK_CHECK_VERSION(2,0,0)
    GtkTreeModel *model;
    GtkTreeIter iter;
    gboolean valid;
    model = gtk_tree_view_get_model (GTK_TREE_VIEW(Xdialog.widget2));
    valid = gtk_tree_model_get_iter_first(model, &iter);
    while (valid)
    {
        gtk_tree_model_get (model, &iter,
                            1, &rowdata, -1);
        if (flag) {
            fprintf(Xdialog.output, "%s", Xdialog.separator);
        }
        fprintf (Xdialog.output, "%s", rowdata->tag);
        flag = TRUE;
        valid = gtk_tree_model_iter_next (model, &iter);
    }
#else // -- GTK1 --
    GList *children = GTK_LIST(Xdialog.widget2)->children;
    while (children)
    { // children->data = GtkListItem
        rowdata = (listname*) g_object_get_data (G_OBJECT(children->data), "listitem");
        if (flag) {
            fprintf(Xdialog.output, "%s", Xdialog.separator);
        }
        fprintf(Xdialog.output, "%s", rowdata->tag);
        flag = TRUE;
        children = g_list_next (children);
    }
#endif
    if (flag) {
        fprintf(Xdialog.output, "\n");
    }
}

/* fselect callback */
gboolean filesel_exit(GtkWidget *filesel, gpointer client_data)
{
#if GTK_CHECK_VERSION(2,4,0)
    char *filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (client_data));
    if (filename) {
        fprintf(Xdialog.output, "%s\n", filename);
        g_free (filename);
    }
#else
    fprintf (Xdialog.output, "%s\n",
             gtk_file_selection_get_filename (GTK_FILE_SELECTION(client_data)));
#endif
    return exit_ok(NULL, NULL);
}


/* colorsel callback */
gboolean colorsel_exit(GtkWidget *colorsel, gpointer client_data)
{
#if GTK_CHECK_VERSION(3,4,0)
    GdkRGBA color;
    gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER(client_data), &color);
    fprintf (Xdialog.output, "%d %d %d\n",
             (int) (color.red   * 256),
             (int) (color.green * 256),
             (int) (color.blue  * 256));
#elif GTK_CHECK_VERSION(2,0,0)
    GdkColor color;
    gtk_color_selection_get_current_color (GTK_COLOR_SELECTION(client_data),
                                           &color);
    fprintf (Xdialog.output, "%d %d %d\n",
             color.red   / 256,
             color.green / 256,
             color.blue  / 256);
#else // -- GTK1 --
    double color[4];
    gtk_color_selection_get_color (GTK_COLOR_SELECTION(client_data), color);
    fprintf (Xdialog.output, "%d %d %d\n",
             (int) (color[0] * 256),
             (int) (color[1] * 256),
             (int) (color[2] * 256));
#endif
    return exit_ok (NULL, NULL);
}


/* fontsel callback */

gboolean fontsel_exit(GtkWidget *fontsel, gpointer client_data)
{
    char * font = NULL;
#if GTK_CHECK_VERSION(3,2,0)
    font = gtk_font_chooser_get_font (GTK_FONT_CHOOSER(client_data));
#else
    font = gtk_font_selection_dialog_get_font_name (GTK_FONT_SELECTION_DIALOG(client_data));
#endif
    if (font) {
        fprintf(Xdialog.output, "%s\n", font);
        g_free (font);
    }
    return exit_ok(NULL, NULL);
}

/* calendar callbacks */

gboolean calendar_exit(gpointer object, gpointer data)
{
    guint day, month, year;
    gtk_calendar_get_date(GTK_CALENDAR(Xdialog.widget1), &year, &month, &day);
    fprintf(Xdialog.output, "%02d/%02d/%d\n", day, month+1, year);

    return TRUE;
}

gboolean calendar_timeout(gpointer data)
{
    return calendar_exit(NULL, NULL);
}

/* timebox callbacks */

gboolean timebox_exit(gpointer object, gpointer data)
{
    fprintf (Xdialog.output, "%02d:%02d:%02d\n",
            gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(Xdialog.widget1)),
            gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(Xdialog.widget2)),
            gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(Xdialog.widget3)));
    return TRUE;
}

gboolean timebox_timeout(gpointer data)
{
    return timebox_exit(NULL, NULL);
}
