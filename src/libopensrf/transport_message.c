#include <opensrf/transport_message.h>

/**
	@file transport_message.c
	@brief Collection of routines for managing transport_messages.

	These routines are largely concerned with the conversion of XML to transport_messages,
	and vice versa.
*/

/**
	@brief Allocate and initialize a new transport_message to be send via Jabber.
	@param body Content of the message.
	@param subject Subject of the message.
	@param thread Thread of the message.
	@param recipient The address of the recipient.
	@param sender The address of the sender.
	@return A pointer to a newly-allocated transport_message.

	This function doesn't populate everything.  Typically there are subsequent calls to
	some combination of message_set_router_info(), message_set_osrf_xid(), and
	set_msg_error() to populate various additional bits and pieces.  Before sending the
	message anywhere, we call message_prepare_xml() to translate the message into XML,
	specifically a message stanza for a Jabber XML stream.

	The calling code is responsible for freeing the transport_message by calling message_free().
*/
transport_message* message_init( const char* body, const char* subject,
		const char* thread, const char* recipient, const char* sender ) {

	transport_message* msg = safe_malloc( sizeof(transport_message) );

	if( body        == NULL ) { body       = ""; }
	if( thread      == NULL ) { thread     = ""; }
	if( subject     == NULL ) { subject    = ""; }
	if( sender      == NULL ) { sender     = ""; }
	if( recipient   == NULL ) { recipient  = ""; }

	msg->body       = strdup(body);
	msg->thread     = strdup(thread);
	msg->subject    = strdup(subject);
	msg->recipient  = strdup(recipient);
	msg->sender     = strdup(sender);

	if( msg->body        == NULL || msg->thread    == NULL  ||
			msg->subject == NULL || msg->recipient == NULL  ||
			msg->sender  == NULL ) {

		osrfLogError(OSRF_LOG_MARK, "message_init(): Out of Memory" );
		free( msg->body );
		free( msg->thread );
		free( msg->subject );
		free( msg->recipient );
		free( msg->sender );
		free( msg );
		return NULL;
	}

	msg->router_from    = NULL;
	msg->router_to      = NULL;
	msg->router_class   = NULL;
	msg->router_command = NULL;
	msg->osrf_xid       = NULL;
	msg->is_error       = 0;
	msg->error_type     = NULL;
	msg->error_code     = 0;
	msg->broadcast      = 0;
	msg->msg_xml        = NULL;
	msg->next           = NULL;

	return msg;
}


