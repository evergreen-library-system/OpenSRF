#ifdef OSRF_JSON_ENABLE_XML_UTILS

#include <stdio.h>
#include <string.h>
#include <libxml/globals.h>
#include <libxml/xmlerror.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xmlmemory.h>

#include <opensrf/osrf_json.h>
#include <opensrf/utils.h>
#include <opensrf/osrf_list.h>


/**
 *	Generates an XML representation of a JSON object */
char* jsonObjectToXML(jsonObject*);


/*
 * Builds a JSON object from the provided XML 
 */
jsonObject* jsonXMLToJSONObject(const char* xml);

#endif
