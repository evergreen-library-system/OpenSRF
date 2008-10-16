package org.opensrf.test;
import org.opensrf.*;
import org.opensrf.util.*;

public class TestMultiSession {
    public static void main(String[] args) {
        try {
            String config = args[0];

            Sys.bootstrapClient(config, "/config/opensrf");
            MultiSession ses = new MultiSession();

            for(int i = 0; i < 40; i++) {
                ses.request("opensrf.settings", "opensrf.system.time");
            }

            while(!ses.isComplete()) 
                System.out.println("result = " + ses.recv(5000) + " and id = " + ses.lastId());

            System.out.println("done");
            Sys.shutdown();
        } catch(Exception e) {
            e.printStackTrace();
        }
    }
}
