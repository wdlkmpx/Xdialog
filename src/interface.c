/*
 * GTK+ interface functions for Xdialog.
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "interface.h"
#include "callbacks.h"
#include "support.h"

/* Global structure and variables */
extern Xdialog_data Xdialog;
extern gboolean dialog_compat;

/* Fixed font loading and character size (in pixels) initialisation */
static PangoFontDescription *fixed_pango_font;

static PangoFontDescription *fixed_font;
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
	GtkWidget *window;
	GtkStyle  *style;
	GdkFont *font;
	gint width, ascent, descent, lbearing, rbearing;
	/* fixed font support by 01micko */
	fixed_font = pango_font_description_new ();
	pango_font_description_set_family (fixed_font, FIXED_FONT);
	pango_font_description_set_weight (fixed_font, PANGO_WEIGHT_MEDIUM);
	pango_font_description_set_size (fixed_font, 10*PANGO_SCALE);
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
			gdk_string_extents(font, ALPHANUM_CHARS, &lbearing,
					   &rbearing, &width, &ascent, &descent);
			xmult = width / 62;		/* 62 = strlen(ALPHANUM_CHARS) */
			ymult = ascent + descent + 2;	/*  2 = spacing pixel lines */
		}
		gtk_widget_destroy(window);
	}
	fixed_pango_font = pango_font_description_new ();
	pango_font_description_set_family (fixed_pango_font, "mono");
	pango_font_description_set_size (fixed_pango_font, 10 * PANGO_SCALE);
}

/* Custom text wrapping (the GTK+ one is buggy) */

static void wrap_text(gchar *str, gint reserved_width)
{
	gint max_line_width, n = 0;
	gchar *p = str, *last_space = NULL;
	gchar tmp[MAX_LABEL_LENGTH];
	GdkFont *current_font = gtk_style_get_font(Xdialog.window->style);

	if (Xdialog.xsize != 0)
		max_line_width = (Xdialog.size_in_pixels ? Xdialog.xsize :
							   Xdialog.xsize * xmult)
				  - reserved_width - 4 * xmult;
	else
		max_line_width =  gdk_screen_width() - reserved_width - 6 * xmult;

	do {
		if (*p == '\n') {
			n = 0;
			last_space = NULL;
		} else {
			tmp[n++] = *p;
			tmp[n] = 0;
			if (gdk_string_width(current_font, tmp) < max_line_width) {
				if (*p == ' ')
					last_space = p;
			} else {
				if (last_space != NULL) {
					*last_space = '\n';
					p = last_space;
					n = 0;
					last_space = NULL;
				} else if (*p == ' ')
					last_space = p;
			}
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

	/* Allow the window to grow, shrink and auto-shrink */
	gtk_window_set_policy(GTK_WINDOW(Xdialog.window), TRUE, TRUE, TRUE);

	/* Set the window placement policy */
	if (Xdialog.set_origin)
		gdk_window_move(Xdialog.window->window,
				Xdialog.xorg >= 0 ? (Xdialog.size_in_pixels ? Xdialog.xorg : Xdialog.xorg*xmult) :
						    gdk_screen_width()  + Xdialog.xorg - Xdialog.xsize - 2*xmult,
				Xdialog.yorg >= 0 ? (Xdialog.size_in_pixels ? Xdialog.yorg : Xdialog.yorg*ymult) :
						    gdk_screen_height() + Xdialog.yorg - Xdialog.ysize - 3*ymult);
	else
		gtk_window_set_position(GTK_WINDOW(Xdialog.window), Xdialog.placement);
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
		gtk_window_set_wmclass(GTK_WINDOW(window), Xdialog.wmclass,
				       Xdialog.wmclass);

	/* Set default events handlers */
	g_signal_connect (G_OBJECT(window), "destroy",
			   G_CALLBACK(destroy_event), NULL);
	g_signal_connect (G_OBJECT(window), "delete_event",
			   G_CALLBACK(delete_event), NULL);

	/* Set the window title */
	gtk_window_set_title(GTK_WINDOW(window), Xdialog.title);

	/* Set the internal border so that the child widgets do not
	 * expand to the whole window (prettier) */
	gtk_container_set_border_width(GTK_CONTAINER(window), xmult/2);

	/* Create the root vbox widget in which all other boxes will
	 * be packed. By setting the "homogeneous" parameter to false,
	 * we allow packing with either gtk_box_pack_start() or
	 * gtk_box_pack_end() and disallow any automatic repartition
	 * of additional space between the child boxes (each box may
	 * therefore be set so to expand or not): this is VERY important
	 * and should not be changed !
	 */
	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(window), vbox);
	gtk_widget_show(vbox);
	Xdialog.vbox = GTK_BOX(vbox);

	gtk_widget_realize(Xdialog.window);

	/* Set the window size and placement policy */
	set_window_size_and_placement();

	if (Xdialog.beep & BEEP_BEFORE && Xdialog.exit_code != 2)
		gdk_beep();

	Xdialog.exit_code = 255;
}

static GtkWidget *set_separator(gboolean from_start)
{
	GtkWidget *separator;

	separator = gtk_hseparator_new();
	if (from_start)
		gtk_box_pack_start(Xdialog.vbox, separator, FALSE, TRUE, ymult/3);
	else
		gtk_box_pack_end(Xdialog.vbox, separator, FALSE, TRUE, ymult/3);
	gtk_widget_show(separator);

	return separator;
}

static void set_backtitle(gboolean sep_flag)
{
	GtkWidget *label;
	GtkWidget *hbox;
	GtkWidget *separator;
	gchar     backtitle[MAX_BACKTITLE_LENGTH];

	if (strlen(Xdialog.backtitle) == 0)
		return;

	if (dialog_compat)
		backslash_n_to_linefeed(Xdialog.backtitle, backtitle, MAX_BACKTITLE_LENGTH);
	else
		trim_string(Xdialog.backtitle, backtitle, MAX_BACKTITLE_LENGTH);

	if (Xdialog.wrap || dialog_compat)
		wrap_text(backtitle, 2*ymult/3);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(Xdialog.vbox, hbox, FALSE, FALSE, ymult/3);
	gtk_box_reorder_child(Xdialog.vbox, hbox, 0);
	label = gtk_label_new(backtitle);
	gtk_container_add(GTK_CONTAINER(hbox), label);
	gtk_widget_show(hbox);
	gtk_widget_show(label);
	if (sep_flag) {
		separator = set_separator(TRUE);
		gtk_box_reorder_child(Xdialog.vbox, separator, 1);
	}
}

static GtkWidget *set_label(gchar *label_text, gboolean expand)
{
	GtkWidget *label;
	GtkWidget *hbox;
	GdkBitmap *mask;
	GdkColor  transparent;
	GdkPixmap *pixmap;
	GtkWidget *icon;
	gchar     text[MAX_LABEL_LENGTH];
	int icon_width = 0;

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(Xdialog.vbox, hbox, expand, TRUE, ymult/3);

	if (Xdialog.icon) {
		pixmap = gdk_pixmap_create_from_xpm(Xdialog.window->window,
						    &mask, &transparent,
						    Xdialog.icon_file);
		if (pixmap != NULL) {
			icon = gtk_pixmap_new(pixmap, mask);
			g_object_unref(pixmap);
			g_object_unref(mask);
			gtk_box_pack_start(GTK_BOX(hbox), icon, FALSE, FALSE, 2);
			gtk_widget_show(icon);
			icon_width = icon->requisition.width + 4;
		}
	}

	trim_string(label_text, text, MAX_LABEL_LENGTH);

	if (Xdialog.wrap || dialog_compat)
		wrap_text(text, icon_width + 2*ymult/3);

	label = gtk_label_new(text);

	if (Xdialog.justify == GTK_JUSTIFY_FILL)
		gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);

	gtk_label_set_justify(GTK_LABEL(label), Xdialog.justify);

	gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, ymult/3);

	gtk_widget_show(hbox);
	gtk_widget_show(label);

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

	if (Xdialog.justify == GTK_JUSTIFY_FILL)
		gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);

	gtk_label_set_justify(GTK_LABEL(label), Xdialog.justify);
	switch (Xdialog.justify) {
		case GTK_JUSTIFY_LEFT:
			gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
			break;
		case GTK_JUSTIFY_RIGHT:
			gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
			break;
	}

	gtk_box_pack_start(Xdialog.vbox, label, expand, TRUE, ymult/3);
	gtk_widget_show(label);

	return label;
}

