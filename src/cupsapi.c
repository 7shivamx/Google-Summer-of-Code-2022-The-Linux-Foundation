/*
 * cupsapi.c
 * 
 * This file contains functions to make IPP Requests and parse IPP responses. 
 * Gateway for communication between GUI and IPP Objects.
 *
 */

#include "printer_setup_gui.h"

/*
 * Local data structure used to pass around values
 */

typedef struct add_attribute_data
{

	ipp_t *response;
	gchar *buff;
	int buff_size;

} add_attribute_data;

/*
 * Converts object_type enum to string value
 * Returns: 
 * 			String representation of object_type enum if valid object_type given.
 * 			NULL otherwise.
 */

gchar *obj_type_string(int object_type) // type of object (enum value)
{
	if (object_type == SYSTEM_OBJECT)
	{
		return "System Object";
	}

	else if (object_type == PRINTER_OBJECT)
	{
		return "Printer Object";
	}

	else if (object_type == SCANNER_OBJECT)
	{
		return "Scanner Object";
	}

	else if (object_type == PRINTER_QUEUE)
	{
		return "Printer Object";
	}

	else
	{
		printf("Error: Invalid object_type passed to obj_type_string.\n");
		return NULL;
	}
}

/*
 * See if last cups request succeeded.
 * Returns: 
 * 			0 if request succeeded.
 * 			1 if request failed.
 */

static int check_if_cups_request_error()
{

	if (cupsLastError() >= IPP_STATUS_ERROR_BAD_REQUEST)
	{
		/* request failed */
		// printf("Request failed: %s\n", cupsLastErrorString());
		return 1;
	}

	else
	{
		// printf("Request succeeded!\n");
		return 0;
	}
}

/*
 * Adds attribute and its value to Object
 */

static void add_attribute(
	char *attr_name,		 // IPP Attribute to be added to object description
	ipp_tag_t value_tag,	 // Type of Attribute
	add_attribute_data data) // Contains IPP response, buffer to add attribute to, and buffer size
{

	ipp_t *response = data.response;
	gchar *buff = data.buff;
	int buff_size = data.buff_size;

	ipp_attribute_t *attr;

	if ((attr = ippFindAttribute(response, attr_name,
								 value_tag)) != NULL)
	{
		const gchar *attr_val;

		if (value_tag == IPP_TAG_ENUM)
		{
			attr_val = ippEnumString(attr_name, ippGetInteger(attr, 0));
		}

		else
		{
			attr_val = ippGetString(attr, 0, NULL);
		}

		snprintf(buff + strlen(buff), buff_size - strlen(buff),
				 "<b>\t %s:</b> = %s\n",
				 attr_name,
				 attr_val);
	}

	else
	{
		snprintf(buff + strlen(buff), buff_size - strlen(buff),
				 "<b>\t %s:</b> = unknown\n",
				 attr_name);
	}
}

/*
 * Get-System-Attributes or Get-(Object)-Attributes Operation
 * Returns:
 * 			1 if success
 * 			0 if failure
 */

int get_attributes(
	int obj_type_enum, // type of object (enum value)
	http_t *http,	   // http connection
	gchar *uri,		   // object uri
	gchar *buff,	   // buffer to add attributes to
	int buff_size)	   // buffer size
{

	int operation;
	char *uri_tag;
	char *resource;

	if (obj_type_enum == SYSTEM_OBJECT)
	{
		operation = IPP_OP_GET_SYSTEM_ATTRIBUTES;
		uri_tag = "system-uri";
		resource = "/ipp/system";
	}

	else
	{

		/* Add other conditions for scanner, print-queue etc. */
		operation = IPP_OP_GET_PRINTER_ATTRIBUTES;
		uri_tag = "printer-uri";
		resource = "/ipp/print";
	}

	ipp_t *request = ippNewRequest(operation);
	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, uri_tag, NULL, uri);
	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsUser());

	ipp_t *response = cupsDoRequest(http, request, "/ipp/system");

	if (check_if_cups_request_error())
	{
		return 0;
	}

	ipp_attribute_t *attr;
	strcpy(buff, "");

	add_attribute_data data = {response, buff, buff_size};

	if (obj_type_enum == SYSTEM_OBJECT)
	{
		add_attribute("system-state", IPP_TAG_ENUM, data);
		add_attribute("system-make-and-model", IPP_TAG_TEXT, data);
		add_attribute("system-dns-sd-name", IPP_TAG_NAME, data);
		add_attribute("system-location", IPP_TAG_TEXT, data);
		add_attribute("system-geo-location", IPP_TAG_URI, data);
	}

	else
	{

		/* Add other conditions for scanner, print-queue etc. */
		add_attribute("printer-state", IPP_TAG_ENUM, data);
		add_attribute("printer-make-and-model", IPP_TAG_TEXT, data);
		add_attribute("printer-dns-sd-name", IPP_TAG_NAME, data);
		add_attribute("printer-location", IPP_TAG_TEXT, data);
		add_attribute("printer-geo-location", IPP_TAG_URI, data);
		add_attribute("printer-more-info", IPP_TAG_URI, data);
		add_attribute("printer-supply-info-uri", IPP_TAG_URI, data);
	}

	return 1;
}

