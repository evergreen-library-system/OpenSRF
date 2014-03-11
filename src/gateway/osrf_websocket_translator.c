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
 * websocket <-> opensrf gateway.  Wrapped opensrf messages are extracted
 * and relayed to the opensrf network.  Responses are pulled from the opensrf
 * network and passed back to the client.  Messages are analyzed to determine
 * when a connect/disconnect occurs, so that the cache of recipients can be
 * properly managed.  We also activity-log REQUEST messages.
 *
 * Messages to/from the websocket client take the following form:
 * {
 *   "service"  : "opensrf.foo", // required
 *   "thread"   : "123454321",   // AKA thread. required for follow-up requests; max 64 chars.
 *   "log_xid"  : "123..32",     // optional log trace ID, max 64 chars;
 *   "osrf_msg" : [<osrf_msg>, <osrf_msg>, ...]   // required
 * }
 *
 * Each translator operates with three threads.  One thread receives messages
 * from the websocket client, translates, and relays them to the opensrf 
 * network. The second thread collects responses from the opensrf network and 
 * relays them back to the websocket client.  The third thread inspects
 * the idle timeout interval t see if it's time to drop the idle client.
 *
 * After the initial setup, all thread actions occur within a thread
 * mutex.  The desired affect is a non-threaded application that uses
 * threads for the sole purpose of having one thread listening for
 * incoming data, while a second thread listens for responses, and a
 * third checks the idle timeout.  When any thread awakens, it's the
 * only thread in town until it goes back to sleep (i.e. listening on
 * its socket for data).
 *
 * Note that with the opensrf "thread", which allows us to identify the
 * opensrf session, the caller does not need to provide a recipient
 * address.  The "service" is only required to start a new opensrf
 * session.  After the sesession is started, all future communication is
 * based solely on the thread.  However, the "service" should be passed
 * by the caller for all requests to ensure it is properly logged in the
 * activity log.
 *
 * Every inbound and outbound message updates the last_activity_time.
 * A separate thread wakes periodically to see if the time since the
 * last_activity_time exceeds the configured idle_timeout_interval.  If
 * so, a disconnect is sent to the client, completing the conversation.
 *
 * Configuration goes directly into the Apache envvars file.  
 * (e.g. /etc/apache2-websockets/envvars).  As of today, it's not 
 * possible to leverage Apache configuration directives directly,
 * since this is not an Apache module, but a shared library loaded
 * by an apache module.  This includes SetEnv / SetEnvIf.
 *
 * export OSRF_WEBSOCKET_IDLE_TIMEOUT=300
 * export OSRF_WEBSOCKET_IDLE_CHECK_INTERVAL=5
 * export OSRF_WEBSOCKET_CONFIG_FILE=/openils/conf/opensrf_core.xml
 * export OSRF_WEBSOCKET_CONFIG_CTXT=gateway
 */

#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include "httpd.h"
#include "http_log.h"
#include "apr_strings.h"
#include "apr_thread_proc.h"
#include "apr_hash.h"
#include "websocket_plugin.h"
#include "opensrf/log.h"
#include "opensrf/osrf_json.h"
#include "opensrf/transport_client.h"
#include "opensrf/transport_message.h"
#include "opensrf/osrf_system.h"                                                
#include "opensrf/osrfConfig.h"

#define MAX_THREAD_SIZE 64
#define RECIP_BUF_SIZE 256
#define WEBSOCKET_TRANSLATOR_INGRESS "ws-translator-v1"

// maximun number of active, CONNECTed opensrf sessions allowed. in
// practice, this number will be very small, rarely reaching double
// digits.  This is just a security back-stop.  A client trying to open
// this many connections is almost certainly attempting to DOS the
// gateway / server.  We may want to lower this further.
#define MAX_ACTIVE_STATEFUL_SESSIONS 128

// default values, replaced during setup (below) as needed.
static char* config_file = "/openils/conf/opensrf_core.xml";
static char* config_ctxt = "gateway";

