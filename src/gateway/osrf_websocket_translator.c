/* -----------------------------------------------------------------------
 * Copyright 2012 Equinox Software, Inc.
 * Bill Erickson <berick@esilibrary.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * -----------------------------------------------------------------------
 */

/**
 * Dumb websocket <-> opensrf gateway.  Wrapped opensrf messages are extracted
 * and relayed to the opensrf network.  Responses are pulled from the opensrf
 * network and passed back to the client.  No attempt is made to understand
 * the contents of the messages.
 *
 * Messages to/from the websocket client take the following form:
 * {
 *   "service"  : "opensrf.foo", // required for new sessions (inbound only)
 *   "thread"   : "123454321",   // AKA thread. required for follow-up requests; max 64 chars.
 *   "log_xid"  : "123..32",     // optional log trace ID, max 64 chars;
 *   "osrf_msg" : {<osrf_msg>}   // required
 * }
 *
 * Each translator operates with two threads.  One thread receives messages
 * from the websocket client, translates, and relays them to the opensrf 
 * network. The second thread collects responses from the opensrf network and 
 * relays them back to the websocket client.
 *
 * The main thread reads from socket A (apache) and writes to socket B 
 * (openesrf), while the responder thread reads from B and writes to A.  The 
 * apr data structures used are threadsafe.  For now, no thread mutex's are 
 * used.
 *
 * Note that with a "thread", which allows us to identify the opensrf session,
 * the caller does not need to provide a recipient address.  The "service" is
 * only required to start a new opensrf session.  After the sesession is 
 * started, all future communication is based solely on the thread.  
 *
 * We use jsonParseRaw and jsonObjectToJSONRaw since this service does not care 
 * about the contents of the messages.
 */

/**
 * TODO:
 * short-timeout mode for brick detachment where inactivity timeout drops way 
 * down for graceful disconnects.
 */

#include "httpd.h"
#include "http_log.h"
#include "http_log.h"
#include "apr_strings.h"
#include "apr_thread_proc.h"
#include "apr_hash.h"
#include "websocket_plugin.h"
#include "opensrf/osrf_json.h"
#include "opensrf/transport_client.h"
#include "opensrf/transport_message.h"
#include "opensrf/osrf_system.h"                                                
#include "opensrf/osrfConfig.h"

#define MAX_THREAD_SIZE 64
#define RECIP_BUF_SIZE 128
static char recipient_buf[RECIP_BUF_SIZE]; // reusable recipient buffer
static transport_client *osrf_handle = NULL;

typedef struct _osrfWebsocketTranslator {
    const WebSocketServer *server;
    apr_pool_t *main_pool; // standline per-process pool
    apr_pool_t *session_pool; // child of trans->main_pool; per-session
    apr_hash_t *session_cache; 
    apr_thread_t *responder_thread;
    int client_connected;
    char* osrf_router;
    char* osrf_domain;
} osrfWebsocketTranslator;

static osrfWebsocketTranslator *trans = NULL;


/**
 * Responder thread main body.
 * Collects responses from the opensrf network and relays them to the 
 * websocket caller.
 */
void* APR_THREAD_FUNC osrf_responder_thread_main(apr_thread_t *thread, void *data) {

    request_rec *r = trans->server->request(trans->server);
    jsonObject *msg_wrapper;
    char *msg_string;

    while (1) {

        transport_message *msg = client_recv(osrf_handle, -1);
        if (!msg) continue; // early exit on interrupt
        
        // discard responses received after client disconnect
        if (!trans->client_connected) {
            ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, 
                "WS discarding response for thread=%s, xid=%s", 
                    msg->thread, msg->osrf_xid);
            message_free(msg);                                                         
            continue; 
        }

        ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, 
            "WS received opensrf response for thread=%s, xid=%s", 
                msg->thread, msg->osrf_xid);

        // build the wrapper object
        msg_wrapper = jsonNewObject(NULL);
        jsonObjectSetKey(msg_wrapper, "thread", jsonNewObject(msg->thread));
        jsonObjectSetKey(msg_wrapper, "log_xid", jsonNewObject(msg->osrf_xid));
        jsonObjectSetKey(msg_wrapper, "osrf_msg", jsonParseRaw(msg->body));

        if (msg->is_error) {
            ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, 
                "WS received jabber error message in response to thread=%s and xid=%s", 
                    msg->thread, msg->osrf_xid);
            jsonObjectSetKey(msg_wrapper, "transport_error", jsonNewBoolObject(1));
        }

        msg_string = jsonObjectToJSONRaw(msg_wrapper);

        // deliver the wrapped message json to the websocket client
        trans->server->send(trans->server, MESSAGE_TYPE_TEXT, 
            (unsigned char*) msg_string, strlen(msg_string));

        // capture the true message sender
        // TODO: this will grow to add one entry per client session.  
        // need a last-touched timeout mechanism to periodically remove old entries
        if (!apr_hash_get(trans->session_cache, msg->thread, APR_HASH_KEY_STRING)) {

            ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, 
                "WS caching sender thread=%s, sender=%s", msg->thread, msg->sender);

            apr_hash_set(trans->session_cache, 
                apr_pstrdup(trans->session_pool, msg->thread),
                APR_HASH_KEY_STRING, 
                apr_pstrdup(trans->session_pool, msg->sender));
        }

        free(msg_string);
        jsonObjectFree(msg_wrapper);
        message_free(msg);                                                         
    }

    return NULL;
}

