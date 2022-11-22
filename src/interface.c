/*
 * GTK+ interface functions for Xdialog.
 */

#include "common.h"

#include <sys/stat.h>

#include "interface.h"
#include "callbacks.h"
#include "support.h"

/* Global structure and variables */
extern Xdialog_data Xdialog;
extern gboolean dialog_compat;

/* Fixed font loading and character size (in pixels) initialisation */
#if GTK_CHECK_VERSION(2,0,0)
static PangoFontDescription *fixed_pango_font;
#endif

static gint xmult = XSIZE_MULT;
static gint ymult = YSIZE_MULT;
static gint ffxmult = XSIZE_MULT;
static gint ffymult = YSIZE_MULT;

/* Parsing of the GTK+ rc file (if any) */

static void parse_rc_file(void)
{
    if (strlen(Xdialog.rc_file) != 0)
        gtk_rc_parse(Xdialog.rc_file);
    if (dialog_compat)
        gtk_rc_parse_string(FIXED_FONT_RC_STRING);
}

/* font_init() is used for two purposes: load the fixed font that Xdialog may
 * use, and calculate the character size in pixels (both for the fixed font
 * and for the font currently in use: the later one may be a proportionnal
 * font and the character width is therefore an averaged value).
 */

static void font_init(void)
{
#if GTK_CHECK_VERSION(2,0,0)
    fixed_pango_font = pango_font_description_new ();
    pango_font_description_set_family (fixed_pango_font, FIXED_FONT);
    pango_font_description_set_weight (fixed_pango_font, PANGO_WEIGHT_MEDIUM);
    pango_font_description_set_size (fixed_pango_font, 10 * PANGO_SCALE);
#endif
#if GTK_MAJOR_VERSION <= 2
    GtkWidget *window;
    GtkStyle  *style;
    GdkFont *font;
    gint width, ascent, descent, lbearing, rbearing;
    if (dialog_compat) {
        xmult = ffxmult;
        ymult = ffymult;
    } else {
        /* We must open and realize a window IOT get the GTK+ theme font... */
        parse_rc_file();
        window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_widget_realize(window);
        style = window->style;
        if (style != NULL) {
            /* For proportionnal fonts, we use the average character width... */
            font = gtk_style_get_font(style);
            gdk_string_extents (font, ALPHANUM_CHARS, &lbearing,
                                &rbearing, &width, &ascent, &descent);
            xmult = width / 62;		/* 62 = strlen(ALPHANUM_CHARS) */
            ymult = ascent + descent + 2;	/*  2 = spacing pixel lines */
        }
        gtk_widget_destroy(window);
    }
#else
    xmult = ffxmult;
    ymult = ffymult;
#endif
}

void get_maxsize(int *x, int *y)
{
    font_init();
    *x = gdk_screen_width()/xmult-2;
    *y = gdk_screen_height()/ymult-2;
}


/* Custom text wrapping (the GTK+ one is buggy) */

static void wrap_text(gchar *str, gint reserved_width)
{
    gint max_line_width, n = 0;
    gchar *p = str, *last_space = NULL;
    gchar tmp[MAX_LABEL_LENGTH];
#if GTK_MAJOR_VERSION <= 2
    GdkFont *current_font = gtk_style_get_font(Xdialog.window->style);
#endif

    if (Xdialog.xsize != 0) {
        max_line_width = (Xdialog.size_in_pixels ? Xdialog.xsize :
                                                   Xdialog.xsize * xmult)
                          - reserved_width - 4 * xmult;
    } else {
        max_line_width =  gdk_screen_width() - reserved_width - 6 * xmult;
    }

    do
    {
        if (*p == '\n') {
            n = 0;
            last_space = NULL;
        } else {
            tmp[n++] = *p;
            tmp[n] = 0;
#if GTK_MAJOR_VERSION <= 2
            if (gdk_string_width(current_font, tmp) < max_line_width) {
                if (*p == ' ')
                    last_space = p;
            } else {
#endif
                if (last_space != NULL) {
                    *last_space = '\n';
                    p = last_space;
                    n = 0;
                    last_space = NULL;
                } else if (*p == ' ')
                    last_space = p;
#if GTK_MAJOR_VERSION <= 2
            }
#endif
        }
    } while (++p < str + strlen(str));
}

/* Some useful functions to setup GTK menus... */

static void set_window_size_and_placement(void)
{
    if (Xdialog.xsize != 0 && Xdialog.ysize != 0) {
        if (Xdialog.size_in_pixels)
            gtk_window_set_default_size(GTK_WINDOW(Xdialog.window),
                                        Xdialog.xsize,
                                        Xdialog.ysize);
        else
            gtk_window_set_default_size(GTK_WINDOW(Xdialog.window),
                                        Xdialog.xsize*xmult,
                                        Xdialog.ysize*ymult);
    }

#if GTK_MAJOR_VERSION <= 2
    /* Allow the window to grow, shrink and auto-shrink */
    gtk_window_set_policy (GTK_WINDOW(Xdialog.window), TRUE, TRUE, TRUE);
#endif

    /* Set the window placement policy */
    if (Xdialog.set_origin)
        gdk_window_move (gtk_widget_get_window (Xdialog.window),
                         Xdialog.xorg >= 0 ? (Xdialog.size_in_pixels ? Xdialog.xorg : Xdialog.xorg*xmult) :
                            gdk_screen_width()  + Xdialog.xorg - Xdialog.xsize - 2*xmult,
                         Xdialog.yorg >= 0 ? (Xdialog.size_in_pixels ? Xdialog.yorg : Xdialog.yorg*ymult) :
                            gdk_screen_height() + Xdialog.yorg - Xdialog.ysize - 3*ymult);
    else
        gtk_window_set_position (GTK_WINDOW(Xdialog.window), Xdialog.placement);
}


static void open_window(void)
{
    GtkWidget *window;
    GtkWidget *vbox;

    font_init();

    /* Open a new GTK top-level window */
    window = Xdialog.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

    /* Apply the custom GTK+ theme, if any. */
    parse_rc_file();

    if (Xdialog.wmclass[0] != 0)
        gtk_window_set_role (GTK_WINDOW(window), Xdialog.wmclass);

    /* Set default events handlers */
    g_signal_connect (G_OBJECT(window), "destroy",
                      G_CALLBACK(destroy_event), NULL);
    g_signal_connect (G_OBJECT(window), "delete_event",
                      G_CALLBACK(delete_event), NULL);

    /* Set the window title */
    gtk_window_set_title(GTK_WINDOW(window), Xdialog.title);

    /* Set the internal border so that the child widgets do not
     * expand to the whole window (prettier) */
    gtk_container_set_border_width (GTK_CONTAINER (window), 7);

    /* main vbox */
    vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(window), vbox);
    gtk_container_set_border_width (GTK_CONTAINER (vbox), 0);
    Xdialog.vbox = GTK_BOX(vbox);

    gtk_widget_realize(Xdialog.window);

    /* Set the window size and placement policy */
    set_window_size_and_placement();

    if (Xdialog.beep & BEEP_BEFORE && Xdialog.exit_code != 2) {
        gdk_beep();
    }
    Xdialog.exit_code = 255;
}


static GtkWidget *set_separator(gboolean from_start)
{
    GtkWidget *separator;
    separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
    if (from_start) {
        gtk_box_pack_start (Xdialog.vbox, separator, FALSE, TRUE, ymult/3);
    } else {
        gtk_box_pack_end (Xdialog.vbox, separator, FALSE, TRUE, ymult/3);
    }
    return separator;
}


static void set_backtitle(gboolean sep_flag)
{
    GtkWidget *label;
    GtkWidget *hbox;
    GtkWidget *separator;
    gchar     backtitle[MAX_BACKTITLE_LENGTH];

    if (strlen(Xdialog.backtitle) == 0) {
        return;
    }
    if (dialog_compat)
        backslash_n_to_linefeed(Xdialog.backtitle, backtitle, MAX_BACKTITLE_LENGTH);
    else
        trim_string(Xdialog.backtitle, backtitle, MAX_BACKTITLE_LENGTH);

    if (Xdialog.wrap || dialog_compat) {
        wrap_text(backtitle, 2*ymult/3);
    }
    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start (Xdialog.vbox, hbox, FALSE, FALSE, ymult/3);
    gtk_box_reorder_child(Xdialog.vbox, hbox, 0);
    label = gtk_label_new(backtitle);
    gtk_container_add(GTK_CONTAINER(hbox), label);
    if (sep_flag) {
        separator = set_separator(TRUE);
        gtk_box_reorder_child(Xdialog.vbox, separator, 1);
    }
}


static GtkWidget *set_label(gchar *label_text, gboolean expand)
{
    GtkWidget *label;
    GtkWidget *hbox;
#if GTK_CHECK_VERSION(2,0,0)
    GdkPixbuf *pixbuf;
    GtkWidget *icon;
#endif
    gchar     text[MAX_LABEL_LENGTH];
    int icon_width = 0;

    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start (Xdialog.vbox, hbox, expand, TRUE, ymult/3);

#if GTK_CHECK_VERSION(2,0,0)
    if (Xdialog.icon) {
        pixbuf = gdk_pixbuf_new_from_file (Xdialog.icon_file, NULL);
        if (pixbuf != NULL) {
            icon = gtk_image_new_from_pixbuf (pixbuf);
            g_object_unref(pixbuf);
            gtk_box_pack_start (GTK_BOX(hbox), icon, FALSE, FALSE, 2);
            GtkRequisition requisition;
            gtk_widget_get_requisition (icon, &requisition);
            icon_width = requisition.width + 4;
        }
    }
#endif
    trim_string(label_text, text, MAX_LABEL_LENGTH);

    if (Xdialog.wrap || dialog_compat) {
        wrap_text(text, icon_width + 2*ymult/3);
    }
    label = gtk_label_new(text);

    if (Xdialog.justify == GTK_JUSTIFY_FILL) {
        gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
    }
    gtk_label_set_justify(GTK_LABEL(label), Xdialog.justify);

    gtk_box_pack_start (GTK_BOX(hbox), label, TRUE, TRUE, ymult/3);

    return label;
}


static GtkWidget *set_secondary_label(gchar *label_text, gboolean expand)
{
    GtkWidget *label;
    gchar     text[MAX_LABEL_LENGTH];

    trim_string(label_text, text, MAX_LABEL_LENGTH);
    if (Xdialog.wrap || dialog_compat)
        wrap_text(text, 2*ymult/3);

    label = gtk_label_new(text);

    if (Xdialog.justify == GTK_JUSTIFY_FILL) {
        gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
    }
    gtk_label_set_justify(GTK_LABEL(label), Xdialog.justify);
    switch (Xdialog.justify) {
        case GTK_JUSTIFY_LEFT:
            gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
            break;
        case GTK_JUSTIFY_RIGHT:
            gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
            break;
    }

    gtk_box_pack_start (Xdialog.vbox, label, expand, TRUE, ymult/3);

    return label;
}


static GtkWidget *set_hbuttonbox(void)
{
    GtkWidget *hbuttonbox;
    hbuttonbox = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_end (Xdialog.vbox, hbuttonbox, FALSE, FALSE, ymult/4);
    gtk_button_box_set_layout (GTK_BUTTON_BOX(hbuttonbox), GTK_BUTTONBOX_END);
    gtk_box_set_spacing (GTK_BOX (hbuttonbox), 7);
    return hbuttonbox;
}