static GtkWidget *set_hbuttonbox(void)
{
	GtkWidget *hbuttonbox;

	hbuttonbox = gtk_hbutton_box_new();
	gtk_box_pack_end(Xdialog.vbox, hbuttonbox, FALSE, FALSE, ymult/4);
 	gtk_button_box_set_layout(GTK_BUTTON_BOX(hbuttonbox), GTK_BUTTONBOX_SPREAD);
	gtk_widget_show(hbuttonbox);

	return hbuttonbox;
}

static GtkWidget *set_button(gchar *default_text, gpointer buttonbox, gint event,
			     gboolean grab_default)
{
	GtkWidget *button;
	gchar	  *stock_id = NULL;
	GtkWidget *icon;
	GtkWidget *hbox;
	GtkWidget *label;
	gchar     *text = default_text;

	if (Xdialog.buttons_style != TEXT_ONLY) {
		if (!strcmp(text, OK) || !strcmp(text, YES))
			stock_id = "gtk-ok";
		else if (!strcmp(text, CANCEL) || !strcmp(text, NO))
			stock_id = "gtk-close";
		else if (!strcmp(text, HELP))
			stock_id = "gtk-help";
		else if (!strcmp(text, EXTRA))
			stock_id = "gtk-execute";
		else if (!strcmp(text, PRINT))
			stock_id = "gtk-print";
		else if (!strcmp(text, NEXT) || !strcmp(text, ADD))
			stock_id = "gtk-go-forward";
		else if (!strcmp(text, PREVIOUS) || !strcmp(text, REMOVE))
			stock_id = "gtk-go-back";
	}

	if (strlen(Xdialog.ok_label) != 0 && (!strcmp(text, OK) || !strcmp(text, YES)))
		text = Xdialog.ok_label;
	if (strlen(Xdialog.extra_label) != 0 && (!strcmp(text, EXTRA)))
		text = Xdialog.extra_label;
	else if (strlen(Xdialog.cancel_label) != 0 && (!strcmp(text, CANCEL) || !strcmp(text, NO)))
		text = Xdialog.cancel_label;

	if (Xdialog.buttons_style == TEXT_ONLY)
		button = gtk_button_new_with_label(text);
	else {			/* buttons with icons */
		button = gtk_button_new();
		hbox = gtk_hbox_new(FALSE, 2);
		gtk_container_add(GTK_CONTAINER(button), hbox);
		gtk_widget_show(hbox);

		icon = gtk_image_new_from_stock(stock_id, GTK_ICON_SIZE_BUTTON);
		gtk_container_add(GTK_CONTAINER(hbox), icon);
		gtk_widget_show(icon);

		if (Xdialog.buttons_style != ICON_ONLY) {	/* icons + text */
			label = gtk_label_new(text);
			gtk_container_add(GTK_CONTAINER(hbox), label);
			gtk_widget_show(label);
		}
	}

	gtk_container_add(GTK_CONTAINER(buttonbox), button);
	GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);

	switch (event) {
		case 0:
			g_signal_connect_after(G_OBJECT(button), "clicked",
						 G_CALLBACK(exit_ok),
						 NULL);
			break;
		case 1:
			g_signal_connect_after(G_OBJECT(button), "clicked",
						 G_CALLBACK(exit_cancel),
						 NULL);
			g_signal_connect_after(G_OBJECT(Xdialog.window), "key_press_event",
						 G_CALLBACK(exit_keypress), NULL);

			break;
		case 2:
			g_signal_connect_after(G_OBJECT(button), "clicked",
						 G_CALLBACK(exit_help),
						 NULL);
			break;
		case 3:
			g_signal_connect_after(G_OBJECT(button), "clicked",
						 G_CALLBACK(exit_previous),
						 NULL);
			break;
		case 4:
			g_signal_connect_after(G_OBJECT(button), "clicked",
						 G_CALLBACK(print_text),
						 NULL);
			break;
		case 5:
			gtk_signal_connect_after(GTK_OBJECT(button), "clicked",
						 GTK_SIGNAL_FUNC(exit_extra),
						 NULL);
			break;

	}
	if (grab_default) {
		gtk_widget_set_can_default(button, TRUE);
		gtk_widget_grab_default(button);
	}

	gtk_widget_show(button);

	return button;
}

static void set_check_button(GtkWidget *box)
{
	GtkWidget *button;
	GtkWidget *hbox;
	gchar     check_label[MAX_LABEL_LENGTH];

	if (!Xdialog.check)
		return;

	trim_string(Xdialog.check_label, check_label, MAX_LABEL_LENGTH);

	if (box == NULL) {
		set_separator(FALSE);
		hbox = gtk_hbox_new(FALSE, 2);
		gtk_box_pack_end(Xdialog.vbox, hbox, FALSE, FALSE, 0);
		gtk_widget_show(hbox);
		set_separator(FALSE);
	} else
		hbox = box;

	button = gtk_check_button_new_with_label(check_label);
	gtk_container_add(GTK_CONTAINER(hbox), button);
	gtk_widget_show(button);

	if (Xdialog.checked)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);

	g_signal_connect (G_OBJECT(button), "toggled",
			   G_CALLBACK(checked), NULL);
}

static GtkWidget *set_all_buttons(gboolean print, gboolean ok)
{
	GtkWidget *hbuttonbox; 
	GtkWidget *button_ok = NULL;

	hbuttonbox = set_hbuttonbox();

	set_check_button(NULL);

	if (Xdialog.wizard)
		set_button(PREVIOUS , hbuttonbox, 3, FALSE);
	else {
		if(Xdialog.extra_button)
			button_ok = set_button(EXTRA, hbuttonbox, 5, !Xdialog.default_no);

		if (ok)
			button_ok = set_button(OK, hbuttonbox, 0, !Xdialog.default_no);
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
	GtkWidget *scrollwin;

	GtkStyle  *style;

	scrollwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_show (scrollwin);
	gtk_box_pack_start (GTK_BOX(Xdialog.vbox), scrollwin, TRUE, TRUE, 0);

	text = gtk_text_view_new();

	if (Xdialog.fixed_font) {
		style = gtk_style_new();
		style->font_desc = fixed_font;
		gtk_widget_modify_font(text, style->font_desc);
		//gtk_widget_modify_font(text, fixed_pango_font);
	}

	gtk_widget_show(text);
	gtk_container_add(GTK_CONTAINER (scrollwin), text);
	return text;
}

static GtkWidget *set_scrolled_window(GtkBox *box, gint border_width, gint xsize,
				      gint list_size, gint spacing)
{
	GtkWidget *scrolled_window;

	scrolled_window = gtk_scrolled_window_new(NULL, NULL);
	gtk_container_set_border_width(GTK_CONTAINER(scrolled_window), border_width);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
				       GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start(box, scrolled_window, TRUE, TRUE, 0);
	gtk_widget_show(scrolled_window);

	if (Xdialog.list_height > 0)
		gtk_widget_set_size_request(scrolled_window, xsize > 0 ? xsize*xmult : -1,
				     Xdialog.list_height * (ymult + spacing));
	else
		gtk_widget_set_size_request(scrolled_window, xsize > 0 ? xsize*xmult : -1,
				     MIN(gdk_screen_height() - 15 * ymult,
					 list_size * (ymult + spacing)));

	return scrolled_window;
}

static GtkWidget *set_scrolled_list(GtkWidget *box, gint xsize, gint list_size,
				    gint spacing, GtkListStore *store)
{
	GtkWidget *scrolled_window;
	GtkWidget *list;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkTreeSelection* selection;

	scrolled_window = set_scrolled_window(GTK_BOX(box), 0, xsize,
		list_size, spacing);
	list = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes("", renderer, "text",
		0, NULL);

	gtk_tree_view_append_column(GTK_TREE_VIEW(list), column);

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(list));
	/* TODO: This seems to screw things up. At the moment it is not important
	   to have multiple selections */
	/* gtk_tree_selection_set_mode(selection, GTK_SELECTION_MULTIPLE); */

	gtk_widget_show(list);
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolled_window),
		list);

	return list;
}

