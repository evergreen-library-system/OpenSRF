package org.opensrf.test;
import org.opensrf.util.XMLTransformer;
import java.io.File;

public class TestXMLTransformer {
    /**
     * arg[0] path to an XML file
     * arg[1] path to the XSL file to apply
     */
    public static void main(String[] args) {
        try {
            File xmlFile = new File(args[0]);
            File xslFile = new File(args[1]);
            XMLTransformer t = new XMLTransformer(xmlFile, xslFile);
            System.out.println(t.apply());
        } catch(Exception e) {
            e.printStackTrace();
        }
    }
}