static GtkWidget *set_button(gchar *default_text,
                             gpointer buttonbox,
                             gint event,
                             gboolean grab_default)
{
    GtkWidget *button;
    gchar     *text = default_text;
#if GTK_CHECK_VERSION(2,0,0)
    GtkWidget *icon;
    gchar	  *stock_id = NULL;

    if (Xdialog.buttons_style != TEXT_ONLY)
    {
        if (!strcmp(text, OK) || !strcmp(text, YES))
            stock_id = "gtk-ok";
        else if (!strcmp(text, CANCEL) || !strcmp(text, NO))
            stock_id = "gtk-close";
        else if (!strcmp(text, HELP))
            stock_id = "gtk-help";
        else if (!strcmp(text, PRINT))
            stock_id = "gtk-print";
        else if (!strcmp(text, NEXT) || !strcmp(text, ADD))
            stock_id = "gtk-go-forward";
        else if (!strcmp(text, PREVIOUS) || !strcmp(text, REMOVE))
            stock_id = "gtk-go-back";
    }
#endif

    switch (event) {
        case 0: // ok
            if (*Xdialog.ok_label) {
                text = Xdialog.ok_label;
            }
            break;
        case 1: // cancel
            if (*Xdialog.cancel_label) {
                text = Xdialog.cancel_label;
            }
            break;
        case 2: // help
            break;
        case 3: // previous
            break;
        case 4: // print
            break;
        case 5: // extra
            if (*Xdialog.extra_label) {
                text = Xdialog.extra_label;
            }
            break;
    }

    button = gtk_button_new_with_mnemonic (text);
    if (Xdialog.buttons_style != TEXT_ONLY) {
        /* buttons with icons */
#if GTK_CHECK_VERSION(2,0,0)
        icon = gtk_image_new_from_stock(stock_id, GTK_ICON_SIZE_BUTTON);
        gtk_button_set_image (GTK_BUTTON (button), icon);
        if (Xdialog.buttons_style == ICON_ONLY) {
            gtk_button_set_label (GTK_BUTTON (button), NULL);
        }
#endif
    }
    gtk_container_add(GTK_CONTAINER(buttonbox), button);

    switch (event) {
        case 0: // ok
            g_signal_connect_after (G_OBJECT(button), "clicked",
                                    G_CALLBACK(exit_ok), NULL);
            break;
        case 1: // cancel
            g_signal_connect_after (G_OBJECT(button), "clicked",
                                    G_CALLBACK(exit_cancel), NULL);
            g_signal_connect_after (G_OBJECT(Xdialog.window), "key_press_event",
                                    G_CALLBACK(exit_keypress), NULL);
            break;
        case 2: // help
            g_signal_connect_after (G_OBJECT(button), "clicked",
                                    G_CALLBACK(exit_help), NULL);
            break;
		case 3: // previous
            g_signal_connect_after (G_OBJECT(button), "clicked",
                                    G_CALLBACK(exit_previous), NULL);
            break;
        case 4: // print
            g_signal_connect_after (G_OBJECT(button), "clicked",
                                    G_CALLBACK(print_text), NULL);
            break;
        case 5: // extra
            g_signal_connect_after (G_OBJECT(button), "clicked",
                                    G_CALLBACK(exit_extra), NULL);
            break;
    }

    if (grab_default) {
        gtk_widget_set_can_default(button, TRUE);
        gtk_widget_grab_default(button);
    }

    return button;
}


static void set_check_button(GtkWidget *box)
{
    GtkWidget *button;
    GtkWidget *hbox;
    gchar     check_label[MAX_LABEL_LENGTH];

    if (!Xdialog.check) {
        return;
    }
    trim_string(Xdialog.check_label, check_label, MAX_LABEL_LENGTH);

    if (box == NULL) {
        set_separator(FALSE);
        hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
        gtk_box_pack_end (Xdialog.vbox, hbox, FALSE, FALSE, 0);
        set_separator(FALSE);
    } else {
        hbox = box;
    }
    button = gtk_check_button_new_with_mnemonic (check_label);
    gtk_container_add(GTK_CONTAINER(hbox), button);

    if (Xdialog.checked) {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);
    }
    g_signal_connect (G_OBJECT(button), "toggled",
                      G_CALLBACK(checked), NULL);
}


static GtkWidget *set_all_buttons(gboolean print, gboolean ok)
{
    GtkWidget *hbuttonbox; 
    GtkWidget *button_ok = NULL;

    hbuttonbox = set_hbuttonbox();
    set_check_button(NULL);

    if (Xdialog.wizard) {
        set_button(PREVIOUS , hbuttonbox, 3, FALSE);
    } else {
        if(Xdialog.extra_button) {
            button_ok = set_button(EXTRA, hbuttonbox, 5, !Xdialog.default_no);
        }
        if (ok) {
            button_ok = set_button(OK, hbuttonbox, 0, !Xdialog.default_no);
        }
    }
    if (Xdialog.cancel_button)
        set_button(CANCEL , hbuttonbox, 1, Xdialog.default_no && !Xdialog.wizard);
    if (Xdialog.wizard)
        button_ok = set_button(NEXT, hbuttonbox, 0, TRUE);
    if (Xdialog.help)
        set_button(HELP, hbuttonbox, 2, FALSE);
    if (print && Xdialog.print)
        set_button(PRINT, hbuttonbox, 4, FALSE);

    return button_ok;
}


static GtkWidget *set_scrollable_text(void)
{
    GtkWidget *text;
#if GTK_CHECK_VERSION(2,0,0)
    GtkWidget *scrollwin;

    scrollwin = gtk_scrolled_window_new(NULL, NULL);
    gtk_box_pack_start (GTK_BOX(Xdialog.vbox), scrollwin, TRUE, TRUE, 0);

    text = gtk_text_view_new();

    if (Xdialog.fixed_font) {
        gtk_widget_override_font(text, fixed_pango_font);
    }

    gtk_container_add(GTK_CONTAINER (scrollwin), text);
#else // -- GTK1 --
    GtkWidget *hbox;
    GtkWidget *vscrollbar;
//    GtkStyle  *style;

    hbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start (Xdialog.vbox, hbox, TRUE, TRUE, xmult/2);
    gtk_widget_show(hbox);
  
    text = gtk_text_new(NULL, NULL);
    gtk_box_pack_start (GTK_BOX(hbox), text, TRUE, TRUE, 0);
//  if (Xdialog.fixed_font) {
//      style = gtk_style_new();
//      gdk_font_unref(style->font);
//      style->font = fixed_font;
//      gtk_widget_push_style(style);     
//      gtk_widget_set_style(text, style);
//      gtk_widget_pop_style();
//  }
    gtk_widget_show(text);
    /* Add a vertical scrollbar to the GtkText widget */
    vscrollbar = gtk_vscrollbar_new(GTK_TEXT(text)->vadj);
    gtk_box_pack_start (GTK_BOX(hbox), vscrollbar, FALSE, FALSE, 0);
    gtk_widget_show(vscrollbar);
#endif
    return text;
}


static GtkWidget *set_scrolled_window (GtkBox *box, gint border_width, gint xsize,
                                       gint list_size, gint spacing)
{
    GtkWidget *scrolled_window;
    scrolled_window = gtk_scrolled_window_new (NULL, NULL);
    gtk_container_set_border_width (GTK_CONTAINER(scrolled_window), border_width);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW(scrolled_window),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start (box, scrolled_window, TRUE, TRUE, 0);
    if (Xdialog.list_height > 0) {
        gtk_widget_set_size_request (scrolled_window, xsize > 0 ? xsize*xmult : -1,
                                     Xdialog.list_height * (ymult + spacing));
    } else {
        gtk_widget_set_size_request (scrolled_window, xsize > 0 ? xsize*xmult : -1,
                                     MIN(gdk_screen_height() - 40 * ymult,
                                     list_size * (ymult + spacing)));
    }
    return scrolled_window;
}


static GtkWidget *set_scrolled_list (GtkWidget *box, gint xsize, gint list_size,
                                     gint spacing, GList *glitems)
{
    GtkWidget *list_w;
    GList *igl;
    listname *listitem;
    GtkWidget *scrolled_window;
    scrolled_window = set_scrolled_window (GTK_BOX(box), 0, xsize, list_size, spacing);
#if GTK_CHECK_VERSION(2,0,0)
    // liststore
    GtkListStore *store;
    GtkTreeIter iter;
    store = gtk_list_store_new (2,
                                G_TYPE_STRING,   // name
                                G_TYPE_POINTER); // listname*

    for (igl = glitems; igl != NULL; igl = igl->next)
    {
        listitem = (listname *) igl->data;
        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter,
                            0, listitem->name,
                            1, listitem,
                            -1);
    }

    // GtkTreeView
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    list_w = gtk_tree_view_new_with_model (GTK_TREE_MODEL(store));
    g_object_unref (G_OBJECT(store));
    gtk_tree_view_set_headers_visible (GTK_TREE_VIEW(list_w), FALSE);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes ("", renderer,
                                                       "text", 0, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW(list_w), column);

    // don't use gtk_scrolled_window_add_with_viewport() with a GtkTreeView
    // use gtk_container_add() instead
    gtk_container_add (GTK_CONTAINER(scrolled_window),list_w);

#else // -- GTK1 --
    GtkWidget *item; // GtkListItem
    GList *gtklistitems = NULL;
    for (igl = glitems; igl != NULL; igl = igl->next)
    {
        listitem = (listname *) igl->data;
        item = gtk_list_item_new_with_label (listitem->name);
        g_object_set_data (G_OBJECT(item), "listitem", (gpointer) listitem);
        gtk_widget_show(item);
        gtklistitems = g_list_append (gtklistitems, item);
        if (Xdialog.tips == 1 && strlen(listitem->tips) > 0) {
            gtk_widget_set_tooltip_text (item, listitem->tips);
        }
    }

    // GtkList
    list_w = gtk_list_new();
    gtk_widget_show (list_w);
    gtk_list_set_selection_mode (GTK_LIST(list_w), GTK_SELECTION_MULTIPLE);
    gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW(scrolled_window), list_w);
    
    gtk_list_append_items (GTK_LIST(list_w), gtklistitems);
#endif
    return list_w;
}


static GtkWidget *set_horizontal_slider(GtkBox *box, gint deflt, gint min, gint max)
{
    GtkWidget *align;
    GtkAdjustment *adj;
    GtkWidget *hscale;
    GtkAdjustment *slider;

    align = gtk_alignment_new(0.5, 0.5, 0.8, 0);
    gtk_box_pack_start (box, align, FALSE, FALSE, 5);

    /* Create an adjusment object to hold the range of the scale */
    slider = GTK_ADJUSTMENT (gtk_adjustment_new(deflt, min, max, 1, 1, 0));
    adj = slider;
    hscale = gtk_scale_new (GTK_ORIENTATION_HORIZONTAL, adj);
    gtk_scale_set_digits(GTK_SCALE(hscale), 0);
    gtk_container_add(GTK_CONTAINER(align), hscale);

    return ((GtkWidget *) slider);
}


static GtkWidget *set_spin_button (GtkWidget *hbox, gint min, gint max, gint deflt,
                                   gint digits, gchar *separator, gboolean align_left)
{
    GtkAdjustment *adjust;
    GtkWidget *spin;
    GtkWidget *label;

    adjust = GTK_ADJUSTMENT(gtk_adjustment_new(deflt, min, max, 1, 1, 0));

    spin = gtk_spin_button_new(adjust, 0.5, 0);
    gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin), TRUE);
    gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(spin), TRUE);
    gtk_widget_set_size_request(spin, (digits+4)*xmult, -1);
    gtk_box_pack_start (GTK_BOX(hbox), spin, FALSE, FALSE, 0);
    if (separator != NULL && strlen(separator) != 0) {
        label = gtk_label_new(separator);
        if (align_left) {
            gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
            gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
        }
        gtk_box_pack_start (GTK_BOX(hbox), label, TRUE, TRUE, xmult);
    }

    return spin;
}