static GtkObject *set_horizontal_slider(GtkBox *box, gint deflt, gint min, gint max)
{
	GtkWidget *align;
	GtkAdjustment *adj;
	GtkWidget *hscale;
	GtkObject *slider;

	align = gtk_alignment_new(0.5, 0.5, 0.8, 0);
	gtk_box_pack_start(box, align, FALSE, FALSE, 5);
	gtk_widget_show(align);

	/* Create an adjusment object to hold the range of the scale */
	slider = gtk_adjustment_new(deflt, min, max, 1, 1, 0);
	adj = GTK_ADJUSTMENT(slider);
 	hscale = gtk_hscale_new(adj);
	gtk_scale_set_digits(GTK_SCALE(hscale), 0);
	gtk_container_add(GTK_CONTAINER(align), hscale);
	gtk_widget_show(hscale);

	return slider;
}

static GtkWidget *set_spin_button(GtkWidget *hbox, gint min, gint max, gint deflt,
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
	gtk_box_pack_start(GTK_BOX(hbox), spin, FALSE, FALSE, 0);
	gtk_widget_show(spin);
	if (separator != NULL && strlen(separator) != 0) {
		label = gtk_label_new(separator);
		if (align_left) {
			gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
			gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
		}
		gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, xmult);
		gtk_widget_show(label);
	}

	return spin;
}

static int item_status(GtkWidget *item, char *status, char *tag)
{
		if (!strcasecmp(status, "on") && strlen(tag) != 0)
			return 1;

		if (!strcasecmp(status, "unavailable") || strlen(tag) == 0) {
			gtk_widget_set_sensitive(item, FALSE);
			return -1;
		}
		return 0;
}

static void set_timeout(void)
{
	if (Xdialog.timeout > 0)
		Xdialog.timer2 = g_timeout_add(Xdialog.timeout*1000, timeout_exit, NULL);
}

/*
 * The Xdialog widgets...
 */

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
	} else 
		set_button(OK, hbuttonbox, 0, TRUE);

	if (Xdialog.help)
		set_button(HELP, hbuttonbox, 2, FALSE);

	set_timeout();
}


void create_infobox(gchar *optarg, gint timeout)
{
	GtkWidget *hbuttonbox;

	open_window();

	set_backtitle(TRUE);
	Xdialog.widget1 = set_label(optarg, TRUE);

	if (Xdialog.buttons && !dialog_compat) {
		hbuttonbox = set_hbuttonbox();
		set_button(timeout > 0 ? OK : CANCEL, hbuttonbox,
			   timeout > 0 ? 0 : 1, TRUE);
	}

	Xdialog.label_text[0] = 0;
	Xdialog.new_label = Xdialog.check = FALSE;

	if (timeout > 0)
		Xdialog.timer = g_timeout_add(timeout, infobox_timeout_exit, NULL);
	else
		Xdialog.timer = g_timeout_add(10, infobox_timeout, NULL);
}


void create_gauge(gchar *optarg, gint percent)
{
	GtkWidget *align;
	GtkProgress *pbar;
	GtkAdjustment *adj;
	int value;

	if (percent < 0)
		value = 0;
	else if (percent > 100)
		value = 100;
	else
		value = percent;

	open_window();

	set_backtitle(TRUE);
	Xdialog.widget2 = set_label(optarg, TRUE);

	align = gtk_alignment_new(0.5, 0.5, 0.8, 0);
	gtk_box_pack_start(Xdialog.vbox, align, FALSE, FALSE, ymult/2);
	gtk_widget_show(align);

	/* Create an Adjusment object to hold the range of the progress bar */
	adj = GTK_ADJUSTMENT(gtk_adjustment_new(0, 0, 100, 0, 0, 0));

	/* Set up the progress bar */
	Xdialog.widget1 = gtk_progress_bar_new_with_adjustment(adj);
	pbar = GTK_PROGRESS(Xdialog.widget1);
	/* Set the start value and the range of the progress bar */
	gtk_progress_configure(pbar, value, 0, 100);
	/* Set the format of the string that can be displayed in the
	 * trough of the progress bar:
	 * %p - percentage
	 * %v - value
	 * %l - lower range value
	 * %u - upper range value */
	gtk_progress_set_format_string(pbar, "%p%%");
	gtk_progress_set_show_text(pbar, TRUE);
	gtk_container_add(GTK_CONTAINER(align), Xdialog.widget1);
	gtk_widget_show(Xdialog.widget1);

	Xdialog.label_text[0] = 0;
	Xdialog.new_label = Xdialog.check = FALSE;

	/* Add a timer callback to update the value of the progress bar */
	Xdialog.timer = g_timeout_add(10, gauge_timeout, NULL);
}


void create_progress(gchar *optarg, gint leading, gint maxdots)
{
	GtkWidget *label;
	GtkWidget *align;
	GtkProgress *pbar;
	GtkAdjustment *adj;
	int ceiling, i;
	unsigned char temp[2];
	size_t rresult;

	if (maxdots <= 0)
		ceiling = 100;
	else
		ceiling = maxdots;

	open_window();

	set_backtitle(TRUE);

	trim_string(optarg, Xdialog.label_text, MAX_LABEL_LENGTH);
	label = set_label(Xdialog.label_text, TRUE);

	align = gtk_alignment_new(0.5, 0.5, 0.8, 0);
	gtk_box_pack_start(Xdialog.vbox, align, FALSE, FALSE, ymult/2);
	gtk_widget_show(align);

	/* Create an Adjusment object to hold the range of the progress bar */
	adj = GTK_ADJUSTMENT(gtk_adjustment_new(0, 0, 100, 0, 0, 0));

	/* Set up the progress bar */
	Xdialog.widget1 = gtk_progress_bar_new_with_adjustment(adj);
	pbar = GTK_PROGRESS(Xdialog.widget1);
	/* Set the start value and the range of the progress bar */
	gtk_progress_configure(pbar, 0, 0, ceiling);
	/* Set the format of the string that can be displayed in the
	 * trough of the progress bar:
	 * %p - percentage
	 * %v - value
	 * %l - lower range value
	 * %u - upper range value */
	gtk_progress_set_format_string(pbar, "%p%%");
	gtk_progress_set_show_text(pbar, TRUE);
	gtk_container_add(GTK_CONTAINER(align), Xdialog.widget1);
	gtk_widget_show(Xdialog.widget1);

	/* Skip the characters to be ignored on the input stream */
	if (leading < 0) {
		for (i=1; i < -leading; i++) {
			rresult = fread(temp, sizeof(char), 1, stdin);
			if (rresult < 0)
				break;
		}
	} else if (leading > 0) {
		for (i=1; i < leading; i++) {
			temp[0] = temp[1] = 0;
			rresult = fread(temp, sizeof(unsigned char), 1, stdin);
			if (rresult < 0)
				break;

			if (temp[0] >= ' ' || temp[0] == '\n')
				strncat(Xdialog.label_text, (char*)temp, sizeof(Xdialog.label_text));
		}
		gtk_label_set_text(GTK_LABEL(label), Xdialog.label_text);
	}

	Xdialog.check = FALSE;

	/* Add a timer callback to update the value of the progress bar */
	Xdialog.timer = g_timeout_add(10, progress_timeout, NULL);
}


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
		} else
			Xdialog.file_init_size = 0;
	}

	if (Xdialog.file == NULL) {
		fprintf(stderr, "Xdialog: can't open %s\n", optarg);
		exit(255);
	}

	if (dialog_compat)
		Xdialog.cancel_button = FALSE;

	if (Xdialog.buttons)
		set_all_buttons(TRUE, Xdialog.ok_button);

	Xdialog.timer = g_timeout_add(10, (GSourceFunc) tailbox_timeout, NULL);

	set_timeout();
}


