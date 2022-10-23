/*
 * system-services-show.c
 * 
 * This file performs 2 main tasks:
 *      1. Setting up the GUI for the project.
 *      2. Browsing and Resolving browser events to find system service instances.
 * 
 */

#include "printer_setup_gui.h"

int OBJ_ATTR_SIZE = 1024;                       // Modify this field to increase character limit of objAttr field of IppObject
gchar *systemServiceType = "_ipps-system._tcp"; // Service type to browse for.

/*
 * Global variables to access GUI and IPP objects
 */

static GtkWidget *main_window = NULL;
static GtkTreeView *tree_view = NULL;
static GtkTreeModel *sortmodel = NULL;
static GtkTreeStore *tree_store = NULL;
static GtkWidget *info_label = NULL;
static AvahiServer *server = NULL;
static GHashTable *system_map_hash_table = NULL;
static GtkWidget *hbox;
static GtkWidget *lvbox;
static GtkWidget *rvbox;
static GtkWidget *scrollWindow1;
static GtkWidget *scrollWindow2;

int get_attributes(
    int obj_type_enum,
    http_t *http,
    char *uri,
    gchar *buff,
    int buff_size);

int get_printers(http_t *http,
                 struct IppObject *so,
                 GtkTreeStore *tree_store,
                 int buff_size);

/*
 * Compares ObjectSources attributes of system object with newly discovered attributes.
 * Returns: 
 *          ObjectSources if any match.
 *          NULL otherwise
 */

struct ObjectSources *is_system_object_present(
    GList *sources,                           // Objectsources (sources) attribute of system object to compare with
    AVAHI_GCC_UNUSED AvahiProtocol protocol,  // protocol discovered
    AVAHI_GCC_UNUSED const char *domain_name, // domain discovered
    const char *host_name,                    // host name discovered.
    uint16_t port)                            // port discovered.
{

    for (GList *l = sources; l; l = l->next)
    {
        struct ObjectSources *s = l->data;

        if (s->family == protocol &&
            s->port == port &&

            avahi_domain_equal(s->host, host_name) &&
            avahi_domain_equal(s->domain_name, domain_name))
        {
            return s;
        }
    }

    return NULL;
}

/*
 * In case of a remove event, remove the sources that no longer exist.
 */

static void remove_from_system_object(
    struct IppObject *so,                     // system object to remove sources from
    AVAHI_GCC_UNUSED AvahiProtocol protocol,  // protocol in remove event
    AVAHI_GCC_UNUSED const char *domain_name, // domain name of remove event
    const char *host_name,                    // host name in remove event
    uint16_t port)                            // port in remove event
{
    struct ObjectSources *source = NULL;

    if (source = is_system_object_present(so->sources, protocol, domain_name, host_name, port))
    {
        so->sources = g_list_remove(so->sources, source);
    }
}

/*
 * Remove entire IppObject.
 * NOTE: Call this on a System Object, it will automatically delete its children.
 */

void remove_object(
    int object_type,      // type of IppObject (enum)
    struct IppObject *so) // IppObject to remove
{

    if (object_type == SYSTEM_OBJECT)
    {

        for (GList *l = so->children; l; l = l->next)
        {
            remove_object(((struct IppObject *)(l->data))->object_type, (struct IppObject *)(l->data));
        }

        g_list_free(so->children);
        so->children = NULL;
    }

    GtkTreeIter iter;
    GtkTreePath *path;

    if ((path = gtk_tree_row_reference_get_path(so->tree_ref)))
    {
        gtk_tree_model_get_iter(GTK_TREE_MODEL(tree_store), &iter, path);
        gtk_tree_path_free(path);
        gtk_tree_store_remove(tree_store, &iter);
    }

    gtk_tree_row_reference_free(so->tree_ref);
    g_free(so->uri);
    g_free(so->objAttr);

    if (object_type == SYSTEM_OBJECT)
    {
        g_hash_table_remove(system_map_hash_table, so->object_name);
    }

    g_free(so->object_name);
    g_free(so);
}

/*
 * Add newly discovered sources to System Object in case of new event.
 */