static int item_status(GtkWidget *item, char *status, char *tag)
{
    if (!strcasecmp(status, "on") && strlen(tag) != 0) {
        return 1;
    }
    if (!strcasecmp(status, "unavailable") || strlen(tag) == 0) {
        if (item) { // this is only for checklists / radiolists
            gtk_widget_set_sensitive(item, FALSE);
            return -1;
        }
    }
    return 0;
}


static void set_timeout(void)
{
    if (Xdialog.timeout > 0) {
        Xdialog.timer2 = g_timeout_add(Xdialog.timeout*1000, timeout_exit, NULL);
    }
}


/* ==============================================================================
 * The Xdialog widgets...
 * ============================================================================== */

void create_msgbox(gchar *optarg, gboolean yesno)
{
    GtkWidget *hbuttonbox; 
    GtkWidget *button;

    open_window();

    set_backtitle(TRUE);
    set_label(optarg, TRUE);

    hbuttonbox = set_hbuttonbox();
    set_check_button(NULL);

    if (yesno) {
        if (Xdialog.wizard) {
            set_button(PREVIOUS , hbuttonbox, 3, FALSE);
            if (Xdialog.cancel_button)
                set_button(CANCEL , hbuttonbox, 1, FALSE);
            set_button(NEXT, hbuttonbox, 0, TRUE);
        } else {
            set_button(YES, hbuttonbox, 0, !Xdialog.default_no);
            button = set_button(NO , hbuttonbox, 1, Xdialog.default_no);
            if (Xdialog.default_no)
                gtk_widget_grab_focus(button);
        }
        if (Xdialog.extra_button) {
            set_button (EXTRA, hbuttonbox, 5, FALSE);
        }
    } else {
        set_button(OK, hbuttonbox, 0, TRUE);
    }

    if (Xdialog.help) {
        set_button(HELP, hbuttonbox, 2, FALSE);
    }
    set_timeout();
}


// ------------------------------------------------------------------------------------------
//                             create_infobox
// ------------------------------------------------------------------------------------------

void create_infobox(gchar *optarg, gint timeout)
{
    GtkWidget *hbuttonbox;

    open_window();

    set_backtitle(TRUE);
    Xdialog.widget1 = set_label(optarg, TRUE);

    if (Xdialog.buttons && !dialog_compat) {
        hbuttonbox = set_hbuttonbox();
        set_button (timeout > 0 ? OK : CANCEL, hbuttonbox,
                    timeout > 0 ? 0 : 1, TRUE);
    }

    Xdialog.label_text[0] = 0;
    Xdialog.new_label = Xdialog.check = FALSE;

    if (timeout > 0)
        Xdialog.timer = g_timeout_add(timeout, infobox_timeout_exit, NULL);
    else
        Xdialog.timer = g_timeout_add(10, infobox_timeout, NULL);
}


// ------------------------------------------------------------------------------------------
//                             create_gauge
// ------------------------------------------------------------------------------------------

void create_gauge(gchar *optarg, gint percent)
{
    gdouble value;
    GtkWidget *pbar;
    GtkWidget *hbox;

    if (percent < 0) {
        value = 0;
    } else if (percent > 100) {
        value = 100;
    } else {
        value = percent;
    }
    open_window();

    set_backtitle(TRUE);
    Xdialog.widget2 = set_label(optarg, TRUE);
    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start (Xdialog.vbox, hbox, FALSE, TRUE, 0);

    /* Set up the progress bar */
#if GTK_CHECK_VERSION(2,0,0)
    pbar = gtk_progress_bar_new ();
    // set initial %
    char txt[20];
    snprintf(txt, sizeof(txt), "%g%%", value); // initial % = 50%
    gtk_progress_bar_set_text (GTK_PROGRESS_BAR(pbar), txt);
    value = value / 100.0;
    gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR(pbar), value);
#else // -- GTK1 --
    pbar = gtk_progress_bar_new ();
    // Set the start value and the range of the progress bar
    gtk_progress_configure (GTK_PROGRESS(pbar), (int)value, 0, 100);
    /// alternate method:
    ///   GtkAdjustment *adj = GTK_ADJUSTMENT(gtk_adjustment_new(value, 0, 100, 0, 0, 0));
    ///   pbar = gtk_progress_bar_new_with_adjustment (adj);
    // Set the format of the string that can be displayed in the progress bar:
    // %p=percentage  /  %v=value  /  %l=lower_range_value  /  %u=upper_range_value
    gtk_progress_set_format_string (GTK_PROGRESS(pbar), "%p%%");
    gtk_progress_set_show_text (GTK_PROGRESS(pbar), TRUE);
#endif
    Xdialog.widget1 = pbar;
    gtk_box_pack_start (GTK_BOX (hbox), Xdialog.widget1, TRUE, TRUE, 10);

    Xdialog.label_text[0] = 0;
    Xdialog.new_label = Xdialog.check = FALSE;

    /* Add a timer callback to update the value of the progress bar */
    Xdialog.timer = g_timeout_add(10, gauge_timeout, NULL);
}


// ------------------------------------------------------------------------------------------
//                             create_progress
// ------------------------------------------------------------------------------------------

void create_progress(gchar *optarg, gint leading, gint maxdots)
{
    GtkWidget * label, * hbox;
    GtkWidget *pbar;
#if GTK_CHECK_VERSION(2,0,0)
    gdouble ceiling;
#else // -- GTK1 --
    int ceiling;
#endif
    int i;
    unsigned char temp[2];
    size_t rresult;

#if GTK_CHECK_VERSION(2,0,0)
    if (maxdots <= 0) {
        ceiling = 1.0;
    } else {
        ceiling = (gdouble) maxdots;
    }
    /* convert to fractions - step */
    Xdialog.progress_step = 1.0 / ceiling;
#else // -- GTK1 --
    if (maxdots <= 0) {
        ceiling = 100;
    } else {
        ceiling = maxdots;
    }
#endif

    open_window();

    set_backtitle(TRUE);

    trim_string(optarg, Xdialog.label_text, MAX_LABEL_LENGTH);
    label = set_label(Xdialog.label_text, TRUE);
    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start (Xdialog.vbox, hbox, FALSE, TRUE, 0);

    /* Set up the progress bar */
#if GTK_CHECK_VERSION(2,0,0)
    pbar = gtk_progress_bar_new ();
    // set initial %
    gtk_progress_bar_set_text (GTK_PROGRESS_BAR (pbar), "0%");
    gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (pbar), 0.0);
#else // -- GTK1 --
    pbar = gtk_progress_bar_new ();
    // Set the start value and the range of the progress bar
    gtk_progress_configure (GTK_PROGRESS(pbar), 0, 0, ceiling);
    /// alternate method:
    ///   GtkAdjustment *adj = GTK_ADJUSTMENT(gtk_adjustment_new(0, 0, ceiling, 0, 0, 0));
    ///   pbar = gtk_progress_bar_new_with_adjustment(adj);
    // Set the format of the string that can be displayed in the progress bar:
    // %p=percentage  /  %v=value  /  %l=lower_range_value  /  %u=upper_range_value
    gtk_progress_set_format_string (GTK_PROGRESS(pbar), "%p%%");
    gtk_progress_set_show_text (GTK_PROGRESS(pbar), TRUE);
#endif
    Xdialog.widget1 = pbar;
    gtk_box_pack_start (GTK_BOX (hbox), Xdialog.widget1, TRUE, TRUE, 10);

    /* Skip the characters to be ignored on the input stream */
    if (leading < 0) {
        for (i=1; i < -leading; i++) {
            rresult = fread(temp, sizeof(char), 1, stdin);
            if (rresult < 1 && feof(stdin))
                break;
        }
    } else if (leading > 0) {
        for (i=1; i < leading; i++) {
            temp[0] = temp[1] = 0;
            rresult = fread(temp, sizeof(unsigned char), 1, stdin);
            if (rresult < 1 && feof(stdin)) {
                break;
            }
            if (temp[0] >= ' ' || temp[0] == '\n') {
                strncat(Xdialog.label_text, (char*)temp, sizeof(Xdialog.label_text));
            }
        }
        gtk_label_set_text(GTK_LABEL(label), Xdialog.label_text);
    }

    Xdialog.check = FALSE;

    /* Add a timer callback to update the value of the progress bar */
    Xdialog.timer = g_timeout_add(10, progress_timeout, NULL);
}


// ------------------------------------------------------------------------------------------
//                             create_tailbox
// ------------------------------------------------------------------------------------------

void create_tailbox(gchar *optarg)
{
    open_window();

    set_backtitle(FALSE);

    Xdialog.widget1 = set_scrollable_text();
    gtk_widget_set_size_request(Xdialog.widget1, 40*xmult, 15*ymult);
    gtk_widget_grab_focus(Xdialog.widget1);
    g_signal_connect (G_OBJECT(Xdialog.widget1), "key_press_event",
                      G_CALLBACK(tailbox_keypress), NULL);

    if (strcmp(optarg, "-") == 0)
        Xdialog.file = stdin;
    else
        Xdialog.file = fopen(optarg, "r");

    if (Xdialog.file == NULL) {
        fprintf(stderr, "Xdialog: can't open %s\n", optarg);
        exit(255);
    }

    if (Xdialog.file != stdin) {
        if (fseek(Xdialog.file, 0, SEEK_END) == 0) {
            Xdialog.file_init_size = ftell(Xdialog.file);
            fseek(Xdialog.file, 0, SEEK_SET);
        } else {
            Xdialog.file_init_size = 0;
        }
    }

    if (Xdialog.file == NULL) {
        fprintf(stderr, "Xdialog: can't open %s\n", optarg);
        exit(255);
    }

    if (dialog_compat) {
        Xdialog.cancel_button = FALSE;
    }
    if (Xdialog.buttons) {
        set_all_buttons(TRUE, Xdialog.ok_button);
    }
    Xdialog.timer = g_timeout_add(10, (GSourceFunc) tailbox_timeout, NULL);

    set_timeout();
}


// ------------------------------------------------------------------------------------------
//                             create_logbox
// ------------------------------------------------------------------------------------------

