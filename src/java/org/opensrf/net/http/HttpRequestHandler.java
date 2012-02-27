package org.opensrf.net.http;

import java.util.List;

/*
 * Handler for async gateway responses.
 */
public abstract class HttpRequestHandler {

    /**
     * Called when all responses have been received.
     *
     * If discardResponses() returns true, will be passed null.
     */
    public void onComplete(HttpRequest request) {
    }

    /**
     * Called with each response received from the server.
     * 
     * @param payload the value returned from the server.
     */
    public void onResponse(HttpRequest request, Object response) {
    }
}
