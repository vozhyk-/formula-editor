#include <stdlib.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#define TEMPLATES_FILE "templates.csv"

typedef struct {
	const char *shown_text;
	const char *tooltip;
	const char *code;
	int move_cursor;
} Template;

Template *templates;
int templates_count;

char *read_line(FILE *stream);
char *read_file(char *path, size_t *len);
void write_file(char *path, char *text);
char *remove_extension(const char *name);

char *formula_filename = NULL;

GtkFileFilter *tex_filter;
GtkWidget *window;
GtkWidget *formula_text_view;
GtkTextBuffer *formula_buffer;
GtkWidget *formula_image;
GtkWidget *progress_bar;

Template *read_templates(char *path);

void read_formula(void);
void write_formula(void);
char saveas_formula(void);
char save_formula(void);
void render_formula_thread(gpointer data);
static void new_cb(GtkWidget *widget,
				   gpointer  data);
static void open_cb(GtkWidget *widget,
					gpointer  data);
static void save_cb(GtkWidget *widget,
					gpointer  data);
static void saveas_cb(GtkWidget *widget,
					  gpointer  data);
static void render_cb(GtkWidget *widget,
					  gpointer  data);
static void insert_template_cb(GtkWidget *widget,
							   Template  *t);

int main(int argc, char *argv[])
{
	GtkAccelGroup *accel_group;
	GtkWidget *vbox;
	GtkWidget *menu_bar;
	GtkWidget *file_menu;
	GtkWidget *file_menu_item;
	GtkWidget *new_menu_item;
	GtkWidget *open_menu_item;
	GtkWidget *save_menu_item;
	GtkWidget *saveas_menu_item;
	GtkWidget *sep_menu_item;
	GtkWidget *quit_menu_item;
	GtkWidget *toolbar;
	GtkWidget *hbox;
	GtkWidget *render_button;

	gdk_threads_init();
	gdk_threads_enter();
	gtk_init(&argc, &argv);

	templates = read_templates(TEMPLATES_FILE);

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

	accel_group = gtk_accel_group_new();
	gtk_window_add_accel_group(GTK_WINDOW(window), accel_group);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(window), vbox);

	tex_filter = gtk_file_filter_new();
	gtk_file_filter_set_name(tex_filter, "TeX formula");
	gtk_file_filter_add_pattern(tex_filter, "*.tex");

	/* menu bar */
	menu_bar = gtk_menu_bar_new();

	file_menu = gtk_menu_new();

	file_menu_item = gtk_menu_item_new_with_mnemonic("_File");
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(file_menu_item), file_menu);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu_bar), file_menu_item);

	new_menu_item = gtk_image_menu_item_new_from_stock(
	                     GTK_STOCK_NEW, accel_group);
	gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), new_menu_item);
	
	open_menu_item = gtk_image_menu_item_new_from_stock(
	                     GTK_STOCK_OPEN, accel_group);
	gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), open_menu_item);
	
	save_menu_item = gtk_image_menu_item_new_from_stock(
	                     GTK_STOCK_SAVE, accel_group);
	gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), save_menu_item);
	
	saveas_menu_item = gtk_image_menu_item_new_from_stock(
	                     GTK_STOCK_SAVE_AS, accel_group);
	gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), saveas_menu_item);
	
	sep_menu_item = gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), sep_menu_item);
	
	quit_menu_item = gtk_image_menu_item_new_from_stock(
	                     GTK_STOCK_QUIT, accel_group);
	gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), quit_menu_item);
	
	gtk_box_pack_start(GTK_BOX(vbox), menu_bar, FALSE, FALSE, 3);
	/* end*/

	toolbar = gtk_toolbar_new();
	gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, FALSE, 0);

	for (int i = 0; i < templates_count; i++) {
		Template t = templates[i];
		gtk_toolbar_append_item(
		    GTK_TOOLBAR(toolbar), t.shown_text, t.tooltip, "", NULL,
		    G_CALLBACK(insert_template_cb), &templates[i]);
	}

	formula_text_view = gtk_text_view_new();
	gtk_box_pack_start(GTK_BOX(vbox), formula_text_view, TRUE, TRUE, 0);

	formula_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(formula_text_view));

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	render_button = gtk_button_new_with_mnemonic("_Render");
	gtk_box_pack_start(GTK_BOX(hbox), render_button, TRUE, TRUE, 0);

	progress_bar = gtk_progress_bar_new();
	gtk_box_pack_start(GTK_BOX(hbox), progress_bar, TRUE, TRUE, 0);

	formula_image = gtk_image_new();
	gtk_box_pack_start(GTK_BOX(vbox), formula_image, FALSE, FALSE, 0);

	g_signal_connect(new_menu_item, "activate",
					 G_CALLBACK(new_cb), NULL);
	g_signal_connect(open_menu_item, "activate",
					 G_CALLBACK(open_cb), NULL);
	g_signal_connect(save_menu_item, "activate",
					 G_CALLBACK(save_cb), NULL);
	g_signal_connect(saveas_menu_item, "activate",
					 G_CALLBACK(saveas_cb), NULL);
	g_signal_connect(quit_menu_item, "activate",
					 G_CALLBACK(gtk_main_quit), NULL);
	g_signal_connect(render_button, "clicked",
					 G_CALLBACK(render_cb), NULL);
	g_signal_connect(window, "delete-event",
					 G_CALLBACK(gtk_main_quit), NULL);
	
	gtk_widget_show_all(window);

	gtk_main();
	gdk_threads_leave();

	return 0;
}

