#include <opensrf/transport_client.h>
#include <opensrf/osrfConfig.h>

transport_client* client_init(const char* domain, 
    int port, const char* username, const char* password) {

    if (domain == NULL || username == NULL || password == NULL) {
        return NULL;
    }

    osrfLogInfo(OSRF_LOG_MARK, 
        "client_init domain=%s port=%d username=%s", domain, port, username);

	transport_client* client = safe_malloc(sizeof(transport_client));
    client->primary_domain = strdup(domain);
    client->connections = osrfNewHash();

    // These 2 only get values if this client works for a service.
	client->service = NULL;
	client->service_address = NULL;
	client->router_address = NULL;

    // Name of the router which runs on our primary domain.
    char* router_name = osrfConfigGetValue(NULL, "/router_name");
    if (router_name == NULL) {
        client->router_name = strdup("router");
    } else {
        client->router_name = router_name;
    }

    client->username = username ? strdup(username) : NULL;
    client->password = password ? strdup(password) : NULL;

    client->port = port;
    client->primary_connection = NULL;

	client->error = 0;

	return client;
}

static transport_con* client_connect_common(
    transport_client* client, const char* domain) {

    osrfLogInfo(OSRF_LOG_MARK, "Connecting to domain: %s", domain);

    transport_con* con = transport_con_new(domain);

    osrfHashSet(client->connections, (void*) con, (char*) domain);

    return con;
}


static transport_con* get_transport_con(transport_client* client, const char* domain) {
    transport_con* con = (transport_con*) osrfHashGet(client->connections, (char*) domain);

    if (con != NULL) { return con; }

    // If we don't have the a connection for the requested domain,
    // it means we're setting up a connection to a remote domain.

    con = client_connect_common(client, domain);

    transport_con_set_address(con, client->username);

    // Connections to remote domains assume the same connection
    // attributes apply, minus the domain.
    transport_con_connect(con, client->port, client->username, client->password);

    return con;
}

// Connect as a service.
int client_connect_as_service(transport_client* client, const char* service) {
    growing_buffer* buf = buffer_init(32);

    buffer_fadd(
        buf, 
        "opensrf:service:%s:%s:%s", 
        client->username,
        client->primary_domain, 
        service
    );

    client->service_address = buffer_release(buf);
    client->service = strdup(service);

    transport_con* con = client_connect_common(client, client->primary_domain);

    transport_con_set_address(con, client->username);

    client->primary_connection = con;

    return transport_con_connect(
        con, client->port, client->username, client->password);
}

int client_connect_as_router(transport_client* client, const char* domain) {
    growing_buffer* buf = buffer_init(32);

    // NOTE domain here is the same as client->primary_domain.
    buffer_fadd(buf, "opensrf:router:%s:%s", client->username, domain);

    client->router_address = buffer_release(buf);

    transport_con* con = client_connect_common(client, client->primary_domain);

    // Routers get their own unique client address in addition to the
    // well-known router address.
    transport_con_set_address(con, client->username);

    client->primary_connection = con;

    return transport_con_connect(
        con, client->port, client->username, client->password);
}

// Connect as a client running on behalf of a service, i.e. not a 
// standalone client.
int client_connect_for_service(transport_client* client, const char* service) {
    client->service = strdup(service);

    transport_con* con = client_connect_common(client, client->primary_domain);

    // Append the service name to the bus address
    transport_con_set_address(con, client->username);

    client->primary_connection = con;

    return transport_con_connect(
        con, client->port, client->username, client->password);
}


int client_connect(transport_client* client) {
    if (client == NULL) { return 0; }

    transport_con* con = client_connect_common(client, client->primary_domain);

    transport_con_set_address(con, client->username);

    client->primary_connection = con;

    return transport_con_connect(
        con, client->port, client->username, client->password);
}

// Disconnect all connections and remove them from the connections hash.
int client_disconnect(transport_client* client) {
    if (client == NULL) { return 0; }

    osrfLogDebug(OSRF_LOG_MARK, "Disconnecting all transport connections");

    osrfHashIterator* iter = osrfNewHashIterator(client->connections);

    transport_con* con;

    while( (con = (transport_con*) osrfHashIteratorNext(iter)) ) {
        osrfLogInternal(OSRF_LOG_MARK, "Disconnecting from domain: %s", con->domain);
        transport_con_disconnect(con);
        transport_con_free(con);
    }

    osrfHashIteratorFree(iter);
    osrfHashFree(client->connections);

    client->connections = osrfNewHash();

    return 1;
}

int client_connected( const transport_client* client ) {
	return (client != NULL && client->primary_connection != NULL);
}

bus_address* parse_bus_address(const char* address) {
    if (address == NULL) { return NULL; }

    char* addr_copy = strdup(address);
    strtok(addr_copy, ":"); // prefix ; opensrf:

    char* purpose = strtok(NULL, ":");
    char* username = strtok(NULL, ":");
    char* domain = strtok(NULL, ":");
    char* remainder = strtok(NULL, ":");

    if (!purpose || !domain || !username) {
        osrfLogError(OSRF_LOG_MARK, "Invalid bus address: %s", address);
        free(addr_copy);
        return NULL;
    }

    bus_address* addr = safe_malloc(sizeof(bus_address));
    addr->purpose = strdup(purpose);
    addr->domain = strdup(domain);
    addr->username = strdup(username);

    if (remainder) {
        addr->remainder = strdup(remainder);
    }

    free(addr_copy);
    return addr;
}

