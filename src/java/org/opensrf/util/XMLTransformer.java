package org.opensrf.util;
import javax.xml.transform.*;
import javax.xml.transform.stream.*;
import javax.xml.parsers.*;
import java.io.File;
import java.io.ByteArrayInputStream;
import java.io.OutputStream;
import java.io.ByteArrayOutputStream;


/**
 * Performs XSL transformations.  
 * TODO: Add ability to pass in XSL variables
 */
public class XMLTransformer {

    /** The XML to transform */
    private Source xmlSource;
    /** The stylesheet to apply */
    private Source xslSource;

    public XMLTransformer(Source xmlSource, Source xslSource) {
        this.xmlSource = xmlSource;
        this.xslSource = xslSource;
    }

    public XMLTransformer(String xmlString, File xslFile) {
        this(
            new StreamSource(new ByteArrayInputStream(xmlString.getBytes())),
            new StreamSource(xslFile));
    }

    public XMLTransformer(File xmlFile, File xslFile) {
        this(
            new StreamSource(xmlFile),
            new StreamSource(xslFile));
    }

    /** 
     * Applies the transformation and puts the result into the provided output stream
     */
    public void apply(OutputStream outStream) throws TransformerException, TransformerConfigurationException {
        Result result = new StreamResult(outStream);
        Transformer trans = TransformerFactory.newInstance().newTransformer(xslSource);
        trans.transform(xmlSource, result);
    }

    /**
     * Applies the transformation and return the resulting string
     * @return The String created by the XSL transformation
     */
    public String apply() throws TransformerException, TransformerConfigurationException {
        OutputStream outStream = new ByteArrayOutputStream();
        this.apply(outStream);
        return outStream.toString();
    }
}