static time_t idle_timeout_interval = 120; 
static time_t idle_check_interval = 5;
static time_t last_activity_time = 0;

// Generally, we do not disconnect the client (as idle) if there is a
// request in flight.  However, we need to have an upper bound on the
// amount of time we will wait for in-flight requests to complete to
// avoid leaving an effectively idle connection open after a request
// died on the backend and no response was received.
// Note that if other activity occurs while a long-running request
// is active, the wait time will get reset with each new activity. 
// This is OK, though, because the goal of max_request_wait_time
// is not to chop requests off at the knees, it's to allow the client
// to timeout as idle when only a single long-running request is active
// and preventing timeout.
static time_t max_request_wait_time = 600;

// Incremented with every REQUEST, decremented with every COMPLETE.
// Gives us a rough picture of the number of reqests we've sent to 
// the server vs. the number for which a completed response has been 
// received.
static int requests_in_flight = 0;

// true if we've received a signal to start graceful shutdown
static int shutdown_requested = 0; 
static void sigusr1_handler(int sig);
static void sigusr1_handler(int sig) {                                       
    shutdown_requested = 1; 
    signal(SIGUSR1, sigusr1_handler);
    osrfLogInfo(OSRF_LOG_MARK, "WS received SIGUSR1 - Graceful Shutdown");
}

static const char* get_client_ip(const request_rec* r) {
#ifdef APACHE_MIN_24
    return r->connection->client_ip;
#else
    return r->connection->remote_ip;
#endif
}

typedef struct _osrfWebsocketTranslator {

    /** Our handle for communicating with the caller */
    const WebSocketServer *server;
    
    /**
     * Standalone, per-process APR pool.  Primarily
     * there for managing thread data, which lasts 
     * the duration of the process.
     */
    apr_pool_t *main_pool;

    /**
     * Map of thread => drone-xmpp-address.  Maintaining this
     * map internally means the caller never need know about
     * internal XMPP addresses and the server doesn't have to 
     * verify caller-specified recipient addresses.  It's
     * all managed internally.  This is only used for stateful
     * (CONNECT'ed) session.  Stateless sessions need not 
     * track the recipient, since they are one-off calls.
     */
    apr_hash_t *stateful_session_cache; 

    /**
     * stateful_session_pool contains the key/value pairs stored in
     * the stateful_session_cache.  The pool is regularly destroyed
     * and re-created to avoid long-term memory consumption
     */
    apr_pool_t *stateful_session_pool;

    /**
     * Thread responsible for collecting responses on the opensrf
     * network and relaying them back to the caller
     */
    apr_thread_t *responder_thread;

    /**
     * Thread responsible for checking inactivity timeout.
     * If no activitity occurs within the configured interval,
     * a disconnect is sent to the client and the connection
     * is terminated.
     */
    apr_thread_t *idle_timeout_thread;

    /**
     * All message handling code is wrapped in a thread mutex such
     * that all actions (after the initial setup) are serialized
     * to minimize the possibility of multi-threading snafus.
     */
    apr_thread_mutex_t *mutex;

    /**
     * True if a websocket client is currently connected
     */
    int client_connected;

    /** OpenSRF jouter name */
    char* osrf_router;

    /** OpenSRF domain */
    char* osrf_domain;

} osrfWebsocketTranslator;

static osrfWebsocketTranslator *trans = NULL;
static transport_client *osrf_handle = NULL;
static char recipient_buf[RECIP_BUF_SIZE]; // reusable recipient buffer

