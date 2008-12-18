#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>
#include <strings.h>
#include "apachetools.h"
#include <opensrf/osrf_app_session.h>
#include <opensrf/osrf_system.h>
#include <opensrf/osrfConfig.h>
#include <opensrf/osrf_json.h>
#include <opensrf/osrf_cache.h>

#define MODULE_NAME "osrf_http_translator_module"
#define OSRF_TRANSLATOR_CONFIG_FILE "OSRFTranslatorConfig"
#define OSRF_TRANSLATOR_CONFIG_CTX "OSRFTranslatorConfigContext"
#define OSRF_TRANSLATOR_CACHE_SERVER "OSRFTranslatorCacheServer"

#define DEFAULT_TRANSLATOR_CONFIG_CTX "gateway"
#define DEFAULT_TRANSLATOR_CONFIG_FILE "/openils/conf/opensrf_core.xml"
#define DEFAULT_TRANSLATOR_TIMEOUT 1200
#define DEFAULT_TRANSLATOR_CACHE_SERVERS "127.0.0.1:11211"

#define MULTIPART_CONTENT_TYPE "multipart/x-mixed-replace;boundary=\"%s\""
#define JSON_CONTENT_TYPE "text/plain"
#define MAX_MSGS_PER_PACKET 256
#define CACHE_TIME 300

#define OSRF_HTTP_HEADER_TO "X-OpenSRF-to"
#define OSRF_HTTP_HEADER_XID "X-OpenSRF-xid"
#define OSRF_HTTP_HEADER_FROM "X-OpenSRF-from"
#define OSRF_HTTP_HEADER_THREAD "X-OpenSRF-thread"
#define OSRF_HTTP_HEADER_TIMEOUT "X-OpenSRF-timeout"
#define OSRF_HTTP_HEADER_SERVICE "X-OpenSRF-service"
#define OSRF_HTTP_HEADER_MULTIPART "X-OpenSRF-multipart"

char* configFile = DEFAULT_TRANSLATOR_CONFIG_FILE;
char* configCtx = DEFAULT_TRANSLATOR_CONFIG_CTX;
char* cacheServers = DEFAULT_TRANSLATOR_CACHE_SERVERS;

char* routerName = NULL;
char* domainName = NULL;
int osrfConnected = 0;
char recipientBuf[128];
char contentTypeBuf[80];

// for development only, writes to apache error log
static void _dbg(char* s, ...) {
    VA_LIST_TO_STRING(s);
    fprintf(stderr, "%s\n", VA_BUF);
    fflush(stderr);
}

// Translator struct
typedef struct {
    request_rec* apreq;
    transport_client* handle;
    osrfList* messages;
    char* body;
    char* delim;
    const char* recipient;
    const char* service;
    const char* thread;
    const char* remoteHost;
    int complete;
    int timeout;
    int multipart;
    int connectOnly; // there is only 1 message, a CONNECT
    int disconnectOnly; // there is only 1 message, a DISCONNECT
    int connecting; // there is a connect message in this batch
    int disconnecting; // there is a connect message in this batch
    int localXid;
} osrfHttpTranslator;


static const char* osrfHttpTranslatorGetConfigFile(cmd_parms *parms, void *config, const char *arg) {
    configFile = (char*) arg;
	return NULL;
}
static const char* osrfHttpTranslatorGetConfigFileCtx(cmd_parms *parms, void *config, const char *arg) {
    configCtx = (char*) arg;
	return NULL;
}
static const char* osrfHttpTranslatorGetCacheServer(cmd_parms *parms, void *config, const char *arg) {
    cacheServers = (char*) arg;
	return NULL;
}

/** set up the configuratoin handlers */
static const command_rec osrf_json_gateway_cmds[] = {
	AP_INIT_TAKE1( OSRF_TRANSLATOR_CONFIG_FILE, osrfHttpTranslatorGetConfigFile,
			NULL, RSRC_CONF, "osrf translator config file"),
	AP_INIT_TAKE1( OSRF_TRANSLATOR_CONFIG_CTX, osrfHttpTranslatorGetConfigFileCtx,
			NULL, RSRC_CONF, "osrf translator config file context"),
	AP_INIT_TAKE1( OSRF_TRANSLATOR_CACHE_SERVER, osrfHttpTranslatorGetCacheServer,
			NULL, RSRC_CONF, "osrf translator cache server"),
    {NULL}
};