void create_logbox(gchar *optarg)
{
    GtkWidget *scrolled_window;
    gint xsize = 60;

    open_window();

    set_backtitle(FALSE);

#if GTK_CHECK_VERSION(2,0,0)
    GtkTreeModel     *model;
    GtkTreeView      *treeview;
    GtkTreeSelection *tree_sel;

    GtkListStore *store;
    store = gtk_list_store_new (LOGBOX_NUM_COLS,
                                G_TYPE_STRING,   // date
                                G_TYPE_STRING,   // text
                                GDK_TYPE_COLOR,  // background color
                                GDK_TYPE_COLOR); // foreground color
    model = GTK_TREE_MODEL (store);

    treeview = g_object_new (GTK_TYPE_TREE_VIEW,
                             "model", model, NULL);
    g_object_unref (model);

    tree_sel = gtk_tree_view_get_selection (treeview);
    gtk_tree_selection_set_mode (tree_sel, GTK_SELECTION_NONE);

    GtkCellRenderer *renderer;
    GtkTreeViewColumn *col0, *col1;

    // col1
    renderer = g_object_new (GTK_TYPE_CELL_RENDERER_TEXT, "xalign", 0.0, NULL);
    col1 = g_object_new (GTK_TYPE_TREE_VIEW_COLUMN,
                         "title",     LOG_MESSAGE,
                         "resizable", TRUE,
                         "clickable", FALSE,
                         NULL);
    gtk_tree_view_column_pack_start (col1, renderer, TRUE);
    gtk_tree_view_column_set_attributes (col1, renderer,
                                         "text",           LOGBOX_COL_TEXT,
                                         "background-gdk", LOGBOX_COL_BGCOLOR,
                                         "foreground-gdk", LOGBOX_COL_FGCOLOR,
                                         NULL);
    gtk_tree_view_append_column (treeview, col1);

    Xdialog.widget1 = GTK_WIDGET (treeview);
    if (Xdialog.time_stamp)
    {
        // col0
        renderer = g_object_new (GTK_TYPE_CELL_RENDERER_TEXT, "xalign", 0.0, NULL);
        col0 = g_object_new (GTK_TYPE_TREE_VIEW_COLUMN,
                             "title",     Xdialog.date_stamp ? DATE_STAMP : TIME_STAMP,
                             "resizable", TRUE,
                             "clickable", FALSE,
                             "sizing",    GTK_TREE_VIEW_COLUMN_AUTOSIZE,
                             NULL);
        gtk_tree_view_column_pack_start (col0, renderer, TRUE);
        gtk_tree_view_column_set_attributes (col0, renderer,
                                             "text",           LOGBOX_COL_DATE,
                                             "background-gdk", LOGBOX_COL_BGCOLOR,
                                             "foreground-gdk", LOGBOX_COL_FGCOLOR,
                                             NULL);
        gtk_tree_view_insert_column (treeview, col0, 0);

        xsize = 80;
    } else {
        gtk_tree_view_set_headers_visible (treeview, FALSE);
    }

#else // -- GTK1 --
    GtkCList *clist;
    Xdialog.widget1 = gtk_clist_new (Xdialog.time_stamp ? 2 : 1);
    clist = GTK_CLIST (Xdialog.widget1);
    gtk_clist_set_selection_mode (clist, GTK_SELECTION_SINGLE);
    gtk_clist_set_shadow_type (clist, GTK_SHADOW_IN);
    if (Xdialog.time_stamp) {
        gtk_clist_set_column_title (clist, 0, Xdialog.date_stamp ? DATE_STAMP : TIME_STAMP);
        gtk_clist_set_column_title (clist, 1, LOG_MESSAGE);
        gtk_clist_column_title_passive (clist, 0);
        gtk_clist_column_title_passive (clist, 1);
        gtk_clist_column_titles_show (clist);
        xsize = (Xdialog.date_stamp ? 69 : 58);
    }
    /* We need to call gtk_clist_columns_autosize IOT avoid
     * Gtk-WARNING **: gtk_widget_size_allocate(): attempt to allocate widget with width 41658 and height 1
     * and similar warnings with GTK+ v1.2.9 (and perhaps with previous versions as well)...
     */
    gtk_clist_columns_autosize(clist);
#endif

    g_signal_connect (G_OBJECT(Xdialog.widget1), "key_press_event",
                      G_CALLBACK(tailbox_keypress), NULL);
    gtk_widget_grab_focus(Xdialog.widget1);

    scrolled_window = set_scrolled_window(Xdialog.vbox, xmult/2, xsize, 12, 2);
    gtk_container_add(GTK_CONTAINER(scrolled_window), Xdialog.widget1);

    if (strcmp(optarg, "-") == 0) {
        Xdialog.file = stdin;
    } else {
        Xdialog.file = fopen(optarg, "r");
    }
    if (Xdialog.file == NULL) {
        fprintf(stderr, "Xdialog: can't open %s\n", optarg);
        exit(255);
    }

    if (Xdialog.file != stdin) {
        if (fseek(Xdialog.file, 0, SEEK_END) == 0) {
            Xdialog.file_init_size = ftell(Xdialog.file);
            fseek(Xdialog.file, 0, SEEK_SET);
        } else
            Xdialog.file_init_size = 0;
    }

    if (Xdialog.buttons)
        set_all_buttons(FALSE, Xdialog.ok_button);

    Xdialog.timer = g_timeout_add(10, (GSourceFunc) logbox_timeout, NULL);

    set_timeout();
}


// ------------------------------------------------------------------------------------------
//                             create_textbox
// ------------------------------------------------------------------------------------------

void create_textbox(gchar *optarg, gboolean editable)
{
    GtkWidget *text;
#if GTK_CHECK_VERSION(2,0,0)
	GtkTextBuffer *text_buffer;
    // g_convert_with_fallback()
    GError* error = NULL;
    gsize bytes_read = 0;
    gsize bytes_written = 0;
    gchar* utf8 = 0;
#endif
    GtkWidget *button_ok = NULL;
    FILE *infile;
    gint i, n = 0, llen = 0, lcnt = 0;

    open_window();

    set_backtitle(FALSE);

    Xdialog.widget1 = set_scrollable_text();
    gtk_widget_grab_focus(Xdialog.widget1);
    text = Xdialog.widget1;
#if GTK_CHECK_VERSION(2,0,0)
    text_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text));
#else // -- GTK1 --
    gtk_text_freeze (GTK_TEXT(text));
#endif
    

    /* Fill the GtkText with the text */
    if (strcmp(optarg, "-") == 0) {
        infile = stdin;
    } else {
        infile = fopen(optarg, "r");
    }
    if (infile) {
        char buffer[1024];
        int nchars;
        do {
            nchars = fread(buffer, 1, 1024, infile);
#if GTK_CHECK_VERSION(2,0,0)
            utf8 = g_convert_with_fallback(buffer, nchars, "UTF-8", "ISO-8859-1", "\357\277\275", &bytes_read, &bytes_written, &error);
            if (error == NULL) {
                gtk_text_buffer_insert_at_cursor(text_buffer, utf8, nchars);
            } else {
                printf("Error converting to UTF-8: GConvertError(%02x): %s", error->code, error->message);
            }
            g_free(utf8);
#else // -- GTK1 --
            gtk_text_insert (GTK_TEXT(text), NULL, NULL, NULL, buffer, nchars);
#endif
            /* Calculate the maximum line length and lines count */
            for (i = 0; i < nchars; i++)
                if (buffer[i] != '\n') {
                    if (buffer[i] == '\t') {
                        n += 8;
                    } else {
                        n++;
                    }
                } else {
                    if (n > llen) {
                        llen = n;
                    }
                    n = 0;
                    lcnt++;
                }
        } while (nchars == 1024);

        if (infile != stdin) {
            fclose(infile);
        }
    }
    llen += 4;
    if (Xdialog.fixed_font) {
        gtk_widget_set_size_request(Xdialog.widget1,
                    MIN(llen*ffxmult, gdk_screen_width()-4*ffxmult),
                    MIN(lcnt*ffymult, gdk_screen_height()-10*ffymult));
    } else {
        gtk_widget_set_size_request(Xdialog.widget1,
                    MIN(llen*xmult, gdk_screen_width()-4*xmult),
                    MIN(lcnt*ymult, gdk_screen_height()-10*ymult));
    }
    /* Set the editable flag depending on what we want (text or edit box) */
#if GTK_CHECK_VERSION(2,0,0)
    gtk_text_view_set_editable (GTK_TEXT_VIEW(text), editable);
    // position the cursor on the first line
    GtkTextIter firstLineIter;
    gtk_text_buffer_get_start_iter(text_buffer, &firstLineIter);
    gtk_text_buffer_place_cursor(text_buffer, &firstLineIter);
#else // -- GTK1 --
    gtk_text_thaw (GTK_TEXT(text));
    gtk_text_set_editable (GTK_TEXT(text), editable);
#endif

    if (dialog_compat && !editable)
        Xdialog.cancel_button = FALSE;

    /* Set the buttons */
    if (Xdialog.buttons || editable) {
        button_ok = set_all_buttons(TRUE, TRUE);
    }
    if (editable) {
        g_signal_connect (G_OBJECT(button_ok), "clicked",
                          G_CALLBACK(editbox_ok), NULL);
    }
    set_timeout();
}


// ------------------------------------------------------------------------------------------
//                             create_inputbox
// ------------------------------------------------------------------------------------------

void create_inputbox(gchar *optarg, gchar *options[], gint entries)
{
    GtkEntry *entry;
    GtkWidget *button_ok = NULL;
    GtkWidget *hide_button;
    gchar deftext[MAX_INPUT_DEFAULT_LENGTH];

    open_window();

    set_backtitle(TRUE);
    set_label(optarg, TRUE);

    if (entries > 1) {
        set_separator(TRUE);
        set_secondary_label(options[0], FALSE);
        strncpy(deftext, options[1], sizeof(deftext));
    } else {
        if (options[0] != NULL)
            strncpy(deftext, options[0], sizeof(deftext));
        else
            deftext[0] = 0;
    }

    Xdialog.widget1 = gtk_entry_new();
    entry = GTK_ENTRY(Xdialog.widget1);
    gtk_entry_set_text(entry, deftext);
    gtk_box_pack_start (Xdialog.vbox, Xdialog.widget1, TRUE, TRUE, 0);
    gtk_widget_grab_focus(Xdialog.widget1);
    
    if (entries > 1) {
        set_secondary_label(options[2], FALSE);

        Xdialog.widget2 = gtk_entry_new();
        entry = GTK_ENTRY(Xdialog.widget2);
        gtk_entry_set_text(entry, options[3]);
        gtk_box_pack_start (Xdialog.vbox, Xdialog.widget2, TRUE, TRUE, 0);
    } else {
        Xdialog.widget2 = NULL;
    }

    if (entries > 2) {
        set_secondary_label(options[4], FALSE);

        Xdialog.widget3 = gtk_entry_new();
        entry = GTK_ENTRY(Xdialog.widget3);
        gtk_entry_set_text(entry, options[5]);
        gtk_box_pack_start (Xdialog.vbox, Xdialog.widget3, TRUE, TRUE, 0);
    } else {
        Xdialog.widget3 = NULL;
    }

    if ((Xdialog.passwd > 0 && Xdialog.passwd < 10) ||
        (Xdialog.passwd > 10 && Xdialog.passwd <= entries + 10)) {
            hide_button = gtk_check_button_new_with_label(HIDE_TYPING);
            gtk_box_pack_start (Xdialog.vbox, hide_button, TRUE, TRUE, 0);
            g_signal_connect (G_OBJECT(hide_button), "toggled",
                              G_CALLBACK(hide_passwords), NULL);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(hide_button), TRUE);
    }

    if (Xdialog.buttons) {
        button_ok = set_all_buttons(FALSE, TRUE);
        g_signal_connect (G_OBJECT(button_ok), "clicked", G_CALLBACK(inputbox_ok), NULL);
    }

    if (Xdialog.interval > 0)
        Xdialog.timer = g_timeout_add(Xdialog.interval, inputbox_timeout, NULL);

    set_timeout();
}


// ------------------------------------------------------------------------------------------
//                             create_combobox
// ------------------------------------------------------------------------------------------

void create_combobox(gchar *optarg, gchar *options[], gint list_size)
{
    GtkWidget *combo;
    GtkWidget *button_ok = NULL;
    int i;

    open_window();

    set_backtitle(TRUE);
    set_label(optarg, TRUE);

    combo = gtk_combo_box_text_new_with_entry ();
#if GTK_CHECK_VERSION(2,4,0)
    Xdialog.widget1 = gtk_bin_get_child (GTK_BIN (combo));
#else
    Xdialog.widget1 = GTK_COMBO(combo)->entry;
#endif
    Xdialog.widget2 = Xdialog.widget3 = NULL;
    gtk_box_pack_start (Xdialog.vbox, combo, TRUE, TRUE, 0);
    gtk_widget_grab_focus(Xdialog.widget1);

    /* Set the popdown strings */
    for (i = 0; i < list_size; i++) {
        gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT(combo), options[i]);
    }

    if (strlen(Xdialog.default_item) != 0) {
        gtk_combo_box_text_prepend_text (GTK_COMBO_BOX_TEXT(combo), Xdialog.default_item);
        gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0);
    }

    gtk_editable_set_editable (GTK_EDITABLE(Xdialog.widget1), Xdialog.editable);

    if (Xdialog.buttons) {
        button_ok = set_all_buttons(FALSE, TRUE);
        g_signal_connect (G_OBJECT(button_ok), "clicked", G_CALLBACK(inputbox_ok), NULL);
    }

    if (Xdialog.interval > 0)
        Xdialog.timer = g_timeout_add(Xdialog.interval, inputbox_timeout, NULL);

    set_timeout();
}


