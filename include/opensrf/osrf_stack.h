#ifndef OSRF_STACK_H
#define OSRF_STACK_H

/**
	@file osrf_stack.c
	@brief Routines to receive and process input osrfMessages.
*/

#include <opensrf/transport_client.h>
#include <opensrf/osrf_message.h>
#include <opensrf/osrf_app_session.h>

#ifdef __cplusplus
extern "C" {
#endif

struct osrf_app_session_struct;

struct osrf_app_session_struct*  osrf_stack_transport_handler( transport_message* msg,
		const char* my_service );

int osrf_stack_process( transport_client* client, int timeout, int* msg_received );

#ifdef __cplusplus
}
#endif

#endif
