package org.opensrf.net.http;
import org.opensrf.*;
import org.opensrf.util.*;

import java.util.List;
import java.util.LinkedList;
import java.net.HttpURLConnection;

public abstract class HttpRequest {

    /** OpenSRF service. */
    protected String service;
    /** OpenSRF method. */
    protected Method method;
    /** Connection to server. */
    protected HttpURLConnection urlConn;
    /** Connection manager. */
    protected HttpConnection httpConn;

    /** 
     * List of responses.
     *
     * Responses are not kept after being passed to onResponse .
     */
    protected List<Object> responseList;

    /** True if all responses data has been read and delivered */
    protected boolean complete;

    // use a no-op handler by default
    protected HttpRequestHandler handler = new HttpRequestHandler() {};

    public HttpRequest() {
        complete = false;
        handler = null;
        urlConn = null;
    }

    /**
     * Constructor.
     *
     * @param conn Connection 
     * @param service The OpenSRF service.
     * @param method The method to send to the server.
     */
    public HttpRequest(HttpConnection conn, String service, Method method) {
        this();
        this.httpConn = conn;
        this.service = service;
        this.method = method;
    }

    /**
     * Send a request asynchronously (via threads).
     *
     * @param handler Manages responses
     */
    public void sendAsync(final HttpRequestHandler handler) {
        if (handler != null) 
            this.handler = handler;
        httpConn.manageAsyncRequest(this);
    }

    protected void pushResponse(Object response) {
        if (responseList == null)
            responseList = new LinkedList<Object>();
        responseList.add(response);
    }

    protected List responses() {
        return responseList;
    }
    
    protected Object nextResponse() {
        if (complete || responseList == null) 
            return null;
        if (responseList.size() > 0)
            return responseList.remove(0);
        return null;
    }

    /**
     * Send a request to the server.
     */
    public abstract GatewayRequest send() throws java.io.IOException;

    /**
     * Receive one response from the server.
     *
     * @return The response object
     */
    public abstract Object recv() throws java.io.IOException;
}


