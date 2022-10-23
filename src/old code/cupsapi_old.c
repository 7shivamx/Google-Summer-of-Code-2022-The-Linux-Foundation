#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cups/cups.h>
#include <assert.h>
#include <time.h>
#include <avahi-client/client.h>
#include <avahi-client/lookup.h>
#include <avahi-common/simple-watch.h>
#include <avahi-common/malloc.h>
#include <avahi-common/error.h>

typedef struct
{
  int num_dests;
  cups_dest_t *dests;
} my_user_data_t;

static const char *const sysattrs[] =
    { // Requested system attributes
        "system-state",
        "system-state-change-date-time",
        "system-state-reasons"};

static const char *const states[] = // *-state strings
    {
        "idle",
        "processing jobs",
        "stopped"};

int my_dest_cb(my_user_data_t *user_data, unsigned flags,
               cups_dest_t *dest)
{
  if (flags & CUPS_DEST_FLAGS_REMOVED)
  {
    /*
    * Remove destination from array...
    */

    user_data->num_dests =
        cupsRemoveDest(dest->name, dest->instance,
                       user_data->num_dests,
                       &(user_data->dests));
  }
  else
  {
    /*
    * Add destination to array...
    */

    user_data->num_dests =
        cupsCopyDest(dest, user_data->num_dests,
                     &(user_data->dests));
  }

  return (1);
}

int my_get_dests(cups_ptype_t type, cups_ptype_t mask,
                 cups_dest_t **dests)
{
  my_user_data_t user_data = {0, NULL};

  if (!cupsEnumDests(CUPS_DEST_FLAGS_NONE, 1000, NULL, type,
                     mask, (cups_dest_cb_t)my_dest_cb,
                     &user_data))
  {
    /*
    * An error occurred, free all of the destinations and
    * return...
    */

    cupsFreeDests(user_data.num_dests, user_data.dests);

    *dests = NULL;

    return (0);
  }

  /*
  * Return the destination array...
  */

  *dests = user_data.dests;

  return (user_data.num_dests);
}

void print_cups_dest_t(cups_dest_t *dests)
{
  printf("name: %s\n", dests->name);
  printf("instance: %s\n", dests->instance);
  printf("is_default: %d\n", dests->is_default);
  printf("num_options: %d\n", dests->num_options);

  /* Get First option name and value: */

  printf("options->name: %s\n", (dests->options + 1)->name);
  printf("options->value: %s\n\n", (dests->options + 1)->value);
}

void cups_dests_discovery()
{
  cups_dest_t **dests = (cups_dest_t **)malloc(sizeof(cups_dest_t *));
  int ndest = my_get_dests(0, 0, dests);

  /* Required destinations are in dests pointer.
     These destinations are discovered by cups using various protocols. */

  if (dests != NULL)
  {
    printf("ndest: %d\n", ndest);

    for (int i = 0; i < ndest; i++)
    {
      print_cups_dest_t(*dests + i);
    }
  }
}

void if_get_printers(ipp_t *response)
{
  for (ipp_attribute_t *attr = ippFindAttribute(response, "printer-name", IPP_TAG_NAME); attr; attr = ippFindNextAttribute(response, "printer-name", IPP_TAG_NAME))
  {
    puts(ippGetString(attr, 0, NULL));
  }
}

void if_get_printer_attributes(ipp_t *response)
{
  ipp_attribute_t *attr;

  if ((attr = ippFindAttribute(response, "printer-state",
                               IPP_TAG_ENUM)) != NULL)
  {
    printf("printer-state=%s\n",
           ippEnumString("printer-state", ippGetInteger(attr, 0)));
  }

  else
  {
    puts("printer-state=unknown");
  }

  if ((attr = ippFindAttribute(response, "printer-state-message",
                               IPP_TAG_TEXT)) != NULL)
  {
    printf("printer-state-message=\"%s\"\n",
           ippGetString(attr, 0, NULL));
  }

  if ((attr = ippFindAttribute(response, "printer-state-reasons",
                               IPP_TAG_KEYWORD)) != NULL)
  {
    int i, count = ippGetCount(attr);

    puts("printer-state-reasons=");
    for (i = 0; i < count; i++)
      printf("    %s\n", ippGetString(attr, i, NULL));
  }
}