// there can only be one, so use a global static one
static osrfHttpTranslator globalTranslator;

/*
 * Constructs a new translator object based on the current apache 
 * request_rec.  Reads the request body and headers.
 */
static osrfHttpTranslator* osrfNewHttpTranslator(request_rec* apreq) {
    osrfHttpTranslator* trans = &globalTranslator;
    trans->apreq = apreq;
    trans->complete = 0;
    trans->connectOnly = 0;
    trans->disconnectOnly = 0;
    trans->connecting = 0;
    trans->disconnecting = 0;
    trans->remoteHost = apreq->connection->remote_ip;
    trans->messages = NULL;

    /* load the message body */
	osrfStringArray* params	= apacheParseParms(apreq);
    trans->body = apacheGetFirstParamValue(params, "osrf-msg");
    osrfStringArrayFree(params);

    /* load the request headers */
    if (apr_table_get(apreq->headers_in, OSRF_HTTP_HEADER_XID)) // force our log xid to match the caller
	    osrfLogForceXid(strdup(apr_table_get(apreq->headers_in, OSRF_HTTP_HEADER_XID)));

    trans->handle = osrfSystemGetTransportClient();
    trans->recipient = apr_table_get(apreq->headers_in, OSRF_HTTP_HEADER_TO);
    trans->service = apr_table_get(apreq->headers_in, OSRF_HTTP_HEADER_SERVICE);
    trans->thread = apr_table_get(apreq->headers_in, OSRF_HTTP_HEADER_THREAD); /* XXX create thread if necessary */

    const char* timeout = apr_table_get(apreq->headers_in, OSRF_HTTP_HEADER_TIMEOUT);
    if(timeout) 
        trans->timeout = atoi(timeout);
    else 
        trans->timeout = DEFAULT_TRANSLATOR_TIMEOUT;

    const char* multipart = apr_table_get(apreq->headers_in, OSRF_HTTP_HEADER_MULTIPART);
    if(multipart && !strcasecmp(multipart, "true"))
        trans->multipart = 1;
    else
        trans->multipart = 0;

    char buf[32];
    snprintf(buf, sizeof(buf), "%d%ld", getpid(), time(NULL));
    trans->delim = md5sum(buf);

    return trans;
}

static void osrfHttpTranslatorFree(osrfHttpTranslator* trans) {
    if(!trans) return;
    if(trans->body)
        free(trans->body);
    if(trans->delim)
        free(trans->delim);
    osrfListFree(trans->messages);
}

static void osrfHttpTranslatorDebug(osrfHttpTranslator* trans) {
    _dbg("-----------------------------------");
    _dbg("body = %s", trans->body);
    _dbg("service = %s", trans->service);
    _dbg("thread = %s", trans->thread);
    _dbg("multipart = %d", trans->multipart);
    _dbg("recipient = %s", trans->recipient);
}

/**
 * Determines the correct recipient address based on the requested 
 * service or recipient address.  
 */