static void add_to_system_object(
    struct IppObject *so,                     // system object to add sources to
    AVAHI_GCC_UNUSED AvahiProtocol protocol,  // protocol in new event
    AVAHI_GCC_UNUSED const char *domain_name, // domain name of new event
    const char *host_name,                    // host name in new event
    uint16_t port)                            // port in new event
{
    struct ObjectSources *source = NULL;

    if (source = is_system_object_present(so->sources, protocol, domain_name, host_name, port))
    {
        /* Object already added */
        return;
    }

    else
    {

        source = g_new(struct ObjectSources, 1);
        source->domain_name = g_strdup(domain_name);
        source->host = g_strdup(host_name);
        source->port = port;
        source->family = protocol;
        so->sources = g_list_prepend(so->sources, source);
    }

    if (so->uri == NULL)
    {

        char uri[1024];
        httpAssembleURI(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipp", NULL,
                        host_name, port, "/ipp/system");

        so->uri = g_strdup(uri);
    }

    if ((so->uri != NULL) && (so->objAttr == NULL))
    {

        http_t *http = httpConnect2(host_name, port, NULL, AF_UNSPEC, HTTP_ENCRYPTION_ALWAYS, 1, 0, NULL);
        gchar buff[OBJ_ATTR_SIZE];

        /* Get System Attributes */

        if (get_attributes(SYSTEM_OBJECT, http, so->uri, buff, OBJ_ATTR_SIZE))
        {
            so->objAttr = g_strdup(buff);
            printf("Get-system-attributes: Success\n");
        }

        else
        {
            printf("Error: Get-system-attributes: Failed\n");
        }
    }

    if ((so->uri != NULL) && (so->children == NULL))
    {

        http_t *http = httpConnect2(host_name, port, NULL, AF_UNSPEC, HTTP_ENCRYPTION_ALWAYS, 1, 0, NULL);

        /* Get Printers */

        if (get_printers(http, so, tree_store, OBJ_ATTR_SIZE))
        {
            printf("Get-Printers: Success\n");
        }

        else
        {
            printf("Error: Get-Printers: Failed\n");
        }

        /* Add other methods to get scanners, get queues */

        /* Expand row */
        GtkTreePath *ppath = gtk_tree_row_reference_get_path(so->tree_ref);
        gtk_tree_view_expand_row(tree_view, ppath, FALSE);
        gtk_tree_path_free(ppath);
    }
}

/*
 * Resolver for AVAHI_BROWSER_REMOVE event.
 */

static void service_remove_resolver_callback(
    AvahiSServiceResolver *r,
    AVAHI_GCC_UNUSED AvahiIfIndex interface,
    AVAHI_GCC_UNUSED AvahiProtocol protocol,
    AvahiResolverEvent event,
    AVAHI_GCC_UNUSED const char *service_name,
    AVAHI_GCC_UNUSED const char *service_type,
    AVAHI_GCC_UNUSED const char *domain_name,
    const char *host_name,
    const AvahiAddress *a,
    uint16_t port,
    AvahiStringList *txt,
    AVAHI_GCC_UNUSED AvahiLookupResultFlags flags,
    void *userdata)
{

    if (!service_name)
    {
        printf("Error: empty service_name passed to resolver\n");
    }

    else if (event == AVAHI_RESOLVER_FOUND)
    {
        struct IppObject *so;

        if (so = g_hash_table_lookup(system_map_hash_table, service_name))
        {

            remove_from_system_object(so, protocol, domain_name, host_name, port);

            /* Checking if system_object is empty */
            if (so->sources == NULL)
            {
                remove_object(SYSTEM_OBJECT, so);
            }
        }
    }

    else if (event == AVAHI_RESOLVER_FAILURE)
    {
        printf("Error: Failed to resolve: %s\n", avahi_strerror(avahi_server_errno(server)));
    }

    avahi_s_service_resolver_free(r);
}

/*
 * Resolver for AVAHI_BROWSER_NEW event.
 */

static void service_new_resolver_callback(
    AvahiSServiceResolver *r,
    AVAHI_GCC_UNUSED AvahiIfIndex interface,
    AVAHI_GCC_UNUSED AvahiProtocol protocol,
    AvahiResolverEvent event,
    AVAHI_GCC_UNUSED const char *service_name,
    AVAHI_GCC_UNUSED const char *service_type,
    AVAHI_GCC_UNUSED const char *domain_name,
    const char *host_name,
    const AvahiAddress *a,
    uint16_t port,
    AvahiStringList *txt,
    AVAHI_GCC_UNUSED AvahiLookupResultFlags flags,
    void *userdata)
{

    if (!service_name)
    {
        printf("Error: empty service_name passed to resolver\n");
    }

    else if (event == AVAHI_RESOLVER_FOUND)
    {
        struct IppObject *so;
        GtkTreePath *path = NULL;
        GtkTreeIter iter;

        if (!(so = g_hash_table_lookup(system_map_hash_table, service_name)))
        {
            so = g_new(struct IppObject, 1);
            so->object_type = SYSTEM_OBJECT;
            so->object_name = g_strdup(service_name);
            so->sources = NULL;
            so->children = NULL;
            so->tree_ref = NULL;
            so->uri = NULL;
            so->objAttr = NULL;

            gtk_tree_store_append(tree_store, &iter, NULL);
            gtk_tree_store_set(tree_store, &iter, 0, so->object_name, 1, obj_type_string(so->object_type), 2, so, -1);
            path = gtk_tree_model_get_path(GTK_TREE_MODEL(tree_store), &iter);
            so->tree_ref = gtk_tree_row_reference_new(GTK_TREE_MODEL(tree_store), path);
            gtk_tree_path_free(path);

            g_hash_table_insert(system_map_hash_table, so->object_name, so);
        }

        add_to_system_object(so, protocol, domain_name, host_name, port);
    }

    else if (event == AVAHI_RESOLVER_FAILURE)
    {
        printf("Error: Failed to resolve: %s\n", avahi_strerror(avahi_server_errno(server)));
    }

    avahi_s_service_resolver_free(r);
}

