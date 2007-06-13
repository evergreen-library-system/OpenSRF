import xml.dom.minidom, re

def osrfXMLFileToObject(filename):
    """Turns the contents of an XML file into a Python object"""
    doc = xml.dom.minidom.parse(filename)
    obj = osrfXMLNodeToObject(doc.childNodes[0])
    doc.unlink()
    return obj

def osrfXMLStringToObject(string):
    """Turns an XML string into a Python object"""
    doc = xml.dom.minidom.parseString(string)
    obj = osrfXMLNodeToObject(doc.childNodes[0])
    doc.unlink()
    return obj

def osrfXMLNodeToObject(xmlNode):
    """Turns an XML node into a Python object"""
    obj = {}

    if xmlNode.nodeType != xmlNode.ELEMENT_NODE:
        return obj

    done = False
    nodeName = xmlNode.nodeName

    for nodeChild in xmlNode.childNodes:
        if nodeChild.nodeType == xmlNode.ELEMENT_NODE:

            # If a node has element children, create a new sub-object 
            # for this node, attach an array for each type of child
            # and recursively collect the children data into the array(s)

            if not obj.has_key(nodeName):
                obj[nodeName] = {}

            sub_obj = osrfXMLNodeToObject(nodeChild);

            if not obj[nodeName].has_key(nodeChild.nodeName):
                # we've encountered 1 sub-node with nodeChild's name
                obj[nodeName][nodeChild.nodeName] = sub_obj[nodeChild.nodeName]

            else:
                if isinstance(obj[nodeName][nodeChild.nodeName], list):
                    # we already have multiple sub-nodes with nodeChild's name
                    obj[nodeName][nodeChild.nodeName].append(sub_obj[nodeChild.nodeName])

                else:
                    # we already have 1 sub-node with nodeChild's name, make 
                    # it a list and append the current node
                    val = obj[nodeName][nodeChild.nodeName]
                    obj[nodeName][nodeChild.nodeName] = [ val, sub_obj[nodeChild.nodeName] ]

            done = True

    if not done:
        # If the node has no element children, clean up the text content 
        # and use that as the data
        xmlNode = xmlNode.childNodes[0] # extract the text node
        data = re.compile('^\s*').sub('', str(xmlNode.nodeValue))
        data = re.compile('\s*$').sub('', data)

        obj[nodeName] = data

    return obj


def osrfObjectFindPath(obj, path, idx=None):
    """Searches an object along the given path for a value to return.

    Path separaters can be '/' or '.', '/' is tried first."""

    parts = []

    if re.compile('/').search(path):
        parts = path.split('/')
    else:
        parts = path.split('.')

    for part in parts:
        try:
            o = obj[part]
        except Exception:
            return None
        if isinstance(o,str): 
            return o
        if isinstance(o,list):
            if( idx != None ):
                return o[idx]
            return o
        if isinstance(o,dict):
            obj = o
        else:
            return o

    return obj


            