static int osrfHttpTranslatorSetTo(osrfHttpTranslator* trans) {
    int stat = 0;
    jsonObject* sessionCache = NULL;

    if(trans->service) {
        if(trans->recipient) {
            osrfLogError(OSRF_LOG_MARK, "Specifying both SERVICE and TO are not allowed");

        } else {
            // service is specified, build a recipient address 
            // from the router, domain, and service
            int size = snprintf(recipientBuf, 128, "%s@%s/%s", routerName, domainName, trans->service);
            recipientBuf[size] = '\0';
            osrfLogDebug(OSRF_LOG_MARK, "Set recipient to %s", recipientBuf);
            trans->recipient = recipientBuf;
            stat = 1;
        }

    } else {

        if(trans->recipient) {
            sessionCache = osrfCacheGetObject(trans->thread);

            if(sessionCache) {
                char* ipAddr = jsonObjectGetString(jsonObjectGetKey(sessionCache, "ip"));
                char* recipient = jsonObjectGetString(jsonObjectGetKey(sessionCache, "jid"));

                // choosing a specific recipient address requires that the recipient and 
                // thread be cached on the server (so drone processes cannot be hijacked)
                if(!strcmp(ipAddr, trans->remoteHost) && !strcmp(recipient, trans->recipient)) {
                    osrfLogDebug(OSRF_LOG_MARK, "Found cached session from host %s and recipient %s", 
                        trans->remoteHost, trans->recipient);
                    stat = 1;
                    trans->service = jsonObjectGetString(jsonObjectGetKey(sessionCache, "service"));

                } else {
                    osrfLogError(OSRF_LOG_MARK, 
                        "Session cache for thread %s does not match request", trans->thread);
                }
            }  else {
                osrfLogError(OSRF_LOG_MARK, 
                    "attempt to send directly to %s without a session", trans->recipient);
            }
        } else {
            osrfLogError(OSRF_LOG_MARK, "No SERVICE or RECIPIENT defined");
        } 
    }

    jsonObjectFree(sessionCache);
    return stat;
}

/**
 * Parses the request body and logs any REQUEST messages to the activity log
 */
static int osrfHttpTranslatorParseRequest(osrfHttpTranslator* trans) {
    osrfMessage* msg;
    osrfMessage* msgList[MAX_MSGS_PER_PACKET];
    int numMsgs = osrf_message_deserialize(trans->body, msgList, MAX_MSGS_PER_PACKET);
    osrfLogDebug(OSRF_LOG_MARK, "parsed %d opensrf messages in this packet", numMsgs);

    if(numMsgs == 0)
        return 0;

    if(numMsgs == 1) {
        msg = msgList[0];
        if(msg->m_type == CONNECT) {
            trans->connectOnly = 1;
            trans->connecting = 1;
            return 1;
        }
        if(msg->m_type == DISCONNECT) {
            trans->disconnectOnly = 1;
            trans->disconnecting = 1;
            return 1;
        }
    }

    // log request messages to the activity log
    int i;
    for(i = 0; i < numMsgs; i++) {
        msg = msgList[i];

        switch(msg->m_type) {

            case REQUEST: {
                jsonObject* params = msg->_params;
                growing_buffer* act = buffer_init(128);	
                buffer_fadd(act, "[%s] [%s] %s %s", trans->remoteHost, "", trans->service, msg->method_name);

                char* str; 
                int i = 0;
                while((str = jsonObjectGetString(jsonObjectGetIndex(params, i++)))) {
                    if( i == 1 )
                        OSRF_BUFFER_ADD(act, " ");
                    else 
                        OSRF_BUFFER_ADD(act, ", ");
                    OSRF_BUFFER_ADD(act, str);
                }
                osrfLogActivity(OSRF_LOG_MARK, act->buf);
                buffer_free(act);
                break;
            }

            case CONNECT:
                trans->connecting = 1;
                break;

            case DISCONNECT:
                trans->disconnecting = 1;
                break;
        }
    }

    return 1;
}

static int osrfHttpTranslatorCheckStatus(osrfHttpTranslator* trans, transport_message* msg) {
    osrfMessage* omsgList[MAX_MSGS_PER_PACKET];
    int numMsgs = osrf_message_deserialize(msg->body, omsgList, MAX_MSGS_PER_PACKET);
    osrfLogDebug(OSRF_LOG_MARK, "parsed %d response messages", numMsgs);
    if(numMsgs == 0) return 0;

    osrfMessage* last = omsgList[numMsgs-1];
    if(last->m_type == STATUS) {
        if(last->status_code == OSRF_STATUS_TIMEOUT) {
            osrfLogDebug(OSRF_LOG_MARK, "removing cached session on request timeout");
            osrfCacheRemove(trans->thread);
            return 0;
        }
        // XXX hm, check for explicit status=COMPLETE message instead??
        if(last->status_code != OSRF_STATUS_CONTINUE)
            trans->complete = 1;
    }

    return 1;
}

