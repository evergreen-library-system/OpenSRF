package org.opensrf.test;
import org.opensrf.*;
import org.opensrf.util.*;
import java.util.Map;
import java.util.Date;
import java.util.List;
import java.util.ArrayList;
import java.io.PrintStream;


public class TestClient {

    public static void main(String args[]) throws Exception {

        /** which opensrf service are we sending our request to */
        String service; 
        /** which opensrf method we're calling */
        String method;
        /** method params, captures from command-line args */
        List<Object> params;
        /** knows how to read JSON */
        JSONReader reader;
        /** opensrf request */
        Request request;
        /** request result */
        Result result;
        /** start time for the request */
        long start;
        /** for brevity */
        PrintStream out = System.out;

        if(args.length < 3) {
            out.println( "usage: org.opensrf.test.TestClient "+
                "<osrfConfigFile> <service> <method> [<JSONparam1>, <JSONparam2>]");
            return;
        }

        /** connect to the opensrf network,  default config context 
         * for opensrf_core.xml is /config/opensrf */
        Sys.bootstrapClient(args[0], "/config/opensrf");

        /* grab the server, method, and any params from the command line */
        service = args[1];
        method = args[2];
        params = new ArrayList<Object>();
        for(int i = 3; i < args.length; i++) 
            params.add(new JSONReader(args[i]).read());


        /** build the client session */
        ClientSession session = new ClientSession(service);

        /** kick off the timer */
        start = new Date().getTime();

        /** Create the request object from the session, method and params */
        request = session.request(method, params);

        while( (result = request.recv(60000)) != null ) { 
            /** loop over the results and print the JSON version of the content */

            if(result.getStatusCode() != 200) { 
                /** make sure the request succeeded */
                out.println("status = " + result.getStatus());
                out.println("status code = " + result.getStatusCode());
                continue;
            }

            /** JSON-ify the resulting object and print it */
            out.println("\nresult JSON: " + new JSONWriter(result.getContent()).write());
        }
        
        /** How long did the request take? */
        out.println("Request round trip took: " + (new Date().getTime() - start) + " ms.");
    }
}