static void clear_cached_recipient(const char* thread) {
    apr_pool_t *pool = NULL;                                                
    request_rec *r = trans->server->request(trans->server);

    if (apr_hash_get(trans->stateful_session_cache, thread, APR_HASH_KEY_STRING)) {

        osrfLogDebug(OSRF_LOG_MARK, "WS removing cached recipient on disconnect");

        // remove it from the hash
        apr_hash_set(trans->stateful_session_cache, thread, APR_HASH_KEY_STRING, NULL);

        if (apr_hash_count(trans->stateful_session_cache) == 0) {
            osrfLogDebug(OSRF_LOG_MARK, "WS re-setting stateful_session_pool");

            // memory accumulates in the stateful_session_pool as
            // sessions are cached then un-cached.  Un-caching removes
            // strings from the hash, but not from the pool.  Clear the
            // pool here. note: apr_pool_clear does not free memory, it
            // reclaims it for use again within the pool.  This is more
            // effecient than freeing and allocating every time.
            apr_pool_clear(trans->stateful_session_pool);
        }
    }
}

void* osrf_responder_thread_main_body(transport_message *tmsg) {

    osrfList *msg_list = NULL;
    osrfMessage *one_msg = NULL;
    int i;

    osrfLogDebug(OSRF_LOG_MARK, 
        "WS received opensrf response for thread=%s", tmsg->thread);

    // first we need to perform some maintenance
    msg_list = osrfMessageDeserialize(tmsg->body, NULL);

    for (i = 0; i < msg_list->size; i++) {
        one_msg = OSRF_LIST_GET_INDEX(msg_list, i);

        osrfLogDebug(OSRF_LOG_MARK, 
            "WS returned response of type %d", one_msg->m_type);

        /*  if our client just successfully connected to an opensrf service,
            cache the sender so that future calls on this thread will use
            the correct recipient. */
        if (one_msg && one_msg->m_type == STATUS) {

            if (one_msg->status_code == OSRF_STATUS_OK) {

                if (!apr_hash_get(trans->stateful_session_cache, 
                        tmsg->thread, APR_HASH_KEY_STRING)) {

                    apr_size_t ses_size = 
                        apr_hash_count(trans->stateful_session_cache);

                    if (ses_size < MAX_ACTIVE_STATEFUL_SESSIONS) {

                        osrfLogDebug(OSRF_LOG_MARK, "WS caching sender "
                            "thread=%s, sender=%s; concurrent=%d", 
                            tmsg->thread, tmsg->sender, ses_size);

                        apr_hash_set(trans->stateful_session_cache, 
                            apr_pstrdup(trans->stateful_session_pool, tmsg->thread),
                            APR_HASH_KEY_STRING, 
                            apr_pstrdup(trans->stateful_session_pool, tmsg->sender));

                    } else {
                        osrfLogWarning(OSRF_LOG_MARK, 
                            "WS max concurrent sessions (%d) reached.  "
                            "Current session will not be tracked",
                            MAX_ACTIVE_STATEFUL_SESSIONS
                        );
                    }
                }

            } else {

                // connection timed out; clear the cached recipient
                if (one_msg->status_code == OSRF_STATUS_TIMEOUT) {
                    clear_cached_recipient(tmsg->thread);

                } else {
                    if (one_msg->status_code == OSRF_STATUS_COMPLETE)
                        requests_in_flight--;
                }
            }
        }
    }

    // osrfMessageDeserialize applies the freeItem handler to the 
    // newly created osrfList.  We only need to free the list and 
    // the individual osrfMessage's will be freed along with it
    osrfListFree(msg_list);
    
    // relay the response messages to the client
    jsonObject *msg_wrapper = NULL;
    char *msg_string = NULL;

    // build the wrapper object
    msg_wrapper = jsonNewObject(NULL);
    jsonObjectSetKey(msg_wrapper, "thread", jsonNewObject(tmsg->thread));
    jsonObjectSetKey(msg_wrapper, "log_xid", jsonNewObject(tmsg->osrf_xid));
    jsonObjectSetKey(msg_wrapper, "osrf_msg", jsonParseRaw(tmsg->body));

    if (tmsg->is_error) {
        osrfLogError(OSRF_LOG_MARK, 
            "WS received jabber error message in response to thread=%s", 
            tmsg->thread);
        jsonObjectSetKey(msg_wrapper, "transport_error", jsonNewBoolObject(1));
    }

    msg_string = jsonObjectToJSONRaw(msg_wrapper);

    // drop the JSON on the outbound wire
    trans->server->send(trans->server, MESSAGE_TYPE_TEXT, 
        (unsigned char*) msg_string, strlen(msg_string));

    free(msg_string);
    jsonObjectFree(msg_wrapper);
}