char *read_line(FILE *stream)
{
	char *s = NULL;
	size_t size = 0;
	ssize_t chars_read = getline(&s, &size, stream);

	if (s[chars_read - 1] == '\n')
		s[chars_read - 1] = '\0';

	return s;
}

Template *read_templates(char *path)
{
	Template *ts;
	FILE *file = fopen(path, "r");
	char *line;
	char **fields;

	if (!file) {
		perror(path);
		gtk_exit(1);
	}

	fscanf(file, "%d\n", &templates_count);

	ts = g_malloc(templates_count * sizeof(*ts));

	for (int i = 0; i < templates_count; i++) {
		line = read_line(file);
		fields = g_strsplit(line, ",", 4);
		ts[i].shown_text  = fields[0];
		ts[i].tooltip     = fields[1];
		ts[i].move_cursor = atoi(fields[2]);
		ts[i].code        = fields[3];
		//g_strfreev(fields)
	}

	return ts;

	fclose(file);
}

char *read_file(char *path, size_t *len)
{
	char *buffer;
	FILE *file = fopen(path, "r");

	if (!file) {
		perror(path);
		gtk_exit(1);
	}

	/* get file length */
	fseek(file, 0L, SEEK_END);
	*len = ftell(file);
	rewind(file);

	buffer = g_malloc(*len + 1);
	if (!buffer) {
		fclose(file);
		fputs("memory alloc to read file fails", stderr);
		gtk_exit(1);
	}
	if (1 != fread(buffer, *len, 1 , file)) {
		fclose(file);
		g_free(buffer);
		fputs("entire read fails", stderr);
		gtk_exit(1);
	}

	fclose(file);

	return buffer;
}

void write_file(char *path, char *text)
{
	FILE *file = fopen(path, "w");

	if (!file) {
		perror(path);
		gtk_exit(1);
	}

	if (fputs(text, file) == EOF) {
		fputs("entire write fails", stderr);
		gtk_exit(1);
	}

	fclose(file);
}

// does not shrink the resulting string
char *remove_extension(const char *name)
{
	char *result = g_strdup(name);

	char *sep = g_strrstr(result, ".");
	if (sep != NULL)
		*sep = '\0';

	return result;
}

void read_formula(void)
{
	size_t text_len;
	char *text = read_file(formula_filename, &text_len);
	
	gtk_text_buffer_set_text(GTK_TEXT_BUFFER(formula_buffer),
							 text, text_len);
}	

void write_formula(void)
{
	GtkTextIter start;
	GtkTextIter end;
	gtk_text_buffer_get_start_iter(GTK_TEXT_BUFFER(formula_buffer),
								   &start);
	gtk_text_buffer_get_end_iter(GTK_TEXT_BUFFER(formula_buffer),
								 &end);

	char *text = gtk_text_buffer_get_text(GTK_TEXT_BUFFER(formula_buffer),
										  &start, &end, FALSE);
	write_file(formula_filename, text);
}

