package org.opensrf;

import org.opensrf.util.*;
import org.opensrf.net.xmpp.*;
import java.util.Random;
import java.util.Date;
import java.net.InetAddress;


public class Sys {

    private static void initLogger(Config config) {
        if(Logger.instance() == null) {
            try {
                String logFile = config.getString("/logfile");
                int logLevel = config.getInt("/loglevel");
                Logger.init( (short) config.getInt("/loglevel"), new FileLogger(logFile));
                /** add syslog support... */
            } catch(Exception e) {
                /* by default, log to stderr at WARN level */
                Logger.init(Logger.WARN, new Logger()); 
            }
        }
    }

    /**
     * Connects to the OpenSRF network so that client sessions may communicate.
     * @param configFile The OpenSRF config file 
     * @param configContext Where in the XML document the config chunk lives.  This
     * allows an OpenSRF client config chunk to live in XML files where other config
     * information lives.
     */
    public static void bootstrapClient(String configFile, String configContext) 
            throws ConfigException, SessionException  {


        /** see if the current thread already has a connection */
        XMPPSession existing = XMPPSession.getThreadSession();
        if(existing != null && existing.connected())
            return;

        /** create the config parser */
        Config config = new Config(configContext);
        config.parse(configFile);
        Config.setConfig(config); /* set this as the global config */

        initLogger(config);

        /** Collect the network connection info from the config */
        String username = config.getString("/username");
        String passwd = config.getString("/passwd");
        String host = (String) config.getFirst("/domains/domain");
        int port = config.getInt("/port");


        /** Create a random login resource string */
        String res = "java_";
        try {
            res += InetAddress.getLocalHost().getHostAddress();
        } catch(java.net.UnknownHostException e) {}
        res += "_"+Math.abs(new Random(new Date().getTime()).nextInt()) 
            + "_t"+ Thread.currentThread().getId();



        try {

            /** Connect to the Jabber network */
            Logger.info("attempting to create XMPP session "+username+"@"+host+"/"+res);
            XMPPSession xses = new XMPPSession(host, port);
            xses.connect(username, passwd, res);
            XMPPSession.setThreadSession(xses);

        } catch(XMPPException e) {
            throw new SessionException("Unable to bootstrap client", e);
        }
    }

    /**
     * Shuts down the connection to the opensrf network
     */
    public static void shutdown() {
        XMPPSession.getThreadSession().disconnect();
    }
}

