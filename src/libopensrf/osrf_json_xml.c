#include <opensrf/osrf_json_xml.h>

#ifdef OSRF_JSON_ENABLE_XML_UTILS

struct osrfXMLGatewayParserStruct {
    osrfList* objStack;
    osrfList* keyStack;
    jsonObject* obj;
    short inString;
    short inNumber;
    short error;
};
typedef struct osrfXMLGatewayParserStruct osrfXMLGatewayParser;

/** returns the attribute value with the given attribute name */
static char* getXMLAttr(const xmlChar** atts, char* attr_name) {
    int i;
    if (atts != NULL) {
        for(i = 0; (atts[i] != NULL); i++) {
            if(strcmp((char*) atts[i++], attr_name) == 0) {
                if(atts[i] != NULL) 
                    return (char*) atts[i];
            }
        }
    }
    return NULL;
}


static void appendChild(osrfXMLGatewayParser* p, jsonObject* obj) {

    if(p->obj == NULL) 
        p->obj = obj;

    if(p->objStack->size == 0)
        return;
    
    jsonObject* parent = OSRF_LIST_GET_INDEX(p->objStack, p->objStack->size - 1);

    if(parent->type == JSON_ARRAY) {
        jsonObjectPush(parent, obj);
    } else {
        char* key = osrfListPop(p->keyStack);
        jsonObjectSetKey(parent, key, obj);
        free(key); /* the list is not setup for auto-freeing */
    }
}



static void startElementHandler(
    void *parser, const xmlChar *name, const xmlChar **atts) {

    osrfXMLGatewayParser* p = (osrfXMLGatewayParser*) parser;
    jsonObject* obj;

    char* hint = getXMLAttr(atts, "class_hint");

    if(!strcmp((char*) name, "null")) {
        appendChild(p, jsonNewObject(NULL));
        return;
    }

    if(!strcmp((char*) name, "string")) {
        p->inString = 1;
        return;
    }

    if(!strcmp((char*) name, "element")) {
       osrfListPush(p->keyStack, strdup(getXMLAttr(atts, "key")));
       return;
    }

    if(!strcmp((char*) name, "object")) {
        obj = jsonNewObject(NULL);
        jsonObjectSetClass(obj, hint); /* OK if hint is NULL */
        obj->type = JSON_HASH;
        appendChild(p, obj);
        osrfListPush(p->objStack, obj);
        return;
    }

    if(!strcmp((char*) name, "array")) {
        obj = jsonNewObject(NULL);
        jsonObjectSetClass(obj, hint); /* OK if hint is NULL */
        obj->type = JSON_ARRAY;
        appendChild(p, obj);
        osrfListPush(p->objStack, obj);
        return;
    }


    if(!strcmp((char*) name, "number")) {
        p->inNumber = 1;
        return;
    }

    if(!strcmp((char*) name, "boolean")) {
        obj = jsonNewObject(NULL);
        obj->type = JSON_BOOL;
        char* val = getXMLAttr(atts, "value");
        if(val && !strcmp(val, "true"))
            obj->value.b = 1;
        
        return;
    }
}

static void endElementHandler( void *parser, const xmlChar *name) {
    if(!strcmp((char*) name, "array") || !strcmp((char*) name, "object")) {
        osrfXMLGatewayParser* p = (osrfXMLGatewayParser*) parser;
        osrfListPop(p->objStack);
    }
}

static void characterHandler(void *parser, const xmlChar *ch, int len) {

    char data[len+1];
    strncpy(data, (char*) ch, len);
    data[len] = '\0';
    osrfXMLGatewayParser* p = (osrfXMLGatewayParser*) parser;

    if(p->inString) {
        appendChild(p, jsonNewObject(data));
        p->inString = 0;
        return;
    }

    if(p->inNumber) {
        appendChild(p, jsonNewNumberObject(atof(data)));
        p->inNumber = 0;
        return;
    }
}

static void parseWarningHandler(void *parser, const char* msg, ...) {
    VA_LIST_TO_STRING(msg);
    fprintf(stderr, "Parser warning %s\n", VA_BUF);
    fflush(stderr);
}

static void parseErrorHandler(void *parser, const char* msg, ...) {

    VA_LIST_TO_STRING(msg);
    fprintf(stderr, "Parser error %s\n", VA_BUF);
    fflush(stderr);

    osrfXMLGatewayParser* p = (osrfXMLGatewayParser*) parser;

    /*  keyStack as strdup'ed strings.  The list may
     *  not be empty, so tell it to free the items
     *  when it's freed (from the main routine)
     */
    osrfListSetDefaultFree(p->keyStack);
    jsonObjectFree(p->obj);

    p->obj = NULL;
    p->error = 1;
}




static xmlSAXHandler SAXHandlerStruct = {
    NULL,                            /* internalSubset */
    NULL,                            /* isStandalone */
    NULL,                            /* hasInternalSubset */
    NULL,                            /* hasExternalSubset */
    NULL,                            /* resolveEntity */
    NULL,                            /* getEntity */
    NULL,                            /* entityDecl */
    NULL,                            /* notationDecl */
    NULL,                            /* attributeDecl */
    NULL,                            /* elementDecl */
    NULL,                            /* unparsedEntityDecl */
    NULL,                            /* setDocumentLocator */
    NULL,                            /* startDocument */
    NULL,                            /* endDocument */
    startElementHandler,        /* startElement */
    endElementHandler,      /* endElement */
    NULL,                            /* reference */
    characterHandler,           /* characters */
    NULL,                            /* ignorableWhitespace */
    NULL,                            /* processingInstruction */
    NULL,                            /* comment */
    parseWarningHandler,     /* xmlParserWarning */
    parseErrorHandler,       /* xmlParserError */
    NULL,                            /* xmlParserFatalError : unused */
    NULL,                            /* getParameterEntity */
    NULL,                            /* cdataBlock; */
    NULL,                            /* externalSubset; */
    1,
    NULL,
    NULL,                            /* startElementNs */
    NULL,                            /* endElementNs */
    NULL                            /* xmlStructuredErrorFunc */
};

