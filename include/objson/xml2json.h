
#include <stdio.h>
#include <string.h>
#include <libxml/globals.h>
#include <libxml/xmlerror.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xmlmemory.h>

#include <objson/object.h>
#include <objson/json_parser.h>
#include <opensrf/utils.h>
#include <opensrf/osrf_list.h>


jsonObject* jsonXMLToJSONObject(const char* xml);



