package org.opensrf.net.http;

import org.opensrf.*;
import org.opensrf.util.*;

import java.io.IOException;
import java.io.BufferedInputStream;
import java.io.OutputStreamWriter;
import java.io.InputStream;
import java.io.IOException;
import java.net.URL;
import java.net.URI;
import java.net.HttpURLConnection;
import java.lang.StringBuffer;
import java.util.List;
import java.util.Iterator;
import java.util.Map;
import java.util.HashMap;
import java.util.Queue;
import java.util.concurrent.ConcurrentLinkedQueue;

public class GatewayRequest extends HttpRequest {

    private boolean readComplete;

    public GatewayRequest(HttpConnection conn, String service, Method method) {
        super(conn, service, method);
        readComplete = false;
    }

    public GatewayRequest send() {
        try {

            String postData = compilePostData(service, method);

            urlConn = (HttpURLConnection) httpConn.url.openConnection();
            urlConn.setRequestProperty("Content-Type", "application/x-www-form-urlencoded"); 
            urlConn.setDoInput(true);
            urlConn.setDoOutput(true);

            OutputStreamWriter wr = new OutputStreamWriter(urlConn.getOutputStream());
            wr.write(postData);
            wr.flush();
            wr.close();

        } catch (java.io.IOException ex) {
            failed = true;
            failure = ex;
        }

        return this;
    }

    public Object recv() {

        if (readComplete) 
            return nextResponse();

        try {

            InputStream netStream = new BufferedInputStream(urlConn.getInputStream());
            StringBuffer readBuf = new StringBuffer();

            int bytesRead = 0;
            byte[] buffer = new byte[1024];

            while ((bytesRead = netStream.read(buffer)) != -1) {
                readBuf.append(new String(buffer, 0, bytesRead));
            }
            
            netStream.close();
            urlConn = null;

            Map<String,?> result = null;

            try {
                result = (Map<String, ?>) new JSONReader(readBuf.toString()).readObject();
            } catch (org.opensrf.util.JSONException ex) {
                ex.printStackTrace();
                return null;
            }

            String status = result.get("status").toString(); 
            if (!"200".equals(status)) {
                failed = true;
                // failure = <some new exception>
            }

             // gateway always returns a wrapper array with the full results set
             responseList = (List) result.get("payload"); 

        } catch (java.io.IOException ex) { 
            failed = true;
            failure = ex;
        }

        readComplete = true;
        return nextResponse();
    }

    private String compilePostData(String service, Method method) {
        URI uri = null;
        StringBuffer postData = new StringBuffer();

        postData.append("service=");
        postData.append(service);
        postData.append("&method=");
        postData.append(method.getName());

        List params = method.getParams();
        Iterator itr = params.iterator();

        while (itr.hasNext()) {
            postData.append("&param=");
            postData.append(new JSONWriter(itr.next()).write());
        }

        try {
            // not using URLEncoder because it replaces ' ' with '+'.
            uri = new URI("http", "", null, postData.toString(), null);
        } catch (java.net.URISyntaxException ex) {
            ex.printStackTrace(); 
        }

        return uri.getRawQuery();
    }
}