/**
 * Allocate the session cache and create the responder thread
 */
int child_init(const WebSocketServer *server) {

    apr_pool_t *pool = NULL;                                                
    apr_thread_t *thread = NULL;
    apr_threadattr_t *thread_attr = NULL;
    request_rec *r = server->request(server);
        
    ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, "WS child_init");

    // osrf_handle will already be connected if this is not the first request
    // served by this process.
    if ( !(osrf_handle = osrfSystemGetTransportClient()) ) {
        char* config_file = "/openils/conf/opensrf_core.xml";
        char* config_ctx = "gateway"; //TODO config
        if (!osrfSystemBootstrapClientResc(config_file, config_ctx, "websocket")) {   
            ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r,                              
                "WS unable to bootstrap OpenSRF client with config %s", config_file); 
            return 1;
        }

        osrf_handle = osrfSystemGetTransportClient();
    }

    // create a standalone pool for our translator data
    if (apr_pool_create(&pool, NULL) != APR_SUCCESS) {
        ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, "WS Unable to create apr_pool");
        return 1;
    }


    // allocate our static translator instance
    trans = (osrfWebsocketTranslator*) 
        apr_palloc(pool, sizeof(osrfWebsocketTranslator));

    if (trans == NULL) {
        ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, "WS Unable to create translator");
        return 1;
    }

    trans->main_pool = pool;
    trans->server = server;
    trans->osrf_router = osrfConfigGetValue(NULL, "/router_name");                      
    trans->osrf_domain = osrfConfigGetValue(NULL, "/domain");

    // Create the responder thread.  Once created, it runs for the lifetime
    // of this process.
    if ( (apr_threadattr_create(&thread_attr, trans->main_pool) == APR_SUCCESS) &&
         (apr_threadattr_detach_set(thread_attr, 0) == APR_SUCCESS) &&
         (apr_thread_create(&thread, thread_attr, 
                osrf_responder_thread_main, trans, trans->main_pool) == APR_SUCCESS)) {

        trans->responder_thread = thread;
        
    } else {
        ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, 
            "WS unable to create responder thread");
        return 1;
    }

    return APR_SUCCESS;
}

/**
 * Create the per-client translator
 */
void* CALLBACK on_connect_handler(const WebSocketServer *server) {
    request_rec *r = server->request(server);
    apr_pool_t *pool;

    ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, 
        "WS connect from %s", r->connection->remote_ip);

    if (!trans) {
        if (child_init(server) != APR_SUCCESS) {
            return NULL;
        }
    }

    // create a standalone pool for the session cache values, which will be
    // destroyed on client disconnect.
    if (apr_pool_create(&pool, trans->main_pool) != APR_SUCCESS) {
        ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, 
            "WS Unable to create apr_pool");
        return NULL;
    }

    trans->session_pool = pool;
    trans->session_cache = apr_hash_make(trans->session_pool);

    ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, 
        "WS created new pool %x", trans->session_pool);

    if (trans->session_cache == NULL) {
        ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, 
            "WS unable to create session cache");
        return NULL;
    }

    ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, 
        "WS created new hash %x", trans->session_cache);

    trans->client_connected = 1;
    return trans;
}



/**
 * Parse opensrf request and relay the request to the opensrf network.
 */