/*
 * Service Browser Callback function.
 * Creates service resolvers to handle AVAHI_BROWSER_NEW and AVAHI_BROWSER_REMOVE events.
 */

static void service_browser_callback(
    AvahiSServiceBrowser *b,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiBrowserEvent event,
    const char *service_name,
    const char *service_type,
    const char *domain_name,
    AvahiLookupResultFlags flags,
    void *userdata)
{

    if (event == AVAHI_BROWSER_NEW)
    {
        printf("Browser: AVAHI_BROWSER_NEW\n");
        avahi_s_service_resolver_new(server, interface, protocol, service_name, service_type, domain_name, AVAHI_PROTO_UNSPEC, 0, service_new_resolver_callback, b);
    }

    else if (event == AVAHI_BROWSER_REMOVE)
    {
        printf("Browser: AVAHI_BROWSER_REMOVE\n");
        avahi_s_service_resolver_new(server, interface, protocol, service_name, service_type, domain_name, AVAHI_PROTO_UNSPEC, 0, service_remove_resolver_callback, b);
    }

    else
    {
        printf("Browser: Non NEW/REMOVE event.\n");
    }
}

/*
 * Update sidebar to show attributes of currently selected IppObject
 */

static void update_label(struct IppObject *so) // Currently selected IppObject
{
    gchar t[OBJ_ATTR_SIZE];

    if (so == NULL)
    {
        gtk_label_set_markup(GTK_LABEL(info_label), "Select a device from the list\n");
        return;
    }

    else if (so->objAttr != NULL)
    {

        gtk_label_set_markup(GTK_LABEL(info_label), so->objAttr);
    }

    else
    {
        snprintf(t, sizeof(t),
                 "<b>GET ATTRIBUTES REQUEST UNSUCCESSFUL</b> \n\n");

        if (so->object_name != NULL)
        {
            snprintf(t + strlen(t), sizeof(t) - strlen(t),
                     "<b>System Object Name:</b> %s\n",
                     so->object_name);
        }

        snprintf(t + strlen(t), sizeof(t) - strlen(t), "<b>Sources: </b>\n");

        for (GList *l = so->sources; l; l = l->next)
        {
            struct ObjectSources *s = l->data;
            snprintf(t + strlen(t), sizeof(t) - strlen(t),
                     "<b>\t Domain name:</b> %s\n"
                     "<b>\t Host:</b> %s\n"
                     "<b>\t Port:</b> %d\n"
                     "<b>\t Family(Protocol):</b> %s\n\n",
                     s->domain_name,
                     s->host,
                     s->port,
                     avahi_proto_to_string(s->family));
        }

        // printf("Label Content: \n%s\n", t);
        gtk_label_set_markup(GTK_LABEL(info_label), t);
    }
}

/*
 * Get currently selected object from GUI
 * Returns: 
 *          Selected IppObject if any entry is selected.
 *          NULL otherwise
 */

static struct IppObject *get_object_on_cursor(void)
{
    GtkTreePath *path;
    GtkTreePath *true_path;
    struct IppObject *so;
    GtkTreeIter iter;

    gtk_tree_view_get_cursor(tree_view, &path, NULL);

    if (!path)
    {
        return NULL;
    }

    true_path = gtk_tree_model_sort_convert_path_to_child_path(GTK_TREE_MODEL_SORT(sortmodel),
                                                               path);

    if (!true_path)
    {
        printf("Error: true_path is NULL\n");
        return NULL;
    }

    gtk_tree_model_get_iter(GTK_TREE_MODEL(tree_store), &iter, true_path);
    gtk_tree_model_get(GTK_TREE_MODEL(tree_store), &iter, 2, &so, -1);
    gtk_tree_path_free(path);
    gtk_tree_path_free(true_path);
    return so;
}

/*
 * Callback function for cursor-changed event
 * Calls functions to see currently select IppObject and update sidebar accordingly
 */

static void tree_view_on_cursor_changed(AVAHI_GCC_UNUSED GtkTreeView *tv, AVAHI_GCC_UNUSED gpointer userdata)
{
    struct IppObject *so = get_object_on_cursor();

    update_label(so);
}

