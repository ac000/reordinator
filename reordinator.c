/*
 * reordinator.c - A program to help with re-ordering lines in a file
 *
 * Copyright (C) 2013, 2020	Andrew Clayton <andrew@digital-domain.net>
 *
 * Released under the GNU General Public License version 2
 * See COPYING
 */

#define _XOPEN_SOURCE		/* for fileno() */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>

#include <glib.h>

#include <gtk/gtk.h>

#define BUF_SIZE	4096
#define PROG_NAME	"reordinator"

struct widgets {
	GtkWidget *window;
	GtkWidget *treeview;
	GtkWidget *filechooser_open;
	GtkWidget *filechooser_save;
	GtkWidget *confirm_quit;
	GtkWidget *save_error;
	GtkWidget *about;
	GtkListStore *liststore;

	gulong sig;
};

static char loaded_file[PATH_MAX];
static bool file_modified;
static int last_row_selected;

static GList *create_path_refs(GList *rows, gint nrows, GtkTreeModel *model)
{
	GList *refs = NULL;
	gint i;

	for (i = 0; i < nrows; i++) {
		GtkTreePath *path;
		GtkTreeRowReference *ref;

		path = g_list_nth_data(rows, i);
		ref = gtk_tree_row_reference_new(model, path);

		refs = g_list_prepend(refs, ref);
	}
	refs = g_list_reverse(refs);

	return refs;
}

static void update_window_title(GtkWidget *window, bool modified)
{
	char title[BUF_SIZE * 2];

	snprintf(title, sizeof(title), "%s - %c(%s)", PROG_NAME,
			(modified) ? '*' : ' ', loaded_file);
	gtk_window_set_title(GTK_WINDOW(window), title);

	file_modified = modified;
}

static void load_file(const char *file, struct widgets *widgets)
{
	FILE *fp;
	char line[BUF_SIZE];
	GtkTreeIter iter;

	/*
	 * Temporarily block the "row-changed" signal while we load in
	 * the data.
	 */
	g_signal_handler_block(widgets->liststore, widgets->sig);
	gtk_list_store_clear(widgets->liststore);

	fp = fopen(file, "r");
	while (fgets(line, sizeof(line), fp) != NULL) {
		gtk_list_store_append(widgets->liststore, &iter);
		/* Loose the trailing '\n' */
		line[strlen(line) - 1] = '\0';
		gtk_list_store_set(widgets->liststore, &iter,
				0, line,
				-1);
	}
	fclose(fp);

	snprintf(loaded_file, sizeof(loaded_file), "%s", file);
	update_window_title(widgets->window, false);
	g_signal_handler_unblock(widgets->liststore, widgets->sig);
}

static void file_open(struct widgets *widgets)
{
	char *filename;

	filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(
				widgets->filechooser_open));
	if (!filename)
		return;

	load_file(filename, widgets);
	g_free(filename);
}

static void save_file_as(struct widgets *widgets)
{
	char *filename;
	FILE *fp;
	GtkTreeModel *model = GTK_TREE_MODEL(widgets->liststore);
	GtkTreeIter iter;

	filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(
				widgets->filechooser_save));
	if (!filename)
		return;

	fp = fopen(filename, "w");
	if (!fp) {
		gtk_dialog_run(GTK_DIALOG(widgets->save_error));
		gtk_widget_hide(widgets->save_error);
		return;
	}

	gtk_tree_model_get_iter_first(model, &iter);
	do {
		char *buf;

		gtk_tree_model_get(model, &iter, 0, &buf, -1);
		fprintf(fp, "%s\n", buf);
		g_free(buf);
	} while (gtk_tree_model_iter_next(model, &iter));
	fsync(fileno(fp));
	fclose(fp);

	snprintf(loaded_file, sizeof(loaded_file), "%s", filename);
	update_window_title(widgets->window, false);
	g_free(filename);
}

void cb_about(GtkWidget *button, struct widgets *widgets)
{
        gtk_dialog_run(GTK_DIALOG(widgets->about));
        gtk_widget_hide(widgets->about);
}

