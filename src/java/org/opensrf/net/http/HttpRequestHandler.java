package org.opensrf.net.http;

import org.opensrf.util.*;
import java.util.List;

/*
 * Handler for async gateway responses.
 */
public abstract class HttpRequestHandler {

    /**
     * Called when all responses have been received.
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

    public void onError(HttpRequest request, Exception ex) {
        Logger.error("HttpRequest Error: " + ex.getStackTrace());
    }
}
