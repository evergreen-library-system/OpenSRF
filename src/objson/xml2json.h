
#include <stdio.h>
#include <string.h>
#include <libxml/globals.h>
#include <libxml/xmlerror.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xmlmemory.h>

#include "object.h"
#include "json_parser.h"
#include "utils.h"
#include "osrf_list.h"



jsonObject* jsonXMLToJSONObject(const char* xml);