void create_logbox(gchar *optarg)
{
	GtkCList *clist;
	GtkWidget *scrolled_window;
	gint xsize = 40;

	open_window();

	set_backtitle(FALSE);

	Xdialog.widget1 = gtk_clist_new(Xdialog.time_stamp ? 2 : 1);
	clist = GTK_CLIST(Xdialog.widget1);
	gtk_clist_set_selection_mode(clist, GTK_SELECTION_SINGLE);
	gtk_clist_set_shadow_type(clist, GTK_SHADOW_IN);
	if (Xdialog.time_stamp) {
		gtk_clist_set_column_title(clist, 0, Xdialog.date_stamp ? DATE_STAMP : TIME_STAMP);
		gtk_clist_set_column_title(clist, 1, LOG_MESSAGE);
		gtk_clist_column_title_passive(clist, 0);
		gtk_clist_column_title_passive(clist, 1);
		gtk_clist_column_titles_show(clist);
		xsize = (Xdialog.date_stamp ? 59 : 48);
	}
	/* We need to call gtk_clist_columns_autosize IOT avoid
	 * Gtk-WARNING **: gtk_widget_size_allocate(): attempt to allocate widget with width 41658 and height 1
	 * and similar warnings with GTK+ v1.2.9 (and perhaps with previous versions as well)...
	 */
	gtk_clist_columns_autosize(clist);
	g_signal_connect (G_OBJECT(Xdialog.widget1), "key_press_event",
			   G_CALLBACK(tailbox_keypress), NULL);
	gtk_widget_show(Xdialog.widget1);
	gtk_widget_grab_focus(Xdialog.widget1);

	scrolled_window = set_scrolled_window(Xdialog.vbox, xmult/2, xsize, 12, 2);
	gtk_container_add(GTK_CONTAINER(scrolled_window), Xdialog.widget1);

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
		} else
			Xdialog.file_init_size = 0;
	}

	if (Xdialog.buttons)
		set_all_buttons(FALSE, Xdialog.ok_button);

	Xdialog.timer = g_timeout_add(10, (GSourceFunc) logbox_timeout, NULL);

	set_timeout();
}


void create_textbox(gchar *optarg, gboolean editable)
{
	GtkTextView *text;
	GtkTextBuffer *text_buffer;
	GtkWidget *button_ok = NULL;
	FILE *infile;
	gint i, n = 0, llen = 0, lcnt = 0;

	open_window();

	set_backtitle(FALSE);

	Xdialog.widget1 = set_scrollable_text();
	gtk_widget_grab_focus(Xdialog.widget1);
	text = GTK_TEXT_VIEW(Xdialog.widget1);
	text_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text));

	/* Fill the GtkText with the text */
	if (strcmp(optarg, "-") == 0)
		infile = stdin;
	else
		infile = fopen(optarg, "r");
	if (infile) {
		char buffer[1024];
		int nchars;
		gsize bytes_read = 0;
		gsize bytes_written = 0;
		GError* error = 0;
		gchar* utf8 = 0;

		do {
			nchars = fread(buffer, 1, 1024, infile);
			utf8 = g_convert_with_fallback(buffer, nchars, "UTF-8", "ISO-8859-1", "\357\277\275", &bytes_read, &bytes_written, &error);
			if (error == NULL)
			{
				gtk_text_buffer_insert_at_cursor(text_buffer, utf8, nchars);
			}
			else
			{
				printf("Error converting to UTF-8: GConvertError(%02x): %s", error->code, error->message);
			}
			g_free(utf8);

			/* Calculate the maximum line length and lines count */
			for (i = 0; i < nchars; i++)
				if (buffer[i] != '\n') {
					if (buffer[i] == '\t')
						n += 8;
					else
						n++;
				} else {
					if (n > llen)
						llen = n;
					n = 0;
					lcnt++;
				}
		} while (nchars == 1024);

		if (infile != stdin)
			fclose(infile);
	}
	llen += 4;
	if (Xdialog.fixed_font)
		gtk_widget_set_size_request(Xdialog.widget1,
				     MIN(llen*ffxmult, gdk_screen_width()-4*ffxmult),
				     MIN(lcnt*ffymult, gdk_screen_height()-10*ffymult));
	else
		gtk_widget_set_size_request(Xdialog.widget1,
				     MIN(llen*xmult, gdk_screen_width()-4*xmult),
				     MIN(lcnt*ymult, gdk_screen_height()-10*ymult));

	/* Set the editable flag depending on what we want (text or edit box) */
	gtk_text_view_set_editable(text, editable);

    // position the cursor on the first line
    GtkTextIter firstLineIter;
    gtk_text_buffer_get_start_iter(text_buffer, &firstLineIter);
    gtk_text_buffer_place_cursor(text_buffer, &firstLineIter);

	if (dialog_compat && !editable)
		Xdialog.cancel_button = FALSE;

	/* Set the buttons */
	if (Xdialog.buttons || editable)
		button_ok = set_all_buttons(TRUE, TRUE);

	if (editable)
		g_signal_connect (G_OBJECT(button_ok), "clicked",
				   G_CALLBACK(editbox_ok), NULL);

	set_timeout();
}


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
	gtk_box_pack_start(Xdialog.vbox, Xdialog.widget1, TRUE, TRUE, 0);
	gtk_widget_grab_focus(Xdialog.widget1);
	gtk_widget_show(Xdialog.widget1);

	if (entries > 1) {
		set_secondary_label(options[2], FALSE);

		Xdialog.widget2 = gtk_entry_new();
		entry = GTK_ENTRY(Xdialog.widget2);
		gtk_entry_set_text(entry, options[3]);
		gtk_box_pack_start(Xdialog.vbox, Xdialog.widget2, TRUE, TRUE, 0);
		gtk_widget_show(Xdialog.widget2);
	} else
		Xdialog.widget2 = NULL;

	if (entries > 2) {
		set_secondary_label(options[4], FALSE);

		Xdialog.widget3 = gtk_entry_new();
		entry = GTK_ENTRY(Xdialog.widget3);
		gtk_entry_set_text(entry, options[5]);
		gtk_box_pack_start(Xdialog.vbox, Xdialog.widget3, TRUE, TRUE, 0);
		gtk_widget_show(Xdialog.widget3);
	} else
		Xdialog.widget3 = NULL;

	if ((Xdialog.passwd > 0 && Xdialog.passwd < 10) ||
            (Xdialog.passwd > 10 && Xdialog.passwd <= entries + 10)) {
		hide_button = gtk_check_button_new_with_label(HIDE_TYPING);
		gtk_box_pack_start(Xdialog.vbox, hide_button, TRUE, TRUE, 0);
		gtk_widget_show(hide_button);
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


void create_combobox(gchar *optarg, gchar *options[], gint list_size)
{
	GtkWidget *combo;
	GtkWidget *button_ok = NULL;
	GList *glist = NULL;
	int i;

	open_window();

	set_backtitle(TRUE);
	set_label(optarg, TRUE);

	combo = gtk_combo_box_entry_new_text(); 
	Xdialog.widget1 = GTK_ENTRY (GTK_BIN (combo)->child);
	Xdialog.widget2 = Xdialog.widget3 = NULL;
	gtk_box_pack_start(Xdialog.vbox, combo, TRUE, TRUE, 0);
	gtk_widget_grab_focus(Xdialog.widget1);
	gtk_widget_show(combo);
	//--- this is not needed???
	g_signal_connect (G_OBJECT(Xdialog.widget1), "key_press_event",
			   G_CALLBACK(input_keypress), NULL);

	/* Set the popdown strings */
	for (i = 0; i < list_size; i++) {
		gtk_combo_box_append_text(GTK_COMBO_BOX(combo), options[i]);
	}

	if (strlen(Xdialog.default_item) != 0) {
		gtk_combo_box_prepend_text(GTK_COMBO_BOX(combo), Xdialog.default_item);
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


void create_spinbox(gchar *optarg, gchar *options[], gint spins)
{
	GtkWidget *frame;
	GtkWidget *hbox;
	GtkWidget *button_ok;

	open_window();

	set_backtitle(TRUE);
	set_label(optarg, TRUE);

	frame = gtk_frame_new(NULL);
	gtk_box_pack_start(Xdialog.vbox, frame, TRUE, TRUE, ymult/2);
	gtk_widget_show(frame);

	hbox = gtk_hbox_new(TRUE, 0);
	gtk_container_add(GTK_CONTAINER(frame), hbox);
	gtk_container_set_border_width(GTK_CONTAINER(hbox), ymult);
	gtk_widget_show(hbox);

	Xdialog.widget1 = set_spin_button(hbox, atoi(options[0]), atoi(options[1]), atoi(options[2]),
					  strlen(options[1]), options[3], TRUE);
	if (spins > 1)
		Xdialog.widget2 = set_spin_button(hbox, atoi(options[4]), atoi(options[5]), atoi(options[6]),
						  strlen(options[5]), options[7], TRUE);
	else
		Xdialog.widget2 = NULL;

	if (spins > 2)
		Xdialog.widget3 = set_spin_button(hbox, atoi(options[8]), atoi(options[9]), atoi(options[10]),
						  strlen(options[9]), options[11], TRUE);
	else
		Xdialog.widget3 = NULL;

	button_ok = set_all_buttons(FALSE, TRUE);

	g_signal_connect (G_OBJECT(button_ok), "clicked", G_CALLBACK(spinbox_exit), NULL);

	if (Xdialog.interval > 0)
		Xdialog.timer = g_timeout_add(Xdialog.interval, spinbox_timeout, NULL);

	set_timeout();
}


void create_itemlist(gchar *optarg, gint type, gchar *options[], gint list_size)
{
	GtkWidget *vbox;
	GtkWidget *scrolled_window;
	GtkWidget *button_ok;
	GtkWidget *item;
	GtkRadioButton *radio = NULL;
	GtkTooltips *tooltips = NULL;
	char temp[MAX_ITEM_LENGTH];
	int i;
	int params = 3 + Xdialog.tips;

	if (Xdialog.tips == 1)
		tooltips = gtk_tooltips_new();

	Xdialog_array(list_size);

	open_window();

	set_backtitle(TRUE);
	set_label(optarg, FALSE);

	scrolled_window = set_scrolled_window(Xdialog.vbox, xmult/2, -1, list_size, ymult + 5);

	vbox = gtk_vbox_new(FALSE, xmult);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), xmult);
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolled_window), vbox);
	gtk_widget_show(vbox);

	button_ok = set_all_buttons(FALSE, TRUE);
	g_signal_connect (G_OBJECT(button_ok), "clicked", G_CALLBACK(print_items), NULL);

	for (i = 0;  i < list_size; i++) {
		strncpy(Xdialog.array[i].tag, options[params*i], sizeof(Xdialog.array[i].tag) );
		temp[0] = 0;
		if (Xdialog.tags && strlen(options[params*i]) != 0) {
			strncpy(temp, options[params*i], sizeof(temp));
			strncat(temp, ": ", sizeof(temp));
		}
		strncat(temp, options[params*i+1], sizeof(temp));

		if (type == CHECKLIST)
			item = gtk_check_button_new_with_label(temp);
		else {
			item = gtk_radio_button_new_with_label_from_widget(radio, temp);
			radio = GTK_RADIO_BUTTON(item);
		}
		gtk_box_pack_start(GTK_BOX(vbox), item, FALSE, FALSE, 0);
		gtk_widget_show(item);

		if (item_status(item, options[params*i+2], Xdialog.array[i].tag) == 1)
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(item), TRUE);

		g_signal_connect (G_OBJECT(item), "toggled",
				   G_CALLBACK(item_toggle), (gpointer)i);
		g_signal_connect (G_OBJECT(item), "button_press_event",
				   G_CALLBACK(double_click_event), button_ok);
		g_signal_emit_by_name (G_OBJECT(item), "toggled");

		if (Xdialog.tips == 1 && strlen(options[params*i+3]) > 0)
			gtk_tooltips_set_tip(tooltips, item, (gchar *) options[params*i+3], NULL);

	}

	if (Xdialog.interval > 0)
		Xdialog.timer = g_timeout_add(Xdialog.interval, itemlist_timeout, NULL);

	set_timeout();
}


