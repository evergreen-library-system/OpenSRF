#include <sys/select.h>
#include <signal.h>
#include "osrf_router.h"
#include "opensrf/utils.h"
#include "opensrf/log.h"
#include "opensrf/osrf_list.h"
#include "opensrf/string_array.h"
#include "opensrf/osrf_hash.h"
#include "opensrf/transport_client.h"
#include "opensrf/transport_message.h"
#include "opensrf/osrf_message.h"

#define POLL_TIMEOUT 5
#define MAX_MSGS_PER_PACKET 256

// Layout:
// Router => ServiceEntry's => ServiceInstance's
//
// Every Router has one primary domain.  Remote domains are added as
// service clients on remote domains register their services with us.

// A specific OpenSRF Listener instance
// Uses the bus/redis address as its unique identifier.
typedef struct ServiceInstanceStruct {
    char* address;
    // Each ServiceInstance runs on a single domain, defined by
    // its bus address.
    char* domain;
} ServiceInstance;

// Models a single OpenSRF service (e.g. opensrf.settings), which will 
// have one or more ServiceInstance's
typedef struct ServiceEntryStruct {
    char* service;
    osrfList* instances;
} ServiceEntry;

// Main Router
// Has one primary domain and zero or more remote domains.
struct RouterStruct {
    char* domain;
    char* username;
    char* password;
    int port;
    transport_client* client;
    osrfStringArray* trusted_clients;
    osrfStringArray* trusted_servers;
    osrfList* services;
    volatile sig_atomic_t stop;
};

void osrfRouterStop(Router* router) {
    if (router) router->stop = 1;
}

// Create a new Router and setup the primary domain connection.
Router* osrfNewRouter(
    const char* domain, 
    const char* username,
    const char* password, 
    int port,
    osrfStringArray* trusted_clients, 
    osrfStringArray* trusted_servers 
) {

    Router* router     = safe_malloc(sizeof(Router));
    router->domain     = strdup(domain);
    router->username   = strdup(username);
    router->password   = strdup(password);
    router->port       = port;
    router->stop       = 0;

    router->trusted_clients = trusted_clients;
    router->trusted_servers = trusted_servers;

    router->client = client_init(domain, port, username, password);
    router->services = osrfNewList();
    
    osrfLogInfo(OSRF_LOG_MARK, "Starting router for domain %s", domain);

    return router;
}

static void osrfFreeServiceInstance(ServiceInstance* si) {
    if (!si) { return; }
    free(si->address);
    free(si->domain);
    free(si);
}

static void osrfFreeServiceEntry(ServiceEntry* se) {
    if (!se) { return; }

    free(se->service);

    osrfListIterator* iter = osrfNewListIterator(se->instances);

    void* instance;
    while ((instance = osrfListIteratorNext(iter))) {
        osrfFreeServiceInstance((ServiceInstance*) instance);
    }

    osrfListIteratorFree(iter);
    osrfListFree(se->instances);

    free(se);
}

void osrfRouterFree(Router* router) {
    if (!router) { return; }

    client_disconnect(router->client);
    client_free(router->client); 

    free(router->password);
    free(router->domain);

    osrfStringArrayFree(router->trusted_clients);
    osrfStringArrayFree(router->trusted_servers);

    osrfListIterator* iter = osrfNewListIterator(router->services);

    void* service;
    while ((service = osrfListIteratorNext(iter))) {
        osrfFreeServiceEntry((ServiceEntry*) service);
    }

    osrfListIteratorFree(iter);
    osrfListFree(router->services);

    free(router);
}

// Connect to the message bus on our primary domain.
int osrfRouterConnect(Router* router) {
    osrfLogDebug(OSRF_LOG_MARK, 
        "Router connecting to bus for domain %s", router->domain);
    return client_connect_as_router(router->client, router->domain) == 0 ? -1 : 0;
}

// Find a service entry by service name linked registered at the
// provided router domain.
static ServiceEntry* osrfGetServiceEntry(Router* router, const char* service) {
    osrfListIterator* iter = osrfNewListIterator(router->services);
    void* vse;
    ServiceEntry* entry = NULL;

    while ((vse = osrfListIteratorNext(iter))) {
        ServiceEntry* se = (ServiceEntry*) vse;
        if (strcmp(se->service, service) == 0) {
            entry = se;
            break;
        }
    }

    osrfListIteratorFree(iter);
    return entry;
}

