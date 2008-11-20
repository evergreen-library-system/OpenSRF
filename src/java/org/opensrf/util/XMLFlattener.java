package org.opensrf.util;

import javax.xml.stream.*;
import javax.xml.stream.events.* ;
import javax.xml.namespace.QName;
import java.util.Map;
import java.util.HashMap;
import java.util.List;
import java.util.ArrayList;
import java.util.ListIterator;
import java.io.InputStream;
import org.opensrf.util.JSONWriter;
import org.opensrf.util.JSONReader;

import com.ctc.wstx.stax.WstxInputFactory;

/**
 * Flattens an XML file into a properties map.  Values are stored as JSON strings or arrays.
 * An array is created if more than one value resides at the same key.
 * e.g. html.head.script = "alert('hello');"
 */
public class XMLFlattener {

    /** Flattened properties map */
    private Map<String, String> props;
    /** Incoming XML stream */
    private InputStream inStream;
    /** Runtime list of encountered elements */
    private List<String> elementList;

    /**
     * Creates a new reader. Initializes the message queue.
     * Sets the stream state to disconnected, and the xml
     * state to in_nothing.
     * @param inStream the inbound XML stream
     */
    public XMLFlattener(InputStream inStream) {
        props = new HashMap<String, String>();
        this.inStream = inStream;
        elementList = new ArrayList<String>();
    }

    /** Turns an array of strings into a dot-separated key string */
    private String listToString() {
        ListIterator itr = elementList.listIterator();
        StringBuffer sb = new StringBuffer();
        while(itr.hasNext()) {
            sb.append(itr.next());
            if(itr.hasNext())
                sb.append(".");
        }
        return sb.toString();
    }

    /**
     * Parses XML data from the provided stream.
     */
    public Map read() throws javax.xml.stream.XMLStreamException {

        //XMLInputFactory factory = XMLInputFactory.newInstance();
        XMLInputFactory factory = new com.ctc.wstx.stax.WstxInputFactory();

        /** disable as many unused features as possible to speed up the parsing */
        factory.setProperty(XMLInputFactory.IS_REPLACING_ENTITY_REFERENCES, Boolean.TRUE);
        factory.setProperty(XMLInputFactory.IS_SUPPORTING_EXTERNAL_ENTITIES, Boolean.FALSE);
        factory.setProperty(XMLInputFactory.IS_NAMESPACE_AWARE, Boolean.FALSE);
        factory.setProperty(XMLInputFactory.IS_COALESCING, Boolean.FALSE);
        factory.setProperty(XMLInputFactory.SUPPORT_DTD, Boolean.FALSE);

        /** create the stream reader */
        XMLStreamReader reader = factory.createXMLStreamReader(inStream);
        int eventType;

        while(reader.hasNext()) {
            /** cycle through the XML events */

            eventType = reader.next();
            if(reader.isWhiteSpace()) continue;

            switch(eventType) {

                case XMLEvent.START_ELEMENT:
                    elementList.add(reader.getName().toString());
                    break;

                case XMLEvent.CHARACTERS:
                    String text = reader.getText();
                    String key = listToString();

                    if(props.containsKey(key)) {

                        /* something in the map already has this key */

                        Object o = null;
                        try {
                            o = new JSONReader(props.get(key)).read();
                        } catch(org.opensrf.util.JSONException e){}

                        if(o instanceof List) {
                            /* if the map contains a list, append to the list and re-encode */
                            ((List) o).add(text);

                        } else {
                            /* if the map just contains a string, start building a new list
                             * with the old string and append the new string */
                            List<String> arr = new ArrayList<String>();
                            arr.add((String) o);
                            arr.add(text);
                            o = arr;
                        }

                        props.put(key, new JSONWriter(o).write());

                    } else {
                        props.put(key, new JSONWriter(text).write());
                    }
                    break;

                case XMLEvent.END_ELEMENT: 
                    elementList.remove(elementList.size()-1);
                    break;
            }
        }

        return props;
    }
}