// ------------------------------------------------------------------------------------------
//                             create_rangebox
// ------------------------------------------------------------------------------------------

void create_rangebox(gchar *optarg, gchar *options[], gint ranges)
{
    GtkWidget *button_ok;
    gint min, max, deflt;

    open_window();

    set_backtitle(TRUE);
    set_label(optarg, TRUE);

    if (ranges > 1) {
        set_separator(TRUE);
        set_secondary_label(options[0], FALSE);
        min = atoi(options[1]);
        max = atoi(options[2]);
        deflt = atoi(options[3]);
    } else {
        min = atoi(options[0]);
        max = atoi(options[1]);
        if (options[2] != NULL)
            deflt = atoi(options[2]);
        else
            deflt = min;
    }

    Xdialog.widget1 = (GtkWidget *) set_horizontal_slider(Xdialog.vbox,
                                                    deflt, min, max);

    Xdialog.widget2 = Xdialog.widget3 = NULL;

    if (ranges > 1) {
        set_secondary_label(options[4], FALSE);
        min = atoi(options[5]);
        max = atoi(options[6]);
        deflt = atoi(options[7]);

        Xdialog.widget2 = (GtkWidget *) set_horizontal_slider(Xdialog.vbox,
                                                        deflt, min, max);
    }

    if (ranges > 2) {
        set_secondary_label(options[8], FALSE);
        min = atoi(options[9]);
        max = atoi(options[10]);
        deflt = atoi(options[11]);

        Xdialog.widget3 = (GtkWidget *) set_horizontal_slider(Xdialog.vbox,
                                                            deflt, min, max);
	}

    button_ok = set_all_buttons(FALSE, TRUE);
    g_signal_connect (G_OBJECT(button_ok), "clicked", G_CALLBACK(rangebox_exit), NULL);

    if (Xdialog.interval > 0)
            Xdialog.timer = g_timeout_add(Xdialog.interval, rangebox_timeout, NULL);

    set_timeout();
}


// ------------------------------------------------------------------------------------------
//                             create_spinbox
// ------------------------------------------------------------------------------------------

void create_spinbox(gchar *optarg, gchar *options[], gint spins)
{
    GtkWidget *frame;
    GtkWidget *hbox;
    GtkWidget *button_ok;

    open_window();

    set_backtitle(TRUE);
    set_label(optarg, TRUE);

    frame = gtk_frame_new(NULL);
    gtk_box_pack_start (Xdialog.vbox, frame, TRUE, TRUE, ymult/2);

    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_set_homogeneous (GTK_BOX(hbox), TRUE);

    gtk_container_add(GTK_CONTAINER(frame), hbox);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), ymult);

    Xdialog.widget1 = set_spin_button(hbox, atoi(options[0]), atoi(options[1]), atoi(options[2]),
                                      strlen(options[1]), options[3], TRUE);
    if (spins > 1) {
        Xdialog.widget2 = set_spin_button(hbox, atoi(options[4]), atoi(options[5]), atoi(options[6]),
                                          strlen(options[5]), options[7], TRUE);
    } else {
        Xdialog.widget2 = NULL;
}   
    if (spins > 2) {
        Xdialog.widget3 = set_spin_button(hbox, atoi(options[8]), atoi(options[9]), atoi(options[10]),
                                          strlen(options[9]), options[11], TRUE);
    } else {
        Xdialog.widget3 = NULL;
    }
    button_ok = set_all_buttons(FALSE, TRUE);

    g_signal_connect (G_OBJECT(button_ok), "clicked", G_CALLBACK(spinbox_exit), NULL);

    if (Xdialog.interval > 0) {
        Xdialog.timer = g_timeout_add(Xdialog.interval, spinbox_timeout, NULL);
    }
    set_timeout();
}


// ------------------------------------------------------------------------------------------
//                            create_itemlist
// ------------------------------------------------------------------------------------------

void create_itemlist(gchar *optarg, gint type, gchar *options[], gint list_size)
{ // radiolist / checklist
    GtkWidget *vbox;
    GtkWidget *scrolled_window;
    GtkWidget *button_ok;
    GtkWidget *item;
    GtkRadioButton *radio = NULL;
    char temp[MAX_ITEM_LENGTH];
    int i;
    int params = 3 + Xdialog.tips;
    char *status;

    Xdialog_array(list_size);

    open_window();

    set_backtitle(TRUE);
    set_label(optarg, FALSE);

    scrolled_window = set_scrolled_window(Xdialog.vbox, xmult/2, -1, list_size, ymult + 5);

    vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, xmult);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), xmult);
    gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolled_window), vbox);

    button_ok = set_all_buttons(FALSE, TRUE);
    g_signal_connect (G_OBJECT(button_ok), "clicked", G_CALLBACK(print_items), NULL);

    for (i = 0;  i < list_size; i++)
    {
        strncpy(Xdialog.array[i].tag, options[params*i], sizeof(Xdialog.array[i].tag) );
        temp[0] = 0;
        if (Xdialog.tags && strlen(options[params*i]) != 0) {
            strncpy(temp, options[params*i], sizeof(temp));
            strncat(temp, ": ", sizeof(temp));
        }
        strncat(temp, options[params*i+1], sizeof(temp));
        status = options[params*i+2];

        if (type == CHECKLIST) {
            item = gtk_check_button_new_with_label(temp);
        } else {
            item = gtk_radio_button_new_with_label_from_widget(radio, temp);
            radio = GTK_RADIO_BUTTON(item);
        }
        gtk_box_pack_start (GTK_BOX(vbox), item, FALSE, FALSE, 0);

        if (item_status(item, status, Xdialog.array[i].tag) == 1) {
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(item), TRUE);
        }
        g_signal_connect (G_OBJECT(item), "toggled",
                          G_CALLBACK(item_toggle), (gpointer)i);
        g_signal_connect (G_OBJECT(item), "button_press_event",
                          G_CALLBACK(double_click_event), button_ok);
        g_signal_emit_by_name (G_OBJECT(item), "toggled");

        if (Xdialog.tips == 1 && strlen(options[params*i+3]) > 0) {
            gtk_widget_set_tooltip_text (item, (gchar *) options[params*i+3]);
        }
    }

    if (Xdialog.interval > 0) {
        Xdialog.timer = g_timeout_add(Xdialog.interval, itemlist_timeout, NULL);
    }
    set_timeout();
}


// ------------------------------------------------------------------------------------------
//                            create_buildlist
// ------------------------------------------------------------------------------------------

void create_buildlist (gchar *optarg, gchar *options[], gint list_size)
{
    GtkWidget *hbox;
    GtkWidget *vbuttonbox;
    GtkWidget *button_add;
    GtkWidget *button_remove;
    GtkWidget *button_ok;
    GList *glist1 = NULL;
    GList *glist2 = NULL;
    gint i, n = 0;
    int params = 3 + Xdialog.tips;
    char *status;

    Xdialog_array(list_size);

    open_window();

    set_backtitle(TRUE);
    set_label(optarg, FALSE);

    /* Put all parameters into an array and calculate the max item width */
    for (i = 0;  i < list_size; i++)
    {
        strncpy (Xdialog.array[i].tag, options[params*i], sizeof(Xdialog.array[i].tag));
        strncpy (Xdialog.array[i].name, options[params*i+1], sizeof(Xdialog.array[i].name));
        status = options[params*i+2];
        if (Xdialog.tips == 1) {
            strncpy(Xdialog.array[i].tips, options[params*i+3], sizeof(Xdialog.array[i].tips));
        }
        if ((gint) strlen(Xdialog.array[i].name) > n) {
            n = strlen(Xdialog.array[i].name);
        }
        if (item_status(NULL, status, Xdialog.array[i].tag) == 1) {
            glist2 = g_list_append (glist2, &Xdialog.array[i]);
        } else {
            glist1 = g_list_append (glist1, &Xdialog.array[i]);
        }
    }

    /* Setup a hbox to hold the scrolled windows and the Add/Remove buttons */
    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start (Xdialog.vbox, hbox, TRUE, TRUE, ymult/3);

    /* Setup the first list into a scrolled window */
    Xdialog.widget1 = set_scrolled_list(hbox, MAX(25, n), list_size, 4, glist1);

    /* Setup the Add/Remove buttons */
    vbuttonbox = gtk_button_box_new (GTK_ORIENTATION_VERTICAL);
    gtk_box_pack_start (GTK_BOX(hbox), vbuttonbox, FALSE, TRUE, 0);
    gtk_button_box_set_layout(GTK_BUTTON_BOX(vbuttonbox), GTK_BUTTONBOX_SPREAD);

    button_add = Xdialog.widget3 = set_button(ADD, vbuttonbox, -1, FALSE);
    g_signal_connect (G_OBJECT(button_add), "clicked",
                      G_CALLBACK(buildlist_add_or_remove), NULL);

    button_remove = Xdialog.widget4 = set_button(REMOVE, vbuttonbox, -1, FALSE);
    g_signal_connect (G_OBJECT(button_remove), "clicked",
                      G_CALLBACK(buildlist_add_or_remove), GINT_TO_POINTER(1));

    /* Setup the second list into a scrolled window */
    Xdialog.widget2 = set_scrolled_list(hbox, MAX(25, n), list_size, 4, glist2);

    g_list_free (glist1);
    g_list_free (glist2);

    button_ok = set_all_buttons(FALSE, TRUE);
    g_signal_connect (G_OBJECT(button_ok), "clicked",
                      G_CALLBACK(buildlist_print_list), NULL);

    buildlist_sensitive_buttons();

    set_timeout();
}


// ------------------------------------------------------------------------------------------
//                            create_menubox
// ------------------------------------------------------------------------------------------

