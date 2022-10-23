#include <stdlib.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <gtk/gtk.h>
#include <gtk/gtkx.h>
#include <math.h>
#include <time.h>
#include <ctype.h>
#include <sys/mman.h>

#define COLUMN_CHAR_SIZE 20

GtkTreeStore *store;
GtkWidget *window;
GtkTreeModel *sortmodel;
GtkWidget *view;
GtkWidget *scrollWindow;
GtkWidget *hbox;
GtkWidget *lvbox;
GtkWidget *rvbox;
GtkWidget *vbox1;
GtkWidget *vbox2;
GtkWidget *modBtn;
GtkWidget *rvLabel;

gchar selected_col1_val[COLUMN_CHAR_SIZE];

char tmp[3][1024];

enum
{
	COL1 = 0,
	COL2,
	COL3,
	N_COLUMNS
};

char column_names[][COLUMN_CHAR_SIZE] = {
	"Device Name",
	"Device Status",
	"Modifiable"};

int setup_tree(void);
int populate_tree_model(void);
void on_destroy();
void render_and_sort_view(GtkTreeViewColumn *col1,
						  GtkTreeViewColumn *col2,
						  GtkTreeViewColumn *col3);
static void on_row_activated(GtkTreeView *treeview,
							 GtkTreePath *path,
							 GtkTreeViewColumn *column,
							 gpointer userdata);
static void modBtn_on_click(GtkButton *button,
							gpointer user_data);

int main(int argc, char *argv[])
{

	gtk_init(&argc, &argv);

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
	gtk_window_set_default_size(GTK_WINDOW(window), 700, 400);
	gtk_window_set_title(GTK_WINDOW(window), "IPP Device Management");
	gtk_container_set_border_width(GTK_CONTAINER(window), 10);

	hbox = gtk_hbox_new(TRUE, 5);
	lvbox = gtk_vbox_new(FALSE, 5);
	rvbox = gtk_vbox_new(FALSE, 5);

	/**
	 * lvbox setup
	 * Contains ScrollWindow to select Printer/MF device
	 */ 

	vbox1 = gtk_vbox_new(FALSE, 5);
	vbox2 = gtk_vbox_new(FALSE, 5);

	scrollWindow = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrollWindow),
								   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrollWindow),
										GTK_SHADOW_IN);

	if (setup_tree() == EXIT_FAILURE)
	{
		return EXIT_FAILURE;
	}

	modBtn = gtk_button_new_with_label("Modify");
	gtk_widget_set_sensitive(modBtn, FALSE);

	gtk_widget_set_halign(modBtn, GTK_ALIGN_END);
	gtk_widget_set_valign(modBtn, GTK_ALIGN_END);

	g_signal_connect(view, "row-activated",
					G_CALLBACK(on_row_activated), NULL);

	g_signal_connect(modBtn, "clicked", G_CALLBACK(modBtn_on_click), NULL);

	gtk_container_add(GTK_CONTAINER(scrollWindow), view);
	gtk_container_add(GTK_CONTAINER(vbox1), scrollWindow);
	gtk_container_add(GTK_CONTAINER(vbox2), modBtn);

	gtk_box_pack_start(GTK_BOX(lvbox), vbox1, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(lvbox), vbox2, FALSE, FALSE, 0);

	/**
	 * rvbox setup
	 * contains printer attributes 
	 */

	rvLabel = gtk_label_new("Select a Device from the list");
	gtk_container_add(GTK_CONTAINER(rvbox), rvLabel);

	/**
	 * Add lvbox rvbox to hbox
	 */

	gtk_box_pack_start(GTK_BOX(hbox), lvbox, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), rvbox, TRUE, TRUE, 0);

	gtk_container_add(GTK_CONTAINER(window), hbox);

	/**
	 * Final setup
	 */




	g_signal_connect(G_OBJECT(window), "destroy",
					 G_CALLBACK(on_destroy), NULL);

	// g_signal_connect(G_OBJECT(window), "destroy",
	// 				G_CALLBACK(gtk_main_quit), G_OBJECT(window));

	gtk_widget_show_all(window);
	// gtk_widget_hide(rvbox);

	gtk_main();
	return EXIT_SUCCESS;
}

