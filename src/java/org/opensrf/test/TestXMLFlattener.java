package org.opensrf.test;
import org.opensrf.util.XMLFlattener;
import java.io.FileInputStream;

public class TestXMLFlattener {
    public static void main(String args[]) throws Exception {
        FileInputStream fis = new FileInputStream(args[0]);
        XMLFlattener f = new XMLFlattener(fis);
        System.out.println(f.read());
    }
}