void create_buildlist(gchar *optarg, gchar *options[], gint list_size)
{
	GtkWidget *hbox;
	GtkWidget *vbuttonbox;
	GtkWidget *button_add;
	GtkWidget *button_remove;
	GtkWidget *button_ok;
	GtkListStore *tree_list1;
	GtkListStore *tree_list2;
	GtkTreeIter tree_iter;
	GtkTooltips *tooltips = NULL;
	gint i, n = 0;
	int params = 3 + Xdialog.tips;

	if (Xdialog.tips == 1)
		tooltips = gtk_tooltips_new();

	Xdialog_array(list_size);

	open_window();

	set_backtitle(TRUE);
	set_label(optarg, FALSE);

	tree_list1 = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING);
	tree_list2 = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING);

	/* Put all parameters into an array and calculate the max item width */
	for (i = 0;  i < list_size; i++) {
		strncpy(Xdialog.array[i].tag, options[params*i], sizeof(Xdialog.array[i].tag));
		strncpy(Xdialog.array[i].name, options[params*i+1], sizeof(Xdialog.array[i].name));
		if (strlen(Xdialog.array[i].name) > n)
			n = strlen(Xdialog.array[i].name);
		gtk_list_store_append(tree_list1, &tree_iter);
		gtk_list_store_set(tree_list1, &tree_iter, 0,
			Xdialog.array[i].name, 1, Xdialog.array[i].tag, -1);
		/* TODO: tooltips support in GTK2 */
	}

	/* Setup a hbox to hold the scrolled windows and the Add/Remove buttons */
	hbox = gtk_hbox_new(FALSE, 0);
	gtk_widget_show(hbox);
	gtk_box_pack_start(Xdialog.vbox, hbox, TRUE, TRUE, ymult/3);

	/* Setup the first list into a scrolled window */
	Xdialog.widget1 = set_scrolled_list(hbox, MAX(15, n), list_size, 4, tree_list1);
	g_object_unref(G_OBJECT(tree_list1));

	/* Setup the Add/Remove buttons */
	vbuttonbox = gtk_vbutton_box_new();
	gtk_widget_show(vbuttonbox);
	gtk_box_pack_start(GTK_BOX(hbox), vbuttonbox, FALSE, TRUE, 0);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(vbuttonbox), GTK_BUTTONBOX_SPREAD);
	button_add = Xdialog.widget3 = set_button(ADD, vbuttonbox, -1, FALSE);
	g_signal_connect (G_OBJECT(button_add), "clicked",
			   G_CALLBACK(add_to_list), NULL);
	button_remove = Xdialog.widget4 = set_button(REMOVE, vbuttonbox, -1, FALSE);
	g_signal_connect (G_OBJECT(button_remove), "clicked",
			   G_CALLBACK(remove_from_list), NULL);

	/* Setup the second list into a scrolled window */
	Xdialog.widget2 = set_scrolled_list(hbox, MAX(15, n), list_size, 4, tree_list2);
	g_object_unref(G_OBJECT(tree_list2));

	button_ok = set_all_buttons(FALSE, TRUE);
	g_signal_connect (G_OBJECT(button_ok), "clicked", G_CALLBACK(print_list), NULL);

	sensitive_buttons();

	if (Xdialog.interval > 0)
		Xdialog.timer = g_timeout_add(Xdialog.interval, buildlist_timeout, NULL);

	set_timeout();
}


