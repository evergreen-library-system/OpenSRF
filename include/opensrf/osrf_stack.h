#include <opensrf/transport_client.h>
#include <opensrf/osrf_message.h>
#include <opensrf/osrf_app_session.h>

#ifndef OSRF_STACK_H
#define OSRF_STACK_H

/* the max number of oilsMessage blobs present in any one root packet */
#define OSRF_MAX_MSGS_PER_PACKET 256
// -----------------------------------------------------------------------------

int osrf_stack_process( transport_client* client, int timeout, int* msg_received );
osrfAppSession*  osrf_stack_transport_handler( transport_message* msg, char* my_service );
int osrf_stack_message_handler( osrf_app_session* session, osrf_message* msg );
int osrf_stack_application_handler( osrf_app_session* session, osrf_message* msg );



#endif