/**
 * Responder thread main body.
 * Collects responses from the opensrf network and relays them to the 
 * websocket caller.
 */
void* APR_THREAD_FUNC osrf_responder_thread_main(apr_thread_t *thread, void *data) {

    transport_message *tmsg;
    while (1) {

        if (apr_thread_mutex_unlock(trans->mutex) != APR_SUCCESS) {
            osrfLogError(OSRF_LOG_MARK, "WS error un-locking thread mutex");
            return NULL;
        }

        // wait for a response
        tmsg = client_recv(osrf_handle, -1);

        if (!tmsg) continue; // interrupt

        if (trans->client_connected) {

            if (apr_thread_mutex_lock(trans->mutex) != APR_SUCCESS) {
                osrfLogError(OSRF_LOG_MARK, "WS error locking thread mutex");
                return NULL;
            }

            osrfLogForceXid(tmsg->osrf_xid);
            osrf_responder_thread_main_body(tmsg);
            last_activity_time = time(NULL);
        }

        message_free(tmsg);                                                         
    }

    return NULL;
}

static int active_connection_count() {

    if (requests_in_flight) {

        time_t now = time(NULL);
        time_t difference = now - last_activity_time;

        if (difference >= max_request_wait_time) {
            osrfLogWarning(OSRF_LOG_MARK, 
                "%d In-flight request(s) took longer than %d seconds "
                "to complete.  Treating request as dead and moving on.",
                requests_in_flight, 
                max_request_wait_time
            );
            requests_in_flight = 0;
        }
    }

    return requests_in_flight;
}

/**
 * Sleep and regularly wake to see if the process has been idle for too
 * long.  If so, send a disconnect to the client.
 */
void* APR_THREAD_FUNC osrf_idle_timeout_thread_main(
        apr_thread_t *thread, void *data) {

    // sleep time defaults to the check interval, but may 
    // be shortened during shutdown.
    int sleep_time = idle_check_interval;
    int shutdown_loops = 0;

    while (1) {

        if (apr_thread_mutex_unlock(trans->mutex) != APR_SUCCESS) {
            osrfLogError(OSRF_LOG_MARK, "WS error un-locking thread mutex");
            return NULL;
        }

        // note: receiving a signal (e.g. SIGUSR1) will not interrupt
        // this sleep(), since it's running within its own thread.
        // During graceful shtudown, we may wait up to 
        // idle_check_interval seconds before initiating shutdown.
        sleep(sleep_time);

        if (apr_thread_mutex_lock(trans->mutex) != APR_SUCCESS) {
            osrfLogError(OSRF_LOG_MARK, "WS error locking thread mutex");
            return NULL;
        }

        // no client is connected.  reset sleep time go back to sleep.
        if (!trans->client_connected) {
            sleep_time = idle_check_interval;
            continue;
        }

        // do we have any active stateful conversations with the client?
        int active_count = active_connection_count();

        if (active_count) {

            if (shutdown_requested) {
                // active conversations means we can't shut down.  
                // shorten the check interval to re-check more often.
                shutdown_loops++;
                osrfLogDebug(OSRF_LOG_MARK, 
                    "WS: %d active conversation(s) found in shutdown after "
                    "%d attempts.  Sleeping...", shutdown_loops, active_count
                );

                if (shutdown_loops > 30) {
                    // this is clearly a long-running conversation, let's
                    // check less frequently to avoid excessive logging.
                    sleep_time = 3;
                } else {
                    sleep_time = 1;
                }
            } 

            // active conversations means keep going.  There's no point in
            // checking the idle time (below) if we're mid-conversation
            continue;
        }

        // no active conversations
             
        if (shutdown_requested) {
            // there's no need to reset the shutdown vars (loops/requested)
            // SIGUSR1 is Apaches reload signal, which means this process
            // will be going away as soon as the client is disconnected.

            osrfLogInfo(OSRF_LOG_MARK,
                "WS: no active conversations remain in shutdown; "
                    "closing client connection");

        } else { 
            // see how long we've been idle.  If too long, kick the client

            time_t now = time(NULL);
            time_t difference = now - last_activity_time;

            osrfLogDebug(OSRF_LOG_MARK, 
                "WS connection idle for %d seconds", difference);

            if (difference < idle_timeout_interval) {
                // Last activity occurred within the idle timeout interval.
                continue;
            }

            // idle timeout exceeded
            osrfLogDebug(OSRF_LOG_MARK, 
                "WS: idle timeout exceeded.  now=%d / last=%d; " 
                "closing client connection", now, last_activity_time);
        }


        // send a disconnect to the client, which will come back around
        // to cause our on_disconnect_handler to run.
        osrfLogDebug(OSRF_LOG_MARK, "WS: sending close() to client");
        trans->server->close(trans->server);

        // client will be going away, reset sleep time
        sleep_time = idle_check_interval;
    }

    // should never get here
    return NULL;
}