int setup_tree(void)
{
	GtkTreeViewColumn *col1;
	GtkTreeViewColumn *col2;
	GtkTreeViewColumn *col3;

	store = gtk_tree_store_new(N_COLUMNS,
							   G_TYPE_STRING,
							   G_TYPE_STRING,
							   G_TYPE_BOOLEAN);

	if (populate_tree_model() == EXIT_FAILURE)
	{
		return EXIT_FAILURE;
	}

	sortmodel = gtk_tree_model_sort_new_with_model(GTK_TREE_MODEL(store));
	view = gtk_tree_view_new_with_model(sortmodel);
	gtk_tree_view_set_activate_on_single_click(GTK_TREE_VIEW(view), TRUE);

	render_and_sort_view(col1, col2, col3);

	return EXIT_SUCCESS;
}

int populate_tree_model(void)
{

	GtkTreeIter iter1; /* Child iter  */

	FILE *f[N_COLUMNS];

	f[0] = fopen("device_records/col1.txt", "r");
	f[1] = fopen("device_records/col2.txt", "r");
	f[2] = fopen("device_records/col3.txt", "r");

	for (int i = 0; i < N_COLUMNS; i++)
	{
		if (f[i] == NULL)
		{
			printf("File col%d not found\n", i + 1);
			return EXIT_FAILURE;
		}
	}

	while (1)
	{

		if (fgets(tmp[0], 1024, f[0]) == NULL)
		{
			fclose(f[0]);
			fclose(f[1]);
			fclose(f[2]);
			break;
		}

		fgets(tmp[1], 1024, f[1]);
		fgets(tmp[2], 1024, f[2]);

		for (int i = 0; i < N_COLUMNS; i++)
		{
			tmp[i][strlen(tmp[i]) - 1] = 0; // remove newline byte
		}

		gtk_tree_store_append(store, &iter1, NULL);
		gtk_tree_store_set(store, &iter1,
						   COL1, tmp[0],
						   COL2, tmp[1],
						   COL3, strcmp(tmp[2], "1") ? TRUE : FALSE,
						   -1);
	}

	return EXIT_SUCCESS;
}

void render_and_sort_view(GtkTreeViewColumn *col1,
						  GtkTreeViewColumn *col2,
						  GtkTreeViewColumn *col3)
{

	GtkCellRenderer *renderer;

	renderer = gtk_cell_renderer_text_new();
	// g_object_set (G_OBJECT (renderer),
	//               "foreground", "red",
	//               NULL);
	col1 = gtk_tree_view_column_new_with_attributes(column_names[0], renderer,
													"text", COL1,
													NULL);

	/* Add the column to the view. */
	gtk_tree_view_append_column(GTK_TREE_VIEW(view), col1);

	renderer = gtk_cell_renderer_text_new();
	col2 = gtk_tree_view_column_new_with_attributes(column_names[1],
													renderer,
													"text", COL2,
													NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(view), col2);

	renderer = gtk_cell_renderer_toggle_new();
	col3 = gtk_tree_view_column_new_with_attributes(column_names[2],
													renderer,
													"active", COL3,
													NULL);

	gtk_tree_view_append_column(GTK_TREE_VIEW(view), col3);

	gtk_tree_view_column_set_sort_column_id(col1, COL1);
	gtk_tree_view_column_set_sort_column_id(col2, COL2);
	gtk_tree_view_column_set_sort_column_id(col3, COL3);
	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(sortmodel), COL1, GTK_SORT_ASCENDING);

}

void on_destroy()
{
	gtk_main_quit();
}

static void
on_row_activated(GtkTreeView *view,
				 GtkTreePath *path,
				 GtkTreeViewColumn *col,
				 gpointer userdata)
{
	GtkTreeIter iter;
	GtkTreePath *true_path;

	/* 
     * We have a path that is sorted. So first, let's
     * undo the sort.
     */
	true_path = gtk_tree_model_sort_convert_path_to_child_path(GTK_TREE_MODEL_SORT(sortmodel),
															   path);

	gchar *col1;
	gchar *col2;
	Bool col3;

	if (gtk_tree_model_get_iter(GTK_TREE_MODEL(store), &iter, true_path))
	{
		gtk_tree_model_get(GTK_TREE_MODEL(store), &iter,
						   COL1, &col1,
						   COL2, &col2,
						   COL3, &col3,
						   -1);

		// printf("%s %s %d\n", col1, col2, col3);
	}

	if (col3)
	{
		gtk_widget_set_sensitive(modBtn, TRUE);
	}

	else
	{
		gtk_widget_set_sensitive(modBtn, FALSE);
	}

	strcpy(selected_col1_val, col1);

	// gtk_widget_show(rvbox);
	gtk_label_set_text(GTK_LABEL(rvLabel), selected_col1_val);

}

static void modBtn_on_click(GtkButton *button,
							gpointer user_data)
{
	printf("Modify %s\n", selected_col1_val);
}
