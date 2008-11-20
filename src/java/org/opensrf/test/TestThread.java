package org.opensrf.test;
import org.opensrf.*;
import org.opensrf.util.*;
import java.util.Map;
import java.util.Date;
import java.util.List;
import java.util.ArrayList;
import java.io.PrintStream;

/**
 * Connects to the opensrf network once per thread and runs
 * and runs a series of request acccross all launched threads.
 * The purpose is to verify that the java threaded client api 
 * is functioning as expected
 */
public class TestThread implements Runnable {

    String args[];

    public TestThread(String args[]) {
        this.args = args;
    }

    public void run() {

        try {

            Sys.bootstrapClient(args[0], "/config/opensrf");
            ClientSession session = new ClientSession(args[3]);
    
            List params = new ArrayList<Object>();
            for(int i = 5; i < args.length; i++) 
                params.add(new JSONReader(args[3]).read());
    
            for(int i = 0; i < Integer.parseInt(args[2]); i++) {
                System.out.println("thread " + Thread.currentThread().getId()+" sending request " + i);
                Request request = session.request(args[4], params);
                Result result = request.recv(3000);
                if(result != null) {
                    System.out.println("thread " + Thread.currentThread().getId()+ 
                        " got result JSON: " + new JSONWriter(result.getContent()).write());
                } else {
                    System.out.println("* thread " + Thread.currentThread().getId()+ " got NO result");
                }
            }
    
            Sys.shutdown();
        } catch(Exception e) {
            System.err.println(e);
        }
    }

    public static void main(String args[]) throws Exception {

        if(args.length < 5) {
            System.out.println( "usage: org.opensrf.test.TestClient "+
                "<osrfConfigFile> <numthreads> <numiter> <service> <method> [<JSONparam1>, <JSONparam2>]");
            return;
        }

        int numThreads = Integer.parseInt(args[1]);
        for(int i = 0; i < numThreads; i++) 
            new Thread(new TestThread(args)).start();
    }
}