static int build_startup_data(const WebSocketServer *server) {

    apr_pool_t *main_pool = NULL;                                                
    apr_pool_t *stateful_session_pool = NULL;                                                
    apr_thread_t *thread = NULL;
    apr_threadattr_t *thread_attr = NULL;
    apr_thread_mutex_t *mutex = NULL;
    request_rec *r = server->request(server);

    // create a pool for our translator data
    // Do not use r->pool as the parent, since r->pool will be freed
    // when the current client disconnects.
    if (apr_pool_create(&main_pool, NULL) != APR_SUCCESS) {
        osrfLogError(OSRF_LOG_MARK, "WS Unable to create apr_pool");
        return 1;
    }

    trans = (osrfWebsocketTranslator*) 
        apr_palloc(main_pool, sizeof(osrfWebsocketTranslator));

    if (trans == NULL) {
        osrfLogError(OSRF_LOG_MARK, "WS Unable to create translator");
        return 1;
    }

    trans->server = server;
    trans->main_pool = main_pool;
    trans->osrf_router = osrfConfigGetValue(NULL, "/router_name");                      
    trans->osrf_domain = osrfConfigGetValue(NULL, "/domain");

    // opensrf session / recipient cache
    trans->stateful_session_cache = apr_hash_make(trans->main_pool);
    if (trans->stateful_session_cache == NULL) {
        osrfLogError(OSRF_LOG_MARK, "WS unable to create session cache");
        return 1;
    }

    // opensrf session / recipient string pool; cleared regularly
    // the only data entering this pools are the session strings.
    if (apr_pool_create(&stateful_session_pool, trans->main_pool) != APR_SUCCESS) {
        osrfLogError(OSRF_LOG_MARK, "WS Unable to create apr_pool");
        return NULL;
    }
    trans->stateful_session_pool = stateful_session_pool;

    if (apr_thread_mutex_create(
            &mutex, APR_THREAD_MUTEX_UNNESTED, 
            trans->main_pool) != APR_SUCCESS) {
        osrfLogError(OSRF_LOG_MARK, "WS unable to create thread mutex");
        return 1;
    }
    trans->mutex = mutex;

    // responder thread
    if ( (apr_threadattr_create(&thread_attr, trans->main_pool) == APR_SUCCESS) &&
         (apr_threadattr_detach_set(thread_attr, 0) == APR_SUCCESS) &&
         (apr_thread_create(&thread, thread_attr, 
                osrf_responder_thread_main, trans, trans->main_pool) == APR_SUCCESS)) {

        trans->responder_thread = thread;
        
    } else {
        osrfLogError(OSRF_LOG_MARK, "WS unable to create responder thread");
        return 1;
    }

    // idle timeout thread
    thread = NULL; // reset
    thread_attr = NULL; // reset
    if ( (apr_threadattr_create(&thread_attr, trans->main_pool) == APR_SUCCESS) &&
         (apr_threadattr_detach_set(thread_attr, 0) == APR_SUCCESS) &&
         (apr_thread_create(&thread, thread_attr, 
            osrf_idle_timeout_thread_main, trans, trans->main_pool) == APR_SUCCESS)) {

        osrfLogDebug(OSRF_LOG_MARK, "WS created idle timeout thread");
        trans->idle_timeout_thread = thread;
        
    } else {
        osrfLogError(OSRF_LOG_MARK, "WS unable to create idle timeout thread");
        return 1;
    }

    return APR_SUCCESS;
}


