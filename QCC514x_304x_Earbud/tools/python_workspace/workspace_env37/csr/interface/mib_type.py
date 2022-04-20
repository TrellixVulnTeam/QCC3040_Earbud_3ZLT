
class MIBTypeError(TypeError):
    pass

class MIBType:
    """
    Class to represent a MIB entry.  
        
    The class is implemented to look somewhat like a normal dictionary, with
    additional checking that invalid keys can't be added.
    
    The list 'keys' stores the allowed key names.
    """

    #Allowed keys
    keys = ["name",
            "psid",
            "label",
            "get",
            "set",
            "description_user",
            "description_internal",
            "default",
            "range_min",
            "range_max",
            "type",
            "is_internal",
            "format"]

    #Every MIB entry has to have a name and an id
    def __init__(self,name,id):
        self.values = {}
        self["name"] = name
        self["psid"] = id 

    #Dictionary-like interface
    def __len__(self):
        return len(self.values)

    # Read-subscripting with new or old keys
    def __getitem__(self,key):
        try: 
            return self.values[key]
        except KeyError:
            raise
    # Write-subscripting with new or old keys
    def __setitem__(self,key,value):
        if not key in self.keys:
            raise IndexError("%s not a valid key for a MIB entry" % key)
        self.values[key] = value

    # Inclusion test with new or old key
    def __contains__(self,item):
        if not item in self.values:
            return False
        return True
    
    #Iteration (implicitly uses new keys)
    def __iter__(self):
        return self.values.__iter__()

        
    #Self-validation
    def validate(self):
        return "name" in self.values and "psid" in self.values

    def has_unmatched_set_function(self):
        return "set" in self.values and not "get" in self.values
