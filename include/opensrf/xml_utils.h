#ifndef XML_UTILS_H
#define XML_UTILS_H

/**
	@file xml_utils.h
	@brief Utility routines for XML documents.
*/

#include <opensrf/osrf_json.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#ifdef __cplusplus
extern "C" {
#endif

jsonObject* xmlDocToJSON(xmlDocPtr doc);

void recurse_doc( xmlNodePtr node );

char* xmlDocToString(xmlDocPtr doc, int full);

const char* xmlSaxAttr( const xmlChar** atts, const char* name ); 

int xmlAddAttrs( xmlNodePtr node, const xmlChar** atts );

#ifdef __cplusplus
}
#endif

#endif