/**
	@brief Translate an XML string into a transport_message.
	@param msg_xml Pointer to a &lt;message&gt; element as passed by Jabber.
	@return Pointer to a newly created transport_message.

	Do @em not populate the following members:
	- router_command
	- is_error
	- error_type
	- error_code.

	The calling code is responsible for freeing the transport_message by calling message_free().
*/
transport_message* new_message_from_xml( const char* msg_xml ) {

	if( msg_xml == NULL || *msg_xml == '\0' )
		return NULL;

	transport_message* new_msg = safe_malloc( sizeof(transport_message) );

	new_msg->body           = NULL;
	new_msg->subject        = NULL;
	new_msg->thread         = NULL;
	new_msg->recipient      = NULL;
	new_msg->sender         = NULL;
	new_msg->router_from    = NULL;
	new_msg->router_to      = NULL;
	new_msg->router_class   = NULL;
	new_msg->router_command = NULL;
	new_msg->osrf_xid       = NULL;
	new_msg->is_error       = 0;
	new_msg->error_type     = NULL;
	new_msg->error_code     = 0;
	new_msg->broadcast      = 0;
	new_msg->msg_xml        = NULL;
	new_msg->next           = NULL;

	/* Parse the XML document and grab the root */
	xmlKeepBlanksDefault(0);
	xmlDocPtr msg_doc = xmlReadDoc( BAD_CAST msg_xml, NULL, NULL, 0 );
	xmlNodePtr root = xmlDocGetRootElement(msg_doc);

	/* Get various attributes of the root, */
	/* and use them to populate the corresponding members */
	xmlChar* sender         = xmlGetProp( root, BAD_CAST "from");
	xmlChar* recipient      = xmlGetProp( root, BAD_CAST "to");
	xmlChar* subject        = xmlGetProp( root, BAD_CAST "subject");
	xmlChar* thread         = xmlGetProp( root, BAD_CAST "thread" );
	xmlChar* router_from    = NULL;
	xmlChar* router_to      = NULL;
	xmlChar* router_class   = NULL;
	xmlChar* router_command = NULL;
	xmlChar* broadcast      = NULL;
	xmlChar* osrf_xid       = NULL;

	if( sender ) {
		new_msg->sender = strdup((const char*)sender);
		xmlFree(sender);
	}

	if( recipient ) {
		new_msg->recipient  = strdup((const char*)recipient);
		xmlFree(recipient);
	}

	if(subject){
		new_msg->subject    = strdup((const char*)subject);
		xmlFree(subject);
	}

	if(thread) {
		new_msg->thread     = strdup((const char*)thread);
		xmlFree(thread);
	}

	/* Within the message element, find the child nodes for "thread", "subject" */
	/* "body", and "opensrf".  Extract their textual content into the corresponding members. */
	xmlNodePtr search_node = root->children;
	while( search_node != NULL ) {

		if( ! strcmp( (const char*) search_node->name, "thread" ) ) {
			if( search_node->children && search_node->children->content )
				new_msg->thread = strdup( (const char*) search_node->children->content );
		}

		if( ! strcmp( (const char*) search_node->name, "subject" ) ) {
			if( search_node->children && search_node->children->content )
				new_msg->subject = strdup( (const char*) search_node->children->content );
		}

		if( ! strcmp( (const char*) search_node->name, "opensrf" ) ) {
			router_from    = xmlGetProp( search_node, BAD_CAST "router_from" );
			router_to      = xmlGetProp( search_node, BAD_CAST "router_to" );
			router_class   = xmlGetProp( search_node, BAD_CAST "router_class" );
			router_command = xmlGetProp( search_node, BAD_CAST "router_command" );
			broadcast      = xmlGetProp( search_node, BAD_CAST "broadcast" );
			osrf_xid       = xmlGetProp( search_node, BAD_CAST "osrf_xid" );

			if( osrf_xid ) {
				message_set_osrf_xid( new_msg, (char*) osrf_xid);
				xmlFree(osrf_xid);
			}

			if( router_from ) {
				if (new_msg->sender) {
					// Sender value applied above.  Clear it and
					// use the router value instead.
					free(new_msg->sender);
				}

				new_msg->sender = strdup((const char*)router_from);
				new_msg->router_from = strdup((const char*)router_from);
				xmlFree(router_from);
			}

			if(router_to) {
				new_msg->router_to  = strdup((const char*)router_to);
				xmlFree(router_to);
			}

			if(router_class) {
				new_msg->router_class = strdup((const char*)router_class);
				xmlFree(router_class);
			}

			if(router_command) {
				new_msg->router_command = strdup((const char*)router_command);
				xmlFree(router_command);
			}

			if(broadcast) {
				if(strcmp((const char*) broadcast,"0") )
					new_msg->broadcast = 1;
				xmlFree(broadcast);
			}
		}

		if( ! strcmp( (const char*) search_node->name, "body" ) ) {
			if( search_node->children && search_node->children->content )
				new_msg->body = strdup((const char*) search_node->children->content );
		}

		search_node = search_node->next;
	}

	if( new_msg->thread == NULL )
		new_msg->thread = strdup("");
	if( new_msg->subject == NULL )
		new_msg->subject = strdup("");
	if( new_msg->body == NULL )
		new_msg->body = strdup("");

	/* Convert the XML document back into a string, and store it. */
	new_msg->msg_xml = xmlDocToString(msg_doc, 0);
	xmlFreeDoc(msg_doc);
	xmlCleanupParser();

	return new_msg;
}

/**
	@brief Populate the osrf_xid (an OSRF extension) of a transport_message.
	@param msg Pointer to the transport_message.
	@param osrf_xid Value of the "osrf_xid" attribute of a message stanza.

	If @a osrf_xid is NULL, populate with an empty string.

	See also message_set_router_info().
*/
void message_set_osrf_xid( transport_message* msg, const char* osrf_xid ) {
	if( msg ) {
		if( msg->osrf_xid ) free( msg->osrf_xid );
		msg->osrf_xid = strdup( osrf_xid ? osrf_xid : "" );
	}
}

/**
	@brief Populate some OSRF extensions to XMPP in a transport_message.
	@param msg Pointer to the transport_message to be populated.
	@param router_from Value of "router_from" attribute in message stanza
	@param router_to Value of "router_to" attribute in message stanza
	@param router_class Value of "router_class" attribute in message stanza
	@param router_command Value of "router_command" attribute in message stanza
	@param broadcast_enabled Value of "broadcast" attribute in message stanza

	If any of the pointer pararmeters is NULL (other than @a msg), populate the
	corresponding member with an empty string.

	See also osrf_set_xid().
*/
void message_set_router_info( transport_message* msg, const char* router_from,
		const char* router_to, const char* router_class, const char* router_command,
		int broadcast_enabled ) {

	if( msg ) {

		/* free old values, if any */
		if( msg->router_from    ) free( msg->router_from );
		if( msg->router_to      ) free( msg->router_to );
		if( msg->router_class   ) free( msg->router_class );
		if( msg->router_command ) free( msg->router_command );

		/* install new values */
		msg->router_from     = strdup( router_from     ? router_from     : "" );
		msg->router_to       = strdup( router_to       ? router_to       : "" );
		msg->router_class    = strdup( router_class    ? router_class    : "" );
		msg->router_command  = strdup( router_command  ? router_command  : "" );
		msg->broadcast = broadcast_enabled;

		if( msg->router_from == NULL || msg->router_to == NULL ||
				msg->router_class == NULL || msg->router_command == NULL )
			osrfLogError(OSRF_LOG_MARK,  "message_set_router_info(): Out of Memory" );
	}
}


