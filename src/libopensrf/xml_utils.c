/**
	@file xml_utils.c
	@brief Utility routines for XML documents.
*/

#include <opensrf/xml_utils.h>

/* helper function */
static void _xmlToJSON(xmlNodePtr node, jsonObject*);

/**
	@brief Write the contents of an xmlNode to standard output.
	@param node Pointer to an xmlNode.

	Write the text content of an xmlNode, and all its dependent nodes recursively, to
	standard output.

	Warning: the output is pig-ugly, in part because whenever the child node is a tag
	(rather than text), the content member is a NULL pointer.

	Designed for debugging.
*/
void recurse_doc( xmlNodePtr node ) {
	if( node == NULL ) return;
	printf("Recurse: %s =>  %s", node->name,
			node->content ? (const char*) node->content : "(null)" );
	xmlNodePtr t = node->children;
	while(t) {
		recurse_doc(t);
		t = t->next;
	}
}

/**
	@brief Translate an XML document into a jsonObject.
	@param doc An xmlDocPtr representing the XML document.
	@return A pointer to the newly created jsonObject.

	The translation pays attention only to tags and enclosed text.  It ignores attributes,
	comments, processing directives, and XML declarations.

	The document as a whole is represented as a JSON_HASH with one member, whose key is the
	root tag.

	Every tag is represented as the key of a member in a JSON_HASH, whose corresponding value
	depends on what the element encloses:

	- If the element is empty, its value is a JSON_NULL.
	- If the element encloses only text, its value is a JSON_STRING containing the enclosed
	  text.  Special characters and UTF-8 characters are escaped according to JSON rules;
	  otherwise, white space is preserved intact.
	- If the element encloses one or more nested elements, its value is a JSON_HASH
	  whose members represent the enclosed elements, except that:
	- If there are two or more elements with the same tag in the same enclosing element,
	  they collapse into a single entry whose value is a JSON_ARRAY of the corresponding values.

	The calling code is responsible for freeing the jsonObject by calling jsonObjectFree().
*/
jsonObject* xmlDocToJSON(xmlDocPtr doc) {
	if( !doc )
		return NULL;
	xmlNodePtr root = xmlDocGetRootElement( doc );
	if( !root || xmlIsBlankNode( root ) )
		return NULL;

	jsonObject* obj = jsonNewObjectType( JSON_HASH );
	_xmlToJSON( root, obj );
	return obj;
}

/**
	@brief Translate an xmlNodePtr into a jsonObject.
	@param node Points to the XML node to be translated.
	@param obj Pointer to an existing jsonObject into which the new jsonObject will be inserted.

	See the description of xmlDocToJSON(), a thin wrapper for _xmlToJSON.
*/
static void _xmlToJSON(xmlNodePtr node, jsonObject* obj) {

	if( !node || !obj ) return;
	if(xmlIsBlankNode(node)) return;

	if(node->type == XML_TEXT_NODE) {
		jsonObjectSetString(obj, (char*) node->content);

	} else if(node->type == XML_ELEMENT_NODE || node->type == XML_ATTRIBUTE_NODE ) {

		jsonObject* new_obj = jsonNewObject(NULL);

		jsonObject* old = jsonObjectGetKey(obj, (char*) node->name);
		if( old ) {
			// We have already encountered an element with the same tag
			if( old->type == JSON_ARRAY ) {
				// Add the new value to an existing JSON_ARRAY
				jsonObjectPush(old, new_obj);
			} else {
				// Replace the earlier value with a JSON_ARRAY containing both values
				jsonObject* arr = jsonNewObjectType( JSON_ARRAY );
				jsonObjectPush( arr, jsonObjectClone(old) );
				jsonObjectPush( arr, new_obj );
				jsonObjectSetKey( obj, (char*) node->name, arr );
			}
		} else {
			jsonObjectSetKey(obj, (char*) node->name, new_obj);
		}

		xmlNodePtr child = node->children;
		if (child) { // at least one...
			if (child != node->last) { // more than one -- ignore TEXT nodes
				while(child) {
					if( child->type != XML_TEXT_NODE )
						_xmlToJSON( child, new_obj );
					child = child->next;
				}
			} else {
				_xmlToJSON( child, new_obj );
			}
		}
	}
}

/**
	@brief Translate an xmlDocPtr to a character string.
	@param doc An xmlDocPtr referencing an XML document.
	@param full  Boolean; controls whether the output includes material outside the root.
	@return Pointer to the generated string.

	If @a full is true, the output includes any material outside of the root element, such
	as processing directives, comments, and XML declarations.  Otherwise it excludes them.

	The calling code is responsible for freeing the string by calling free().
*/
char* xmlDocToString(xmlDocPtr doc, int full) {

	if(!doc) return NULL;

	char* xml;

	if(full) {

		xmlChar* xmlbuf;
		int size;
		xmlDocDumpMemory(doc, &xmlbuf, &size);
		xml = strdup((char*) (xmlbuf));
		xmlFree(xmlbuf);
		return xml;

	} else {

		xmlBufferPtr xmlbuf = xmlBufferCreate();
		xmlNodeDump( xmlbuf, doc, xmlDocGetRootElement(doc), 0, 0);
		xml = strdup((char*) (xmlBufferContent(xmlbuf)));
		xmlBufferFree(xmlbuf);
		return xml;

	}
}

/**
	@brief Search for the value of a given attribute in an attribute array.
	@param atts Pointer to the attribute array to be searched.
	@param name Pointer to the attribute name to be sought.
	@return A pointer to the attribute value if found, or NULL if not.

	The @a atts parameter points to a ragged array of strings.  The @a atts[0] pointer points
	to an attribute name, and @a atts[1] points to the corresponding attribute value.  The
	remaining pointers likewise point alternately to names and values.  The end of the
	list is marked by a NULL.

	In practice, the XML parser constructs the @a atts array and passes it to a callback
	function.
*/
const char* xmlSaxAttr( const xmlChar** atts, const char* name ) {
	if( atts && name ) {
		int i;
		for(i = 0; (atts[i] != NULL); i++) {
			if(!strcmp((char*) atts[i], name)) {
				if(atts[++i])
					return (const char*) atts[i];
			}
		}
	}
	return NULL;
}

/**
	@brief Add a series of attributes to an xmlNode.
	@param node Pointer to the xmlNode to which the attributes will be added.
	@param atts Pointer to the attributes to be added.
	@return Zero in all cases.

	The @a atts parameter points to a ragged array of strings.  The @a atts[0] pointer points
	to an attribute name, and @a atts[1] points to the corresponding attribute value.  The
	remaining pointers likewise point alternately to names and values.  The end of the
	list is marked by a NULL.

	In practice, the XML parser constructs the @a atts array and passes it to a callback
	function.
*/
int xmlAddAttrs( xmlNodePtr node, const xmlChar** atts ) {
	if( node && atts ) {
		int i;
		for(i = 0; (atts[i] != NULL); i++) {
			if(atts[i+1]) {
				xmlSetProp(node, atts[i], atts[i+1]);
				i++;
			}
		}
	}
	return 0;
}
