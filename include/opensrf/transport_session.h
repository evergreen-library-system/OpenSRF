#ifndef TRANSPORT_SESSION_H
#define TRANSPORT_SESSION_H

/**
	@file transport_session.h
	@brief Header for routines to manage a connection to a Jabber server.

	Manages the Jabber session.  Reads data from a socket and pushes it into a SAX
	parser as it arrives.  When key Jabber document elements are met, logic ensues.
*/

#include <opensrf/transport_message.h>

#include <opensrf/utils.h>
#include <opensrf/log.h>
#include <opensrf/socket_bundle.h>

#include "sha.h"

#include <string.h>
#include <libxml/globals.h>
#include <libxml/xmlerror.h>
#include <libxml/parser.h>
#include <libxml/parserInternals.h> /* only for xmlNewInputFromFile() */
#include <libxml/tree.h>
#include <libxml/debugXML.h>
#include <libxml/xmlmemory.h>

#ifdef __cplusplus
extern "C" {
#endif

enum TRANSPORT_AUTH_TYPE { AUTH_PLAIN, AUTH_DIGEST };

struct jabber_state_machine_struct;
typedef struct jabber_state_machine_struct jabber_machine;

// ---------------------------------------------------------------------------------
// Transport session.  This maintains all the various parts of a session
// ---------------------------------------------------------------------------------
struct transport_session_struct {

	/* our socket connection */
	socket_manager* sock_mgr;

	/* our Jabber state machine */
	jabber_machine* state_machine;
	/* our SAX push parser context */
	xmlParserCtxtPtr parser_ctxt;

	/* our text buffers for holding text data */
	growing_buffer* body_buffer;
	growing_buffer* subject_buffer;
	growing_buffer* thread_buffer;
	growing_buffer* from_buffer;
	growing_buffer* recipient_buffer;
	growing_buffer* status_buffer;
	growing_buffer* message_error_type;
	growing_buffer* session_id;
	int message_error_code;

	/* for OILS extenstions */
	growing_buffer* router_to_buffer;
	growing_buffer* router_from_buffer;
	growing_buffer* router_class_buffer;
	growing_buffer* router_command_buffer;
	growing_buffer* osrf_xid_buffer;
	int router_broadcast;

	/* this can be anything.  It will show up in the
		callbacks for your convenience. Otherwise, it's
		left untouched.  */
	void* user_data;

	char* server;
	char* unix_path;
	int	port;
	int sock_id;

	int component; /* true if we're a component */

	/* the Jabber message callback */
	void (*message_callback) ( void* user_data, transport_message* msg );
	//void (iq_callback) ( void* user_data, transport_iq_message* iq );
};
typedef struct transport_session_struct transport_session;

transport_session* init_transport( const char* server, int port,
	const char* unix_path, void* user_data, int component );

int session_wait( transport_session* session, int timeout );

// ---------------------------------------------------------------------------------
// Sends the given Jabber message
// ---------------------------------------------------------------------------------
int session_send_msg( transport_session* session, transport_message* msg );

int session_connected( transport_session* session );

int session_free( transport_session* session );

int session_connect( transport_session* session,
		const char* username, const char* password,
		const char* resource, int connect_timeout,
		enum TRANSPORT_AUTH_TYPE auth_type );

int session_disconnect( transport_session* session );

#ifdef __cplusplus
}
#endif

#endif