/**
	@brief Free a transport_message and all the memory it owns.
	@param msg Pointer to the transport_message to be destroyed.
	@return 1 if successful, or 0 upon error.  The only error condition is if @a msg is NULL.
*/
int message_free( transport_message* msg ){
	if( msg == NULL ) { return 0; }

	free(msg->body);
	free(msg->thread);
	free(msg->subject);
	free(msg->recipient);
	free(msg->sender);
	free(msg->router_from);
	free(msg->router_to);
	free(msg->router_class);
	free(msg->router_command);
	free(msg->osrf_xid);
	if( msg->error_type != NULL ) free(msg->error_type);
	if( msg->msg_xml != NULL ) free(msg->msg_xml);
	free(msg);
	return 1;
}


/**
	@brief Build a &lt;message&gt; element and store it as a string in the msg_xml member.
	@param msg Pointer to a transport_message.
	@return 1 if successful, or 0 if not.  The only error condition is if @a msg is NULL.

	If msg_xml is already populated, keep it, and return immediately.

	The contents of the &lt;message&gt; element come from various members of the
	transport_message.  Store the resulting string as the msg_xml member.

	To build the XML we first build a DOM structure, and then export it to a string.  That
	way we can let the XML library worry about replacing certain characters with
	character entity references -- not a trivial task when UTF-8 characters may be present.
*/
int message_prepare_xml( transport_message* msg ) {

	if( !msg ) return 0;
	if( msg->msg_xml ) return 1;   /* already done */

	xmlNodePtr  message_node;
	xmlNodePtr  body_node;
	xmlNodePtr  thread_node;
	xmlNodePtr  opensrf_node;
	xmlNodePtr  subject_node;
	xmlNodePtr  error_node;

	xmlDocPtr   doc;

	xmlKeepBlanksDefault(0);

	doc = xmlReadDoc( BAD_CAST "<message/>", NULL, NULL, XML_PARSE_NSCLEAN );
	message_node = xmlDocGetRootElement(doc);

	if( msg->is_error ) {
		error_node = xmlNewChild(message_node, NULL, BAD_CAST "error" , NULL );
		xmlAddChild( message_node, error_node );
		xmlNewProp( error_node, BAD_CAST "type", BAD_CAST msg->error_type );
		char code_buf[16];
		osrf_clearbuf( code_buf, sizeof(code_buf));
		sprintf(code_buf, "%d", msg->error_code );
		xmlNewProp( error_node, BAD_CAST "code", BAD_CAST code_buf  );
	}

	/* set from and to */
	xmlNewProp( message_node, BAD_CAST "to", BAD_CAST msg->recipient );
	xmlNewProp( message_node, BAD_CAST "from", BAD_CAST msg->sender );

	/* set from and to on a new node, also */
	opensrf_node = xmlNewChild(message_node, NULL, (xmlChar*) "opensrf", NULL );
	xmlNewProp( opensrf_node, BAD_CAST "router_from", BAD_CAST msg->router_from );
	xmlNewProp( opensrf_node, BAD_CAST "router_to", BAD_CAST msg->router_to );
	xmlNewProp( opensrf_node, BAD_CAST "router_class", BAD_CAST msg->router_class );
	xmlNewProp( opensrf_node, BAD_CAST "router_command", BAD_CAST msg->router_command );
	xmlNewProp( opensrf_node, BAD_CAST "osrf_xid", BAD_CAST msg->osrf_xid );

	xmlAddChild(message_node, opensrf_node);

	if( msg->broadcast ) {
		xmlNewProp( opensrf_node, BAD_CAST "broadcast", BAD_CAST "1" );
	}

	/* Now add nodes where appropriate */
	char* body      = msg->body;
	char* subject   = msg->subject;
	char* thread    = msg->thread;

	if( thread && *thread ) {
		thread_node = xmlNewChild(message_node, NULL, (xmlChar*) "thread", NULL );
		xmlNodePtr txt = xmlNewText((xmlChar*) thread);
		xmlAddChild(thread_node, txt);
		xmlAddChild(message_node, thread_node);
	}

	if( subject && *subject ) {
		subject_node = xmlNewChild(message_node, NULL, (xmlChar*) "subject", NULL );
		xmlNodePtr txt = xmlNewText((xmlChar*) subject);
		xmlAddChild(subject_node, txt);
		xmlAddChild( message_node, subject_node );
	}

	if( body && *body ) {
		body_node = xmlNewChild(message_node, NULL, (xmlChar*) "body", NULL);
		xmlNodePtr txt = xmlNewText((xmlChar*) body);
		xmlAddChild(body_node, txt);
		xmlAddChild( message_node, body_node );
	}

	// Export the xmlDoc to a string
	xmlBufferPtr xmlbuf = xmlBufferCreate();
	xmlNodeDump( xmlbuf, doc, xmlDocGetRootElement(doc), 0, 0);
	msg->msg_xml = strdup((const char*) (xmlBufferContent(xmlbuf)));

	xmlBufferFree(xmlbuf);
	xmlFreeDoc( doc );
	xmlCleanupParser();

	return 1;
}