/*
 * Get-Printers Operation
 * Returns:
 			1 if success
 			0 if failure
 */

int get_printers(http_t *http,			   // http connection
				 struct IppObject *so,	   // system object (on which get_printers is to be run)
				 GtkTreeStore *tree_store, // tree_store of GUI treeview (to add printers to GUI)
				 int buff_size)			   // size of object attribute field
{
	gchar *uri = so->uri;
	GtkTreeRowReference *tree_ref = so->tree_ref;
	int check = 1;

	ipp_t *request = ippNewRequest(IPP_OP_GET_PRINTERS);
	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "system-uri", NULL, uri);
	ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsUser());

	ipp_t *response = cupsDoRequest(http, request, "/ipp/system");

	if (check_if_cups_request_error())
	{
		return 0;
	}

	ipp_attribute_t *attr = NULL;
	GList *printer_names = NULL;
	GList *printer_uris = NULL;

	for (attr = ippFindAttribute(response, "printer-name", IPP_TAG_NAME); attr; attr = ippFindNextAttribute(response, "printer-name", IPP_TAG_NAME))
	{
		gchar *str = g_strdup(ippGetString(attr, 0, NULL));
		printer_names = g_list_prepend(printer_names, str);
	}

	for (attr = ippFindAttribute(response, "printer-uri-supported", IPP_TAG_URI); attr; attr = ippFindNextAttribute(response, "printer-uri-supported", IPP_TAG_URI))
	{
		gchar *str = g_strdup(ippGetString(attr, 0, NULL));
		printer_uris = g_list_prepend(printer_uris, str);
	}

	if (g_list_length(printer_names) != g_list_length(printer_uris))
	{
		puts("Error: printer-name and printer-uri-supported attributes not returning correct number of values");
		return 0;
	}

	for (GList *l1 = printer_names, *l2 = printer_uris; (l1 && l2); l1 = l1->next, l2 = l2->next)
	{

		char *printer_name = l1->data;
		char *printer_uri = l2->data;

		gchar buff[buff_size];

		/* Get Printer Attributes */

		if (get_attributes(PRINTER_OBJECT, http, printer_uri, buff, buff_size))
		{
			struct IppObject *printer = g_new(struct IppObject, 1);
			printer->object_type = PRINTER_OBJECT;
			printer->object_name = g_strdup(printer_name);
			printer->sources = NULL;
			printer->children = NULL;
			printer->tree_ref = NULL;
			printer->uri = g_strdup(printer_uri);
			printer->objAttr = g_strdup(buff);

			so->children = g_list_prepend(so->children, printer);

			GtkTreeIter iter;
			GtkTreeIter parentIter;
			GtkTreePath *parentPath = gtk_tree_row_reference_get_path(tree_ref);
			gtk_tree_model_get_iter(GTK_TREE_MODEL(tree_store), &parentIter, parentPath);
			gtk_tree_store_append(tree_store, &iter, &parentIter);
			gtk_tree_store_set(tree_store, &iter, 0, printer->object_name, 1, obj_type_string(printer->object_type), 2, printer, -1);
			GtkTreePath *path = gtk_tree_model_get_path(GTK_TREE_MODEL(tree_store), &iter);
			printer->tree_ref = gtk_tree_row_reference_new(GTK_TREE_MODEL(tree_store), path);

			gtk_tree_path_free(parentPath);
			gtk_tree_path_free(path);

			printf("Get-Printer-attributes: Success\n");
		}

		else
		{
			printf("Error: Get-Printer-attributes: Failed\n");
			check = 0;
		}
	}

	return check;
}