static void osrfHttpTranslatorInitHeaders(osrfHttpTranslator* trans, transport_message* msg) {
    apr_table_set(trans->apreq->headers_out, OSRF_HTTP_HEADER_FROM, msg->sender);
    apr_table_set(trans->apreq->headers_out, OSRF_HTTP_HEADER_THREAD, trans->thread);
    if(trans->multipart) {
        sprintf(contentTypeBuf, MULTIPART_CONTENT_TYPE, trans->delim);
        contentTypeBuf[79] = '\0';
        osrfLogDebug(OSRF_LOG_MARK, "content type %s : %s : %s", MULTIPART_CONTENT_TYPE, trans->delim, contentTypeBuf);
	    ap_set_content_type(trans->apreq, contentTypeBuf);
        ap_rprintf(trans->apreq, "--%s\n", trans->delim);
    } else {
	    ap_set_content_type(trans->apreq, JSON_CONTENT_TYPE);
    }
}

/**
 * Cache the transaction with the JID of the backend process we are talking to
 */
static void osrfHttpTranslatorCacheSession(osrfHttpTranslator* trans, const char* jid) {
    jsonObject* cacheObj = jsonNewObject(NULL);
    jsonObjectSetKey(cacheObj, "ip", jsonNewObject(trans->remoteHost));
    jsonObjectSetKey(cacheObj, "jid", jsonNewObject(jid));
    jsonObjectSetKey(cacheObj, "service", jsonNewObject(trans->service));
    osrfCachePutObject((char*) trans->thread, cacheObj, CACHE_TIME);
}

           
/**
 * Writes a single chunk of multipart/x-mixed-replace content
 */
static void osrfHttpTranslatorWriteChunk(osrfHttpTranslator* trans, transport_message* msg) {
    osrfLogInternal(OSRF_LOG_MARK, "sending multipart chunk %s", msg->body);
    ap_rprintf(trans->apreq, 
        "Content-type: %s\n\n%s\n\n", JSON_CONTENT_TYPE, msg->body);
    if(trans->complete)
        ap_rprintf(trans->apreq, "--%s--\n", trans->delim);
    else
        ap_rprintf(trans->apreq, "--%s\n", trans->delim);
    ap_rflush(trans->apreq);
}

static int osrfHttpTranslatorProcess(osrfHttpTranslator* trans) {
    if(trans->body == NULL)
        return HTTP_BAD_REQUEST;

    if(!osrfHttpTranslatorSetTo(trans))
        return HTTP_BAD_REQUEST;

    if(!osrfHttpTranslatorParseRequest(trans))
        return HTTP_BAD_REQUEST;

    while(client_recv(trans->handle, 0))
        continue; // discard any old status messages in the recv queue

    // send the message to the recipient
    transport_message* tmsg = message_init(
        trans->body, NULL, trans->thread, trans->recipient, NULL);
    message_set_osrf_xid(tmsg, osrfLogGetXid());
    client_send_message(trans->handle, tmsg);
    message_free(tmsg); 

    if(trans->disconnectOnly) {
        osrfLogDebug(OSRF_LOG_MARK, "exiting early on disconnect");
        osrfCacheRemove(trans->thread);
        return OK;
    }

    // process the response from the opensrf service
    int firstWrite = 1;
    while(!trans->complete) {
        transport_message* msg = client_recv(trans->handle, trans->timeout);

        if(trans->handle->error) {
            osrfLogError(OSRF_LOG_MARK, "Transport error");
            osrfCacheRemove(trans->thread);
            return HTTP_INTERNAL_SERVER_ERROR;
        }

        if(msg == NULL)
            return HTTP_GATEWAY_TIME_OUT;

        if(msg->is_error) {
            osrfLogError(OSRF_LOG_MARK, "XMPP message resulted in error code %d", msg->error_code);
            osrfCacheRemove(trans->thread);
            return HTTP_NOT_FOUND;
        }

        if(!osrfHttpTranslatorCheckStatus(trans, msg))
            continue;

        if(firstWrite) {
            osrfHttpTranslatorInitHeaders(trans, msg);
            if(trans->connecting)
                osrfHttpTranslatorCacheSession(trans, msg->sender);
            firstWrite = 0;
        }

        if(trans->multipart) {
            osrfHttpTranslatorWriteChunk(trans, msg);
            if(trans->connectOnly)
                break;
        } else {
            if(!trans->messages)
                trans->messages = osrfNewList();
            osrfListPush(trans->messages, msg->body);

            if(trans->complete || trans->connectOnly) {
                growing_buffer* buf = buffer_init(128);
                int i;
                OSRF_BUFFER_ADD(buf, osrfListGetIndex(trans->messages, 0));
                for(i = 1; i < trans->messages->size; i++) {
                    buffer_chomp(buf); // chomp off the closing array bracket
                    char* body = osrfListGetIndex(trans->messages, i);
                    char newbuf[strlen(body)];
                    sprintf(newbuf, body+1); // chomp off the opening array bracket
                    OSRF_BUFFER_ADD_CHAR(buf, ',');
                    OSRF_BUFFER_ADD(buf, newbuf);
                }
                
                ap_rputs(buf->buf, trans->apreq);
                buffer_free(buf);
            }
        }
    }

    if(trans->disconnecting) // DISCONNECT within a multi-message batch
        osrfCacheRemove(trans->thread);

    return OK;
}

