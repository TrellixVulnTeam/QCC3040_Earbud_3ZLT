############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2013 - 2016 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
import os
from glob import glob

from .lib_util import make_octet_array
from csr.interface.mib_xml_parser import read_mib_xml

class MIBMetaData:
    '''
    Convenience class to give access to a single MIB's metadata.  The main point
    is to give type-aware access where the raw MIBType class just returns
    (unicode) strings for everything.
    '''
    def __init__(self,mib):
        '''
        Initialise with a single MIBType object
        '''
        self.mib = mib

    def default(self):
        '''
        If there's a default, returns an integer or an octet array containing
        it.  If there's no default, returns None.
        '''
        try:
            default_str = self.mib["default"]
        except KeyError:
            return None

        if self.mib["type"] == "octet_string":
            #Convert to an octet array
            clean_string = str(default_str).replace(" ","").replace("[","").replace("]","")
            return make_octet_array(clean_string)
        elif self.mib["type"] == "boolean":
            #Convert boolean to 0 or 1
            if str(default_str).lower() == "false":
                return 0
            elif str(default_str).lower() == "true":
                return 1
            else:
                return None
        else:
            #Convert integer(hex or decimal) to integer
            return int(default_str, 0)

    def type_string(self):
        '''
        Returns the type string from the XML
        '''
        #MIBType contains unicode strings thanks to the DOM parser so we convert
        #to plain strings
        return str(self.mib["type"])

    def is_integer(self):
        '''
        Is this MIB key an integer type?
        '''
        return self.type_string() != "octet_string"

    def psid(self):
        '''
        Return the MIBID/PSID of the MIB
        '''
        return int(self.mib["psid"])

    def getmib(self):
        '''
        Return mib information
        '''
        return self.mib

class MIBDB:
    '''
    Name-lookup database of MIB metadata.

    Parses the MIB metadata XML using core Python tools code and turns it into
    a dictionary of (name,MIBMetaData) pairs.

    Callers can specify the MIB metadata file but the standard
    file in the same tree as this file is picked up by default.
    '''
    def __init__(self, file="",results = None):
        '''
        Parse the XML and create the name-based dictionary of MIBMetaDatas.
        '''

        self.results = results
        
        # Initialise the MIB dictionary to be empty, it will remain empty if 
        # reading the XML fails.
        self.mib_dict = {}

        if not file:
            localdir = os.path.dirname(os.path.abspath(__file__))
            mib_fileglob = os.path.sep.join([localdir,"..","..","..","..","..",
                                             "common","mib","*_mib.xml"])
            mib_filenames = glob(mib_fileglob)
            if len(mib_filenames) != 1:
                raise RuntimeError("Expected exactly one file '*_mib.xml' in common/mib!")
            mib_filename = mib_filenames[0]
        else:
            mib_filename = file

            # B-256662: For Curator HTOL If the MIB XML file 
            # is empty, we continue without loading any MIB database
            # and don't raise an error.
            # NB We test this by attempting to open the file because this
            # constructor is expected to raise IOError in the absence of the
            # supplied file
            if not open(mib_filename).read():
                return
                
        xml_data = read_mib_xml(mib_filename)
        self.xmlmetadata = xml_data["metadata"]
        mib_array = xml_data["mibs"]
        for mib in mib_array:
            self.mib_dict[mib["name"]] = MIBMetaData(mib)

    def _get_mib(self,name):
        '''
        Basic access to a particular key's metadata.  Logs any KeyErrors if
        a logger has been provided.
        '''
        try:
            return self.mib_dict[name]

        except KeyError:
            if self.results:
                self.results.log("Bad MIB access (%s)" % name)
            raise

    def __getitem__(self,name):
        '''
        Read subscript operator.  Note there is not corresponding write
        operator - this is a read-only database.
        '''
        return self._get_mib(name)

    def __iter__(self):
        '''
        Calls the dictionary iterator.
        '''
        return self.mib_dict.__iter__()

    def items(self):
        return iter(self.mib_dict.items())

    def get_xmlmetadata(self):
        '''
        Gets the XML metadata as a dictionary
        '''
        return self.xmlmetadata;
