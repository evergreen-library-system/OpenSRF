#ifndef TRANSPORT_MESSAGE_H
#define TRANSPORT_MESSAGE_H

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

// ---------------------------------------------------------------------------------
// Jabber message object.
// ---------------------------------------------------------------------------------
struct transport_message_struct {
	char* body;
	char* subject;
	char* thread;
	char* recipient;
	char* sender;
	char* router_from;
	char* router_to;
	char* router_class;
	char* router_command;
	char* osrf_xid;
	int is_error;
	char* error_type;
	int error_code;
	int broadcast;
	char* msg_xml; /* the entire message as XML complete with entity encoding */
	struct transport_message_struct* next;
};
typedef struct transport_message_struct transport_message;

// ---------------------------------------------------------------------------------
// Allocates and returns a transport_message.  All chars are safely re-allocated
// within this method.
// Returns NULL on error
// ---------------------------------------------------------------------------------
transport_message* message_init( const char* body, const char* subject, 
		const char* thread, const char* recipient, const char* sender );

transport_message* new_message_from_xml( const char* msg_xml );


void message_set_router_info( transport_message* msg, const char* router_from,
		const char* router_to, const char* router_class, const char* router_command,
		int broadcast_enabled );

void message_set_osrf_xid( transport_message* msg, const char* osrf_xid );

// ---------------------------------------------------------------------------------
// Call this to create the encoded XML for sending on the wire.
// This is a seperate function so that encoding will not necessarily have
// to happen on all messages (i.e. typically only occurs outbound messages).
// ---------------------------------------------------------------------------------
int message_prepare_xml( transport_message* msg );

// ---------------------------------------------------------------------------------
// Deallocates the memory used by the transport_message
// Returns 0 on error
// ---------------------------------------------------------------------------------
int message_free( transport_message* msg );

// ---------------------------------------------------------------------------------
// Determines the username of a Jabber ID.  This expects a pre-allocated char 
// array for the return value.
// ---------------------------------------------------------------------------------
void jid_get_username( const char* jid, char buf[], int size );

// ---------------------------------------------------------------------------------
// Determines the resource of a Jabber ID.  This expects a pre-allocated char 
// array for the return value.
// ---------------------------------------------------------------------------------
void jid_get_resource( const char* jid, char buf[], int size );

/** Puts the domain portion of the given jid into the pre-allocated buffer */
void jid_get_domain( const char* jid, char buf[], int size );

void set_msg_error( transport_message*, const char* error_type, int error_code);

#ifdef __cplusplus
}
#endif

#endif