static gboolean main_window_on_delete_event(AVAHI_GCC_UNUSED GtkWidget *widget, AVAHI_GCC_UNUSED GdkEvent *event, AVAHI_GCC_UNUSED gpointer user_data)
{
    gtk_main_quit();
    return FALSE;
}

int main(int argc, char *argv[])
{
    AvahiServerConfig config;
    GtkTreeViewColumn *col1;
    GtkTreeViewColumn *col2;
    gint error;
    AvahiGLibPoll *poll_api;
    gint window_width = 1000;
    gint window_height = 600;

    gtk_init(&argc, &argv);

    avahi_set_allocator(avahi_glib_allocator());

    poll_api = avahi_glib_poll_new(NULL, G_PRIORITY_DEFAULT);

    main_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_position(GTK_WINDOW(main_window), GTK_WIN_POS_CENTER);
    gtk_window_set_default_size(GTK_WINDOW(main_window), window_width, window_height);
    gtk_window_set_title(GTK_WINDOW(main_window), "IPP Device Management");
    gtk_container_set_border_width(GTK_CONTAINER(main_window), 10);
    g_signal_connect(main_window, "delete-event", (GCallback)main_window_on_delete_event, NULL);

    hbox = gtk_hbox_new(TRUE, 5);
    lvbox = gtk_vbox_new(FALSE, 5);
    rvbox = gtk_vbox_new(FALSE, 5);

    scrollWindow1 = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrollWindow1),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrollWindow1),
                                        GTK_SHADOW_IN);

    scrollWindow2 = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrollWindow1),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrollWindow1),
                                        GTK_SHADOW_IN);

    info_label = gtk_label_new("Select a Device from the list");

    gtk_container_add(GTK_CONTAINER(main_window), hbox);
    gtk_box_pack_start(GTK_BOX(hbox), lvbox, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), rvbox, TRUE, TRUE, 0);

    tree_store = gtk_tree_store_new(3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER);
    sortmodel = gtk_tree_model_sort_new_with_model(GTK_TREE_MODEL(tree_store));
    tree_view = GTK_TREE_VIEW(gtk_tree_view_new_with_model(sortmodel));

    g_signal_connect(GTK_WIDGET(tree_view), "cursor-changed", (GCallback)tree_view_on_cursor_changed, NULL);

    gtk_container_add(GTK_CONTAINER(lvbox), scrollWindow1);
    gtk_container_add(GTK_CONTAINER(scrollWindow1), GTK_WIDGET(tree_view));
    gtk_container_add(GTK_CONTAINER(rvbox), scrollWindow2);
    gtk_container_add(GTK_CONTAINER(scrollWindow2), info_label);

    gtk_tree_view_insert_column_with_attributes(tree_view, -1, "Name", gtk_cell_renderer_text_new(), "text", 0, NULL);
    gtk_tree_view_insert_column_with_attributes(tree_view, -1, "Object Type", gtk_cell_renderer_text_new(), "text", 1, NULL);
    gtk_tree_view_column_set_sort_column_id(gtk_tree_view_get_column(tree_view, 0), 0);
    gtk_tree_view_column_set_sort_column_id(gtk_tree_view_get_column(tree_view, 1), 1);
    gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(sortmodel), 0, GTK_SORT_ASCENDING);

    gtk_tree_view_column_set_resizable(col1 = gtk_tree_view_get_column(tree_view, 0), TRUE);
    gtk_tree_view_column_set_resizable(col2 = gtk_tree_view_get_column(tree_view, 1), TRUE);

    gtk_tree_view_column_set_sizing(col1, GTK_TREE_VIEW_COLUMN_FIXED);
    gtk_tree_view_column_set_fixed_width(col1, window_width / 4);

    gtk_tree_view_column_set_sizing(col1, GTK_TREE_VIEW_COLUMN_FIXED);
    gtk_tree_view_column_set_fixed_width(col1, window_width / 4);

    gtk_tree_view_column_set_expand(col1, TRUE);
    gtk_tree_view_column_set_expand(col2, TRUE);

    /* Using avahi_domain_hash as the hashing function. Can use different hash function. */

    system_map_hash_table = g_hash_table_new((GHashFunc)avahi_domain_hash, (GEqualFunc)avahi_domain_equal);

    avahi_server_config_init(&config);
    config.publish_hinfo = config.publish_addresses = config.publish_domain = config.publish_workstation = FALSE;
    server = avahi_server_new(avahi_glib_poll_get(poll_api), &config, NULL, NULL, &error);
    avahi_server_config_free(&config);

    g_assert(server);

    avahi_s_service_browser_new(server, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, systemServiceType, argc >= 2 ? argv[1] : NULL, 0, service_browser_callback, NULL);

    gtk_widget_show_all(main_window);
    gtk_main();

    avahi_server_free(server);
    avahi_glib_poll_free(poll_api);

    return 0;
}