void cb_confirm_quit(struct widgets *widgets)
{
	int response;

	response = gtk_dialog_run(GTK_DIALOG(widgets->confirm_quit));
	switch (response) {
	case GTK_RESPONSE_YES:
		gtk_main_quit();
		break;
	case GTK_RESPONSE_NO:
		break;
	}

	gtk_widget_hide(widgets->confirm_quit);
}

static void do_quit(struct widgets *widgets)
{
	return file_modified ? cb_confirm_quit(widgets) : gtk_main_quit();
}

void cb_window_destroy(GtkWidget *widget, GdkEvent *event,
		       struct widgets *widgets)
{
	do_quit(widgets);
}

void cb_menu_quit(GtkMenuItem *menuitem, struct widgets *widgets)
{
	do_quit(widgets);
}

void cb_save_file(GtkMenuItem *menuitem, struct widgets *widgets)
{
	FILE *fp;
	GtkTreeModel *model;
	GtkTreeIter iter;
	char tmp_file[PATH_MAX];

	if (strlen(loaded_file) == 0)
		return;

	snprintf(tmp_file, sizeof(tmp_file), "%s.%d.tmp", loaded_file,
			getpid());
	fp = fopen(tmp_file, "w");
	if (!fp) {
		gtk_dialog_run(GTK_DIALOG(widgets->save_error));
		gtk_widget_hide(widgets->save_error);
		return;
	}

	model = GTK_TREE_MODEL(widgets->liststore);
	gtk_tree_model_get_iter_first(model, &iter);
	do {
		char *buf;

		gtk_tree_model_get(model, &iter, 0, &buf, -1);
		fprintf(fp, "%s\n", buf);
		g_free(buf);
	} while (gtk_tree_model_iter_next(model, &iter));
	fsync(fileno(fp));
	fclose(fp);

	rename(tmp_file, loaded_file);
	update_window_title(widgets->window, false);
}

void cb_save_as(GtkMenuItem *menuitem, struct widgets *widgets)
{
	int response;

	if (strlen(loaded_file) == 0)
		return;

	response = gtk_dialog_run(GTK_DIALOG(widgets->filechooser_save));
	switch (response) {
	case GTK_RESPONSE_OK:
		save_file_as(widgets);
		break;
	case GTK_RESPONSE_CANCEL:
		break;
	}

	gtk_widget_hide(widgets->filechooser_save);
}

void cb_open(GtkMenuItem *menuitem, struct widgets *widgets)
{
	int response;

	response = gtk_dialog_run(GTK_DIALOG(widgets->filechooser_open));
	switch (response) {
	case GTK_RESPONSE_OK:
		file_open(widgets);
		break;
	case GTK_RESPONSE_CANCEL:
		break;
	}

	gtk_widget_hide(widgets->filechooser_open);
}

void cb_delete(GtkWidget *button, struct widgets *widgets)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model = GTK_TREE_MODEL(widgets->liststore);
	GList *rows;
	GList *refs;
	int i;
	int nr_rows;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(
				widgets->treeview));
	nr_rows = gtk_tree_selection_count_selected_rows(selection);

	rows = gtk_tree_selection_get_selected_rows(selection, &model);
	refs = create_path_refs(rows, nr_rows, model);
	for (i = 0; i < nr_rows; i++) {
		GtkTreeRowReference *ref;
		GtkTreePath *path;
		GtkTreeIter iter;

		ref = g_list_nth_data(refs, i);
		path = gtk_tree_row_reference_get_path(ref);
		gtk_tree_model_get_iter(model, &iter, path);
		gtk_list_store_remove(widgets->liststore, &iter);
	}
	g_list_free_full(rows, (GDestroyNotify)gtk_tree_path_free);
	g_list_free_full(refs, (GDestroyNotify)gtk_tree_row_reference_free);

	update_window_title(widgets->window, true);
}