void if_get_system_attributes(ipp_t *response)
{
  ipp_attribute_t *attr;

  if ((attr = ippFindAttribute(response, "system-state",
                               IPP_TAG_ENUM)) != NULL)
  {
    printf("system-state=%s\n",
           ippEnumString("system-state", ippGetInteger(attr, 0)));
  }

  else
  {
    puts("system-state=unknown");
  }

  if ((attr = ippFindAttribute(response, "system-state-message",
                               IPP_TAG_TEXT)) != NULL)
  {
    printf("system-state-message=\"%s\"\n",
           ippGetString(attr, 0, NULL));
  }

  if ((attr = ippFindAttribute(response, "system-state-reasons",
                               IPP_TAG_KEYWORD)) != NULL)
  {
    int i, count = ippGetCount(attr);

    puts("system-state-reasons=");
    for (i = 0; i < count; i++)
      printf("    %s\n", ippGetString(attr, i, NULL));
  }
}

static int check_if_cups_request_error()
{

  if (cupsLastError() >= IPP_STATUS_ERROR_BAD_REQUEST)
  {
    /* request failed */
    printf("Request failed: %s\n", cupsLastErrorString());
    return 1;
  }

  else
  {
    printf("Request succeeded!\n");
    return 0;
  }
}

static int setup_and_send_request(const char *name,
                                  const char *type,
                                  const char *domain,
                                  const char *host_name,
                                  uint16_t port)
{
  char uri[1024];
  httpAssembleURI(HTTP_URI_CODING_ALL, uri, sizeof(uri), "ipp", NULL,
                  host_name, port, "/ipp/system");

  
  // char portString[10];
  // sprintf(portString, "%d", port);

  /* need to figure out why (or if) addrlist is needed. */
  // http_addrlist_t *addrlist = httpAddrGetList(host_name, AF_UNSPEC, portString);
  http_t *http = httpConnect2(host_name, port, NULL, AF_UNSPEC, HTTP_ENCRYPTION_ALWAYS, 1, 0, NULL);

  printf("uri: %s\n", uri);

  ipp_t *request = ippNewRequest(IPP_OP_GET_PRINTERS);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_URI, "system-uri", NULL, uri);
  ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_NAME, "requesting-user-name", NULL, cupsUser());
  // ippAddString(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD, "requested-attributes", NULL, "printer-state");
  // ippAddStrings(request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD, "requested-attributes", (int)(sizeof(sysattrs) / sizeof(sysattrs[0])), NULL, sysattrs);

  ipp_t *response = cupsDoRequest(http, request, "/ipp/system");

  if (check_if_cups_request_error())
  {
    return 1;
  }

  // if_get_printer_attributes(response);
  if_get_system_attributes(response);
  if_get_printers(response);

  return 0;
}

int dnssd(const char *service_type);

void resolve_callback(
    AvahiServiceResolver *r,
    AVAHI_GCC_UNUSED AvahiIfIndex interface,
    AVAHI_GCC_UNUSED AvahiProtocol protocol,
    AvahiResolverEvent event,
    const char *name,
    const char *type,
    const char *domain,
    const char *host_name,
    const AvahiAddress *address,
    uint16_t port,
    AvahiStringList *txt,
    AvahiLookupResultFlags flags,
    AVAHI_GCC_UNUSED void *userdata)
{
  assert(r);
  /* Called whenever a service has been resolved successfully or timed out */
  switch (event)
  {
  case AVAHI_RESOLVER_FAILURE:
    fprintf(stderr, "(Resolver) Failed to resolve service '%s' of type '%s' in domain '%s': %s\n", name, type, domain, avahi_strerror(avahi_client_errno(avahi_service_resolver_get_client(r))));
    break;
  case AVAHI_RESOLVER_FOUND:
  {
    char a[AVAHI_ADDRESS_STR_MAX], *t;
    fprintf(stderr, "Service '%s' of type '%s' in domain '%s':\n", name, type, domain);
    avahi_address_snprint(a, sizeof(a), address);
    t = avahi_string_list_to_string(txt);
    fprintf(stderr,
            "\t%s:%u (%s)\n"
            "\tTXT=%s\n"
            "\tcookie is %u\n"
            "\tis_local: %i\n"
            "\tour_own: %i\n"
            "\twide_area: %i\n"
            "\tmulticast: %i\n"
            "\tcached: %i\n",
            host_name, port, a,
            t,
            avahi_string_list_get_service_cookie(txt),
            !!(flags & AVAHI_LOOKUP_RESULT_LOCAL),
            !!(flags & AVAHI_LOOKUP_RESULT_OUR_OWN),
            !!(flags & AVAHI_LOOKUP_RESULT_WIDE_AREA),
            !!(flags & AVAHI_LOOKUP_RESULT_MULTICAST),
            !!(flags & AVAHI_LOOKUP_RESULT_CACHED));
    avahi_free(t);
  }
  }
  avahi_service_resolver_free(r);

  int status = setup_and_send_request(name, type, domain, host_name, port);
}

int main()
{

  // cups_dests_discovery();
  // dnssd("_http._tcp");
  dnssd("_ipps-system._tcp");
  return 0;
}