char saveas_formula(void)
{
	GtkWidget *dialog;
	char success = 0;
	
	dialog = gtk_file_chooser_dialog_new("Save File",
										 GTK_WINDOW(window),
										 GTK_FILE_CHOOSER_ACTION_SAVE,
										 GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
										 GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
										 NULL);
	gtk_file_chooser_set_do_overwrite_confirmation(
	    GTK_FILE_CHOOSER(dialog), TRUE);
	gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(dialog),
								tex_filter);
	if (formula_filename == NULL) {
		//gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), default_folder_for_saving);
		gtk_file_chooser_set_current_name(
		    GTK_FILE_CHOOSER(dialog), "Untitled.tex");
	} else {
		gtk_file_chooser_set_filename(
		    GTK_FILE_CHOOSER(dialog), formula_filename);
	}

	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
		formula_filename = gtk_file_chooser_get_filename(
		                       GTK_FILE_CHOOSER(dialog));
		write_formula();
		success = 1;
	}

	gtk_widget_destroy (dialog);

	return success;
}

char save_formula(void)
{
	if (formula_filename == NULL) {
		return saveas_formula();
	} else {
		write_formula();
		return 1;
	}
}

void render_formula_thread(gpointer data)
{
	void set_progress(gdouble value)
	{
		gdk_threads_enter();
		gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar),
									  value);
		gdk_threads_leave();
	}
	void set_text(char *text)
	{
		gdk_threads_enter();
		gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progress_bar),
								  text);
		gdk_threads_leave();
	}

	char *formula_img_filename = g_strconcat(
	    remove_extension(formula_filename), ".png", NULL);
	char *command = g_strconcat(
	    "tex2im -t 073642 -b e0d9bf -r 300 ", formula_filename, NULL);

	g_print("Formula image filename is: %s\n", formula_img_filename);

	g_print("Rendering.");
	set_progress(0.0);

	g_print(".");
	set_text("Rendering...");
	set_progress(0.33);

	system(command);

	gtk_image_set_from_file(GTK_IMAGE(formula_image), formula_img_filename);

	g_print(".\n");
	set_text("Done");
	set_progress(1.0);

	g_free(formula_img_filename);
	g_free(command);
}

static void new_cb(GtkWidget *widget,
				   gpointer  data)
{
	gtk_text_buffer_set_text(GTK_TEXT_BUFFER(formula_buffer),
							 "", 0);
	gtk_image_clear(GTK_IMAGE(formula_image));
	formula_filename = NULL;

	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar), 0.0);
	gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progress_bar), "");
}

static void open_cb(GtkWidget *widget,
					gpointer  data)
{
	GtkWidget *dialog;
	
	dialog = gtk_file_chooser_dialog_new("Open File",
										 GTK_WINDOW(window),
										 GTK_FILE_CHOOSER_ACTION_OPEN,
										 GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
										 GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
										 NULL);
	gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(dialog),
								tex_filter);
	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
		formula_filename = gtk_file_chooser_get_filename(
		                       GTK_FILE_CHOOSER(dialog));
		read_formula();
	}
	gtk_widget_destroy (dialog);
}

static void save_cb(GtkWidget *widget,
					gpointer  data)
{
	save_formula();
}

static void saveas_cb(GtkWidget *widget,
					  gpointer  data)
{
	saveas_formula();
}

static void render_cb(GtkWidget *widget,
					  gpointer  data)
{
	if (save_formula()) {
		g_thread_new("render",
					 (GThreadFunc)&render_formula_thread,
					 NULL);
	} else {
		GtkWidget *dialog =
			gtk_message_dialog_new(GTK_WINDOW(window),
								   GTK_DIALOG_MODAL, // replace!
								   GTK_MESSAGE_WARNING,
								   GTK_BUTTONS_OK,
								   "You must save the file to render it!");
		gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(dialog);
	}
	//render_formula(progress_bar);
}

static void insert_template_cb(GtkWidget *widget,
							   Template  *t)
{
	GtkTextIter iter;

	gtk_text_buffer_insert_at_cursor(formula_buffer, t->code, -1);

	/* get iterator at insert cursor position */
	gtk_text_buffer_get_iter_at_mark(
	    formula_buffer, &iter,
	    gtk_text_buffer_get_insert(formula_buffer));
	/* move the cursor forward by N chars */
	gtk_text_iter_forward_chars(&iter, t->move_cursor);
	gtk_text_buffer_place_cursor(formula_buffer, &iter);
}

/*
gboolean entry_keypress_cb(GtkWidget   *widget,
						   GdkEventKey *event,
						   gpointer    data)
{
	switch (event->keyval) {
	case GDK_Return:
		g_print("Return pressed.\n");
		render_formula();
		return TRUE;
	default:
		g_print("entry: Key pressed.\n");
		return FALSE;t
	}
}
*/