void create_menubox (gchar *optarg, gchar *options[], gint list_size)
{
    GtkWidget *button_ok;
    GtkWidget *scrolled_window;
    GtkWidget *status_bar = NULL;
    GtkWidget *hbox = NULL;
    int i;
    int rownum = 0;
    int params = 2 + Xdialog.tips;

    Xdialog_array(list_size);
    Xdialog.array[0].state = -1;

    open_window();

    set_backtitle(TRUE);
    set_label(optarg, FALSE);

    scrolled_window = set_scrolled_window(Xdialog.vbox, xmult/2, -1, list_size, 4);

    if (Xdialog.tips == 1) {
        hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_box_pack_start (Xdialog.vbox, hbox, FALSE, FALSE, 0);
        status_bar      = gtk_statusbar_new ();
        Xdialog.widget1 = status_bar;
        gtk_container_add (GTK_CONTAINER(hbox), status_bar);
        Xdialog.status_id = gtk_statusbar_get_context_id (GTK_STATUSBAR(status_bar), "tips");
        gtk_statusbar_push (GTK_STATUSBAR(status_bar), Xdialog.status_id,
                            Xdialog.array[0].tips);
    }

    button_ok = set_all_buttons(FALSE, TRUE);

#if GTK_CHECK_VERSION(2,0,0)
    GtkTreeModel     *tree_model;
    GtkTreeView      *treeview;
    GtkTreeSelection *tree_sel;
    GtkTreeIter  iter;
    GtkListStore *store;

    store = gtk_list_store_new (3,
                                G_TYPE_STRING,   // tag
                                G_TYPE_STRING,   // name
                                G_TYPE_POINTER); // listname* (hidden)
    tree_model = GTK_TREE_MODEL (store);

    Xdialog.widget2 = gtk_tree_view_new_with_model (tree_model);
    treeview = GTK_TREE_VIEW (Xdialog.widget2);
    gtk_container_add (GTK_CONTAINER(scrolled_window), GTK_WIDGET(treeview));

    g_object_unref (G_OBJECT(store));
    gtk_tree_view_set_headers_visible (treeview, FALSE);

    tree_sel = gtk_tree_view_get_selection (treeview);
    gtk_tree_selection_set_mode (tree_sel, GTK_SELECTION_BROWSE);

    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;

    // column 0 - tag
    renderer = g_object_new (GTK_TYPE_CELL_RENDERER_TEXT,
                           "xalign", 0.0,     /* justify left */
                           NULL);
    column = g_object_new (GTK_TYPE_TREE_VIEW_COLUMN,
                          "title",          "tag",
                          NULL);
    gtk_tree_view_column_pack_start (column, renderer, FALSE);
    gtk_tree_view_column_add_attribute (column, renderer, "text", 0);
    gtk_tree_view_append_column (treeview, column);
    gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
    if (!Xdialog.tags) {
        gtk_tree_view_column_set_visible (column, FALSE);
    }

    // column 1 - name
    renderer = g_object_new (GTK_TYPE_CELL_RENDERER_TEXT,
                            "xalign", 0.0,
                            NULL);
    column = g_object_new (GTK_TYPE_TREE_VIEW_COLUMN,
                           "title",          "name",
                           NULL);
    gtk_tree_view_column_pack_start (column, renderer, FALSE);
    gtk_tree_view_column_add_attribute (column, renderer, "text", 1);
    gtk_tree_view_append_column (treeview, column);
    gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
    //----

    for (i = 0; i < list_size; i++)
    {
        strncpy(Xdialog.array[i].tag, options[params*i], sizeof(Xdialog.array[i].tag));
        strncpy(Xdialog.array[i].name, options[params*i+1], sizeof(Xdialog.array[i].name));
        if (Xdialog.tips == 1) {
            strncpy(Xdialog.array[i].tips, options[params*i+2], sizeof(Xdialog.array[i].tips));
        }
        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter,
                            0, Xdialog.array[i].tag,
                            1, Xdialog.array[i].name,
                            2, &Xdialog.array[i],
                            -1);
        if (*Xdialog.default_item && !strcmp(Xdialog.default_item, Xdialog.array[i].tag)) {
            rownum = i;  // found --default-item
        }
    }
    Xdialog.array[0].state = rownum;

    g_signal_connect (G_OBJECT (treeview),  "row_activated",
                      G_CALLBACK(on_menubox_treeview_row_activated_cb), NULL);
    if (Xdialog.tips == 1) {
        g_signal_connect (G_OBJECT(tree_sel), "changed",
                          G_CALLBACK(on_menubox_tip_treeview_changed), status_bar);
    }

    /* select default row */
    GtkTreePath  *tpath = gtk_tree_path_new_from_indices (rownum, -1);
    // select rownum
    gtk_tree_selection_select_path (tree_sel, tpath);
    // scroll to the selected row
    gtk_tree_view_set_cursor (treeview, tpath, NULL, FALSE);
    gtk_tree_view_scroll_to_cell (treeview, tpath, NULL, TRUE, 0.5, 0.5);
    // free tpath
    gtk_tree_path_free (tpath);

#else // -- GTK1 --

    GtkCList *clist;
    static gchar *null_row[] = {NULL, NULL};
    int selrow = -1;
    static const GdkColor GREY1 = { 0, 0x6000, 0x6000, 0x6000 };
    static const GdkColor GREY2 = { 0, 0xe000, 0xe000, 0xe000 };

    Xdialog.widget2 = gtk_clist_new (2);
    clist = GTK_CLIST(Xdialog.widget2);
    gtk_container_add (GTK_CONTAINER(scrolled_window), Xdialog.widget2);

    gtk_clist_set_selection_mode (clist, GTK_SELECTION_BROWSE);
    gtk_clist_set_shadow_type (clist, GTK_SHADOW_IN);
    if (!Xdialog.tags) {
        gtk_clist_set_column_visibility(clist, 0, FALSE);
    }

    for (i = 0; i < list_size; i++)
    {
        strncpy(Xdialog.array[i].tag, options[params*i], sizeof(Xdialog.array[i].tag));
        strncpy(Xdialog.array[i].name, options[params*i+1], sizeof(Xdialog.array[i].name));
        if (Xdialog.tips == 1) {
            strncpy(Xdialog.array[i].tips, options[params*i+2], sizeof(Xdialog.array[i].tips));
        }
        rownum = gtk_clist_append (clist, null_row);

        if (strlen(Xdialog.array[i].tag) == 0) {
            gtk_clist_set_text (clist, rownum, 0, "~");
            gtk_clist_set_selectable (clist, rownum, FALSE);
            gtk_clist_set_foreground (clist, rownum, (GdkColor *) &GREY1);
            gtk_clist_set_background (clist, rownum, (GdkColor *) &GREY2);
        } else {
            gtk_clist_set_text (clist, rownum, 0, Xdialog.array[i].tag);
            gtk_clist_set_row_data (clist, rownum, (gpointer) &Xdialog.array[i]);
            if (selrow == -1) {
                selrow = rownum; // first selectable row
            }
        }
        gtk_clist_set_text (clist, rownum, 1, Xdialog.array[i].name);

        if (*Xdialog.default_item && !strcmp(Xdialog.default_item, Xdialog.array[i].tag)) {
            selrow = rownum; // found --default-item
        }
    }

    gtk_clist_columns_autosize (clist);

    gtk_signal_connect (GTK_OBJECT(Xdialog.widget2), "select_row",
                        GTK_SIGNAL_FUNC(on_menubox_item_select), NULL);
    /* selected default row */
    if (selrow >= 0) {
        Xdialog.array[0].state = selrow;
        gtk_clist_select_row (clist, selrow, 1);
    }
    // We can't move to the default selected row right from here,
    // we need a timeout function to do so... It will run only once course !
    g_timeout_add (0, move_to_row_timeout, (gpointer) Xdialog.widget2);
    ///gtk_clist_moveto (GTK_CLIST(clist), rownum, 0, 0.5, 0.0);

    gtk_signal_connect (GTK_OBJECT(Xdialog.widget2), "button_press_event",
                        GTK_SIGNAL_FUNC(double_click_event), button_ok);
#endif

    g_signal_connect (G_OBJECT(button_ok), "clicked", // GtkTreeView or GtkCList
                      G_CALLBACK(on_menubox_ok_click), Xdialog.widget2); 

    set_timeout();
}


// ------------------------------------------------------------------------------------------
//                            create_treeview
// ------------------------------------------------------------------------------------------

void create_treeview (gchar *optarg, gchar *options[], gint list_size)
{
    GtkWidget *scrolled_window;
    GtkWidget *button_ok;
    int depth = 0;
    int i;
    int params = 4 + Xdialog.tips;
    char *status;

    Xdialog_array(list_size);

    open_window();

    set_backtitle(TRUE);
    set_label(optarg, FALSE);

    /* Create and hookup the ok button */
    button_ok = set_all_buttons (FALSE, TRUE);
    g_signal_connect (G_OBJECT(button_ok), "clicked", G_CALLBACK(print_tree_selection), NULL);

    /* Create the tree view in a scrolled window */
    scrolled_window = set_scrolled_window(Xdialog.vbox, xmult/2, -1, list_size, 4);

#if GTK_CHECK_VERSION(2,0,0)
    GtkTreeStore *store;
    GtkTreeViewColumn *column;
    GtkCellRenderer *renderer;
    GtkTreeIter tree_iter[MAX_TREE_DEPTH];
    GtkTreeIter selected_iter;
    GtkTreeSelection *tree_sel;
    GtkWidget *tree;
    gboolean set_default_item = FALSE;

    /* Fill the store with the data */
    store = gtk_tree_store_new (2,
                                G_TYPE_STRING,   // name
                                G_TYPE_POINTER); // listname* (hidden)
    for (i = 0 ; i < list_size ; i++)
    {
        strncpy(Xdialog.array[i].tag, options[params*i], sizeof(Xdialog.array[i].tag));
        strncpy(Xdialog.array[i].name, options[params*i+1], sizeof(Xdialog.array[i].name));
        status = options[params*i+2];
        Xdialog.array[i].state = 0;

        depth = atoi(options[params*i+3]);

        if (depth > MAX_TREE_DEPTH) {
            fprintf(stderr,
                "Xdialog: Max allowed depth for "\
                "--treeview is %d !  Aborting...\n",
                MAX_TREE_DEPTH);
            exit(255);
        }

        if (depth == 0) {
            gtk_tree_store_append (store, &tree_iter[0], NULL);
            gtk_tree_store_set (store, &tree_iter[0],
                                0, Xdialog.array[i].name,
                                1, &Xdialog.array[i],
                                -1);
        } else {
            gtk_tree_store_append (store,
                                   &tree_iter[depth],
                                   &tree_iter[depth-1]);
            gtk_tree_store_set (store, &tree_iter[depth],
                                0, Xdialog.array[i].name,
                                1, &Xdialog.array[i],
                                -1);
        }
        if (item_status(NULL, status, Xdialog.array[i].tag) == 1 && !set_default_item) {
            set_default_item = TRUE;
            selected_iter = tree_iter[depth];
        }
    }

    tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL (store));
    Xdialog.widget1 = tree;
    g_object_unref (G_OBJECT (store));
    gtk_tree_view_set_headers_visible (GTK_TREE_VIEW(tree), FALSE);

    /* Setup the selection handler - currently not used... */
    tree_sel = gtk_tree_view_get_selection (GTK_TREE_VIEW(tree));
    gtk_tree_selection_set_mode (tree_sel, GTK_SELECTION_SINGLE);
    g_signal_connect(G_OBJECT(tree_sel), "changed", G_CALLBACK(tree_selection_changed), NULL);

    g_signal_connect (G_OBJECT(tree), "button_press_event",
                      G_CALLBACK(double_click_event), button_ok);

    renderer = gtk_cell_renderer_text_new();
    column   = gtk_tree_view_column_new_with_attributes ("", renderer, "text", 0, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW(tree), column);

    gtk_container_add (GTK_CONTAINER(scrolled_window), tree);

    /// expand root node
    //GtkTreePath  *rpath = gtk_tree_path_new_from_string ("0");
    //gtk_tree_view_expand_row (GTK_TREE_VIEW(tree), rpath, FALSE);
    //gtk_tree_path_free (rpath);

    /// must expand all nodes for the selection to take effect
    gtk_tree_view_expand_all (GTK_TREE_VIEW(tree));

    if (set_default_item) {
        gtk_tree_selection_select_iter (tree_sel, &selected_iter);
    }
