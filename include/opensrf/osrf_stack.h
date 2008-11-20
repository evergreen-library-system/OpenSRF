#ifndef OSRF_STACK_H
#define OSRF_STACK_H

#include <opensrf/transport_client.h>
#include <opensrf/osrf_message.h>
#include <opensrf/osrf_app_session.h>

osrfAppSession*  osrf_stack_transport_handler( transport_message* msg,
		const char* my_service );

#endif
