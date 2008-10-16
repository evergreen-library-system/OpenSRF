package org.opensrf.util;
import java.io.BufferedWriter;
import java.io.FileWriter;


public class FileLogger extends Logger {

    /** File to log to */
    private String filename;

    /** 
     * FileLogger constructor
     * @param filename The path to the log file
     */
    public FileLogger(String filename) {
        this.filename = filename;
    }

    /**
     * Logs the mesage to a file.
     * @param level The log level
     * @param msg The mesage to log
     */
    protected synchronized void log(short level, String msg) {
        if(level > logLevel) return;

        BufferedWriter out = null;
        try {
            out = new BufferedWriter(new FileWriter(this.filename, true));
            out.write(formatMessage(level, msg) + "\n");

        } catch(Exception e) {
            /** If we are unable to write our log message, go ahead and
              * fall back to the default (stdout) logger */
            Logger.init(logLevel, new Logger());
            Logger.logByLevel(ERROR, "Unable to write to log file " + this.filename);
            Logger.logByLevel(level, msg);
        }

        try {
            out.close();
        } catch(Exception e) {}
    }
}