void cb_move_to_top(GtkWidget *button, struct widgets *widgets)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model = GTK_TREE_MODEL(widgets->liststore);
	GList *rows;
	GList *refs;
	int i;
	int nr_rows;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(
				widgets->treeview));
	nr_rows = gtk_tree_selection_count_selected_rows(selection);

	rows = gtk_tree_selection_get_selected_rows(selection, &model);
	refs = create_path_refs(rows, nr_rows, model);
	/* Keep them in the same order */
	refs = g_list_reverse(refs);
	for (i = 0; i < nr_rows; i++) {
		GtkTreeRowReference *ref;
		GtkTreePath *path;
		GtkTreeIter iter;
		GtkTreeIter parent;

		ref = g_list_nth_data(refs, i);
		path = gtk_tree_row_reference_get_path(ref);
		gtk_tree_model_get_iter(model, &iter, path);
		gtk_tree_model_get_iter_first(model, &parent);
		gtk_list_store_move_before(widgets->liststore, &iter, &parent);
	}
	g_list_free_full(rows, (GDestroyNotify)gtk_tree_path_free);
	g_list_free_full(refs, (GDestroyNotify)gtk_tree_row_reference_free);

	update_window_title(widgets->window, true);
}

void cb_move_up(GtkWidget *button, struct widgets *widgets)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model = GTK_TREE_MODEL(widgets->liststore);
	GList *rows;
	GList *refs;
	int i;
	int nr_rows;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(
				widgets->treeview));
	nr_rows = gtk_tree_selection_count_selected_rows(selection);

	rows = gtk_tree_selection_get_selected_rows(selection, &model);
	refs = create_path_refs(rows, nr_rows, model);
	for (i = 0; i < nr_rows; i++) {
		GtkTreeRowReference *ref;
		GtkTreePath *path;
		GtkTreeIter iter;
		GtkTreeIter parent;

		ref = g_list_nth_data(refs, i);
		path = gtk_tree_row_reference_get_path(ref);
		gtk_tree_model_get_iter(model, &iter, path);
		gtk_tree_model_iter_previous(model, &iter);
		gtk_tree_model_get_iter(model, &parent, path);
		gtk_list_store_move_after(widgets->liststore, &iter, &parent);
	}
	g_list_free_full(rows, (GDestroyNotify)gtk_tree_path_free);
	g_list_free_full(refs, (GDestroyNotify)gtk_tree_row_reference_free);

	update_window_title(widgets->window, true);
}

void cb_move_to_bottom(GtkWidget *button, struct widgets *widgets)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model = GTK_TREE_MODEL(widgets->liststore);
	GList *rows;
	GList *refs;
	int i;
	int nr_rows;
	int nr_items;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(
				widgets->treeview));
	nr_rows = gtk_tree_selection_count_selected_rows(selection);
	nr_items = gtk_tree_model_iter_n_children(model, NULL);

	rows = gtk_tree_selection_get_selected_rows(selection, &model);
	refs = create_path_refs(rows, nr_rows, model);
	for (i = 0; i < nr_rows; i++) {
		GtkTreeRowReference *ref;
		GtkTreePath *path;
		GtkTreeIter iter;
		GtkTreeIter parent;

		ref = g_list_nth_data(refs, i);
		path = gtk_tree_row_reference_get_path(ref);
		gtk_tree_model_get_iter(model, &iter, path);
		gtk_tree_model_iter_nth_child(model, &parent, NULL,
				nr_items - 1);
		gtk_list_store_move_after(widgets->liststore, &iter, &parent);
	}
	g_list_free_full(rows, (GDestroyNotify)gtk_tree_path_free);
	g_list_free_full(refs, (GDestroyNotify)gtk_tree_row_reference_free);

	update_window_title(widgets->window, true);
}

void cb_move_down(GtkWidget *button, struct widgets *widgets)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model = GTK_TREE_MODEL(widgets->liststore);
	GList *rows;
	GList *refs;
	int i;
	int nr_rows;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(
				widgets->treeview));
	nr_rows = gtk_tree_selection_count_selected_rows(selection);

	rows = gtk_tree_selection_get_selected_rows(selection, &model);
	refs = create_path_refs(rows, nr_rows, model);
	refs = g_list_reverse(refs);
	for (i = 0; i < nr_rows; i++) {
		GtkTreeRowReference *ref;
		GtkTreePath *path;
		GtkTreeIter iter;
		GtkTreeIter parent;

		ref = g_list_nth_data(refs, i);
		path = gtk_tree_row_reference_get_path(ref);
		gtk_tree_model_get_iter(model, &iter, path);
		gtk_tree_model_iter_next(model, &iter);
		gtk_tree_model_get_iter(model, &parent, path);
		gtk_list_store_move_before(widgets->liststore, &iter, &parent);
	}
	g_list_free_full(rows, (GDestroyNotify)gtk_tree_path_free);
	g_list_free_full(refs, (GDestroyNotify)gtk_tree_row_reference_free);

	update_window_title(widgets->window, true);
}