/**
 * Connect to OpenSRF, create the main pool, responder thread
 * session cache and session pool.
 */
int child_init(const WebSocketServer *server) {
    request_rec *r = server->request(server);

    // osrf_handle will already be connected if this is not the first request
    // served by this process.
    if ( !(osrf_handle = osrfSystemGetTransportClient()) ) {
        
        // load config values from the env
        char* timeout = getenv("OSRF_WEBSOCKET_IDLE_TIMEOUT");
        if (timeout) {
            if (!atoi(timeout)) {
                ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, 
                    "WS: invalid OSRF_WEBSOCKET_IDLE_TIMEOUT: %s", timeout);
            } else {
                idle_timeout_interval = (time_t) atoi(timeout);
            }
        }

        ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, 
            "WS: timeout set to %d", idle_timeout_interval);

        timeout = getenv("OSRF_WEBSOCKET_MAX_REQUEST_WAIT_TIME");
        if (timeout) {
            if (!atoi(timeout)) {
                ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, 
                    "WS: invalid OSRF_WEBSOCKET_MAX_REQUEST_WAIT_TIME: %s", 
                    timeout
                );
            } else {
                max_request_wait_time = (time_t) atoi(timeout);
            }
        }

        ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, 
            "WS: max request wait time set to %d", max_request_wait_time);

        char* interval = getenv("OSRF_WEBSOCKET_IDLE_CHECK_INTERVAL");
        if (interval) {
            if (!atoi(interval)) {
                ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, 
                    "WS: invalid OSRF_WEBSOCKET_IDLE_CHECK_INTERVAL: %s", 
                    interval
                );
            } else {
                idle_check_interval = (time_t) atoi(interval);
            }
        } 

        ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, 
            "WS: idle check interval set to %d", idle_check_interval);

      
        char* cfile = getenv("OSRF_WEBSOCKET_CONFIG_FILE");
        if (cfile) {
            ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r,
                "WS: config file set to %s", cfile);
            config_file = cfile;
        }

        char* ctxt = getenv("OSRF_WEBSOCKET_CONFIG_CTXT");
        if (ctxt) {
            ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, 
                "WS: config context set to %s", ctxt);
            config_ctxt = ctxt;
        }

        // connect to opensrf
        if (!osrfSystemBootstrapClientResc(
                config_file, config_ctxt, "websocket")) {   

            osrfLogError(OSRF_LOG_MARK, 
                "WS unable to bootstrap OpenSRF client with config %s "
                "and context %s", config_file, config_ctxt
            ); 
            return 1;
        }

        osrfLogSetAppname("osrf_websocket_translator");
        osrf_handle = osrfSystemGetTransportClient();
    }

    signal(SIGUSR1, sigusr1_handler);
    return APR_SUCCESS;
}

/**
 * Create the per-client translator
 */
void* CALLBACK on_connect_handler(const WebSocketServer *server) {
    request_rec *r = server->request(server);

    if (!trans) { // first connection

        // connect to opensrf
        if (child_init(server) != APR_SUCCESS)
            return NULL;

        // build pools, thread data, and the translator
        if (build_startup_data(server) != APR_SUCCESS)
            return NULL;
    }

    const char* client_ip = get_client_ip(r);
    osrfLogInfo(OSRF_LOG_MARK, "WS connect from %s", client_ip);

    last_activity_time = time(NULL);
    trans->client_connected = 1;
    return trans;
}