static void testConnection(request_rec* r) {
	if(!osrfConnected || !osrfSystemGetTransportClient()) {
        osrfLogError(OSRF_LOG_MARK, "We're not connected to OpenSRF");
		ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, "We're not connected to OpenSRF");
		usleep(100000); // .1 second to prevent process die/start overload
		exit(1);
	}
}

// it's dead, Jim
static apr_status_t childExit(void* data) {
    osrf_system_shutdown();
    return OK;
}

static void childInit(apr_pool_t *p, server_rec *s) {
	if(!osrfSystemBootstrapClientResc(configFile, configCtx, "translator")) {
		ap_log_error( APLOG_MARK, APLOG_ERR, 0, s, 
			"Unable to Bootstrap OpenSRF Client with config %s..", configFile);
		return;
	}

    routerName = osrfConfigGetValue(NULL, "/router_name");
    domainName = osrfConfigGetValue(NULL, "/domain");
    const char* servers[] = {cacheServers};
    osrfCacheInit(servers, 1, 86400);
	osrfConnected = 1;

    // at pool destroy time (= child exit time), cleanup
    // XXX causes us to disconnect even for clone()'d process cleanup (as in mod_cgi)
    //apr_pool_cleanup_register(p, NULL, childExit, apr_pool_cleanup_null);
}

static int handler(request_rec *r) {
    int stat = OK;
	if(strcmp(r->handler, MODULE_NAME)) return DECLINED;
    if(r->header_only) return stat;

	r->allowed |= (AP_METHOD_BIT << M_GET);
	r->allowed |= (AP_METHOD_BIT << M_POST);

	osrfLogSetAppname("osrf_http_translator");
    testConnection(r);

    osrfHttpTranslator* trans = osrfNewHttpTranslator(r);
	osrfLogMkXid();
    if(trans->body) {
        stat = osrfHttpTranslatorProcess(trans);
        //osrfHttpTranslatorDebug(trans);
        osrfLogInfo(OSRF_LOG_MARK, "translator resulted in status %d", stat);
    } else {
        osrfLogWarning(OSRF_LOG_MARK, "no message body to process");
    }
    osrfHttpTranslatorFree(trans);
	return stat;
}


static void registerHooks (apr_pool_t *p) {
	ap_hook_handler(handler, NULL, NULL, APR_HOOK_MIDDLE);
	ap_hook_child_init(childInit, NULL, NULL, APR_HOOK_MIDDLE);
}


module AP_MODULE_DECLARE_DATA osrf_http_translator_module = {
	STANDARD20_MODULE_STUFF,
    NULL,
	NULL,
    NULL,
	NULL,
    NULL,
	registerHooks,
};