/* It looks like clists are particularly buggy in GTK+ (at least up to and
 * including GTK+ v1.2.10), so let's try to work around these (annoying !)
 * bugs.
 */
#define GTK_CLIST_BUG_WORK_AROUND 1	/* Work-around for column 0 visibility bug */

void create_menubox(gchar *optarg, gchar *options[], gint list_size)
{
	GtkWidget *button_ok;
	GtkWidget *scrolled_window;
	GtkWidget *status_bar = NULL;
	GtkWidget *hbox = NULL;
	GtkCList *clist;
	static gchar *null_row[] = {NULL, NULL};
	gint rownum = 0;
	gint n = 0; /* Dougal: use for max tag length */
	gint first_selectable = -1;
	int i;
	int params = 2 + Xdialog.tips;
	const GdkColor GREY1 = { 0, 0x6000, 0x6000, 0x6000 };
	const GdkColor GREY2 = { 0, 0xe000, 0xe000, 0xe000 };

	Xdialog_array(list_size);
	Xdialog.array[0].state = -1;

	open_window();

	set_backtitle(TRUE);
	set_label(optarg, FALSE);

	scrolled_window = set_scrolled_window(Xdialog.vbox, xmult/2, -1, list_size, 4);

	Xdialog.widget2 = gtk_clist_new(2);
	clist = GTK_CLIST(Xdialog.widget2);
	gtk_clist_set_selection_mode(clist, GTK_SELECTION_BROWSE);
	gtk_clist_set_shadow_type(clist, GTK_SHADOW_IN);

	for (i = 0; i < list_size; i++) {
		strncpy(Xdialog.array[i].tag, options[params*i], sizeof(Xdialog.array[i].tag));
		strncpy(Xdialog.array[i].name, options[params*i+1], sizeof(Xdialog.array[i].name));
		if (Xdialog.tips == 1) {
			strncpy(Xdialog.array[i].tips, options[params*i+2], sizeof(Xdialog.array[i].tips));
		}
		rownum = gtk_clist_append(clist, null_row);

		if (strlen(Xdialog.array[i].tag) == 0) {
			gtk_clist_set_text(clist, rownum, 0, "~");
			gtk_clist_set_selectable(clist, rownum, FALSE);
			gtk_clist_set_foreground(clist, rownum, (GdkColor *) &GREY1);
			gtk_clist_set_background(clist, rownum, (GdkColor *) &GREY2);
		} else {
			gtk_clist_set_text(clist, rownum, 0, Xdialog.array[i].tag);
			if ( first_selectable == -1 ) {
				first_selectable = rownum;
			}
			if (strlen(Xdialog.array[i].tag) > n)
			n = strlen(Xdialog.array[i].tag); /* Dougal:get max length for later */
		}

	/* FIX ME !
	 * There is apparently a bug in GTK+ preventing to hide the first column
	 * of a CLIST... This is to say that in order to hide the tags when the
	 * --no-tags option is in force, we must put the <item>s text in both
	 * columns 0 and 1 and then hide column 1, instead of putting the <tag>s
	 * in column 0, the <item>s in column 1 and hiding the column 0...
	 * The callback function had also to be made independant of the text
	 * in the column 0 (which was supposed to hold the <tag>s name): it now
	 * retreives the tag name of the selected row right from the Xdialog.array
	 * structure instead of the CLIST itself (this is not a problem though)...
	 */
#if GTK_CLIST_BUG_WORK_AROUND
		gtk_clist_set_text(clist, rownum, Xdialog.tags ? 1 : 0, Xdialog.array[i].name);
#else
		gtk_clist_set_text(clist, rownum, 1, Xdialog.array[i].name);
#endif
		/* Select this row as default if the tag is the good one */
		if (strlen(Xdialog.default_item) != 0 && !strcmp(Xdialog.default_item, Xdialog.array[i].tag)) {
			Xdialog.array[0].state = rownum;
			gtk_clist_select_row(clist, rownum, 0);
		}
	}

	gtk_clist_columns_autosize(clist);
	/* Dougal: This is a crude workaround for what seems like a GTK bug -- */
	/*  1st column is too narrow after autoresize */
	if ( n > 0 )
		gtk_clist_set_column_width (clist, 0, n*9.5);
	
	/* Select the first selectable row as default if no other row is selected */
	if (Xdialog.array[0].state < 0) {
		if ( first_selectable >= 0 ) {
			Xdialog.array[0].state = first_selectable;
			gtk_clist_select_row(clist, first_selectable, 0);
		}
	}
	/* We can't move to the default selected row right from here,
	 * we need a timeout function to do so... It will run only once
	 * of course !
	 */
	g_timeout_add(1, move_to_row_timeout, NULL);

	if (!Xdialog.tags)
#if GTK_CLIST_BUG_WORK_AROUND
		gtk_clist_set_column_visibility(clist, 1, FALSE);
#else
		gtk_clist_set_column_visibility(clist, 0, FALSE);
#endif
	
	gtk_container_add(GTK_CONTAINER(scrolled_window), Xdialog.widget2);
	gtk_widget_show(Xdialog.widget2);
	g_signal_connect (G_OBJECT(Xdialog.widget2), "select_row",
			   G_CALLBACK(item_select), NULL);

	button_ok = set_all_buttons(FALSE, TRUE);
	g_signal_connect (G_OBJECT(button_ok), "clicked", G_CALLBACK(print_selection), NULL);
	g_signal_connect (G_OBJECT(Xdialog.widget2), "button_press_event",
			   G_CALLBACK(double_click_event), button_ok);

	if (Xdialog.tips == 1) {
		hbox = gtk_hbox_new(FALSE, 0);
		gtk_box_pack_end(Xdialog.vbox, hbox, FALSE, FALSE, 0);
		gtk_widget_show(hbox);
		status_bar = Xdialog.widget1 = gtk_statusbar_new();
		gtk_container_add(GTK_CONTAINER(hbox), status_bar);
		gtk_widget_show(status_bar);
		Xdialog.status_id = gtk_statusbar_get_context_id(GTK_STATUSBAR(status_bar), "tips");
		gtk_statusbar_push(GTK_STATUSBAR(status_bar), Xdialog.status_id,
				   Xdialog.array[0].tips);
	}

	if (Xdialog.interval > 0)
		Xdialog.timer = g_timeout_add(Xdialog.interval, menu_timeout, NULL);

	set_timeout();
}

