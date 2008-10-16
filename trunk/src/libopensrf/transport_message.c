#include <opensrf/transport_message.h>


// ---------------------------------------------------------------------------------
// Allocates and initializes a new transport_message
// ---------------------------------------------------------------------------------
transport_message* message_init( const char* body, const char* subject,
		const char* thread, const char* recipient, const char* sender ) {

	transport_message* msg = safe_malloc( sizeof(transport_message) );

	if( body				== NULL ) { body		= ""; }
	if( thread				== NULL ) { thread		= ""; }
	if( subject				== NULL ) { subject		= ""; }
	if( sender				== NULL ) { sender		= ""; }
	if( recipient			== NULL ) { recipient	= ""; }

	msg->body				= strdup(body);
	msg->thread				= strdup(thread);
	msg->subject			= strdup(subject);
	msg->recipient			= strdup(recipient);
	msg->sender				= strdup(sender);

	if(	msg->body		== NULL || msg->thread				== NULL	||
			msg->subject	== NULL || msg->recipient			== NULL	||
			msg->sender		== NULL ) {

		osrfLogError(OSRF_LOG_MARK,  "message_init(): Out of Memory" );
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

	return msg;
}


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

	xmlKeepBlanksDefault(0);
	xmlDocPtr msg_doc = xmlReadDoc( BAD_CAST msg_xml, NULL, NULL, 0 );
	xmlNodePtr root = xmlDocGetRootElement(msg_doc);

	xmlChar* sender	= xmlGetProp(root, BAD_CAST "from");
	xmlChar* recipient	= xmlGetProp(root, BAD_CAST "to");
	xmlChar* subject		= xmlGetProp(root, BAD_CAST "subject");
	xmlChar* thread		= xmlGetProp( root, BAD_CAST "thread" );
	xmlChar* router_from	= xmlGetProp( root, BAD_CAST "router_from" );
	xmlChar* router_to	= xmlGetProp( root, BAD_CAST "router_to" );
	xmlChar* router_class= xmlGetProp( root, BAD_CAST "router_class" );
	xmlChar* broadcast	= xmlGetProp( root, BAD_CAST "broadcast" );
	xmlChar* osrf_xid    = xmlGetProp( root, BAD_CAST "osrf_xid" );

   if( osrf_xid ) {
      message_set_osrf_xid( new_msg, (char*) osrf_xid);
      xmlFree(osrf_xid);
   }

	if( router_from ) {
		new_msg->sender		= strdup((const char*)router_from);
	} else {
		if( sender ) {
			new_msg->sender		= strdup((const char*)sender);
			xmlFree(sender);
		}
	}

	if( recipient ) {
		new_msg->recipient	= strdup((const char*)recipient);
		xmlFree(recipient);
	}
	if(subject){
		new_msg->subject		= strdup((const char*)subject);
		xmlFree(subject);
	}
	if(thread) {
		new_msg->thread		= strdup((const char*)thread);
		xmlFree(thread);
	}
	if(router_from) {
		new_msg->router_from	= strdup((const char*)router_from);
		xmlFree(router_from);
	}
	if(router_to) {
		new_msg->router_to	= strdup((const char*)router_to);
		xmlFree(router_to);
	}
	if(router_class) {
		new_msg->router_class = strdup((const char*)router_class);
		xmlFree(router_class);
	}
	if(broadcast) {
		if(strcmp((const char*) broadcast,"0") )
			new_msg->broadcast	= 1;
		xmlFree(broadcast);
	}

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

	new_msg->msg_xml = xmlDocToString(msg_doc, 0);
   xmlFreeDoc(msg_doc);
   xmlCleanupParser();

	return new_msg;
}

void message_set_osrf_xid( transport_message* msg, const char* osrf_xid ) {
   if(!msg) return;
   if( osrf_xid )
      msg->osrf_xid = strdup(osrf_xid);
   else msg->osrf_xid = strdup("");
}

void message_set_router_info( transport_message* msg, const char* router_from,
		const char* router_to, const char* router_class, const char* router_command,
		int broadcast_enabled ) {

	if( !msg ) return;
	
	if(router_from)
		msg->router_from		= strdup(router_from);
	else
		msg->router_from		= strdup("");

	if(router_to)
		msg->router_to			= strdup(router_to);
	else
		msg->router_to			= strdup("");

	if(router_class)
		msg->router_class		= strdup(router_class);
	else 
		msg->router_class		= strdup("");
	
	if(router_command)
		msg->router_command	= strdup(router_command);
	else
		msg->router_command	= strdup("");

	msg->broadcast = broadcast_enabled;

	if( msg->router_from == NULL || msg->router_to == NULL ||
			msg->router_class == NULL || msg->router_command == NULL ) 
		osrfLogError(OSRF_LOG_MARK,  "message_set_router_info(): Out of Memory" );

	return;
}



/* encodes the message for traversal */
int message_prepare_xml( transport_message* msg ) {
	if( !msg ) return 0;
	if( msg->msg_xml == NULL )
		msg->msg_xml = message_to_xml( msg );
	return 1;
}


// ---------------------------------------------------------------------------------
//
// ---------------------------------------------------------------------------------
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
	
// ---------------------------------------------------------------------------------
// Allocates a char* holding the XML representation of this jabber message
// ---------------------------------------------------------------------------------
char* message_to_xml( const transport_message* msg ) {

	//int			bufsize;
	//xmlChar*		xmlbuf;
	//char*			encoded_body;

	xmlNodePtr	message_node;
	xmlNodePtr	body_node;
	xmlNodePtr	thread_node;
	xmlNodePtr	subject_node;
	xmlNodePtr	error_node;
	
	xmlDocPtr	doc;

	xmlKeepBlanksDefault(0);

	if( ! msg ) { 
		osrfLogWarning(OSRF_LOG_MARK,  "Passing NULL message to message_to_xml()"); 
		return NULL; 
	}

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
	xmlNewProp( message_node, BAD_CAST "router_from", BAD_CAST msg->router_from );
	xmlNewProp( message_node, BAD_CAST "router_to", BAD_CAST msg->router_to );
	xmlNewProp( message_node, BAD_CAST "router_class", BAD_CAST msg->router_class );
	xmlNewProp( message_node, BAD_CAST "router_command", BAD_CAST msg->router_command );
	xmlNewProp( message_node, BAD_CAST "osrf_xid", BAD_CAST msg->osrf_xid );

	if( msg->broadcast )
		xmlNewProp( message_node, BAD_CAST "broadcast", BAD_CAST "1" );

	/* Now add nodes where appropriate */
	char* body				= msg->body;
	char* subject			= msg->subject;
	char* thread			= msg->thread; 

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

	xmlBufferPtr xmlbuf = xmlBufferCreate();
	xmlNodeDump( xmlbuf, doc, xmlDocGetRootElement(doc), 0, 0);
	char* xml = strdup((const char*) (xmlBufferContent(xmlbuf)));
	xmlBufferFree(xmlbuf);
	xmlFreeDoc( doc );		 
	xmlCleanupParser();
	return xml;
}



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
		buf[dlen] = '\0'; // memcpy doesn't provide the nul
	}
	else
		buf[ 0 ] = '\0';
}

void set_msg_error( transport_message* msg, const char* type, int err_code ) {

	if( !msg ) return;
	
	if( type != NULL && *type ) {
		msg->error_type = safe_malloc( strlen(type)+1); 
		strcpy( msg->error_type, type );
		msg->error_code = err_code;
	}
	msg->is_error = 1;
}
