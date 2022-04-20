from csr.wheels.global_streams import iprint
from xml.dom import minidom, Node
from .mib_type import MIBType, MIBTypeError
import os

"""
Module for parsing XML MIB metatata files and stuffing the contents
into an array of MIBTypes.
"""

def _processChildren(children,mib):
    """
    (Slightly) recursive function that sets up MIBTypes for
    entries in the MIB metadata file.  In general uses the plain XML
    tagname as the key and the plain text data as the value for each
    field in the MIBType.  The one exception is for get/set
    functions, which involve the only element nodes which don't have one
    child (i.e. text data).
    """
    for child in children:
        if child.nodeType == Node.ELEMENT_NODE:
            if child.hasChildNodes():
                if len(child.childNodes) == 1:
                    if child.childNodes[0].nodeType == Node.TEXT_NODE:
                        #Transparently insert an entry into the MIBType.
                        #If the XML tag is "temporality", silently ignore it;
                        #otherwise, fall over on unknown tags.
                        try:
                            mib[child.tagName] = child.childNodes[0].data
                        except IndexError:
                            if child.tagName != "temporality":
                                raise
                else:
                    _processChildren(child.childNodes,mib)
            else:
                mib[child.getAttribute("type")] = child.getAttribute("function_name")
                

def _processConfigElement(element):
    """
    Simple top-level constructor of individual MIB entry
    """
    mib = MIBType(element.getAttribute("name"),
                 element.getAttribute("psid"))
    if element.hasChildNodes():
        _processChildren(element.childNodes,mib)
        
    if mib.has_unmatched_set_function():
        raise MIBTypeError(
                  "set function with no get function for key %s" % mib["name"])
    return mib
    
def read_mib_xml(mibfile, subsystem_name=""):
    """
    Reads the metadata section of the XML-format MIB metadata file.
    Assumes there is a metadata section with a subsystem name matching the
    filename. 
    "mibfile" must be a file name unless subsystem_name is given.  The subsystem
    name is determined from the filename if not.
    """

    try:
        doc = minidom.parse(mibfile)
    
        if not subsystem_name:
            # Get subsystem name from the mib file name. For Curator patches
            # there's an extra "_patch" suffix to remove as well. 
            subsystem_name = os.path.basename(mibfile).replace("_mib.xml",
                                                               "").replace("_patch",
                                                                           "")
        # The hydra stub code has the name "generic" in the xml.
        if subsystem_name == "hydra":
            subsystem_name = "generic"
        
        for metadata in doc.getElementsByTagName("metadata"):
            if metadata.getAttribute("subsystem_name") == subsystem_name:
                mibs = [_processConfigElement(e) \
                         for e in metadata.getElementsByTagName(
                                                             "config_element")]
                return {"metadata":dict(list(metadata.attributes.items())),
                        "mibs":mibs}
        
        iprint("ERROR: No <metadata> section found for %s" % subsystem_name)
        return False
    
    except MIBTypeError as e:
        iprint("ERROR: mib_xml_parser: %s" % e)
        return False
