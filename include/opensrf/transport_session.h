#ifndef TRANSPORT_SESSION_H
#define TRANSPORT_SESSION_H

/**
	@file transport_session.h
	@brief Header for routines to manage a connection to a Jabber server.

	Manages a Jabber session.  Reads messages from a socket and pushes them into a SAX parser
	as they arrive, responding to various Jabber document elements as they appear.  When it
	sees the end of a complete message, it sends a representation of that message to the
	calling code via a callback function.
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

/** Note whether the login information should be sent as plaintext or as a hash digest. */
enum TRANSPORT_AUTH_TYPE { AUTH_PLAIN, AUTH_DIGEST };


// ---------------------------------------------------------------------------------
// Jabber state machine.  This is how we know where we are in the Jabber
// conversation.
// ---------------------------------------------------------------------------------
struct jabber_state_machine_struct {
	int connected;
	int connecting;
	int in_message;
	int in_message_body;
	int in_thread;
	int in_subject;
	int in_error;
	int in_message_error;
	int in_iq;
	int in_presence;
	int in_status;
};
typedef struct jabber_state_machine_struct jabber_machine;


/**
	@brief Collection of things for managing a Jabber session.
*/
struct transport_session_struct {

	/* our socket connection */
	socket_manager* sock_mgr;             /**< Manages the socket to the Jabber server. */

	/* our Jabber state machine */
	jabber_machine* state_machine;        /**< Keeps track of where we are in the XML. */
	/* our SAX push parser context */
	xmlParserCtxtPtr parser_ctxt;         /**< Used by the XML parser. */

	/* our text buffers for holding text data */
	growing_buffer* body_buffer;          /**< Text of &lt;body&gt; of message stanza. */
	growing_buffer* subject_buffer;       /**< Text of &lt;subject&gt; of message stanza. */
	growing_buffer* thread_buffer;        /**< Text of &lt;thread&gt; of message stanza. */
	growing_buffer* from_buffer;          /**< "from" attribute of &lt;message&gt;. */
	growing_buffer* recipient_buffer;     /**< "to" attribute of &lt;message&gt;. */
	growing_buffer* status_buffer;        /**< Text of &lt;status&gt; of message stanza. */
	growing_buffer* message_error_type;   /**< "type" attribute of &lt;error&gt;. */
	growing_buffer* session_id;           /**< "id" attribute of stream header. */
	int message_error_code;               /**< "code" attribute of &lt;error&gt;. */

	/* for OILS extensions */
	growing_buffer* router_to_buffer;     /**< "router_to" attribute of &lt;message&gt;. */
	growing_buffer* router_from_buffer;   /**< "router_from" attribute of &lt;message&gt;. */
	growing_buffer* router_class_buffer;  /**< "router_class" attribute of &lt;message&gt;. */
	growing_buffer* router_command_buffer; /**< "router_command" attribute of &lt;message&gt;. */
	growing_buffer* osrf_xid_buffer;      /**< "osrf_xid" attribute of &lt;message&gt;. */
	int router_broadcast;                 /**< "broadcast" attribute of &lt;message&gt;. */

	void* user_data;                      /**< Opaque pointer from calling code. */

	char* server;                         /**< address of Jabber server. */
	char* unix_path;                      /**< Unix pathname (if any) of socket to Jabber server */
	int	port;                             /**< Port number of Jabber server. */
	int sock_id;                          /**< File descriptor of socket to Jabber. */

	int component;                        /**< Boolean; true if we're a Jabber component. */

	/** Callback from calling code, for when a complete message stanza is received. */
	void (*message_callback) ( void* user_data, transport_message* msg );
	//void (iq_callback) ( void* user_data, transport_iq_message* iq );
};
typedef struct transport_session_struct transport_session;

transport_session* init_transport( const char* server, int port,
	const char* unix_path, void* user_data, int component );

int session_wait( transport_session* session, int timeout );

int session_send_msg( transport_session* session, transport_message* msg );

int session_connected( transport_session* session );

int session_free( transport_session* session );

int session_discard( transport_session* session );

int session_connect( transport_session* session,
		const char* username, const char* password,
		const char* resource, int connect_timeout,
		enum TRANSPORT_AUTH_TYPE auth_type );

int session_disconnect( transport_session* session );

#ifdef __cplusplus
}
#endif

#endif
