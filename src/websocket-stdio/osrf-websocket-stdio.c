/* --------------------------------------------------------------------
 * Copyright (C) 2018 King County Library Service
 * Bill Erickson <berickxx@gmail.com>
 *
 * Code borrows heavily from osrf_websocket_translator.c
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
--------------------------------------------------------------------- */

/**
 * OpenSRF Websockets Relay
 *
 * Reads Websockets requests on STDIN
 * Sends replies to requests on STDOUT
 *
 * Built to function with websocketd:
 * https://github.com/joewalnes/websocketd
 *
 * Synopsis:
 *
 * websocketd --port 7682 --max-forks 250 ./osrf-websocket-stdio /path/to/opensrf_core.xml &
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <opensrf/utils.h>
#include <opensrf/osrf_hash.h>
#include <opensrf/transport_client.h>
#include <opensrf/osrf_message.h>
#include <opensrf/osrf_app_session.h>
#include <opensrf/log.h>

#define MAX_THREAD_SIZE 64
#define RECIP_BUF_SIZE 256
#define WEBSOCKET_INGRESS "ws-translator-v2"

// maximun number of active, CONNECTed opensrf sessions allowed. in
// practice, this number will be very small, rarely reaching double
// digits.  This is just a security back-stop.  A client trying to open
// this many connections is almost certainly attempting to DOS the
// gateway / server.
#define MAX_ACTIVE_STATEFUL_SESSIONS 64

// Message exceeding this size are discarded.
// This value must be greater than RESET_MESSAGE_SIZE (below)
// ~10M
#define MAX_MESSAGE_SIZE 10485760

// After processing any message this size or larger, free and
// recreate the stdin buffer to release the memory.
// ~100k
#define RESET_MESSAGE_SIZE 102400

// default values, replaced during setup (below) as needed.
static char* config_file = "/openils/conf/opensrf_core.xml";
static char* config_ctxt = "gateway";
static char* osrf_router = NULL;
static char* osrf_domain = NULL;

// Cache of opensrf thread strings and back-end receipients.
// Tracking this here means the caller only needs to track the thread.
// It also means we don't have to expose internal XMPP IDs
static osrfHash* stateful_session_cache = NULL;
// Message on STDIN go into our reusable buffer
static growing_buffer* stdin_buf = NULL;
// OpenSRF XMPP connection handle
static transport_client* osrf_handle = NULL;
// Reusable string buf for recipient addresses
static char recipient_buf[RECIP_BUF_SIZE];
// Websocket client IP address (for logging)
static char* client_ip = NULL;

static void rebuild_stdin_buffer();
static void child_init(int argc, char* argv[]);
static void read_from_stdin();
static void relay_stdin_message(const char*);
static char* extract_inbound_messages();
static void log_request(const char*, osrfMessage*);
static void read_from_osrf();
static void read_one_osrf_message(transport_message*);
static int shut_it_down(int);
static void release_hash_string(char*, void*);

// Websocketd sends SIGINT for shutdown, followed by SIGTERM
// if SIGINT takes too long.
static void sigint_handler(int sig) {
    osrfLogInfo(OSRF_LOG_MARK, "WS received SIGINT - graceful shutdown");
    shut_it_down(0);
}

int main(int argc, char* argv[]) {

    // Handle shutdown signal -- only needed once.
    signal(SIGINT, sigint_handler);

    // Connect to OpenSR -- exits on error
    child_init(argc, argv);

    // Disable output buffering.
    setbuf(stdout, NULL);
    rebuild_stdin_buffer();

    // The main loop waits for data to be available on both STDIN
    // (websocket client request) and the OpenSRF XMPP socket 
    // (replies returning to the websocket client).
    fd_set fds;
    int stdin_no = fileno(stdin);
    int osrf_no = osrf_handle->session->sock_id;
    int maxfd = osrf_no > stdin_no ? osrf_no : stdin_no;
    int sel_resp;

    while (1) {

        FD_ZERO(&fds);
        FD_SET(osrf_no, &fds);
        FD_SET(stdin_no, &fds);

        // Wait indefinitely for activity to process
        sel_resp = select(maxfd + 1, &fds, NULL, NULL, NULL);

        if (sel_resp < 0) { // error

            if (errno == EINTR) {
                // Interrupted by a signal.  Start the loop over.
                continue;
            }

            osrfLogError(OSRF_LOG_MARK,
                "WS select() failed with [%s]. Exiting", strerror(errno));

            shut_it_down(1);
        }

        if (FD_ISSET(stdin_no, &fds)) {
            read_from_stdin();
        }

        if (FD_ISSET(osrf_no, &fds)) {
            read_from_osrf();
        }
    }

    return shut_it_down(0);
}

static void rebuild_stdin_buffer() {

    if (stdin_buf != NULL) {
        buffer_free(stdin_buf);
    }

    stdin_buf = buffer_init(1024);
}

static int shut_it_down(int stat) {
    osrfHashFree(stateful_session_cache);
    buffer_free(stdin_buf);
    osrf_system_shutdown(); // clean XMPP disconnect
    exit(stat);
    return stat;
}


// Connect to OpenSRF/XMPP
// Apply settings and command line args.
static void child_init(int argc, char* argv[]) {

    if (argc > 1) {
        config_file = argv[1];
    }

    if (!osrf_system_bootstrap_client(config_file, config_ctxt) ) {
        fprintf(stderr, "Cannot boostrap OSRF\n");
        shut_it_down(1);
    }

	osrf_handle = osrfSystemGetTransportClient();
	osrfAppSessionSetIngress(WEBSOCKET_INGRESS);

    osrf_router = osrfConfigGetValue(NULL, "/router_name");
    osrf_domain = osrfConfigGetValue(NULL, "/domain");

    stateful_session_cache = osrfNewHash();
    osrfHashSetCallback(stateful_session_cache, release_hash_string);

    client_ip = getenv("REMOTE_ADDR");
    osrfLogInfo(OSRF_LOG_MARK, "WS connect from %s", client_ip);
}

// Called by osrfHash when a string is removed.  We strdup each
// string before it goes into the hash.
static void release_hash_string(char* key, void* str) {
    if (str == NULL) return;
    free((char*) str);
}


// Relay websocket client messages from STDIN to OpenSRF.  Reads one
// message then returns, allowing responses to intermingle with long
// series of requests.
static void read_from_stdin() {
    char char_buf[1];
    char c;

    // Read one char at a time so we can stop at the first newline
    // and leave any other data on the wire until read_from_stdin()
    // is called again.

    while (1) {
        int stat = read(fileno(stdin), char_buf, 1);

        if (stat < 0) {

            if (errno == EAGAIN) {
                // read interrupted.  Return to main loop to resume.
                // Returning here will leave any in-progress message in
                // the stdin_buf.  We return to the main select loop
                // to confirm we really have more data to read and to
                // perform additional error checking on the stream.
                return;
            }

            // All other errors reading STDIN are considered fatal.
            osrfLogError(OSRF_LOG_MARK,
                "WS STDIN read failed with [%s]. Exiting", strerror(errno));
            shut_it_down(1);
            return;
        }

        if (stat == 0) { // EOF
            osrfLogInfo(OSRF_LOG_MARK, "WS exiting on disconnect");
            shut_it_down(0);
            return;
        }

        c = char_buf[0];

        if (c == '\n') { // end of current message

            if (stdin_buf->n_used >= MAX_MESSAGE_SIZE) {
                osrfLogError(OSRF_LOG_MARK,
                    "WS message exceeded MAX_MESSAGE_SIZE, discarding");
                rebuild_stdin_buffer();
                return;
            }

            if (stdin_buf->n_used > 0) {
                relay_stdin_message(stdin_buf->buf);

                if (stdin_buf->n_used >= RESET_MESSAGE_SIZE) {
                    // Current message is large.  Rebuild the buffer
                    // to free the excess memory.
                    rebuild_stdin_buffer();

                } else {

                    // Reset the buffer and carry on.
                    buffer_reset(stdin_buf);
                }
            }

            return;

        } else {

            if (stdin_buf->n_used >= MAX_MESSAGE_SIZE) {
                // Message exceeds max message size.  Continue reading
                // and discarding data.  NOTE: don't reset stdin_buf
                // here becase we check n_used again once reading is done.
                continue;
            }

            // Add the char to our current message buffer
            buffer_add_char(stdin_buf, c);
        }
    }
}

// Relays a single websocket request to the OpenSRF/XMPP network.
static void relay_stdin_message(const char* msg_string) {

    jsonObject *msg_wrapper = NULL; // free me
    const jsonObject *tmp_obj = NULL;
    const jsonObject *osrf_msg = NULL;
    const char *service = NULL;
    const char *thread = NULL;
    const char *log_xid = NULL;
    char *msg_body = NULL;
    char *recipient = NULL;

    // generate a new log trace for this request. it
    // may be replaced by a client-provided trace below.
    osrfLogMkXid();

    osrfLogInternal(OSRF_LOG_MARK, "WS received inbound message: %s", msg_string);

    msg_wrapper = jsonParse(msg_string);

    if (msg_wrapper == NULL) {
        osrfLogWarning(OSRF_LOG_MARK, "WS Invalid JSON: %s", msg_string);
        return;
    }

    osrf_msg = jsonObjectGetKeyConst(msg_wrapper, "osrf_msg");

    if ( (tmp_obj = jsonObjectGetKeyConst(msg_wrapper, "service")) )
        service = jsonObjectGetString(tmp_obj);

    if ( (tmp_obj = jsonObjectGetKeyConst(msg_wrapper, "thread")) )
        thread = jsonObjectGetString(tmp_obj);

    if ( (tmp_obj = jsonObjectGetKeyConst(msg_wrapper, "log_xid")) )
        log_xid = jsonObjectGetString(tmp_obj);

    if (log_xid) {

        // use the caller-provide log trace id
        if (strlen(log_xid) > MAX_THREAD_SIZE) {
            osrfLogWarning(OSRF_LOG_MARK, "WS log_xid exceeds max length");
            return;
        }

        osrfLogForceXid(log_xid);
    }

    if (thread) {

        if (strlen(thread) > MAX_THREAD_SIZE) {
            osrfLogWarning(OSRF_LOG_MARK, "WS thread exceeds max length");
            return;
        }

        // since clients can provide their own threads at session start time,
        // the presence of a thread does not guarantee a cached recipient
        recipient = (char*) osrfHashGet(stateful_session_cache, thread);

        if (recipient) {
            osrfLogDebug(OSRF_LOG_MARK, "WS found cached recipient %s", recipient);
        }
    }

    if (!recipient) {

        if (service) {
            int size = snprintf(recipient_buf, RECIP_BUF_SIZE - 1,
                "%s@%s/%s", osrf_router, osrf_domain, service);
            recipient_buf[size] = '\0';
            recipient = recipient_buf;

        } else {
            osrfLogWarning(OSRF_LOG_MARK, "WS Unable to determine recipient");
            return;
        }
    }

    osrfLogDebug(OSRF_LOG_MARK,
        "WS relaying message to opensrf thread=%s, recipient=%s",
            thread, recipient);

    // 'recipient' will be freed in extract_inbound_messages
    // during a DISCONNECT call.  Retain a local copy.
    recipient = strdup(recipient);

    msg_body = extract_inbound_messages(service, thread, osrf_msg);

    osrfLogInternal(OSRF_LOG_MARK,
        "WS relaying inbound message: %s", msg_body);

    transport_message *tmsg = message_init(
        msg_body, NULL, thread, recipient, NULL);

    free(recipient);

    message_set_osrf_xid(tmsg, osrfLogGetXid());

    if (client_send_message(osrf_handle, tmsg) != 0) {
        osrfLogError(OSRF_LOG_MARK, "WS failed sending data to OpenSRF, exiting");
        shut_it_down(1);
    }

    osrfLogClearXid();
    message_free(tmsg);
    jsonObjectFree(msg_wrapper);
    free(msg_body);
}

// Turn the OpenSRF message JSON into a set of osrfMessage's for 
// analysis, ingress application, and logging.
static char* extract_inbound_messages(
        const char* service, const char* thread, const jsonObject *osrf_msg) {

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

    for (i = 0; i < num_msgs; i++) {
        msg = msg_list[i];
        osrfMessageSetIngress(msg, WEBSOCKET_INGRESS);

        switch (msg->m_type) {

            case CONNECT:
                break;

            case REQUEST:
                log_request(service, msg);
                break;

            case DISCONNECT:
                osrfHashRemove(stateful_session_cache, thread);
                break;

            default:
                osrfLogError(OSRF_LOG_MARK, "WS received unexpected message "
                    "type from WebSocket client: %d", msg->m_type);
                break;
        }
    }

    char* finalMsg = osrfMessageSerializeBatch(msg_list, num_msgs);

    // clean up our messages
    for (i = 0; i < num_msgs; i++)
        osrfMessageFree(msg_list[i]);

    return finalMsg;
}

// All REQUESTs are logged as activity.
static void log_request(const char* service, osrfMessage* msg) {

    const jsonObject* params = msg->_params;
    growing_buffer* act = buffer_init(128);
    char* method = msg->method_name;
    const jsonObject* obj = NULL;
    int i = 0;
    const char* str;
    int redactParams = 0;

    buffer_fadd(act, "[%s] [%s] %s %s", client_ip, "", service, method);

    while ( (str = osrfStringArrayGetString(log_protect_arr, i++)) ) {
        if (!strncmp(method, str, strlen(str))) {
            redactParams = 1;
            break;
        }
    }

    if (redactParams) {
        OSRF_BUFFER_ADD(act, " **PARAMS REDACTED**");
    } else {
        i = 0;
        while ((obj = jsonObjectGetIndex(params, i++))) {
            char* str = jsonObjectToJSON(obj);
            if (i == 1)
                OSRF_BUFFER_ADD(act, " ");
            else
                OSRF_BUFFER_ADD(act, ", ");
            OSRF_BUFFER_ADD(act, str);
            free(str);
        }
    }

    osrfLogActivity(OSRF_LOG_MARK, "%s", act->buf);
    buffer_free(act);
}



// Relay response messages from OpenSRF to STDIN
// Relays all available messages
static void read_from_osrf() {
    transport_message* tmsg = NULL;

    // Double check the socket connection before continuing.
    if (!client_connected(osrf_handle) ||
        !socket_connected(osrf_handle->session->sock_id)) {
        osrfLogWarning(OSRF_LOG_MARK,
            "WS: Jabber socket disconnected, exiting");
        shut_it_down(1);
    }

    // Once client_recv is called all data waiting on the socket is
    // read.  This means we can't return to the main select() loop after
    // each message, because any subsequent messages will get stuck in
    // the opensrf receive queue. Process all available messages.
    while ( (tmsg = client_recv(osrf_handle, 0)) ) {
        read_one_osrf_message(tmsg);
        message_free(tmsg);
    }
}

// Process a single OpenSRF response message and print the reponse
// to STDOUT for delivery to the websocket client.
static void read_one_osrf_message(transport_message* tmsg) {
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

                if (!osrfHashGet(stateful_session_cache, tmsg->thread)) {

                    unsigned long ses_size =
                        osrfHashGetCount(stateful_session_cache);

                    if (ses_size < MAX_ACTIVE_STATEFUL_SESSIONS) {

                        osrfLogDebug(OSRF_LOG_MARK, "WS caching sender "
                            "thread=%s, sender=%s; concurrent=%d",
                            tmsg->thread, tmsg->sender, ses_size);

                        char* sender = strdup(tmsg->sender); // free in *Remove
                        osrfHashSet(stateful_session_cache, sender, tmsg->thread);

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
                    osrfHashRemove(stateful_session_cache, tmsg->thread);
                }
            }
        }
    }

    // osrfMessageDeserialize applies the freeItem handler to the
    // newly created osrfList.  We only need to free the list and
    // the individual osrfMessage's will be freed along with it
    osrfListFree(msg_list);

    // Pack the response into a websocket wrapper message.
    jsonObject *msg_wrapper = NULL;
    char *msg_string = NULL;
    msg_wrapper = jsonNewObject(NULL);

    jsonObjectSetKey(msg_wrapper, "thread", jsonNewObject(tmsg->thread));
    jsonObjectSetKey(msg_wrapper, "log_xid", jsonNewObject(tmsg->osrf_xid));
    jsonObjectSetKey(msg_wrapper, "osrf_msg", jsonParseRaw(tmsg->body));

    if (tmsg->is_error) {
        // tmsg->sender is the original recipient. they get swapped
        // in error replies.
        osrfLogError(OSRF_LOG_MARK,
            "WS received XMPP error message in response to thread=%s and "
            "recipient=%s.  Likely the recipient is not accessible/available.",
            tmsg->thread, tmsg->sender);
        jsonObjectSetKey(msg_wrapper, "transport_error", jsonNewBoolObject(1));
    }

    msg_string = jsonObjectToJSONRaw(msg_wrapper);

    // Send the JSON to STDOUT
    printf("%s\n", msg_string);

    free(msg_string);
    jsonObjectFree(msg_wrapper);
}


