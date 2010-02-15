#include <opensrf/xml_utils.h>

/* helper function */
static jsonObject* _xmlToJSON(xmlNodePtr node, jsonObject*);

/**
	@brief Write then contents of an xmlNode to standard output.
	@param node Pointer to an xmlNode

	Write the text content of an xmlNode, and all its dependent nodes recursively, to
	standard output.
*/
void recurse_doc( xmlNodePtr node ) {
	if( node == NULL ) return;
	printf("Recurse: %s =>  %s", node->name, node->content );
	xmlNodePtr t = node->children;
	while(t) {
		recurse_doc(t);
		t = t->next;
	}
}

jsonObject* xmlDocToJSON(xmlDocPtr doc) {
	if(!doc) return NULL;
	return _xmlToJSON(xmlDocGetRootElement(doc), NULL);
}

static jsonObject* _xmlToJSON(xmlNodePtr node, jsonObject* obj) {

	if(!node) return NULL;
	if(xmlIsBlankNode(node)) return NULL;
	if(obj == NULL) obj = jsonNewObject(NULL);

	if(node->type == XML_TEXT_NODE) {
		jsonObjectSetString(obj, (char*) node->content);

	} else if(node->type == XML_ELEMENT_NODE || node->type == XML_ATTRIBUTE_NODE ) {

		jsonObject* new_obj = jsonNewObject(NULL);

		jsonObject* old;

		/* do the duplicate node / array shuffle */
		if( (old = jsonObjectGetKey(obj, (char*) node->name)) ) {
			if(old->type == JSON_ARRAY ) {
				jsonObjectPush(old, new_obj);
			} else {
				jsonObject* arr = jsonNewObject(NULL);
				jsonObjectPush(arr, jsonObjectClone(old));
				jsonObjectPush(arr, new_obj);
				jsonObjectSetKey(obj, (char*) node->name, arr);
			}
		} else {
			jsonObjectSetKey(obj, (char*) node->name, new_obj);
		}

		xmlNodePtr child = node->children;
		if (child) { // at least one...
			if (child != node->last) { // more than one -- ignore TEXT nodes
				while(child) {
					if (child->type != XML_TEXT_NODE) _xmlToJSON(child, new_obj);
					child = child->next;
				}
			} else {
				_xmlToJSON(child, new_obj);
			}
		}
	}

	return obj;
}


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
	@param atts Pointer to the attribute name to be sought.
	@return A pointer to the attribute value if found, or NULL if not.

	The @a atts parameter points to a ragged array of strings.  The @a atts[0] pointer points
	to an attribute name, and @a atts[1] points to the corresponding attribute value.  The
	remaining pointers likewise point alternately to names and values.  The end of the
	list is marked by a NULL.

	In practice, the @a atts array is constructed by the XML parser and passed to a callback
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

	In practice, the @a atts array is constructed by the XML parser and passed to a callback
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

