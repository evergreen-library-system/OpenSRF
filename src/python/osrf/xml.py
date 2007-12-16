import xml.dom.minidom, re

def osrfXMLFileToObject(filename):
    """Turns the contents of an XML file into a Python object"""
    doc = xml.dom.minidom.parse(filename)
    obj = osrfXMLNodeToObject(doc.documentElement)
    doc.unlink()
    return obj

def osrfXMLStringToObject(string):
    """Turns an XML string into a Python object"""
    doc = xml.dom.minidom.parseString(string)
    obj = osrfXMLNodeToObject(doc.documentElement)
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
            subObj = osrfXMLNodeToObject(nodeChild);
            __appendChildNode(obj, nodeName, nodeChild.nodeName, subObj)
            done = True

    for attr in xmlNode.attributes.values():
        __appendChildNode(obj, nodeName, attr.name, dict([(attr.name, attr.value)]))
        

    if not done and len(xmlNode.childNodes) > 0:
        # If the node has no element children, clean up the text 
        # content and use that as the data
        textNode = xmlNode.childNodes[0] # extract the text node
        data = unicode(textNode.nodeValue).replace('^\s*','')
        data = data.replace('\s*$','')

        if nodeName in obj:
            # the current element contains attributes and text
            obj[nodeName]['#text'] = data
        else:
            # the current element contains text only
            obj[nodeName] = data

    return obj


def __appendChildNode(obj, nodeName, childName, subObj):
    """ If a node has element children, create a new sub-object 
        for this node, attach an array for each type of child
        and recursively collect the children data into the array(s) """

    if not obj.has_key(nodeName):
        obj[nodeName] = {}

    if not obj[nodeName].has_key(childName):
        # we've encountered 1 sub-node with nodeChild's name
        if childName in subObj:
            obj[nodeName][childName] = subObj[childName]
        else:
            obj[nodeName][childName] = None

    else:
        if isinstance(obj[nodeName][childName], list):
            # we already have multiple sub-nodes with nodeChild's name
            obj[nodeName][childName].append(subObj[childName])

        else:
            # we already have 1 sub-node with nodeChild's name, make 
            # it a list and append the current node
            val = obj[nodeName][childName]
            obj[nodeName][childName] = [ val, subObj[childName] ]


def osrfObjectFindPath(obj, path, idx=None):
    """Searches an object along the given path for a value to return.

    Path separaters can be '/' or '.', '/' is tried first."""

    parts = []

    if re.search('/', path):
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


            

