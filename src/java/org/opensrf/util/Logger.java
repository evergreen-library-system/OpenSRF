package org.opensrf.util;
import java.text.SimpleDateFormat;
import java.text.FieldPosition;
import java.util.Date;

/**
 * Basic OpenSRF logging API.  This default implementation
 * logs to stderr.
 */
public class Logger {

    /** Log levels */
    public static final short ERROR = 1;
    public static final short WARN  = 2;
    public static final short INFO  = 3;
    public static final short DEBUG = 4;
    public static final short INTERNAL = 5;

    /** The global log instance */
    private static Logger instance;
    /** The global log level */
    protected static short logLevel;

    public Logger() {}

    /** Sets the global Logger instance
     * @param level The global log level.
     * @param l The Logger instance to use
     */
    public static void init(short level, Logger l) {
        instance = l;
        logLevel = level;
    }

    /** 
     * @return The global Logger instance
     */
    public static Logger instance() {
        return instance;
    }

    /**
     * Logs an error message
     * @param msg The message to log
     */
    public static void error(String msg) {
        instance.log(ERROR, msg);
    }

    /**
     * Logs an warning message
     * @param msg The message to log
     */
    public static void warn(String msg) {
        instance.log(WARN, msg);
    }

    /**
     * Logs an info message
     * @param msg The message to log
     */
    public static void info(String msg) {
        instance.log(INFO, msg);
    }

    /**
     * Logs an debug message
     * @param msg The message to log
     */
    public static void debug(String msg) {
        instance.log(DEBUG, msg);
    }

    /**
     * Logs an internal message
     * @param msg The message to log
     */
    public static void internal(String msg) {
        instance.log(INTERNAL, msg);
    }


    /** 
     * Appends the text representation of the log level
     * @param sb The stringbuffer to append to
     * @param level The log level
     */
    protected static void appendLevelString(StringBuffer sb, short level) {
        switch(level) {
            case DEBUG:
                sb.append("DEBG"); break;
            case INFO:
                sb.append("INFO"); break;
            case INTERNAL:
                sb.append("INT "); break;
            case WARN:
                sb.append("WARN"); break;
            case ERROR:
                sb.append("ERR "); break;
        }
    }

    /**
     * Formats a message for logging.  Appends the current date+time
     * and the log level string.
     * @param level The log level
     * @param msg The message to log
     */
    protected static String formatMessage(short level, String msg) {

        StringBuffer sb = new StringBuffer();
        new SimpleDateFormat("yyyy-MM-dd HH:mm:ss.SSS").format(
            new Date(), sb, new FieldPosition(0));

        sb.append(" [");
        appendLevelString(sb, level);
        sb.append(":");
        sb.append(Thread.currentThread().getId());
        sb.append("] ");
        sb.append(msg);
        return sb.toString();
    }

    /**
     * Logs a message by passing the log level explicitly
     * @param level The log level
     * @param msg The message to log
     */
    public static void logByLevel(short level, String msg) {
        instance.log(level, msg);
    }

    /**
     * Performs the actual logging.  Subclasses should override 
     * this method.
     * @param level The log level
     * @param msg The message to log
     */
    protected synchronized void log(short level, String msg) {
        if(level > logLevel) return;
        System.err.println(formatMessage(level, msg));
    }
}