/* TODO: implement tooltips support */
void create_treeview(gchar *optarg, gchar *options[], gint list_size)
{
	GtkWidget *scrolled_window;
	GtkWidget *button_ok;
	GtkTreeStore *store;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	GtkTreeIter tree_iter[MAX_TREE_DEPTH];
	GtkTreeSelection *select;
/*	GtkTooltips *tooltips = NULL; */
	int depth = 0;
	int i;
	int params = 4 + Xdialog.tips;

/*	if (Xdialog.tips == 1)
		tooltips = gtk_tooltips_new(); */

	Xdialog_array(list_size);

	open_window();

	set_backtitle(TRUE);
	set_label(optarg, FALSE);

	/* Fill the store with the data */
	store = gtk_tree_store_new(2, G_TYPE_STRING, G_TYPE_STRING);
	for (i = 0 ; i < list_size ; i++) {
		strncpy(Xdialog.array[i].tag, options[params*i], sizeof(Xdialog.array[i].tag));
		strncpy(Xdialog.array[i].name, options[params*i+1], sizeof(Xdialog.array[i].name));
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
			gtk_tree_store_append(store, &tree_iter[0], NULL);
			gtk_tree_store_set(store, &tree_iter[0], 0,
				Xdialog.array[i].name, 1, Xdialog.array[i].tag, -1);
		} else {
			gtk_tree_store_append(store, &tree_iter[depth],
				&tree_iter[depth-1]);
			gtk_tree_store_set(store, &tree_iter[depth], 0,
				Xdialog.array[i].name, 1, Xdialog.array[i].tag, -1);
		}
	}     

	/* Create the tree view in a scrolled window */
	scrolled_window = set_scrolled_window(Xdialog.vbox, xmult/2, -1, list_size, 4);

	Xdialog.widget1 = gtk_tree_view_new_with_model(GTK_TREE_MODEL (store));
	g_object_unref(G_OBJECT (store));

	renderer = gtk_cell_renderer_text_new();

	column = gtk_tree_view_column_new_with_attributes ("", renderer, "text", 0, NULL);

	gtk_tree_view_append_column(GTK_TREE_VIEW(Xdialog.widget1), column);

	gtk_widget_show(Xdialog.widget1);
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolled_window), Xdialog.widget1);

	/* Create and hookup the ok button */
	button_ok = set_all_buttons(FALSE, TRUE);
	g_signal_connect (G_OBJECT(button_ok), "clicked", G_CALLBACK(print_tree_selection), NULL);

	/* Setup the selection handler */
	select = gtk_tree_view_get_selection(GTK_TREE_VIEW(Xdialog.widget1));
	gtk_tree_selection_set_mode(select, GTK_SELECTION_SINGLE);
	g_signal_connect(G_OBJECT(select), "changed", G_CALLBACK(cb_selection_changed), NULL);

	if (Xdialog.interval > 0)
		Xdialog.timer = g_timeout_add(Xdialog.interval, menu_timeout, NULL);

	set_timeout();
}

void create_filesel(gchar *optarg, gboolean dsel_flag)
{
	GtkFileSelection *filesel;
	GtkWidget *hbuttonbox;
	GtkWidget *button;
	gboolean flag;

	font_init();

	parse_rc_file();

	/* Create a file selector and update Xdialog structure accordingly */
	Xdialog.window = gtk_file_selection_new(Xdialog.title);
	filesel = GTK_FILE_SELECTION(Xdialog.window);
	Xdialog.vbox = GTK_BOX(filesel->main_vbox);

	/* Set the backtitle */
	set_backtitle(TRUE);

	/* Set the default filename */
	gtk_file_selection_set_filename(filesel, optarg);
	gtk_file_selection_complete(filesel, optarg);

	/* If we want a directory selector, then hide the file list parent and
           the file list entry field. Also clear the file selection to erase
           the auto-completed filename. Finally, disable the file operation
           buttons to keep only the "make new directory" one. */
	if (dsel_flag) {
		gtk_widget_hide(GTK_WIDGET(GTK_WIDGET(filesel->file_list)->parent));
		gtk_widget_hide(GTK_WIDGET(filesel->selection_entry));
		gtk_file_selection_set_filename(filesel, "");
		gtk_widget_set_sensitive(GTK_WIDGET(filesel->fileop_del_file), FALSE);
		gtk_widget_set_sensitive(GTK_WIDGET(filesel->fileop_ren_file), FALSE);
	}

	/* Hide fileops buttons if requested */
	if (!Xdialog.buttons || dialog_compat)
		gtk_file_selection_hide_fileop_buttons(filesel);

	/* If requested, add a check button into the filesel action area */
	set_check_button(GTK_WIDGET(filesel->action_area));

	/* We must realize the widget before moving it and creating the buttons pixmaps */
	gtk_widget_realize(Xdialog.window);

	/* Set the window size and placement policy */
	set_window_size_and_placement();

	/* Find the existing hbuttonbox pointer */
	hbuttonbox = gtk_widget_get_ancestor(filesel->ok_button, gtk_hbutton_box_get_type());

	/* Remove the fileselector buttons IOT put ours in place */
	gtk_object_destroy(GTK_OBJECT(filesel->ok_button));
	gtk_object_destroy(GTK_OBJECT(filesel->cancel_button));

	/* Setup our own buttons */
	if (Xdialog.wizard)
		set_button(PREVIOUS , hbuttonbox, 3, FALSE);
	else {
		button = set_button(OK, hbuttonbox, 0, flag = !Xdialog.default_no);
		if (flag)
			gtk_widget_grab_focus(button);
		filesel->ok_button = button;
	}
	if (Xdialog.cancel_button) {
		button = set_button(CANCEL, hbuttonbox, 1,
				    flag = Xdialog.default_no && !Xdialog.wizard);
		if (flag)
			gtk_widget_grab_focus(button);
		filesel->cancel_button = button;
	}
	if (Xdialog.wizard) {
		button = set_button(NEXT, hbuttonbox, 0, TRUE);
		gtk_widget_grab_focus(button);
		filesel->ok_button = button;
	}
	if (Xdialog.help)
		set_button(HELP, hbuttonbox, 2, FALSE);

	/* Setup callbacks */
	g_signal_connect (G_OBJECT(Xdialog.window), "destroy",
			   G_CALLBACK(destroy_event), NULL);
	g_signal_connect (G_OBJECT(Xdialog.window), "delete_event",
			   G_CALLBACK(delete_event), NULL);
	if (dsel_flag)
		g_signal_connect (G_OBJECT(filesel->ok_button),
			   "clicked", G_CALLBACK(dirsel_exit), filesel);
	else
		g_signal_connect (G_OBJECT(filesel->ok_button),
			   "clicked", G_CALLBACK(filesel_exit), filesel);

	/* Beep if requested */
	if (Xdialog.beep & BEEP_BEFORE && Xdialog.exit_code != 2)
		gdk_beep();

	/* Default exit code */
	Xdialog.exit_code = 255;

	set_timeout();
}


void create_colorsel(gchar *optarg, gdouble *colors)
{
	GtkColorSelectionDialog *colorsel;
	GtkWidget *box;
	GtkWidget *hbuttonbox;
	GtkWidget *button;
	GdkColor gcolor;
	gboolean flag;

	font_init();

	parse_rc_file();

	/* Create a color selector and update Xdialog structure accordingly */
	Xdialog.window = gtk_color_selection_dialog_new(Xdialog.title);
	colorsel = GTK_COLOR_SELECTION_DIALOG(Xdialog.window);
	Xdialog.vbox = GTK_BOX(gtk_widget_get_ancestor(colorsel->colorsel, gtk_box_get_type()));

	gcolor.red   = colors[0] * 256;
	gcolor.green = colors[1] * 256;
	gcolor.blue  = colors[2] * 256;
	gtk_color_selection_set_current_color (
		GTK_COLOR_SELECTION (gtk_color_selection_dialog_get_color_selection (colorsel)),
		&gcolor);

	/* We must realize the widget before moving it and creating the icon and
           buttons pixmaps...
        */
	gtk_widget_realize(Xdialog.window);

	/* Set the text */
	box = gtk_widget_get_ancestor(set_label(optarg, TRUE), gtk_box_get_type());
	gtk_box_reorder_child(Xdialog.vbox, box, 0);

	/* Set the backtitle */
	set_backtitle(TRUE);

	/* If requested, add a check button into the colorsel action area */
	set_check_button(GTK_WIDGET(Xdialog.vbox));

	/* Set the window size and placement policy */
	set_window_size_and_placement();

	/* Find the existing hbuttonbox pointer */
	hbuttonbox = gtk_widget_get_ancestor(colorsel->ok_button, gtk_hbutton_box_get_type());

	/* Remove the colour selector buttons IOT put ours in place */
	gtk_object_destroy(GTK_OBJECT(colorsel->ok_button));
	gtk_object_destroy(GTK_OBJECT(colorsel->cancel_button));
	gtk_object_destroy(GTK_OBJECT(colorsel->help_button));

	/* Setup our own buttons */
	if (Xdialog.wizard)
		set_button(PREVIOUS , hbuttonbox, 3, FALSE);
	else {
		button = set_button(OK, hbuttonbox, 0, flag = !Xdialog.default_no);
		if (flag)
			gtk_widget_grab_focus(button);
		colorsel->ok_button = button;
	}
	if (Xdialog.cancel_button) {
		button = set_button(CANCEL, hbuttonbox, 1,
				    flag = Xdialog.default_no && !Xdialog.wizard);
		if (flag)
			gtk_widget_grab_focus(button);
		colorsel->cancel_button = button;
	}
	if (Xdialog.wizard) {
		button = set_button(NEXT, hbuttonbox, 0, TRUE);
		gtk_widget_grab_focus(button);
		colorsel->ok_button = button;
	}
	if (Xdialog.help)
		set_button(HELP, hbuttonbox, 2, FALSE);

	/* Setup callbacks */
	g_signal_connect (G_OBJECT(Xdialog.window), "destroy",
			   G_CALLBACK(destroy_event), NULL);
	g_signal_connect (G_OBJECT(Xdialog.window), "delete_event",
			   G_CALLBACK(delete_event), NULL);
	g_signal_connect (G_OBJECT(colorsel->ok_button), "clicked",
			   G_CALLBACK(colorsel_exit), G_OBJECT(colorsel->colorsel));

	/* Beep if requested */
	if (Xdialog.beep & BEEP_BEFORE && Xdialog.exit_code != 2)
		gdk_beep();

	/* Default exit code */
	Xdialog.exit_code = 255;

	set_timeout();
}


