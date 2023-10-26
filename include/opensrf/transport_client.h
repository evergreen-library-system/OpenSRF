#ifndef TRANSPORT_CLIENT_H
#define TRANSPORT_CLIENT_H

/**
	@file transport_client.h
	@brief Header for implementation of transport_client.

	The transport_client routines provide an interface for sending and receiving Jabber
	messages, one at a time.
*/

#include <time.h>
#include <opensrf/transport_session.h>
#include <opensrf/transport_connection.h>
#include <opensrf/utils.h>
#include <opensrf/log.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bus_address_struct {
    char* purpose;
    char* domain;
    char* username;
    char* remainder;
} bus_address;

bus_address* parse_bus_address(const char* address);
void bus_address_free(bus_address*);

struct message_list_struct;

/**
	@brief A collection of members used for keeping track of transport_messages.

	Among other things, this struct includes a queue of incoming messages, and
	a Jabber ID for outgoing messages.
*/
struct transport_client_struct {
    char* primary_domain;
    char* service; // NULL if this is a standalone client.
    char* service_address; // NULL if this is not a service
    char* router_address; // NULL if this is not a router
    char* router_name; // name of the router running on our primary domain.
    osrfHash* connections;

    int port;
    char* username;
    char* password;
    transport_con* primary_connection;

	int error;                       /**< Boolean: true if an error has occurred */
};
typedef struct transport_client_struct transport_client;

transport_client* client_init(const char* server, 
    int port, const char* username, const char* password);

/// Top-level service connection
int client_connect_as_service(transport_client* client, const char* service);
int client_connect_as_router(transport_client* client, const char* name);
/// Client connecting on behalf of a service
int client_connect_for_service(transport_client* client, const char* service);
int client_connect(transport_client* client); 

int client_disconnect( transport_client* client );

int client_free( transport_client* client );

int client_discard( transport_client* client );

int client_send_message( transport_client* client, transport_message* msg );
int client_send_message_to(transport_client* client, 
    transport_message* msg, const char* recipient);


char* get_domain_from_address(const char* address);

int client_send_message_to_from_at(
    transport_client* client, 
    transport_message* msg, 
    const char* recipient, 
    const char* sender,
    const char* domain
);

int client_connected( const transport_client* client );

transport_message* client_recv_stream(transport_client* client, int timeout, const char* stream);
transport_message* client_recv(transport_client* client, int timeout);
transport_message* client_recv_for_service(transport_client* client, int timeout);
transport_message* client_recv_for_router(transport_client* client, int timeout);

int client_sock_fd( transport_client* client );

#ifdef __cplusplus
}
#endif

#endif