#else // -- GTK1 --

    GtkWidget *item;
    GtkWidget *selected = NULL;
    GtkTree *tree;
    GtkTree *oldtree[MAX_TREE_DEPTH];
    GtkWidget *subtree;
    int level;

    Xdialog.widget1 = gtk_tree_new();
    gtk_widget_show (Xdialog.widget1);
    gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW(scrolled_window),
                                           Xdialog.widget1);

    tree = oldtree[0] = GTK_TREE(Xdialog.widget1);

    gtk_tree_set_view_mode (tree, GTK_TREE_VIEW_ITEM);
    gtk_tree_set_selection_mode (tree, GTK_SELECTION_BROWSE);

    for (i = 0 ; i < list_size ; i++)
    {
        strncpy (Xdialog.array[i].tag, options[params*i], sizeof(Xdialog.array[i].tag));
        strncpy (Xdialog.array[i].name, options[params*i+1], sizeof(Xdialog.array[i].name));
        status = options[params*i+2];

        item = gtk_tree_item_new_with_label (Xdialog.array[i].name);
        g_object_set_data (G_OBJECT(item), "listitem", (gpointer) &Xdialog.array[i]);

        level = atoi(options[params*i+3]);
        if (i > 0) {
            if (atoi(options[params*(i-1)+3]) > level) {
                depth = level;
                tree = oldtree[level];
            }
        }

        gtk_tree_append(tree, item);

        if (i+1 < list_size)
        {
            if (level < atoi(options[params*(i+1)+3])) {
                if (atoi(options[params*(i+1)+3]) != level + 1) {
                    fprintf(stderr,
                        "Xdialog: You cannot increment the --treeview depth "\
                        "by more than one level each time !  Aborting...\n");
                        exit(255);
                }
                subtree = gtk_tree_new();
                g_signal_connect (G_OBJECT(subtree), "button_press_event",
                                  G_CALLBACK(double_click_event),
                                  button_ok);
                gtk_tree_item_set_subtree(GTK_TREE_ITEM(item), subtree);
                depth++;
                if (depth > MAX_TREE_DEPTH) {
                    fprintf(stderr,
                        "Xdialog: Max allowed depth for "\
                        "--treeview is %d !  Aborting...\n",
                        MAX_TREE_DEPTH);
                        exit(255);
                }
                tree = GTK_TREE(subtree);
                oldtree[depth] = tree;
                gtk_tree_set_selection_mode (tree, GTK_SELECTION_BROWSE);
            }
        }

        if (!selected && item_status(item, status, Xdialog.array[i].tag) == 1) {
            selected = item;
            Xdialog.array[0].state = i;
        }
        gtk_widget_show(item);

        if (Xdialog.tips == 1 && strlen(options[params*i+4]) > 0) {
            gtk_widget_set_tooltip_text (item, (gchar *) options[params*i+4]);
        }
    }

    g_signal_connect (G_OBJECT(Xdialog.widget1), "selection_changed",
                      G_CALLBACK(tree_selection_changed), NULL);

    // expand root node
    GtkWidget *root_item = GTK_TREE(Xdialog.widget1)->children->data;
    gtk_tree_item_expand (GTK_TREE_ITEM(root_item));

    if (selected != NULL) {
        /// gtk_tree_item_select() is buggy, causes segfaults and weird behavior
        gtk_tree_select_child (GTK_TREE(Xdialog.widget1), selected);
    }
#endif

    set_timeout();
}


// ------------------------------------------------------------------------------------------
//                            create_filesel
// ------------------------------------------------------------------------------------------

void create_filesel (gchar *optarg, gboolean dsel_flag)
{
    GtkWidget *hbuttonbox;
    GtkWidget *button, *ok_button = NULL;
    gboolean flag;
    GtkWidget *filesel;

    font_init();
    parse_rc_file();

    /* Create a file selector and update Xdialog structure accordingly */
#if GTK_CHECK_VERSION(2,4,0)
    GtkFileChooserAction action;
    if (dsel_flag) {
        action = GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER;
    } else {
        action = GTK_FILE_CHOOSER_ACTION_OPEN;
    }
    filesel = gtk_file_chooser_dialog_new (
                                Xdialog.title,
                                NULL,
                                action,
//                              ok_label,     GTK_RESPONSE_ACCEPT,
//                              cancel_label, GTK_RESPONSE_CANCEL,
                                NULL, NULL );
#else
    filesel = gtk_file_selection_new (Xdialog.title);
#endif
    Xdialog.window = filesel;

#if GTK_CHECK_VERSION(2,0,0)
    Xdialog.vbox = GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (filesel)));
    hbuttonbox   = gtk_dialog_get_action_area (GTK_DIALOG (filesel));
#else // -- GTK1 --: GtkFileSection derives from GtkWindow
    Xdialog.vbox = GTK_BOX (GTK_FILE_SELECTION(filesel)->main_vbox);
    hbuttonbox   = GTK_FILE_SELECTION(filesel)->action_area;
#endif

    /* Set the backtitle */
    set_backtitle(TRUE);

    /* Set the default filename */
#if GTK_CHECK_VERSION(2,4,0)
    gtk_file_chooser_set_show_hidden (GTK_FILE_CHOOSER(filesel), TRUE);
    if (dsel_flag) {
        gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER(filesel), optarg);
    } else {
        struct stat sb;
        if (stat(optarg, &sb) == 0 && S_ISDIR(sb.st_mode)) {
            gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER(filesel), optarg);
        } else {
            gtk_file_chooser_set_filename (GTK_FILE_CHOOSER(filesel), optarg);
        }
    }
#else
    gtk_file_selection_set_filename (GTK_FILE_SELECTION(filesel), optarg);
    gtk_file_selection_complete (GTK_FILE_SELECTION(filesel), optarg);
    if (dsel_flag) {
         // directory, hide the file list parent box and
         // the file list entry field, keep only the "make directory" button.
        gtk_widget_hide_all (GTK_FILE_SELECTION(filesel)->file_list->parent);
        gtk_widget_hide (GTK_FILE_SELECTION(filesel)->selection_entry);
        gtk_widget_set_sensitive (GTK_FILE_SELECTION(filesel)->file_list->parent, FALSE);
        gtk_widget_set_sensitive (GTK_FILE_SELECTION(filesel)->selection_entry, FALSE);
        gtk_widget_destroy (GTK_FILE_SELECTION(filesel)->fileop_del_file);
        gtk_widget_destroy (GTK_FILE_SELECTION(filesel)->fileop_ren_file);
        // clear entry
        gtk_file_selection_set_filename (GTK_FILE_SELECTION(filesel), "");
    }
    if (!Xdialog.buttons || dialog_compat) { // Hide fileops buttons if requested
        gtk_file_selection_hide_fileop_buttons (GTK_FILE_SELECTION(filesel));
    }
    // Remove the fileselector buttons IOT put ours in place
    gtk_widget_destroy (GTK_FILE_SELECTION(filesel)->ok_button);
    gtk_widget_destroy (GTK_FILE_SELECTION(filesel)->cancel_button);
#endif

    /* If requested, add a check button into the filesel action area */
    set_check_button (hbuttonbox);

    /* We must realize the widget before moving it and creating the buttons pixbufs */
    gtk_widget_realize(Xdialog.window);

    /* Set the window size and placement policy */
    set_window_size_and_placement();

    /* Setup our own buttons */
    if (Xdialog.wizard)
        set_button(PREVIOUS , hbuttonbox, 3, FALSE);
    else {
        button = set_button(OK, hbuttonbox, 0, flag = !Xdialog.default_no);
        if (flag)
            gtk_widget_grab_focus(button);
        ok_button = button;
    }
    if (Xdialog.cancel_button) {
        button = set_button(CANCEL, hbuttonbox, 1,
                            flag = Xdialog.default_no && !Xdialog.wizard);
        if (flag)
            gtk_widget_grab_focus(button);
    }
    if (Xdialog.wizard) {
        button = set_button(NEXT, hbuttonbox, 0, TRUE);
        gtk_widget_grab_focus(button);
        ok_button = button;
    }
    if (Xdialog.help)
        set_button(HELP, hbuttonbox, 2, FALSE);

    /* Setup callbacks */
    g_signal_connect (G_OBJECT(Xdialog.window), "destroy",
                      G_CALLBACK(destroy_event), NULL);
    g_signal_connect (G_OBJECT(Xdialog.window), "delete_event",
                      G_CALLBACK(delete_event), NULL);
    if (ok_button) {
        g_signal_connect (G_OBJECT(ok_button), "clicked",
                          G_CALLBACK(filesel_exit), filesel);
    }
    /* Beep if requested */
    if (Xdialog.beep & BEEP_BEFORE && Xdialog.exit_code != 2)
        gdk_beep();

    /* Default exit code */
    Xdialog.exit_code = 255;

    set_timeout();
}


// ------------------------------------------------------------------------------------------
//                            create_colorsel
// ------------------------------------------------------------------------------------------

void create_colorsel(gchar *optarg, int *rgb)
{
    GtkWidget *colorsel_dlg;
    GtkWidget *colorsel;
    GtkWidget *box, *label;
    GtkWidget *ok_button;
    GtkWidget *cancel_button;
    GtkWidget *help_button = NULL;
#if GTK_CHECK_VERSION(3,4,0)
    GdkRGBA color;
#elif GTK_CHECK_VERSION(2,0,0)
    GdkColor gcolor;
#else
    double color[4];
#endif
    font_init();

    parse_rc_file();

    /* Create a color selector and update Xdialog structure accordingly */
#if GTK_CHECK_VERSION(3,4,0)
    colorsel_dlg = gtk_color_chooser_dialog_new (Xdialog.title, NULL);
    colorsel = colorsel_dlg;
#else // GTK2 / GTK1
    colorsel_dlg = gtk_color_selection_dialog_new (Xdialog.title);
# if GTK_CHECK_VERSION(2,14,0)
    colorsel = gtk_color_selection_dialog_get_color_selection (GTK_COLOR_SELECTION_DIALOG(colorsel_dlg));
    ///g_object_get (colorsel_dlg, "color-selection", &colorsel, NULL);
# else 
    colorsel = GTK_COLOR_SELECTION_DIALOG(colorsel_dlg)->colorsel;
# endif
#endif
    Xdialog.window = colorsel_dlg;

#if GTK_CHECK_VERSION(2,0,0)
    Xdialog.vbox = GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (Xdialog.window)));
#else // -- GTK1 --: GtkColorSelectionDialog derives from GtkWindow
    Xdialog.vbox = GTK_BOX (GTK_COLOR_SELECTION_DIALOG(colorsel_dlg)->main_vbox);
#endif

#if GTK_CHECK_VERSION(3,4,0)
    color.red = color.green = color.blue = color.alpha = 1.0;
    if (rgb[0] > 0) color.red   = (double) rgb[0] / 256.0;
    if (rgb[1] > 0) color.green = (double) rgb[1] / 256.0;
    if (rgb[2] > 0) color.blue  = (double) rgb[2] / 256.0;
    gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER(colorsel), &color);

#elif GTK_CHECK_VERSION(2,0,0)
    gcolor.red   = rgb[0] * 256;
    gcolor.green = rgb[1] * 256;
    gcolor.blue  = rgb[2] * 256;
    gtk_color_selection_set_current_color (GTK_COLOR_SELECTION(colorsel), &gcolor);
    gtk_color_selection_set_has_palette (GTK_COLOR_SELECTION(colorsel), TRUE);

#else // -- GTK1 --
    // the ColorSelection doesn't work as expected unless the color is 100% white
    color[0] = color[1] = color[2] = color[3] = 1.0;
    // apply color only if rgb > 0
    if (rgb[0] > 0) color[0] = (double) rgb[0] / 256.0;
    if (rgb[1] > 0) color[1] = (double) rgb[1] / 256.0;
    if (rgb[2] > 0) color[2] = (double) rgb[2] / 256.0;
    gtk_color_selection_set_color (GTK_COLOR_SELECTION(colorsel), color);
    // not using opacity (colors[3])
    gtk_color_selection_set_opacity (GTK_COLOR_SELECTION(colorsel), FALSE);
    gtk_widget_set_sensitive (GTK_COLOR_SELECTION(colorsel)->opacity_label, FALSE);
    gtk_widget_set_sensitive (GTK_COLOR_SELECTION(colorsel)->scales[6], FALSE);
    gtk_widget_set_sensitive (GTK_COLOR_SELECTION(colorsel)->entries[6], FALSE);
#endif

    /* We must realize the widget before moving it and creating the icon and buttons pixbufs... */
    gtk_widget_realize(Xdialog.window);

    /* Set the text */
    label = set_label(optarg, TRUE);
    box = gtk_widget_get_parent (label);
    gtk_box_reorder_child(Xdialog.vbox, box, 0);

    /* Set the backtitle */
    set_backtitle(TRUE);

    /* If requested, add a check button into the colorsel_dlg action area */
