#include <opensrf/transport_connection.h>
#include<unistd.h>

transport_con* transport_con_new(const char* domain) {

    osrfLogInternal(OSRF_LOG_MARK, "TCON transport_con_new() domain=%s", domain);

    transport_con* con = safe_malloc(sizeof(transport_con));

    con->bus = NULL;
    con->address = NULL;
    con->domain = strdup(domain);
    con->max_queue = 1000; // TODO pull from config

    osrfLogInternal(OSRF_LOG_MARK, 
        "TCON created transport connection with domain: %s", con->domain);

    return con;
}

void transport_con_msg_free(transport_con_msg* msg) {
    osrfLogInternal(OSRF_LOG_MARK, "TCON transport_con_msg_free()");

    if (msg == NULL) { return; } 
    if (msg->msg_json) { free(msg->msg_json); }

    free(msg);
}

void transport_con_free(transport_con* con) {
    osrfLogInternal(OSRF_LOG_MARK, "TCON transport_con_free()");

    osrfLogInternal(
        OSRF_LOG_MARK, "Freeing transport connection for %s", con->domain);

    if (con->bus) { free(con->bus); }
    if (con->address) { free(con->address); }
    if (con->domain) { free(con->domain); }

    free(con);
}

int transport_con_connected(transport_con* con) {
    return con->bus != NULL;
} 

void transport_con_set_address(transport_con* con, const char* username) {
    osrfLogInternal(OSRF_LOG_MARK, "TCON transport_con_set_address()");

    char hostname[1024];
    hostname[1023] = '\0';
    gethostname(hostname, 1023);

    growing_buffer *buf = buffer_init(64);
    buffer_fadd(buf, "opensrf:client:%s:%s:%s:", username, con->domain, hostname);

    buffer_fadd(buf, "%ld", (long) getpid());

    char junk[256];
    snprintf(junk, sizeof(junk), 
        "%f%d", get_timestamp_millis(), (int) time(NULL));

    char* md5 = md5sum(junk);

    buffer_add(buf, ":");
    buffer_add_n(buf, md5, 8);

    con->address = buffer_release(buf);

    osrfLogDebug(OSRF_LOG_MARK, "Connection set address to %s", con->address);
}

int transport_con_connect(
    transport_con* con, int port, const char* username, const char* password) {
    osrfLogInternal(OSRF_LOG_MARK, "TCON transport_con_connect()");

    osrfLogDebug(OSRF_LOG_MARK, "Transport con connecting with bus "
        "domain=%s; address=%s; port=%d; username=%s", 
        con->domain,
        con->address,
        port, 
        username
    );

    con->bus = redisConnect(con->domain, port);

    if (con->bus == NULL) {
        osrfLogError(OSRF_LOG_MARK, "Could not connect to Redis instance");
        return 0;
    }

    osrfLogDebug(OSRF_LOG_MARK, "Connected to Redis instance OK");

    redisReply *reply = 
        redisCommand(con->bus, "AUTH %s %s", username, password);

    if (handle_redis_error(reply, "AUTH %s %s", username, password)) { 
        return 0; 
    }

    freeReplyObject(reply);

    return 1;
}

int transport_con_clear(transport_con* con) {
    if (con == NULL || con->bus == NULL) { return -1; }

    redisReply *reply = redisCommand(con->bus, "DEL %s", con->address);

    if (!handle_redis_error(reply, "DEL %s", con->address)) {
        freeReplyObject(reply);
    }

    return 0;
}

int transport_con_disconnect(transport_con* con) {
    transport_con_clear(con);
    redisFree(con->bus);

    con->bus = NULL;

    return 0;
}

// Returns 0 on success.
int transport_con_send(transport_con* con, const char* msg_json, const char* recipient) {

    osrfLogInternal(OSRF_LOG_MARK, "Sending to recipient=%s: %s", recipient, msg_json);

    redisReply *reply = redisCommand(con->bus, "RPUSH %s %s", recipient, msg_json);

    int stat = handle_redis_error(reply, "RPUSH %s %s", recipient, msg_json);

    if (!stat) {
        freeReplyObject(reply);
    }

    return stat;
}

