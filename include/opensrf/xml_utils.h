#ifndef XML_UTILS_H
#define XML_UTILS_H

#include <opensrf/osrf_json.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#ifdef __cplusplus
extern "C" {
#endif

jsonObject* xmlDocToJSON(xmlDocPtr doc);

/* debug function, prints each node and content */
void recurse_doc( xmlNodePtr node );


/* turns an XML doc into a char*.  
	User is responsible for freeing the returned char*
	if(full), then we return the whole doc (xml declaration, etc.)
	else we return the doc from the root node down
	*/
char* xmlDocToString(xmlDocPtr doc, int full);


/* Takes an xmlChar** from a SAX callback and returns the value
	for the attribute with name 'name'
	*/
const char* xmlSaxAttr( const xmlChar** atts, const char* name ); 

/**
  Sets the xml attributes from atts to the given dom node 
 */
int xmlAddAttrs( xmlNodePtr node, const xmlChar** atts );

#ifdef __cplusplus
}
#endif

#endif