static char* osrfHandleRouterApiRequest(Router* router, osrfMessage* msg) {
    if (!msg->method_name) { return NULL; }

    osrfLogDebug(OSRF_LOG_MARK, "Received router API request %s", msg->method_name);

    if (strcmp(msg->method_name, "opensrf.router.info.class.list") == 0) {
        
        jsonObject* list = jsonNewObjectType(JSON_ARRAY);
        osrfListIterator* iter = osrfNewListIterator(router->services);
        void* vse;
        while ((vse = osrfListIteratorNext(iter))) {
            ServiceEntry* se = (ServiceEntry*) vse;
            jsonObjectPush(list, jsonNewObject(se->service));
        }

        osrfListIteratorFree(iter);
        char* value = jsonObjectToJSON(list);
        jsonObjectFree(list);

        return value;
    }

    return NULL;
}

// Some Router requests are packaged as method calls.  Handle those here.
static void osrfProcessRouterApiRequest(Router* router, transport_message* tmsg) {
    osrfMessage* arr[MAX_MSGS_PER_PACKET];

    /* Convert the message body into one or more osrfMessages */
    int num_msgs = osrf_message_deserialize(tmsg->body, arr, MAX_MSGS_PER_PACKET);

    int i;
    for (i = 0; i < num_msgs; i++) {
        osrfMessage* request_msg = arr[i];
        char* response_value = osrfHandleRouterApiRequest(router, request_msg);

        if (response_value) {
            // If the router API call produced a value, send it as a reply.

            osrfMessage* response_msg = 
                osrf_message_init(RESULT, request_msg->thread_trace, request_msg->protocol);

            osrf_message_set_result_content(response_msg, response_value);

            char* response_json = osrf_message_serialize(response_msg);

            transport_message* response_tmsg = message_init(
                response_json, "", tmsg->thread, tmsg->sender, NULL);

            client_send_message(router->client, response_tmsg);

            free(response_value);
            free(response_json);
            osrfMessageFree(response_msg);
            message_free(response_tmsg);
        }

        // Now send the 'request complete' message
        osrfMessage* status_msg = osrf_message_init(STATUS, request_msg->thread_trace, 1);
        osrf_message_set_status_info(status_msg, 
            "osrfConnectStatus", "Request Complete", OSRF_STATUS_COMPLETE);

        char* status_data = osrf_message_serialize(status_msg);

        transport_message* status_tmsg = message_init(
            status_data, "", tmsg->thread, tmsg->sender, NULL);

        client_send_message(router->client, status_tmsg);

        free(status_data);
        message_free(status_tmsg);
        osrfMessageFree(status_msg);
        osrfMessageFree(request_msg);
    }
}

// Route an OpenSRF API call to a service listener.  More specifically,
// it delivers the API call message to first domain found which hosts
// the specified service, starting with the pimary domain.
static void osrfRouteApiCall(
    Router* router, 
    transport_message* tmsg, 
    const char* service
) {

    char* domain = get_domain_from_address(tmsg->sender);
    if (domain == NULL) {
        osrfLogError(OSRF_LOG_MARK, "Invalid sender address: %s", tmsg->sender);
        return;
    }

    bool allowed = osrfStringArrayContains(router->trusted_clients, domain);
    free(domain);
    
    // Verify the sending domain is allowed to route calls through us.
    if (!allowed) {
        osrfLogError(OSRF_LOG_MARK, 
            "Sending domain is not allowed to route: %s", tmsg->sender);
        return;
    }

    if (strcmp(service, "router") == 0) {
        return osrfProcessRouterApiRequest(router, tmsg);
    }

    ServiceEntry* se = osrfGetServiceEntry(router, service);
    if (se == NULL || osrfListGetCount(se->instances) == 0) {

        osrfLogError(OSRF_LOG_MARK, 
            "Service %s is not registered on this router domain=%s", 
            service, router->domain);

        osrfMessage* status_msg = osrf_message_init(STATUS, 1, 1);
        osrf_message_set_status_info(status_msg, 
            "osrfServiceException", "Service Not Found", OSRF_STATUS_NOTFOUND);

        char* status_data = osrf_message_serialize(status_msg);

        transport_message* status_tmsg = message_init(
            status_data, "", tmsg->thread, tmsg->sender, NULL);

        client_send_message(router->client, status_tmsg);

        free(status_data);
        message_free(status_tmsg);
        osrfMessageFree(status_msg);

        return;
    }

    ServiceInstance* si = (ServiceInstance*) osrfListGetIndex(se->instances, 0);
    
    if (si == NULL) {
        // Confirmed se->instances contains at least one instance above.
        // However, just to be safe...
        return;
    }

    int stat = client_send_message_to_from_at(
        router->client, 
        tmsg, 
        tmsg->recipient,    // opensrf:service:<servicename>
        tmsg->sender,       // sending client address
        si->domain          // domain of our ServiceInstance
    );

    if (stat != 0) {
        osrfLogError(OSRF_LOG_MARK, 
            "Error relaying message to domain %s", router->domain);
    }
}