static const xmlSAXHandlerPtr SAXHandler = &SAXHandlerStruct;

jsonObject* jsonXMLToJSONObject(const char* xml) {

    osrfXMLGatewayParser parser;

    /* don't define freeItem, since objects will be cleaned by freeing the parent */
    parser.objStack = osrfNewList(); 
    /* don't define freeItem, since the list eill end up empty if there are no errors*/
    parser.keyStack = osrfNewList(); 
    parser.obj = NULL;
    parser.inString = 0;
    parser.inNumber = 0;

    xmlParserCtxtPtr ctxt = xmlCreatePushParserCtxt(SAXHandler, &parser, "", 0, NULL);
    xmlParseChunk(ctxt, xml, strlen(xml), 1);

    osrfListFree(parser.objStack);
    osrfListFree(parser.keyStack);
    xmlFreeParserCtxt(ctxt);
    xmlCleanupCharEncodingHandlers();
    xmlDictCleanup();
    xmlCleanupParser();

    return parser.obj;
}






static char* _escape_xml (char*);
static int _recurse_jsonObjectToXML(jsonObject*, growing_buffer*);

char* jsonObjectToXML(jsonObject* obj) {

	growing_buffer * res_xml;
	char * output;

	res_xml = buffer_init(1024);

	if (!obj)
		return strdup("<null/>");
	
	_recurse_jsonObjectToXML( obj, res_xml );
	output = buffer_data(res_xml);
	
	buffer_free(res_xml);

	return output;

}

int _recurse_jsonObjectToXML(jsonObject* obj, growing_buffer* res_xml) {

	char * hint = NULL;
	char * bool_val = NULL;
	int i = 0;
	
	if (obj->classname)
		hint = strdup(obj->classname);

	if(obj->type == JSON_NULL) {

		if (hint)
			buffer_fadd(res_xml, "<null class_hint=\"%s\"/>",hint);
		else
			buffer_add(res_xml, "<null/>");

	} else if(obj->type == JSON_BOOL) {

		if (obj->value.b)
			bool_val = strdup("true");
		else
			bool_val = strdup("false");

		if (hint)
			buffer_fadd(res_xml, "<boolean value=\"%s\" class_hint=\"%s\"/>", bool_val, hint);
		else
			buffer_fadd(res_xml, "<boolean value=\"%s\"/>", bool_val);

		free(bool_val);
                
	} else if (obj->type == JSON_STRING) {
		if (hint) {
			char * t = _escape_xml(jsonObjectGetString(obj));
			buffer_fadd(res_xml,"<string class_hint=\"%s\">%s</string>", hint, t);
			free(t);
		} else {
			char * t = _escape_xml(jsonObjectGetString(obj));
			buffer_fadd(res_xml,"<string>%s</string>", t);
			free(t);
		}

	} else if(obj->type == JSON_NUMBER) {
		double x = jsonObjectGetNumber(obj);
		if (hint) {
			if (x == (int)x)
				buffer_fadd(res_xml,"<number class_hint=\"%s\">%d</number>", hint, (int)x);
			else
				buffer_fadd(res_xml,"<number class_hint=\"%s\">%lf</number>", hint, x);
		} else {
			if (x == (int)x)
				buffer_fadd(res_xml,"<number>%d</number>", (int)x);
			else
				buffer_fadd(res_xml,"<number>%lf</number>", x);
		}

	} else if (obj->type == JSON_ARRAY) {

		if (hint) 
        	       	buffer_fadd(res_xml,"<array class_hint=\"%s\">", hint);
		else
               		buffer_add(res_xml,"<array>");

	       	for ( i = 0; i!= obj->size; i++ )
			_recurse_jsonObjectToXML(jsonObjectGetIndex(obj,i), res_xml);

		buffer_add(res_xml,"</array>");

	} else if (obj->type == JSON_HASH) {

		if (hint)
        	       	buffer_fadd(res_xml,"<object class_hint=\"%s\">", hint);
		else
			buffer_add(res_xml,"<object>");

		jsonIterator* itr = jsonNewIterator(obj);
		jsonObject* tmp;
		while( (tmp = jsonIteratorNext(itr)) ) {
			buffer_fadd(res_xml,"<element key=\"%s\">",itr->key);
			_recurse_jsonObjectToXML(tmp, res_xml);
			buffer_add(res_xml,"</element>");
		}
		jsonIteratorFree(itr);

		buffer_add(res_xml,"</object>");
	}

	if (hint)
		free(hint);

	return 1;
}

char* _escape_xml (char* text) {
	char* out;
	growing_buffer* b = buffer_init(256);
	int len = strlen(text);
	int i;
	for (i = 0; i < len; i++) {
		if (text[i] == '&')
			buffer_add(b,"&amp;");
		else if (text[i] == '<')
			buffer_add(b,"&lt;");
		else if (text[i] == '>')
			buffer_add(b,"&gt;");
		else
			buffer_add_char(b,text[i]);
	}
	out = buffer_data(b);
	buffer_free(b);
	return out;
}

#endif
