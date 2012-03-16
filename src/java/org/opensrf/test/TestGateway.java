package org.opensrf.test;

import org.opensrf.*;
import org.opensrf.util.*;
import org.opensrf.net.http.*;

import java.net.URL;
import java.util.List;
import java.util.Arrays;

public class TestGateway {
    
    public static void main(String[] args) {

        if (args.length == 0) {
            System.err.println("Please provide a gateway URL: e.g. http://example.org/osrf-gateway-v1");
            return;
        }

        try {

            // configure the connection
            HttpConnection conn = new HttpConnection(args[0]);

            Method method = new Method("opensrf.system.echo");
            method.addParam("Hello, Gateway");
            method.addParam(new Integer(12345));
            method.addParam(new Boolean(true));
            method.addParam(Arrays.asList(8,6,7,5,3,0,9));

            // sync test
            HttpRequest req = new GatewayRequest(conn, "opensrf.math", method).send();
            Object resp;
            while ( (resp = req.recv()) != null) {
                System.out.println("Sync Response: " + resp);
            }

            // async test
            for (int i = 0; i < 10; i++) {
                final int ii = i; // required for nested class
                HttpRequest req2 = new GatewayRequest(conn, "opensrf.math", method);

                req2.sendAsync(
                    new HttpRequestHandler() {

                        // called once per response
                        public void onResponse(HttpRequest req, Object resp) {
                            System.out.println("Async Response: " + ii + " : " + resp);
                        }

                        // called after all responses have been received
                        // used primarily when you don't know how many responses will be returned
                        public void onComplete(HttpRequest req) {
                            System.out.println("Async Request complete : " + ii);
                        }

                        // ruh-roh
                        public void onError(HttpRequest req, Exception ex) {
                            if (ex instanceof java.io.IOException) 
                                System.err.println("Trouble communicating with gateway server!");
                            ex.printStackTrace();
                        }
                    }
                );
            }

        } catch (java.net.MalformedURLException ex) {
            System.err.println("Malformed Gateway URL! " + args[0]);
            ex.printStackTrace();

        } catch (java.io.IOException ex) {
            System.err.println("Trouble communicating with gateway server!");
            ex.printStackTrace();
        }
    }
}