// Get the ServiceInstance linked to the provided service entry with
// the provided sender address.
static ServiceInstance* osrfGetServiceInstance(ServiceEntry* se, const char* sender) {
    osrfListIterator* iter = osrfNewListIterator(se->instances);
    void* vsi;
    ServiceInstance* instance = NULL;

    while ((vsi = osrfListIteratorNext(iter))) {
        ServiceInstance* si = (ServiceInstance*) vsi;
        if (strcmp(si->address, sender) == 0) {
            instance = si;
            break;
        }
    }

    osrfListIteratorFree(iter);
    return instance;
}

static void osrfRemoveServiceInstance(Router* router, const char* service, const char* sender) {
    osrfLogDebug(OSRF_LOG_MARK, 
        "Trying to remove service instance service=%s router-domain=%s instance=%s", 
        service, router->domain, sender
    );

    ServiceEntry* se = osrfGetServiceEntry(router, service);
    if (se == NULL) { return; }

    // XXX osrfListRemove() does not shift list entries to fill the gaps.
    // Instead, if leaves NULL entries.  Create a new list with the 
    // data we want instead of using osrfListRemove();
    osrfList* new_instances = osrfNewList(osrfListGetCount(se->instances) - 1);

    ServiceInstance* si;
    osrfListIterator* iter = osrfNewListIterator(se->instances);

    while ((si = (ServiceInstance*) osrfListIteratorNext(iter))) {
        if (strcmp(si->address, sender) == 0) {
            osrfLogDebug(OSRF_LOG_MARK,
                "Found service instance to remove service=%s router-domain=%s instance=%s",
                service, router->domain, sender
            );

            osrfFreeServiceInstance(si);
        } else {
            osrfListPush(new_instances, si);
        }
    }

    // Replace our instances array with our new array.
    osrfListIteratorFree(iter);
    osrfListFree(se->instances);
    se->instances = new_instances;

    if (osrfListGetCount(se->instances) > 0) {
        return;
    }

    osrfLogDebug(OSRF_LOG_MARK,
        "Removing service entry after removing final instance service=%s router-domain=%s",
        service, router->domain
    );

    osrfList* new_entries = osrfNewListSize(osrfListGetCount(router->services) - 1);
    iter = osrfNewListIterator(router->services);

    while ((se = (ServiceEntry*) osrfListIteratorNext(iter))) {
        if (strcmp(se->service, service) == 0) {
            osrfLogDebug(OSRF_LOG_MARK,
                "Found service entry to remove service=%s router-domain=%s",
                service, router->domain
            );
            osrfFreeServiceEntry(se);
        } else {
            osrfListPush(new_entries, se);
        }
    }

    osrfListIteratorFree(iter);
    osrfListFree(router->services);
    router->services = new_entries;
}