/** 
 * for each inbound opensrf message:
 * 1. Stamp the ingress
 * 2. REQUEST: log it as activity
 * 3. DISCONNECT: remove the cached recipient
 * then re-string-ify for xmpp delivery
 */

static char* extract_inbound_messages(
        const request_rec *r, 
        const char* service, 
        const char* thread, 
        const char* recipient, 
        const jsonObject *osrf_msg) {

    int i;
    int num_msgs = osrf_msg->size;
    osrfMessage* msg;
    osrfMessage* msg_list[num_msgs];

    // here we do an extra json round-trip to get the data
    // in a form osrf_message_deserialize can understand
    // TODO: consider a version of osrf_message_init which can 
    // accept a jsonObject* instead of a JSON string.
    char *osrf_msg_json = jsonObjectToJSON(osrf_msg);
    osrf_message_deserialize(osrf_msg_json, msg_list, num_msgs);
    free(osrf_msg_json);

    // should we require the caller to always pass the service?
    if (service == NULL) service = "";

    for(i = 0; i < num_msgs; i++) {
        msg = msg_list[i];
        osrfMessageSetIngress(msg, WEBSOCKET_TRANSLATOR_INGRESS);

        switch(msg->m_type) {

            case REQUEST: {
                const jsonObject* params = msg->_params;
                growing_buffer* act = buffer_init(128);
                char* method = msg->method_name;
                buffer_fadd(act, "[%s] [%s] %s %s", 
                    get_client_ip(r), "", service, method);

                const jsonObject* obj = NULL;
                int i = 0;
                const char* str;
                int redactParams = 0;
                while( (str = osrfStringArrayGetString(log_protect_arr, i++)) ) {
                    if(!strncmp(method, str, strlen(str))) {
                        redactParams = 1;
                        break;
                    }
                }
                if(redactParams) {
                    OSRF_BUFFER_ADD(act, " **PARAMS REDACTED**");
                } else {
                    i = 0;
                    while((obj = jsonObjectGetIndex(params, i++))) {
                        char* str = jsonObjectToJSON(obj);
                        if( i == 1 )
                            OSRF_BUFFER_ADD(act, " ");
                        else
                            OSRF_BUFFER_ADD(act, ", ");
                        OSRF_BUFFER_ADD(act, str);
                        free(str);
                    }
                }
                osrfLogActivity(OSRF_LOG_MARK, "%s", act->buf);
                buffer_free(act);
                requests_in_flight++;
                break;
            }

            case DISCONNECT:
                clear_cached_recipient(thread);
                break;
        }
    }

    char* finalMsg = osrfMessageSerializeBatch(msg_list, num_msgs);

    // clean up our messages
    for(i = 0; i < num_msgs; i++) 
        osrfMessageFree(msg_list[i]);

    return finalMsg;
}

/**
 * Parse opensrf request and relay the request to the opensrf network.
 */
