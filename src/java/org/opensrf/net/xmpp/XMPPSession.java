package org.opensrf.net.xmpp;

import java.io.*;
import java.net.Socket;
import java.util.Map;
import java.util.Iterator;
import java.util.concurrent.ConcurrentHashMap;


/**
 * Represents a single XMPP session.  Sessions are responsible for writing to
 * the stream and for managing a stream reader.
 */
public class XMPPSession {

    /** Initial jabber message */
    public static final String JABBER_CONNECT = 
        "<stream:stream to='%s' xmlns='jabber:client' xmlns:stream='http://etherx.jabber.org/streams'>";

    /** Basic auth message */
    public static final String JABBER_BASIC_AUTH =  
        "<iq id='123' type='set'><query xmlns='jabber:iq:auth'>" +
        "<username>%s</username><password>%s</password><resource>%s</resource></query></iq>";

    public static final String JABBER_DISCONNECT = "</stream:stream>";

    private static Map threadConnections = new ConcurrentHashMap();

    /** jabber domain */
    private String host;
    /** jabber port */
    private int port;
    /** jabber username */
    private String username;
    /** jabber password */
    private String password;
    /** jabber resource */
    private String resource;

    /** XMPP stream reader */
    XMPPReader reader;
    /** Fprint-capable socket writer */
    PrintWriter writer;
    /** Raw socket output stream */
    OutputStream outStream;
    /** The raw socket */
    Socket socket;

    /** The process-wide session.  All communication occurs
     * accross this single connection */
    private static XMPPSession globalSession;


    /**
     * Creates a new session.
     * @param host The jabber domain
     * @param port The jabber port
     */
    public XMPPSession( String host, int port ) {
        this.host = host;
        this.port = port;
    }

    /**
     * Returns the global, process-wide session
     */
    /*
    public static XMPPSession getGlobalSession() {
        return globalSession;
    }
    */

    public static XMPPSession getThreadSession() {
        return (XMPPSession) threadConnections.get(new Long(Thread.currentThread().getId()));
    }

    /**
     * Sets the given session as the global session for the current thread
     * @param ses The session
     */
    public static void setThreadSession(XMPPSession ses) {
        /* every time we create a new connection, clean up any dead threads. 
         * this is cheaper than cleaning up the dead threads at every access. */
        cleanupThreadSessions();
        threadConnections.put(new Long(Thread.currentThread().getId()), ses);
    }

    /**
     * Analyzes the threadSession data to see if there are any sessions
     * whose controlling thread has gone away.  
     */
    private static void cleanupThreadSessions() {
        Thread threads[] = new Thread[Thread.activeCount()]; 
        Thread.enumerate(threads);
        for(Iterator i = threadConnections.keySet().iterator(); i.hasNext(); ) {
            boolean found = false;
            Long id = (Long) i.next();
            for(Thread t : threads) {
                if(t.getId() == id.longValue()) {
                    found = true;
                    break;
                }
            }
            if(!found) 
                threadConnections.remove(id);
        }
    }

    /**
     * Sets the global, process-wide section
     */
    /*
    public static void setGlobalSession(XMPPSession ses) {
        globalSession = ses;
    }
    */


    /** true if this session is connected to the server */
    public boolean connected() {
        return (
                reader != null && 
                reader.getXMPPStreamState() == XMPPReader.XMPPStreamState.CONNECTED &&
                !socket.isClosed()
            );
    }


    /**
     * Connects to the network.
     * @param username The jabber username
     * @param password The jabber password
     * @param resource The Jabber resource
     */
    public void connect(String username, String password, String resource) throws XMPPException {

        this.username = username;
        this.password = password;
        this.resource = resource;

        try { 
            /* open the socket and associated streams */
            socket = new Socket(host, port);

            /** the session maintains control over the output stream */
            outStream = socket.getOutputStream();
            writer = new PrintWriter(outStream, true);

            /** pass the input stream to the reader */
            reader = new XMPPReader(socket.getInputStream());

        } catch(IOException ioe) {
            throw new 
                XMPPException("unable to communicate with host " + host + " on port " + port);
        }

        /* build the reader thread */
        Thread thread = new Thread(reader);
        thread.setDaemon(true);
        thread.start();

        synchronized(reader) {
            /* send the initial jabber message */
            sendConnect();
            reader.waitCoreEvent(10000);
        }
        if( reader.getXMPPStreamState() != XMPPReader.XMPPStreamState.CONNECT_RECV ) 
            throw new XMPPException("unable to connect to jabber server");

        synchronized(reader) {
            /* send the basic auth message */
            sendBasicAuth(); 
            reader.waitCoreEvent(10000);
        }
        if(!connected()) 
            throw new XMPPException("Authentication failed");
    }

    /** Sends the initial jabber message */
    private void sendConnect() {
        reader.setXMPPStreamState(XMPPReader.XMPPStreamState.CONNECT_SENT);
        writer.printf(JABBER_CONNECT, host);
    }

    /** Send the basic auth message */
    private void sendBasicAuth() {
        reader.setXMPPStreamState(XMPPReader.XMPPStreamState.AUTH_SENT);
        writer.printf(JABBER_BASIC_AUTH, username, password, resource);
    }


    /**
     * Sends an XMPPMessage.
     * @param msg The message to send.
     */
    public synchronized void send(XMPPMessage msg) throws XMPPException {
        checkConnected();
        try {
            String xml = msg.toXML();
            outStream.write(xml.getBytes()); 
        } catch (Exception e) {
            throw new XMPPException(e.toString());
        }
    }


    /**
     * @throws XMPPException if we are no longer connected.
     */
    private void checkConnected() throws XMPPException {
        if(!connected())
            throw new XMPPException("Disconnected stream");
    }


    /**
     * Receives messages from the network.  
     * @param timeout Maximum number of milliseconds to wait for a message to arrive.
     * If timeout is negative, this method will wait indefinitely.
     * If timeout is 0, this method will not block at all, but will return a 
     * message if there is already a message available.
     */
    public XMPPMessage recv(long timeout) throws XMPPException {

        XMPPMessage msg;

        if(timeout < 0) {

            while(true) { /* wait indefinitely for a message to arrive */
                reader.waitCoreEvent(timeout);
                msg = reader.popMessageQueue();
                if( msg != null ) return msg;
                checkConnected();
            }

        } else {

            while(timeout >= 0) { /* wait at most 'timeout' milleseconds for a message to arrive */
                msg = reader.popMessageQueue();
                if( msg != null ) return msg;
                timeout -= reader.waitCoreEvent(timeout);
                msg = reader.popMessageQueue();
                if( msg != null ) return msg;
                checkConnected();
            }
        }

        return reader.popMessageQueue();
    }


    /**
     * Disconnects from the jabber server and closes the socket
     */
    public void disconnect() {
        try {
            outStream.write(JABBER_DISCONNECT.getBytes());
            socket.close();
        } catch(Exception e) {}
    }
}

