package org.opensrf.test;
import org.opensrf.util.Logger;
import org.opensrf.util.FileLogger;


/** Simple test class for tesing the logging functionality */
public class TestLog {
    public static void main(String args[]) {
       Logger.init(Logger.DEBUG, new FileLogger("test.log")); 
       Logger.error("Hello, world");
       Logger.warn("Hello, world");
       Logger.info("Hello, world");
       Logger.debug("Hello, world");
    }
}