void cb_update(GtkTreeModel *tree_model, GtkTreePath *path,
	       GtkTreeIter *iter, gpointer user_data)
{
	char *pathstr = gtk_tree_path_to_string(path);

	if (atoi(pathstr) != last_row_selected)
		update_window_title(user_data, true);

	free(pathstr);
}

/*
 * This call-back only exists so that we can tell what the last
 * selected row was so if we drag a row but drop it back on itself,
 * we wont mark the file as having been updated.
 */
void cb_change(GtkWidget *widget, gpointer label)
{
	GtkTreeModel *model;
	GtkTreeRowReference *ref;
	GtkTreePath *path;
	char *pathstr;
	GList *rows;
	GList *refs;

	/* XXX: There must be a better way ...? */
	rows = gtk_tree_selection_get_selected_rows(GTK_TREE_SELECTION(widget),
						    &model);
	refs = create_path_refs(rows, 1, model);
	ref = g_list_nth_data(refs, 0);
	path = gtk_tree_row_reference_get_path(ref);
	pathstr = gtk_tree_path_to_string(path);
	last_row_selected = pathstr ? atoi(pathstr) : -1;

	free(pathstr);
	g_list_free_full(rows, (GDestroyNotify)gtk_tree_path_free);
	g_list_free_full(refs, (GDestroyNotify)gtk_tree_row_reference_free);
}

static void get_widgets(struct widgets *widgets, GtkBuilder *builder)
{
	widgets->window = GTK_WIDGET(gtk_builder_get_object(builder,
				"window1"));
	widgets->treeview = GTK_WIDGET(gtk_builder_get_object(builder,
				"treeview1"));
	widgets->liststore = GTK_LIST_STORE(gtk_builder_get_object(builder,
				"liststore1"));
	widgets->filechooser_open = GTK_WIDGET(gtk_builder_get_object(builder,
				"filechooserdialog2"));
	widgets->filechooser_save = GTK_WIDGET(gtk_builder_get_object(builder,
				"filechooserdialog1"));
	widgets->confirm_quit = GTK_WIDGET(gtk_builder_get_object(builder,
				"messagedialog1"));
	widgets->save_error = GTK_WIDGET(gtk_builder_get_object(builder,
				"messagedialog2"));
	widgets->about = GTK_WIDGET(gtk_builder_get_object(builder,
				"aboutdialog1"));

	widgets->sig = g_signal_connect(G_OBJECT(widgets->liststore),
					"row-changed",
					G_CALLBACK(cb_update),
					widgets->window);
}

int main(int argc, char **argv)
{
	GtkBuilder *builder;
	GError *error = NULL;
	struct widgets *widgets;
	struct stat sb;
	const char *glade_path;
	int ret;

	gtk_init(&argc, &argv);

	ret = stat("reordinator.glade", &sb);
	if (ret == 0)
		glade_path = "reordinator.glade";
	else
		glade_path = "/usr/share/reordinator/reordinator.glade";

	builder = gtk_builder_new();
	if (!gtk_builder_add_from_file(builder, glade_path, &error)) {
		g_warning("%s", error->message);
		exit(EXIT_FAILURE);
	}

	widgets = g_slice_new(struct widgets);
	get_widgets(widgets, builder);
	gtk_builder_connect_signals(builder, widgets);
	g_object_unref(G_OBJECT(builder));

	gtk_widget_show(widgets->window);

	if (argc == 2)
		load_file(argv[1], widgets);
	gtk_main();

	g_slice_free(struct widgets, widgets);

	exit(EXIT_SUCCESS);
}