#if GTK_CHECK_VERSION(2,0,0)
    set_check_button (gtk_dialog_get_action_area (GTK_DIALOG (colorsel_dlg)));
#else // -- GTK1 --: GtkColorSelectionDialog doesn't have action area
    set_check_button (GTK_WIDGET(Xdialog.vbox));
#endif

    /* Set the window size and placement policy */
    set_window_size_and_placement();

#if GTK_CHECK_VERSION(3,4,0)
    // this also works with gtk2
    ok_button     = gtk_dialog_get_widget_for_response (GTK_DIALOG(colorsel_dlg), GTK_RESPONSE_OK);
    cancel_button = gtk_dialog_get_widget_for_response (GTK_DIALOG(colorsel_dlg), GTK_RESPONSE_CANCEL);
#elif GTK_CHECK_VERSION(2,14,0)
    // these properties were added after 2.4.0 (need to know what version)
    g_object_get (colorsel_dlg, "ok-button",     &ok_button,     NULL);
    g_object_get (colorsel_dlg, "cancel-button", &cancel_button, NULL);
    g_object_get (colorsel_dlg, "help-button",   &help_button,   NULL);
#else // -- GTK1 --
    ok_button     = GTK_COLOR_SELECTION_DIALOG(colorsel_dlg)->ok_button;
    cancel_button = GTK_COLOR_SELECTION_DIALOG(colorsel_dlg)->cancel_button;
    help_button   = GTK_COLOR_SELECTION_DIALOG(colorsel_dlg)->help_button;
#endif

    if (Xdialog.default_no)
        gtk_widget_grab_focus(cancel_button);

    if (Xdialog.ok_label && *Xdialog.ok_label) {
        gtk_button_set_label (GTK_BUTTON (ok_button), Xdialog.ok_label);
    }
    if (Xdialog.cancel_label && *Xdialog.cancel_label) {
        gtk_button_set_label (GTK_BUTTON (cancel_button), Xdialog.cancel_label);
    }

    /* Setup callbacks */
    g_signal_connect (G_OBJECT(Xdialog.window), "destroy",
                      G_CALLBACK(destroy_event), NULL);
    g_signal_connect (G_OBJECT(Xdialog.window), "delete_event",
                      G_CALLBACK(delete_event), NULL);
    g_signal_connect (G_OBJECT(ok_button), "clicked",
                      G_CALLBACK(colorsel_exit), G_OBJECT(colorsel));
    g_signal_connect (G_OBJECT(cancel_button), "clicked",
                      G_CALLBACK(exit_cancel), NULL);
    if (Xdialog.help) {
        g_signal_connect (G_OBJECT(help_button), "clicked",
                          G_CALLBACK(exit_help), NULL);
    } else if (help_button) {
        gtk_widget_destroy (help_button);
    }

    /* Beep if requested */
    if (Xdialog.beep & BEEP_BEFORE && Xdialog.exit_code != 2)
        gdk_beep();

    /* Default exit code */
    Xdialog.exit_code = 255;

    set_timeout();
}


// ------------------------------------------------------------------------------------------
//                            create_fontsel
// ------------------------------------------------------------------------------------------

void create_fontsel(gchar *optarg)
{
    GtkWidget * fontsel_dlg;
    GtkWidget * fontsel;
    GtkWidget * ok_button, * cancel_button;
#if GTK_MAJOR_VERSION <= 2
    GtkWidget *apply_button = NULL;
#endif
    font_init();
    parse_rc_file();

    /* Create a font selector and update Xdialog structure accordingly */
#if GTK_CHECK_VERSION(3,2,0)
    fontsel_dlg = gtk_font_chooser_dialog_new (Xdialog.title, NULL);
    fontsel = fontsel_dlg;
#else // GTK2 / GTK1
    fontsel_dlg = gtk_font_selection_dialog_new (Xdialog.title);
# if GTK_CHECK_VERSION(2,22,0)
    fontsel = gtk_font_selection_dialog_get_font_selection (GTK_FONT_SELECTION_DIALOG(fontsel_dlg));
# else
    fontsel = GTK_FONT_SELECTION_DIALOG(fontsel_dlg)->fontsel;
# endif
#endif
    Xdialog.window = fontsel_dlg;

#if GTK_CHECK_VERSION(2,0,0)
    Xdialog.vbox = GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (fontsel_dlg)));
#else // -- GTK1 --: GtkFontSelectionDialog derives from GtkWindow
    Xdialog.vbox = GTK_BOX (GTK_FONT_SELECTION_DIALOG(fontsel_dlg)->main_vbox);
#endif

    /* Set the backtitle */
    set_backtitle(FALSE);

    /* Set the default font name */
#if GTK_CHECK_VERSION(3,2,0)
    gtk_font_chooser_set_font (GTK_FONT_CHOOSER(fontsel), optarg);
    gtk_font_chooser_set_preview_text (GTK_FONT_CHOOSER(fontsel), "abcdefghijklmnopqrstuvwxyz 0123456789");
#else
    gtk_font_selection_set_font_name (GTK_FONT_SELECTION(fontsel), optarg);
    ///gtk_font_selection_dialog_set_font_name (GTK_FONT_SELECTION_DIALOG(fontsel_dlg), optarg);
    gtk_font_selection_set_preview_text (GTK_FONT_SELECTION(fontsel), "abcdefghijklmnopqrstuvwxyz 0123456789");
    ///gtk_font_selection_dialog_set_preview_text (GTK_FONT_SELECTION_DIALOG(fontsel_dlg), "abcdefghijklmnopqrstuvwxyz 0123456789");
#endif

    /* If requested, add a check button into the fontsel action area */
#if GTK_CHECK_VERSION(2,0,0)
    set_check_button (gtk_dialog_get_action_area (GTK_DIALOG (fontsel_dlg)));
#else // -- GTK1 --: GtkFontSelectionDialog derives from GtkWindow
    set_check_button (GTK_FONT_SELECTION_DIALOG(fontsel_dlg)->action_area);
#endif

    /* We must realize the widget before moving it and creating the buttons pixbufs */
    gtk_widget_realize(Xdialog.window);

    /* Set the window size and placement policy */
    set_window_size_and_placement();

#if GTK_CHECK_VERSION(3,2,0)
    // this also works with gtk2
    ok_button     = gtk_dialog_get_widget_for_response (GTK_DIALOG(fontsel_dlg), GTK_RESPONSE_OK);
    cancel_button = gtk_dialog_get_widget_for_response (GTK_DIALOG(fontsel_dlg), GTK_RESPONSE_CANCEL);
#elif GTK_CHECK_VERSION(2,14,0)
    ok_button     = gtk_font_selection_dialog_get_ok_button     (GTK_FONT_SELECTION_DIALOG(fontsel_dlg));
    cancel_button = gtk_font_selection_dialog_get_cancel_button (GTK_FONT_SELECTION_DIALOG(fontsel_dlg));
    apply_button  = gtk_font_selection_dialog_get_apply_button  (GTK_FONT_SELECTION_DIALOG(fontsel_dlg));
    gtk_widget_destroy (apply_button);
#else
    ok_button     = GTK_FONT_SELECTION_DIALOG(fontsel_dlg)->ok_button;
    cancel_button = GTK_FONT_SELECTION_DIALOG(fontsel_dlg)->cancel_button;
    apply_button  = GTK_FONT_SELECTION_DIALOG(fontsel_dlg)->apply_button;
    gtk_widget_destroy (apply_button);
#endif

    if (Xdialog.default_no)
        gtk_widget_grab_focus(cancel_button);

    if (Xdialog.ok_label && *Xdialog.ok_label) {
        gtk_button_set_label (GTK_BUTTON (ok_button), Xdialog.ok_label);
    }
    if (Xdialog.cancel_label && *Xdialog.cancel_label) {
        gtk_button_set_label (GTK_BUTTON (cancel_button), Xdialog.cancel_label);
    }

    /* Setup callbacks */
    g_signal_connect (G_OBJECT(Xdialog.window), "destroy",
                      G_CALLBACK(destroy_event), NULL);
    g_signal_connect (G_OBJECT(Xdialog.window), "delete_event",
                      G_CALLBACK(delete_event), NULL);
    g_signal_connect (G_OBJECT(ok_button),
                      "clicked", G_CALLBACK(fontsel_exit), fontsel_dlg);
    g_signal_connect (G_OBJECT(cancel_button),
                      "clicked", G_CALLBACK(exit_cancel), fontsel_dlg);

    /* Beep if requested */
    if (Xdialog.beep & BEEP_BEFORE && Xdialog.exit_code != 2)
        gdk_beep();

    /* Default exit code */
    Xdialog.exit_code = 255;

    set_timeout();
}


// ------------------------------------------------------------------------------------------
//                             create_calendar
// ------------------------------------------------------------------------------------------

void create_calendar(gchar *optarg, gint day, gint month, gint year)
{
    GtkCalendar *calendar;
    GtkWidget *button_ok;
    gint flags;

    open_window();

    set_backtitle(TRUE);
    set_label(optarg, FALSE);

    flags = GTK_CALENDAR_SHOW_HEADING | GTK_CALENDAR_SHOW_DAY_NAMES | \
            GTK_CALENDAR_SHOW_WEEK_NUMBERS;

    Xdialog.widget1 = gtk_calendar_new();
    gtk_box_pack_start (Xdialog.vbox, Xdialog.widget1, TRUE, TRUE, 5);

    calendar = GTK_CALENDAR(Xdialog.widget1);
    gtk_calendar_set_display_options(calendar, flags);

    gtk_calendar_select_month(calendar, month-1, year);
    gtk_calendar_select_day(calendar, day);

    button_ok = set_all_buttons(FALSE, TRUE);
    g_signal_connect (G_OBJECT(button_ok), "clicked", G_CALLBACK(calendar_exit), NULL);
    g_signal_connect (G_OBJECT(Xdialog.widget1), "day_selected_double_click",
                      G_CALLBACK(calendar_exit), NULL);
    g_signal_connect_after(G_OBJECT(Xdialog.widget1), "day_selected_double_click",
                      G_CALLBACK(exit_ok), NULL);

    if (Xdialog.interval > 0) {
        Xdialog.timer = g_timeout_add(Xdialog.interval, calendar_timeout, NULL);
    }
    set_timeout();
}


// ------------------------------------------------------------------------------------------
//                             create_timebox
// ------------------------------------------------------------------------------------------

void create_timebox(gchar *optarg, gint hours, gint minutes, gint seconds)
{
    GtkWidget *frame;
    GtkWidget *hbox;
    GtkWidget *button_ok;

    open_window();

    set_backtitle(TRUE);
    set_label(optarg, TRUE);

    frame = gtk_frame_new(TIME_FRAME_LABEL);
    gtk_frame_set_label_align(GTK_FRAME(frame), 0.5, 0.5);
    gtk_box_pack_start (Xdialog.vbox, frame, TRUE, TRUE, ymult);

    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_container_add(GTK_CONTAINER(frame), hbox);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), ymult);

    Xdialog.widget1 = set_spin_button(hbox, 0, 23, hours, 2, ":",  FALSE);
    Xdialog.widget2 = set_spin_button(hbox, 0, 59, minutes,  2, ":",  FALSE);
    Xdialog.widget3 = set_spin_button(hbox, 0, 59, seconds,  2, NULL, FALSE);

    button_ok = set_all_buttons(FALSE, TRUE);
    g_signal_connect (G_OBJECT(button_ok), "clicked", G_CALLBACK(timebox_exit), NULL);

    if (Xdialog.interval > 0) {
        Xdialog.timer = g_timeout_add(Xdialog.interval, timebox_timeout, NULL);
    }
    set_timeout();
}