// Register an OpenSRF service instance at the provided domain.
//
// This will add the required objects as needed: ServiceEntry => ServiceInstance.
static void osrfHandleRegister(
    Router* router, 
    const char* service,
    const char* sender
) {

    char* domain = get_domain_from_address(sender);
    if (domain == NULL) {
        osrfLogError(OSRF_LOG_MARK, "Invalid sender address: %s", sender);
        return;
    }

    bool allowed = osrfStringArrayContains(router->trusted_servers, domain);

    if (!allowed) {
        free(domain);
        osrfLogError(OSRF_LOG_MARK, 
            "Sending domain is not allowed on this router: %s", sender);
        return;
    }

    ServiceEntry* se = osrfGetServiceEntry(router, service);

    if (se == NULL) {
        osrfLogDebug(OSRF_LOG_MARK, 
            "Adding service entry for service=%s", service);

        se = safe_malloc(sizeof(ServiceEntry));
        se->instances = osrfNewList();
        se->service = strdup(service);

        // Add this new entry to the router domain entry list
        osrfListPush(router->services, se);

        osrfLogDebug(OSRF_LOG_MARK,
            "Router domain %s now has %d service entries",
            router->domain, osrfListGetCount(router->services)
        );
    }

    ServiceInstance* si = osrfGetServiceInstance(se, sender);

    if (si == NULL) {

        osrfLogDebug(OSRF_LOG_MARK, 
            "Adding service instance for service=%s addr=%s", 
            service, sender
        );

        si = safe_malloc(sizeof(ServiceInstance));
        si->address = strdup(sender);
        si->domain = get_domain_from_address(sender);

        // Add this new instance to the entry instances
        osrfListPush(se->instances, si);

        osrfLogDebug(OSRF_LOG_MARK,
            "Service entry %s for domain %s now has %d instances", 
            service, router->domain, osrfListGetCount(se->instances)
        );
    }
}

static void osrfHandleUnregister(
    Router* router, 
    const char* service,
    const char* sender
) {
    osrfRemoveServiceInstance(router, service, sender);

    osrfLogDebug(OSRF_LOG_MARK,
        "Router domain %s now has %d service entries",
        router->domain, osrfListGetCount(router->services)
    );
}

static void osrfHandleRouterCommand(Router* router, transport_message* tmsg) {
    const char* router_command = tmsg->router_command;
    const char* router_class = tmsg->router_class;
    const char* sender = tmsg->sender;

    osrfLogDebug(OSRF_LOG_MARK, 
        "Handling router command %s class=%s sender=%s",
        router_command, router_class, sender
    );

    if (router_command == NULL || router_class == NULL) {
        osrfLogError(OSRF_LOG_MARK, "Message missing router_command/router_class");
        return;
    }

    if (strcmp(router_command, "register") == 0) {
        osrfHandleRegister(router, router_class, sender);
    } else if (strcmp(router_command, "unregister") == 0) {
        osrfHandleUnregister(router, router_class, sender);
    }
}

static void osrfRouteMessage(Router* router, transport_message* tmsg) {

    if (!tmsg->recipient || !tmsg->sender) { 
        osrfLogError(OSRF_LOG_MARK, "Transport message has no sender/recipient");
        return;
    }

    char* recipient = strdup(tmsg->recipient);
    strtok(recipient, ":"); // opensrf:service:<service>
    const char* purpose = strtok(NULL, ":");
    const char* service = strtok(NULL, ":");

    osrfLogDebug(OSRF_LOG_MARK, 
        "Routing message from %s to %s, service=%s", 
        tmsg->sender, tmsg->recipient, service
    );

    if (purpose == NULL) {
        osrfLogError(OSRF_LOG_MARK, "Invalid recipient address: %s", recipient);
    } else {
        if (strcmp(purpose, "service") == 0) {
            osrfRouteApiCall(router, tmsg, service);
        } else if (strcmp(purpose, "router") == 0) {
            osrfHandleRouterCommand(router, tmsg);
        } else {
            osrfLogError(OSRF_LOG_MARK, "Unexpected recipient: %s", recipient);
        }
    }

    free(recipient);
}

void osrfRouterRun(Router* router) {
    transport_message* tmsg = NULL;

    while (!router->stop) {

        osrfLogDebug(OSRF_LOG_MARK, "Waiting for messages at %s", router->domain);

        tmsg = client_recv_for_router(router->client, POLL_TIMEOUT);

        if (!tmsg) { continue; } // Timed out or interrupted

        osrfLogInternal(OSRF_LOG_MARK, "Router received message from: %s", tmsg->sender);

        osrfRouteMessage(router, tmsg);

        message_free(tmsg);
    }
}