void create_fontsel(gchar *optarg)
{
	GtkFontSelectionDialog *fontsel;
	GtkWidget *hbuttonbox;
	GtkWidget *button;
	gboolean flag;

	font_init();

	parse_rc_file();

	/* Create a font selector and update Xdialog structure accordingly */
	Xdialog.window = gtk_font_selection_dialog_new(Xdialog.title);
	fontsel = GTK_FONT_SELECTION_DIALOG(Xdialog.window);
	Xdialog.vbox = GTK_BOX(fontsel->main_vbox);

	/* Set the backtitle */
	set_backtitle(FALSE);

	/* Set the default font name */
	gtk_font_selection_set_font_name(GTK_FONT_SELECTION(fontsel->fontsel), optarg);
	gtk_font_selection_set_preview_text(GTK_FONT_SELECTION(fontsel->fontsel),
                                            "abcdefghijklmnopqrstuvwxyz 0123456789");

	/* If requested, add a check button into the fontsel action area */
	set_check_button(fontsel->action_area);

	/* We must realize the widget before moving it and creating the buttons pixmaps */
	gtk_widget_realize(Xdialog.window);

	/* Set the window size and placement policy */
	set_window_size_and_placement();

	/* Find the existing hbuttonbox pointer */
	hbuttonbox = fontsel->action_area;

	/* Remove the font selector buttons IOT put ours in place */
	gtk_object_destroy(GTK_OBJECT(fontsel->ok_button));
	gtk_object_destroy(GTK_OBJECT(fontsel->cancel_button));
	gtk_object_destroy(GTK_OBJECT(fontsel->apply_button));

	/* Setup our own buttons */
	if (Xdialog.wizard)
		set_button(PREVIOUS , hbuttonbox, 3, FALSE);
	else {
		button = set_button(OK, hbuttonbox, 0, flag = !Xdialog.default_no);
		if (flag)
			gtk_widget_grab_focus(button);
		fontsel->ok_button = button;
	}
	if (Xdialog.cancel_button) {
		button = set_button(CANCEL, hbuttonbox, 1,
				    flag = Xdialog.default_no && !Xdialog.wizard);
		if (flag)
			gtk_widget_grab_focus(button);
		fontsel->cancel_button = button;
	}
	if (Xdialog.wizard) {
		button = set_button(NEXT, hbuttonbox, 0, TRUE);
		gtk_widget_grab_focus(button);
		fontsel->ok_button = button;
	}
	if (Xdialog.help)
		set_button(HELP, hbuttonbox, 2, FALSE);

	/* Setup callbacks */
	g_signal_connect (G_OBJECT(Xdialog.window), "destroy",
			   G_CALLBACK(destroy_event), NULL);
	g_signal_connect (G_OBJECT(Xdialog.window), "delete_event",
			   G_CALLBACK(delete_event), NULL);
	g_signal_connect (G_OBJECT(fontsel->ok_button),
			   "clicked", (GtkSignalFunc) fontsel_exit, fontsel);

	/* Beep if requested */
	if (Xdialog.beep & BEEP_BEFORE && Xdialog.exit_code != 2)
		gdk_beep();

	/* Default exit code */
	Xdialog.exit_code = 255;

	set_timeout();
}


void create_calendar(gchar *optarg, gint day, gint month, gint year)
{
	GtkCalendar *calendar;
	GtkWidget *button_ok;
	gint flags;

	open_window();

	set_backtitle(TRUE);
	set_label(optarg, FALSE);

	flags = GTK_CALENDAR_SHOW_HEADING | GTK_CALENDAR_SHOW_DAY_NAMES;
	/* There is a bug in GTK+ v1.2.7 preventing the week numbers to show
	 * properly (all numbers are 0 !)...
	 */
	if (!(gtk_major_version == 1 && gtk_minor_version == 2 &&
	    gtk_micro_version == 7))
		flags = flags | GTK_CALENDAR_SHOW_WEEK_NUMBERS;

	Xdialog.widget1 = gtk_calendar_new();
	gtk_box_pack_start(Xdialog.vbox, Xdialog.widget1, TRUE, TRUE, 5);
	gtk_widget_show(Xdialog.widget1);

	calendar = GTK_CALENDAR(Xdialog.widget1);
	gtk_calendar_display_options(calendar, flags);

	gtk_calendar_select_month(calendar, month-1, year);
	gtk_calendar_select_day(calendar, day);

	button_ok = set_all_buttons(FALSE, TRUE);
	g_signal_connect (G_OBJECT(button_ok), "clicked", G_CALLBACK(calendar_exit), NULL);
	g_signal_connect (G_OBJECT(Xdialog.widget1), "day_selected_double_click",
			   G_CALLBACK(calendar_exit), NULL);
	g_signal_connect_after(G_OBJECT(Xdialog.widget1), "day_selected_double_click",
			   G_CALLBACK(exit_ok), NULL);

	if (Xdialog.interval > 0)
		Xdialog.timer = g_timeout_add(Xdialog.interval, calendar_timeout, NULL);

	set_timeout();
}


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
	gtk_box_pack_start(Xdialog.vbox, frame, TRUE, TRUE, ymult);
	gtk_widget_show(frame);

	hbox = gtk_hbox_new(TRUE, 0);
	gtk_container_add(GTK_CONTAINER(frame), hbox);
	gtk_container_set_border_width(GTK_CONTAINER(hbox), ymult);
	gtk_widget_show(hbox);

	Xdialog.widget1 = set_spin_button(hbox, 0, 23, hours, 2, ":",  FALSE);
	Xdialog.widget2 = set_spin_button(hbox, 0, 59, minutes,  2, ":",  FALSE);
	Xdialog.widget3 = set_spin_button(hbox, 0, 59, seconds,  2, NULL, FALSE);

	button_ok = set_all_buttons(FALSE, TRUE);
	g_signal_connect (G_OBJECT(button_ok), "clicked", G_CALLBACK(timebox_exit), NULL);

	if (Xdialog.interval > 0)
		Xdialog.timer = g_timeout_add(Xdialog.interval, timebox_timeout, NULL);

	set_timeout();
}


void get_maxsize(int *x, int *y)
{
	font_init();
	*x = gdk_screen_width()/xmult-2;
	*y = gdk_screen_height()/ymult-2;
}
