package org.opensrf.net.http;
import org.opensrf.*;
import org.opensrf.util.*;

import java.util.List;
import java.util.LinkedList;
import java.net.HttpURLConnection;

public abstract class HttpRequest {

    protected String service;
    protected Method method;
    protected HttpURLConnection urlConn;
    protected HttpConnection httpConn;
    protected HttpRequestHandler handler;
    protected List<Object> responseList;
    protected Exception failure;
    protected boolean failed;
    protected boolean complete;

    public HttpRequest() {
        failed = false;
        complete = false;
        handler = null;
        urlConn = null;
    }

    public HttpRequest(HttpConnection conn, String service, Method method) {
        this();
        this.httpConn = conn;
        this.service = service;
        this.method = method;
    }

    public void sendAsync(final HttpRequestHandler handler) {
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
        if (complete || failed) return null;
        if (responseList.size() > 0)
            return responseList.remove(0);
        return null;
    }

    public Exception getFailure() {
        return failure;
    }

    public abstract HttpRequest send();

    public abstract Object recv();
}