// TODO migrate use of this function to parse_bus_address().
char* get_domain_from_address(const char* address) {
    bus_address* addr = parse_bus_address(address);
    if (!addr) { return NULL; }

    char* domain = strdup(addr->domain);
    bus_address_free(addr);
    return domain;
}

int client_send_message(transport_client* client, transport_message* msg) {
    return client_send_message_to(client, msg, msg->recipient) ;
}

int client_send_message_to(
    transport_client* client, 
    transport_message* msg, 
    const char* recipient
) {
	if (client == NULL || client->error) { return -1; }

    // By default, all messages come from our primary address.
    if (msg->sender) { free(msg->sender); }
    msg->sender = strdup(client->primary_connection->address);

    const char* receiver = recipient == NULL ? msg->recipient : recipient;

    char* domain = NULL;
    if (strstr(receiver, "opensrf:client") || strstr(receiver, "opensrf:router")) {
        // We may be talking to a worker that runs on a remote domain.
        // Find or create a connection to the domain.

        domain = get_domain_from_address(receiver);

        if (!domain) { return -1; }
    }

    int stat = client_send_message_to_from_at(client, msg, receiver, msg->sender, domain);

    free(domain);

    return stat;
}

int client_send_message_to_from_at(
    transport_client* client, 
    transport_message* msg, 
    const char* recipient,
    const char* sender,
    const char* domain
) {
    transport_con* con = domain == NULL ? 
        client->primary_connection : get_transport_con(client, domain);

    if (!con) {
        osrfLogError(
            OSRF_LOG_MARK, "Error creating connection for domain: %s", domain);

        return -1;
    }

    message_prepare_json(msg);

    osrfLogInternal(OSRF_LOG_MARK, 
        "client_send_message() to=%s from=%s at=%s %s", 
        recipient, sender, domain, msg->msg_json
    );

    return transport_con_send(con, msg->msg_json, recipient);
}

transport_message* client_recv_stream(transport_client* client, int timeout, const char* stream) {
    if (client == NULL) { return NULL; }

    transport_con_msg* con_msg = 
        transport_con_recv(client->primary_connection, timeout, stream);

    if (con_msg == NULL) { return NULL; } // Receive timed out.

	transport_message* msg = new_message_from_json(con_msg->msg_json);

    transport_con_msg_free(con_msg);

    if (msg) { // Can be NULL if the con_msg->msg_json is invalid.
        osrfLogInternal(OSRF_LOG_MARK, 
            "client_recv() read response for thread %s", msg->thread);
    }

	return msg;
}

transport_message* client_recv(transport_client* client, int timeout) {
    if (client == NULL || client->primary_connection == NULL) { return NULL; }

    return client_recv_stream(client, timeout, client->primary_connection->address);
}

transport_message* client_recv_for_service(transport_client* client, int timeout) {
    if (client == NULL) { return NULL; }

    osrfLogInternal(OSRF_LOG_MARK, "TCLIENT Receiving for service %s", client->service);

    return client_recv_stream(client, timeout, client->service_address);
}

transport_message* client_recv_for_router(transport_client* client, int timeout) {
    if (client == NULL || client->router_address == NULL) { return NULL; }

    osrfLogInternal(OSRF_LOG_MARK, "TCLIENT Receiving for router at %s", client->router_address);

    return client_recv_stream(client, timeout, client->router_address);
}

/**
	@brief Free a transport_client, along with all resources it owns.
	@param client Pointer to the transport_client to be freed.
	@return 1 if successful, or 0 if not.  The only error condition is if @a client is NULL.
*/
int client_free( transport_client* client ) {
	if (client == NULL) { return 0; }
	return client_discard( client );
}

/**
	@brief Free a transport_client's resources, but without disconnecting.
	@param client Pointer to the transport_client to be freed.
	@return 1 if successful, or 0 if not.  The only error condition is if @a client is NULL.

	A child process may call this in order to free the resources associated with the parent's
	transport_client, but without disconnecting from Jabber, since disconnecting would
	disconnect the parent as well.
 */
int client_discard( transport_client* client ) {
	if (client == NULL) { return 0; }

    osrfLogInternal(OSRF_LOG_MARK, 
        "Discarding client on domain %s", client->primary_domain);

	if (client->primary_domain) { free(client->primary_domain); }
	if (client->service) { free(client->service); }
	if (client->service_address) { free(client->service_address); }
	if (client->router_address) { free(client->router_address); }
    if (client->username) { free(client->username); }
    if (client->password) { free(client->password); }
    if (client->router_name) { free(client->router_name); }

    // Clean up our connections.
    // We do not disconnect here since they caller may or may
    // not want the socket closed.
    // If disconnect() was just called, the connections hash
    // will be empty.
    osrfHashIterator* iter = osrfNewHashIterator(client->connections);

    transport_con* con;

    while( (con = (transport_con*) osrfHashIteratorNext(iter)) ) {
        osrfLogInternal(OSRF_LOG_MARK, 
            "client_discard() freeing connection for %s", con->domain);
        transport_con_free(con);
    }

    osrfHashIteratorFree(iter);
    osrfHashFree(client->connections);

	free(client);

	return 1;
}


void bus_address_free(bus_address* addr) {
    if (!addr) { return; }
    if (addr->purpose) { free(addr->purpose); }
    if (addr->domain) { free(addr->domain); }
    if (addr->username) { free(addr->username); }
    if (addr->remainder) { free(addr->remainder); }
    free(addr);
}
