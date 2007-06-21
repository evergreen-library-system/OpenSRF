#include <objson/xml2json.h>

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







