package org.opensrf;
import java.util.List;
import java.util.ArrayList;
import org.opensrf.util.ConfigException;

public class MultiSession {

    class RequestContainer {
        Request request;
        int id;
        RequestContainer(Request r) {
            request = r; 
        }
    }

    private boolean complete;
    private List<RequestContainer> requests;
    private int lastId;

    public MultiSession() {
        requests = new ArrayList<RequestContainer>();
    }

    public boolean isComplete() {
        return complete;
    }

    public int lastId() {
        return lastId;
    }

    /**
     * Adds a new request to the set of requests.
     * @param service The OpenSRF service
     * @param method The OpenSRF method
     * @param params The array of method params
     * @return The request ID, which is used to map results from recv() to the original request.
     */
    public int request(String service, String method, Object[] params) throws SessionException, ConfigException {
        ClientSession ses = new ClientSession(service);
        return request(ses.request(method, params));
    }


    public int request(String service, String method) throws SessionException, ConfigException {
        ClientSession ses = new ClientSession(service);
        return request(ses.request(method));
    }

    private int request(Request req) {
        RequestContainer c = new RequestContainer(req);
        c.id = requests.size();
        requests.add(c);
        return c.id;
    }


    /**
     * Calls recv on all pending requests until there is data to return.  The ID which
     * maps the received object to the request can be retrieved by calling lastId().
     * @param millis Number of milliseconds to wait for some data to arrive.
     * @return The object result or null if all requests are complete
     * @throws MethodException Thrown if no response is received within 
     * the given timeout or the method fails.
     */
    public Object recv(int millis) throws MethodException {
        if(complete) return null;

        Request req = null;
        Result res = null;
        RequestContainer cont = null;

        long duration = 0;
        long blockTime = 100;

        /* if there is only 1 outstanding request, don't poll */
        if(requests.size() == 1)
            blockTime = millis;

        while(true) {
            for(int i = 0; i < requests.size(); i++) {

                cont = requests.get(i);
                req = cont.request;

                try {
                    if(i == 0) {
                        res = req.recv(blockTime);
                    } else {
                        res = req.recv(0);
                    }
                } catch(SessionException e) {
                    throw new MethodException(e);
                }

                if(res != null) break;
            }

            if(res != null) break;
            duration += blockTime;

            if(duration >= millis) {
                System.out.println("duration = " + duration + " millis = " + millis);
                throw new MethodException("No request received within " + millis + " milliseconds");
            }
        }

        if(res.getStatusCode() != 200) {
            throw new MethodException("Request " + cont.id + " failed  with status code " + 
                res.getStatusCode() + " and status message " + res.getStatus());
        }

        if(req.isComplete())
            requests.remove(requests.indexOf(cont));

        if(requests.size() == 0)
            complete = true;

        lastId = cont.id;
        return res.getContent();
    }
}