transport_con_msg* transport_con_recv_once(transport_con* con, int timeout, const char* recipient) {

    osrfLogInternal(OSRF_LOG_MARK, 
        "TCON transport_con_recv_once() timeout=%d recipient=%s", timeout, recipient);

    if (recipient == NULL) { recipient = con->address; }

    size_t len = 0;
    char command_buf[256];

    if (timeout == 0) { // Non-blocking list pop

        len = snprintf(command_buf, 256, "LPOP %s", recipient);

    } else {
        
        if (timeout < 0) { // Block indefinitely

            len = snprintf(command_buf, 256, "BLPOP %s 0", recipient);

        } else { // Block up to timeout seconds

            len = snprintf(command_buf, 256, "BLPOP %s %d", recipient, timeout);
        }
    }

    command_buf[len] = '\0';

    osrfLogInternal(OSRF_LOG_MARK, 
        "recv_one_chunk() sending command: %s", command_buf);

    redisReply* reply = redisCommand(con->bus, command_buf);
    if (handle_redis_error(reply, command_buf)) { return NULL; }

    char* json = NULL;
    if (reply->type == REDIS_REPLY_STRING) { // LPOP
        json = strdup(reply->str);

    } else if (reply->type == REDIS_REPLY_ARRAY) { // BLPOP

        // BLPOP returns [list_name, popped_value]
        if (reply->elements == 2 && reply->element[1]->str != NULL) {
            json = strdup(reply->element[1]->str); 
        } else {
            osrfLogInternal(OSRF_LOG_MARK, 
                "No response returned within timeout: %d", timeout);
        }
    }

    freeReplyObject(reply);

    osrfLogInternal(OSRF_LOG_MARK, "recv_one_chunk() read json: %s", json);

    if (json == NULL) {
        return NULL;
    }

    transport_con_msg* tcon_msg = safe_malloc(sizeof(transport_con_msg));
    tcon_msg->msg_json = json;

    return tcon_msg;
}


transport_con_msg* transport_con_recv(transport_con* con, int timeout, const char* stream) {
    osrfLogInternal(OSRF_LOG_MARK, "TCON transport_con_recv() stream=%s", stream);

    if (timeout == 0) {
        return transport_con_recv_once(con, 0, stream);

    } else if (timeout < 0) {
        // Keep trying until we have a result.

        while (1) {
            transport_con_msg* msg = transport_con_recv_once(con, -1, stream);
            if (msg != NULL) { return msg; }
        }
    }

    time_t seconds = (time_t) timeout;

    while (seconds > 0) {
        // Keep trying until we get a response or our timeout is exhausted.

        time_t now = time(NULL);
        transport_con_msg* msg = transport_con_recv_once(con, timeout, stream);

        if (msg == NULL) {
            seconds -= now;
        } else {
            return msg;
        }
    }

    return NULL;
}

void transport_con_flush_socket(transport_con* con) {
}

// Returns false/0 on success, true/1 on failure.
// On error, the reply is freed.
int handle_redis_error(redisReply *reply, const char* command, ...) {
    VA_LIST_TO_STRING(command);

    if (reply != NULL && reply->type != REDIS_REPLY_ERROR) {
        osrfLogInternal(OSRF_LOG_MARK, "Redis Command: %s", VA_BUF);
        return 0;
    }

    char* err = reply == NULL ? "" : reply->str;
    osrfLogError(OSRF_LOG_MARK, "REDIS Error [%s] %s", err, VA_BUF);
    freeReplyObject(reply);

    // Some bus error conditions can lead to looping on an unusable
    // connection.  Avoid flooding the logs by inserting a short
    // wait after any Redis errors.  Note, these should never happen
    // under normal wear and tear.  It's possible we should just exit
    // here, but need to collect some data first.
    osrfLogError(OSRF_LOG_MARK, "Resting for a few seconds after bus failure...");
    sleep(3);

    return 1;
}