/**
	@brief Extract the username from a Jabber ID.
	@param jid Pointer to the Jabber ID.
	@param buf Pointer to a receiving buffer supplied by the caller.
	@param size Maximum number of characters to copy, not including the terminal nul.

	A jabber ID is of the form "username@domain/resource", where the resource is optional.
	Here we copy the username portion into the supplied buffer, plus a terminal nul.  If there
	is no "@" character, leave the buffer as an empty string.
*/
void jid_get_username( const char* jid, char buf[], int size ) {

	if( jid == NULL || buf == NULL || size <= 0 ) { return; }

	buf[ 0 ] = '\0';

	/* find the @ and return whatever is in front of it */
	int len = strlen( jid );
	int i;
	for( i = 0; i != len; i++ ) {
		if( jid[i] == '@' ) {
			if(i > size)  i = size;
			memcpy( buf, jid, i );
			buf[i] = '\0';
			return;
		}
	}
}


/**
	@brief Extract the resource from a Jabber ID.
	@param jid Pointer to the Jabber ID.
	@param buf Pointer to a receiving buffer supplied by the caller.
	@param size Maximum number of characters to copy, not including the terminal nul.

	A jabber ID is of the form "username@domain/resource", where the resource is optional.
	Here we copy the resource portion, if present into the supplied buffer, plus a terminal nul.
	If there is no resource, leave the buffer as an empty string.
*/
void jid_get_resource( const char* jid, char buf[], int size)  {
	if( jid == NULL || buf == NULL || size <= 0 ) { return; }

	// Find the last slash, if any
	const char* start = strrchr( jid, '/' );
	if( start ) {

		// Copy the text beyond the slash, up to a maximum size
		size_t len = strlen( ++start );
		if( len > size ) len = size;
		memcpy( buf, start, len );
		buf[ len ] = '\0';
	}
	else
		buf[ 0 ] = '\0';
}

/**
	@brief Extract the domain from a Jabber ID.
	@param jid Pointer to the Jabber ID.
	@param buf Pointer to a receiving buffer supplied by the caller.
	@param size Maximum number of characters to copy, not including the terminal nul.

	A jabber ID is of the form "username@domain/resource", where the resource is optional.
	Here we copy the domain portion into the supplied buffer, plus a terminal nul.  If the
	Jabber ID is ill-formed, the results may be ill-formed or empty.
*/
void jid_get_domain( const char* jid, char buf[], int size ) {

	if(jid == NULL) return;

	int len = strlen(jid);
	int i;
	int index1 = 0;
	int index2 = 0;

	for( i = 0; i!= len; i++ ) {
		if(jid[i] == '@')
			index1 = i + 1;
		else if(jid[i] == '/' && index1 != 0)
			index2 = i;
	}

	if( index1 > 0 && index2 > 0 && index2 > index1 ) {
		int dlen = index2 - index1;
		if(dlen > size) dlen = size;
		memcpy( buf, jid + index1, dlen );
		buf[dlen] = '\0';
	}
	else
		buf[ 0 ] = '\0';
}

/**
	@brief Turn a transport_message into an error message.
	@param msg Pointer to the transport_message.
	@param type Pointer to a short string denoting the type of error.
	@param err_code The error code.

	The @a type and @a err_code parameters correspond to the "type" and "code" attributes of
	a Jabber error element.
*/
void set_msg_error( transport_message* msg, const char* type, int err_code ) {

	if( !msg ) return;

	if( type != NULL && *type ) {
		if( msg->error_type )
			free( msg->error_type );
		msg->error_type = safe_malloc( strlen(type)+1 );
		strcpy( msg->error_type, type );
		msg->error_code = err_code;
	}
	msg->is_error = 1;
}
