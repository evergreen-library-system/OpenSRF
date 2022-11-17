#ifndef TRANSPORT_MESSAGE_H
#define TRANSPORT_MESSAGE_H

/**
	@file transport_message.h
	@brief Header for routines to manage transport_messages.

	A transport_message is a representation of a Jabber message. i.e. a message stanza in a
	Jabber XML stream.
*/

#include <string.h>
#include <libxml/globals.h>
#include <libxml/xmlerror.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/debugXML.h>
#include <libxml/xmlmemory.h>

#include <opensrf/utils.h>
#include <opensrf/xml_utils.h>
#include <opensrf/log.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
	@brief A representation of a Jabber message.

	A transport_message represents a message stanza of a Jabber XML stream.  The various
	members store the contents of various attributes or text in the XML.  The following
	members represent OSRF extensions:
	- router_from
	- router_to
	- router_class
	- router_command
	- osrf_xid
	- broadcast
*/
struct transport_message_struct {
	char* body;            /**< Text enclosed by the body element. */
	char* subject;         /**< Text enclosed by the subject element. */
	char* thread;          /**< Text enclosed by the thread element. */
	char* recipient;       /**< Value of the "to" attribute in the message element. */
	char* sender;          /**< Value of the "from" attribute in the message element. */
	char* router_from;     /**< Value of the "router_from" attribute in the message element. */
	char* router_to;       /**< Value of the "router_to" attribute in the message element. */
	char* router_class;    /**< Value of the "router_class" attribute in the message element. */
	char* router_command;  /**< Value of the "router_command" attribute in the message element. */
	char* osrf_xid;        /**< Value of the "osrf_xid" attribute in the message element. */
	int is_error;          /**< Boolean; true if &lt;error&gt; is present. */
	char* error_type;      /**< Value of the "type" attribute of &lt;error&gt;. */
	int error_code;        /**< Value of the "code" attribute of &lt;error&gt;. */
	int broadcast;         /**< Value of the "broadcast" attribute in the message element. */
	char* msg_xml;         /**< The entire message as XML, complete with entity encoding. */
	char* msg_json;         /**< The entire message as JSON*/
	struct transport_message_struct* next;
};
typedef struct transport_message_struct transport_message;

transport_message* message_init( const char* body, const char* subject,
		const char* thread, const char* recipient, const char* sender );

transport_message* new_message_from_xml( const char* msg_xml );
transport_message* new_message_from_json( const char* msg_json );

void message_set_router_info( transport_message* msg, const char* router_from,
		const char* router_to, const char* router_class, const char* router_command,
		int broadcast_enabled );

void message_set_osrf_xid( transport_message* msg, const char* osrf_xid );

int message_prepare_xml( transport_message* msg );
int message_prepare_json( transport_message* msg );

int message_free( transport_message* msg );

void jid_get_username( const char* jid, char buf[], int size );

void jid_get_resource( const char* jid, char buf[], int size );

void jid_get_domain( const char* jid, char buf[], int size );

void set_msg_error( transport_message*, const char* error_type, int error_code);

#ifdef __cplusplus
}
#endif

#endif
