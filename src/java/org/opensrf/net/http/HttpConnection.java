package org.opensrf.net.http;

import java.net.URL;
import java.net.MalformedURLException;
import java.util.Queue;
import java.util.concurrent.ConcurrentLinkedQueue;
import org.opensrf.*;
import org.opensrf.util.*;


/**
 * Manages connection parameters and thread limiting for opensrf json gateway connections.
 */

public class HttpConnection {

    /** Compiled URL object */
    protected URL url;
    /** Number of threads currently communicating with the server */
    protected int activeThreads;
    /** Queue of pending async requests */
    protected Queue<HttpRequest> pendingThreadQueue;
    /** maximum number of actively communicating threads allowed */
    protected int maxThreads = 10;

    public HttpConnection(String fullUrl) throws java.net.MalformedURLException {
        activeThreads = 0;
        pendingThreadQueue = new ConcurrentLinkedQueue();
        url = new URL(fullUrl);
    }

    /** 
     * Maximun number of threads allowed to communicate with the server.
     */
    public int getMaxThreads() {
        return maxThreads;
    }

    /** 
     * Set the maximum number of actively communicating (async) threads allowed.
     *
     * This has no effect on synchronous communication.
     */
    public void setMaxThreads(int max) {
        maxThreads = max;
    }

    /**
     * Launches or queues an asynchronous request.
     *
     * If the maximum active thread count has not been reached,
     * start a new thread and use it to send and receive the request.
     * The response is passed to the request's HttpRequestHandler
     * onComplete().  After complete, if the number of active threads
     * is still lower than the max, one request will be pulled (if 
     * present) from the async queue and fired.
     *
     * If there are too many active threads, the main request is
     * pushed onto the async queue for later processing
     */
    protected void manageAsyncRequest(final HttpRequest request) {

        if (activeThreads >= maxThreads) {
            pendingThreadQueue.offer(request);
            return;
        }

        activeThreads++;

         //Send the request receive the response, fire off the next 
         //thread if necessary, then pass the result to the handler
        Runnable r = new Runnable() {
            public void run() {
                Object response;

                try {
                    request.send();
                    while ((response = request.recv()) != null)
                        request.handler.onResponse(request, response);

                    request.handler.onComplete(request);

                } catch (Exception ex) {
                    request.handler.onError(request, ex);

                } finally {
                    // server communication has completed
                    activeThreads--;
                }

                if (activeThreads < maxThreads) {
                    try {
                        manageAsyncRequest(pendingThreadQueue.remove());
                    } catch (java.util.NoSuchElementException ex) {
                        // may have been gobbled by another thread
                    }
                }
            }
        };

        new Thread(r).start();
    }
}


