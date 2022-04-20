############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2021 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################

from .base_stream_adaptor import BaseStreamAdaptor
from collections import namedtuple


def evaluate(cell):
    # If we have an int or float that has been rendered to a string already, convert it back
    try:
        value = int(cell, 0)
    except ValueError:
        try:
            value = float(cell)
        except ValueError:
            value = cell
    except TypeError:
        value = cell

    try:
        value = value.strip()
    except AttributeError:
        pass
    return value

RenderedGroup = namedtuple("RenderedGroup", "content groups")

class ObjectAdaptor(BaseStreamAdaptor):
    """
    Attempts to "render back" renderables into Python objects.  This is only a 
    partial implementation because of the complexities of the more sophisticated
    types of table, and because sometimes information is irretrievably lost when
    raw Python objects get inserted into a renderable (Text, Table, Group etc).

    Because of the complexity of the object created it is highly recommended
    not to use this to render high level reports.  It is much more suitable 
    for small reports (e.g. pmalloc.info(), sqif_state(), etc).
    """
    def __init__(self, model):

        self._name = None
        self._object = None
        self._serialise(model)

    def _serialise_code(self, code, colour=None):
        """
        Text renderables are just converted back to strings
        """
        self._name = None
        self._object = code.text

    def _serialise_group(self, grp):
        """
        Groups are handled differently depending on what mix of member types they
        have.  
         - If all the members are subgroups, the object constructed is a
           dictionary of (subgroup name, de-rendered subgroup) pairs
         - If none of the members are subgroups, the object constructed is a list
           of de-rendered (non-group) members.
         - If there is a mix, the object constructed is a RenderedGroup, which is
           a small named tuple with attributes "content" (entry 0) and "groups"
           (entry 1).  "content" contains the de-rendered non-group members and
           "groups" contains a dictionary of subgroups.
        
        As an additional simplification, if the non-group members list has length
        1 its single entry is used in its own right instead.
        """
        content_list = []
        subgroups = {}
        for member in grp.members:
            self._serialise(member)
            if self._name is not None:
                subgroups[self._name] = self._object
            else:
                content_list.append(self._object)
        if len(content_list) == 1:
            content_list = content_list[0]
        else:
            # If content_list is a list of strings of a certain kind, it could be reformatted
            if all(isinstance(ctnt, str) for ctnt in content_list):
                content_list = [ctnt.strip() for ctnt in content_list]
                if all(len(ctnt.split(":")) == 2 for ctnt in content_list): 
                    content_split = [ctnt.split(":") for ctnt in content_list]
                    content_list = {evaluate(key) : evaluate(value) for key, value in content_split}


        self._name = grp.title

        if not subgroups and not content_list:
            self._object = None
        elif not subgroups:
            self._object = content_list
        elif not content_list:
            self._object = subgroups
        else:      
            self._object = RenderedGroup(content_list, subgroups)

    def _serialise_table(self, tbl):
        """
        A Table is rendered differently depending on whether it has headings or not.
        If so, it is turned into a list of dictionaries where each dictionary corresponds
        to a row, and the dictionary entries are (column heading, value) pairs.
        If not, it is simply turned into a list of lists, where the inner lists correspond
        to the rows.

        The values found in the rows are rendered back from strings to ints or floats if
        appropriate.  Otherwise they are left alone.
        """
        self._name = None 
            
        rows = tbl.all_rows_for_adaptor
        if rows and rows[0].is_header:
            # Turn each row of the table into a dictionary mapping heading to row value in that column
            headings = [cell.contents for cell in rows[0].cells]
            other_rows = rows[1:]
            self._object = [{hdg : evaluate(other_rows[i].cells[j].contents) 
                                    for (j,hdg) in enumerate(headings)} for i in range(len(other_rows))]

        else:
            # There are no headers, so the table is simply turned into a list of lists
            self._object = [[evaluate(cell.contents) for cell in row.cells] for row in rows]


def object_adaptor(model):
    return ObjectAdaptor(model)._object