static size_t on_message_handler_body(void *data,
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
    int i;

    if (buffer_size <= 0) return OK;

    // generate a new log trace for this request. it 
    // may be replaced by a client-provided trace below.
    osrfLogMkXid();

    osrfLogDebug(OSRF_LOG_MARK, "WS received message size=%d", buffer_size);

    // buffer may not be \0-terminated, which jsonParse requires
    char buf[buffer_size + 1];
    memcpy(buf, buffer, buffer_size);
    buf[buffer_size] = '\0';

    osrfLogInternal(OSRF_LOG_MARK, "WS received inbound message: %s", buf);

    msg_wrapper = jsonParse(buf);

    if (msg_wrapper == NULL) {
        osrfLogWarning(OSRF_LOG_MARK, "WS Invalid JSON: %s", buf);
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
            osrfLogWarning(OSRF_LOG_MARK, "WS log_xid exceeds max length");
            return HTTP_BAD_REQUEST;
        }

        osrfLogForceXid(log_xid);
    }

    if (thread) {

        if (strlen(thread) > MAX_THREAD_SIZE) {
            osrfLogWarning(OSRF_LOG_MARK, "WS thread exceeds max length");
            return HTTP_BAD_REQUEST;
        }

        // since clients can provide their own threads at session start time,
        // the presence of a thread does not guarantee a cached recipient
        recipient = (char*) apr_hash_get(
            trans->stateful_session_cache, thread, APR_HASH_KEY_STRING);

        if (recipient) {
            osrfLogDebug(OSRF_LOG_MARK, "WS found cached recipient %s", recipient);
        }
    }

    if (!recipient) {

        if (service) {
            int size = snprintf(recipient_buf, RECIP_BUF_SIZE - 1,
                "%s@%s/%s", trans->osrf_router, trans->osrf_domain, service);                                    
            recipient_buf[size] = '\0';                                          
            recipient = recipient_buf;

        } else {
            osrfLogWarning(OSRF_LOG_MARK, "WS Unable to determine recipient");
            return HTTP_BAD_REQUEST;
        }
    }

    osrfLogDebug(OSRF_LOG_MARK, 
        "WS relaying message to opensrf thread=%s, recipient=%s", 
            thread, recipient);

    msg_body = extract_inbound_messages(
        r, service, thread, recipient, osrf_msg);

    osrfLogInternal(OSRF_LOG_MARK, 
        "WS relaying inbound message: %s", msg_body);

    transport_message *tmsg = message_init(
        msg_body, NULL, thread, recipient, NULL);

    message_set_osrf_xid(tmsg, osrfLogGetXid());
    client_send_message(osrf_handle, tmsg);


    osrfLogClearXid();
    message_free(tmsg);                                                         
    jsonObjectFree(msg_wrapper);
    free(msg_body);

    last_activity_time = time(NULL);
    return OK;
}

static size_t CALLBACK on_message_handler(void *data,
                const WebSocketServer *server, const int type, 
                unsigned char *buffer, const size_t buffer_size) {

    if (apr_thread_mutex_lock(trans->mutex) != APR_SUCCESS) {
        osrfLogError(OSRF_LOG_MARK, "WS error locking thread mutex");
        return 1; // TODO: map to apr_status_t value?
    }

    apr_status_t stat = on_message_handler_body(data, server, type, buffer, buffer_size);

    if (apr_thread_mutex_unlock(trans->mutex) != APR_SUCCESS) {
        osrfLogError(OSRF_LOG_MARK, "WS error locking thread mutex");
        return 1;
    }

    return stat;
}


/**
 * Clear the session cache, release the session pool
 */
void CALLBACK on_disconnect_handler(
    void *data, const WebSocketServer *server) {

    // if the threads wake up during disconnect, this tells 
    // them to go back to sleep.
    trans->client_connected = 0;

    request_rec *r = server->request(server);
    osrfLogInfo(OSRF_LOG_MARK, "WS disconnect from %s", get_client_ip(r)); 

    // Clear any lingering session data
    // NOTE: we could apr_pool_destroy the stateful_session_pool to truly free
    // the memory, but since there is a limit to the size of the pool
    // (max_concurrent_sessions), the memory cannot grow unbounded, 
    // so there's no need.
    apr_hash_clear(trans->stateful_session_cache);
    apr_pool_clear(trans->stateful_session_pool);
}

static WebSocketPlugin osrf_websocket_plugin = {
    sizeof(WebSocketPlugin),
    WEBSOCKET_PLUGIN_VERSION_0,
    NULL, // on_destroy_handler
    on_connect_handler,
    on_message_handler,
    on_disconnect_handler
};

extern EXPORT WebSocketPlugin * CALLBACK osrf_websocket_init() {
    return &osrf_websocket_plugin;
}