static size_t CALLBACK on_message_handler(void *data,
                const WebSocketServer *server, const int type, 
                unsigned char *buffer, const size_t buffer_size) {

    request_rec *r = server->request(server);

    jsonObject *msg_wrapper = NULL; // free me
    const jsonObject *tmp_obj = NULL;
    const jsonObject *osrf_msg = NULL;
    const char *service = NULL;
    const char *thread = NULL;
    const char *log_xid = NULL;
    char *msg_body = NULL;
    char *recipient = NULL;

    if (buffer_size <= 0) return OK;

    ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, 
        "WS received message size=%d", buffer_size);

    // buffer may not be \0-terminated, which jsonParse requires
    char buf[buffer_size + 1];
    memcpy(buf, buffer, buffer_size);
    buf[buffer_size] = '\0';

    msg_wrapper = jsonParseRaw(buf);

    if (msg_wrapper == NULL) {
        ap_log_rerror(APLOG_MARK, 
            APLOG_NOTICE, 0, r, "WS Invalid JSON: %s", buf);
        return HTTP_BAD_REQUEST;
    }

    osrf_msg = jsonObjectGetKeyConst(msg_wrapper, "osrf_msg");

    if (tmp_obj = jsonObjectGetKeyConst(msg_wrapper, "service")) 
        service = jsonObjectGetString(tmp_obj);

    if (tmp_obj = jsonObjectGetKeyConst(msg_wrapper, "thread")) 
        thread = jsonObjectGetString(tmp_obj);

    if (tmp_obj = jsonObjectGetKeyConst(msg_wrapper, "log_xid")) 
        log_xid = jsonObjectGetString(tmp_obj);

    if (log_xid) {
        // use the caller-provide log trace id
        if (strlen(log_xid) > MAX_THREAD_SIZE) {
            ap_log_rerror(APLOG_MARK, APLOG_NOTICE, 
                0, r, "WS log_xid exceeds max length");
            return HTTP_BAD_REQUEST;
        }
        osrfLogSetXid(log_xid); // TODO: make with with non-client
    } else {
        // generate a new log trace id for this relay
        osrfLogMkXid();
    }

    if (thread) {

        if (strlen(thread) > MAX_THREAD_SIZE) {
            ap_log_rerror(APLOG_MARK, APLOG_NOTICE, 
                0, r, "WS thread exceeds max length");
            return HTTP_BAD_REQUEST;
        }

        // since clients can provide their own threads at session start time,
        // the presence of a thread does not guarantee a cached recipient
        recipient = (char*) apr_hash_get(
            trans->session_cache, thread, APR_HASH_KEY_STRING);

        if (recipient) {
            ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, 
                "WS found cached recipient %s", recipient);
        }
    }

    if (!recipient) {

        if (service) {
            int size = snprintf(recipient_buf, RECIP_BUF_SIZE - 1,
                "%s@%s/%s", trans->osrf_router, trans->osrf_domain, service);                                    
            recipient_buf[size] = '\0';                                          
            recipient = recipient_buf;

        } else {
            ap_log_rerror(APLOG_MARK, APLOG_NOTICE, 
                0, r, "WS Unable to determine recipient");
            return HTTP_BAD_REQUEST;
        }
    }

    // TODO: activity log entry? -- requires message analysis
    ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, 
        "WS relaying message thread=%s, xid=%s, recipient=%s", 
            thread, osrfLogGetXid(), recipient);

    msg_body = jsonObjectToJSONRaw(osrf_msg);

    transport_message *tmsg = message_init(
        msg_body, NULL, thread, recipient, NULL);

    message_set_osrf_xid(tmsg, osrfLogGetXid());                                
    client_send_message(osrf_handle, tmsg);                                   
    osrfLogClearXid();

    message_free(tmsg);                                                         
    free(msg_wrapper);
    free(msg_body);

    return OK;
}


/**
 * Release all memory allocated from the translator pool and kill the pool.
 */
void CALLBACK on_disconnect_handler(
    void *data, const WebSocketServer *server) {

    osrfWebsocketTranslator *trans = (osrfWebsocketTranslator*) data;
    trans->client_connected = 0;

    apr_hash_clear(trans->session_cache);
    apr_pool_destroy(trans->session_pool);
    trans->session_pool = NULL;
    trans->session_cache = NULL;

    request_rec *r = server->request(server);
    ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, 
        "WS disconnect from %s", r->connection->remote_ip);
}

void CALLBACK on_destroy_handler(WebSocketPlugin *plugin) {
    fprintf(stderr, "WS on_destroy_handler()\n");
    fflush(stderr);

    if (trans) {
        apr_thread_exit(trans->responder_thread, APR_SUCCESS);
        apr_pool_destroy(trans->main_pool);
    }

    trans = NULL;
}

static WebSocketPlugin osrf_websocket_plugin = {
    sizeof(WebSocketPlugin),
    WEBSOCKET_PLUGIN_VERSION_0,
    on_destroy_handler,
    on_connect_handler,
    on_message_handler,
    on_disconnect_handler
};

extern EXPORT WebSocketPlugin * CALLBACK osrf_websocket_init() {
    return &osrf_websocket_plugin;
}

