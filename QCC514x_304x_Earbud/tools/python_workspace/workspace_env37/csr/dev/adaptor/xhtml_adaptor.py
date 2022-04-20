############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2013 - 2016 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
from csr.dev.adaptor.base_stream_adaptor import BaseStreamAdaptor
from xml.etree import ElementTree as ET

class XHTMLAdaptor (BaseStreamAdaptor):
    """\
    Generates an HTML view of a Model, to HTML5 standards (not that that is).
    
    Feel free to produce fancy adaptors, especially js ones, but don't 
    pollute this one beyond basic html & styles.
    
    Sample view:
        
    <!DOCTYPE html>
    <html><head><meta charset="UTF-8">
    <style ...
        </style>
    <title>flashheart device</title>
    </head>
    <body>
      <p><a class="warning" href="#_Warning1">Warning: Your text here</a></p>    <section>
        <h1>amber-lab-or-similar device</h1>
        <section>
            <h2>amber-r01-v0 chip</h2>
            <section>
                <h3>Curator subsystem</h3>
                <section>
                    <h4>cpu core</h4>
                    <section>
                        <h5>Firmware</h5>
                    </section>
                </section>
                <section>
                    <h4>MMU</h4>
                    <section>
                        <h5>Buffer Summary</h5>
                        <pre>
                        bh size  used  offs   bh size  used  offs   bh size  used  offs   bh size  used  offs
                        01 01000 01000 00852  02 00200 00200 00144  03 00100 00100 00048  04 00040 00040 00020  
                        05 00100 00100 -----  07 00080 00080 0002c  
                        </pre>
                    </section>
                    <section>
                        <h5>Buffer 1</h5>
                        <pre>
                        Buffer 0x1 | Offset 0x852 | Size 0x6 | Pages 64
                        0000 :  f4 17 00 8a 99 dd f0 07 14 35 00 16 10 19 00 ff
                        0010 :  00 47 9c 82 00 8a 19 d3 00 8a 1a 1a 16 28 12 27
                        ...
    """
    def __init__(self, model, out_stream):

        self._name = 0
        self._level = 1

        # Need a dummy element as root
        self._root = ET.Element('html', attrib = {"lang":"en"})
        self._root.tail = '\n' 

        # All our Javascript and CSS goes into a <head> along with other things
        # that must go into <head> (such as document title, <meta> tags).
        self._current = ET.SubElement(self._root, 'head')
        self._current.tail = '\n'
        self._allow_text = False

        title = 'PyDbg Core Report'
        to_render = model

        try:
            title = model.title
        except AttributeError:
            pass

        try:
            to_render = model.members
            if self._default_expand_level > 0:
                self._default_expand_level -= 1
        except AttributeError:
            pass

        self._preamble()
        title_tag = ET.SubElement(self._current, 'title')
        title_tag.text = title
        title_tag.tail = '\n'

        # Our content goes into a <body> tag.
        self._body = ET.SubElement(self._root, 'body')
        self._body.tail = '\n'

        for model in to_render:
            self._current = self._body
            self._serialise(model)

        # Find errors and warning nodes
        # Iterating for "p" or "class" quicker/clearer? but requires more code
        self._errors = self._root.findall(".//p[@class='error']")
        self._warnings = self._root.findall(".//p[@class='warning']")

        self._prepend_errors_warnings_in_body()

        # We're going to declare ourselves as HTML5. The DOCTYPE declaration
        # is necessary to turn off quirks rendering mode in browsers.
        #
        # Note that the DOCTYPE declaration is command to the browser, not
        # HTML/XHTML/XML element. It must not have a closing tag.
        out_stream.writelines("<!DOCTYPE html>\n")

        # Need a tree to write it
        ET.ElementTree(self._root).write(out_stream, method="html")        

    def _prepend_errors_warnings_in_body(self):
        """
        Helper function to take the list of errors and warnings and insert them
        at the top of the ElementTree with hyper-links to the warning/error
        in the text.
        """
        # Always insert as the first item, rather than remembering insert point
        # hence reverse the list
        for ew in reversed(self._errors + self._warnings):
            # Applying the style to the link otherwise link follows browser defaults
            p = ET.Element('p')
            p.tail = '\n'
            a = ET.SubElement(p, 'a', {'class':ew.get("class"),
                                       'href':"#%s"%ew.get("id")})
            a.text = ew.text
            self._body.insert(0,p)

    @property
    def _document_css(self):
        """
        The CSS for this style of document, as opposed to the CSS needed to
        render common element.
        """
        return ""

    def _css_defs(self):
        css_el = ET.SubElement(self._current, "style", attrib={"type" : "text/css"})
        # Warning colour is darkorange
        css_el.tail = '\n'
        css_el.text = """
table, td, th { border: 1px solid gray }
.warning {
  color: #FF8C00;
} 
.error {
  color: red;
}
.cluster {
  border-spacing: 0em;
  font-size: x-small;
  empty-cells: show;
  border: none;
}
.cluster td {
  padding-top: 0em;
  padding-left: 0.2em;
  padding-right: 0.2em;
  padding-bottom: 0em;
  text-align: center;
  vertical-align: middle;
  border: none;
}
.cluster tr {
  height: 1em;
}
.cluster-center {
  font-size: medium;
}
td.cluster-top {
  vertical-align: bottom;
}
td.cluster-bottom {
  vertical-align: top;
}
td.cluster-left {
  text-align: right;
  padding-left: 0em;
}
td.cluster-right {
  text-align: left;
  padding-right: 0em;
}
.protocol-tree {
  border-spacing: 0.2em;
  border: none;
}
.protocol, .protocol_key {
  border: medium solid black;
  margin: 1em;
}
.protocol_key {
  background: #ddd;
}
.protocol-tree tr {
  vertical-align: middle;
}
.protocol-tree th, .protocol-gap {
  border: none;
}
""" + self._document_css

    def _down_level(self):
        self._level += 1

    def _up_level(self):
        self._level -= 1

    # Protected / BaseStreamAdaptor compliance

    def _preamble(self):
        """
        Write out the CSS definitions for the default display styles, and
        the JS functions for collapsing and expanding a particular node number
        """
        meta = ET.SubElement(self._current, 'meta',
                             attrib={"charset": "UTF-8"})
        meta.tail = '\n'
        self._css_defs()

    def _serialise_code(self, code, colour=None):
        """\
        Map Code to <pre> element
        
        Note we currently don't handle colour support for this, colour arguement
        is to provide a consistent interface
        """
        pre = ET.SubElement(self._current, 'pre')
        pre.text = code.text
        pre.tail = '\n'

    def _serialise_text(self, text):
        """\
        Map Code to <p> element
        """
        if self._allow_text:
            self._current.text = text.text
        else:
            p = ET.SubElement(self._current, 'p')
            p.text = text.text
            p.tail = '\n'

    def _serialise_warning(self, warning):
        """\
        Map Code to <p> element with an appropriate display class
        """
        p = ET.SubElement(self._current, 'p', {'class':'warning',
                                               'id':warning.label,})
        p.text = "Warning: " + warning.text
        p.tail = '\n'

    def _serialise_error(self, error):
        """\
        Map Code to <p> element with an appropriate display class
        """
        p = ET.SubElement(self._current, 'p', {'class':'error',
                                               'id':error.label,})
        p.text = "Error: " + error.text
        p.tail = '\n'

    def _add_heading_at_level(self, parent, text):
        """
        Add a heading of level self._level
        """
        heading = ET.SubElement(parent, 'h%d' % self._level)
        heading.text = text
        heading.tail = '\n'


    def _serialise_titled_group(self, section, title, members):
        self._add_heading_at_level(section, title)

        contents = self._serialise_group_members(members)

        if list(contents):
            section.append(contents)
        else:
            # If a section has a title and is empty the put in a message to
            # say that it's an empty session, otherwise things are confusing.
            contents.text = "Nothing to report."
            section.append(contents)

    def _serialise_untitled_group(self, section, members):
        contents = self._serialise_group_members(members)

        section.append(contents)

    def _serialise_group_members(self, members):
        contents = ET.Element('div')
        contents.tail = '\n'

        self._down_level()

        for member in members:
            self._current = contents
            self._serialise(member)

        self._up_level()

        return contents


    def _serialise_group(self, group):
        """\
        Map Group to <section> with optional <h1> title and render all members
        (recursively)
        """         
        section = ET.SubElement(self._current, 'section')
        section.tail = '\n'

        allow_text = self._allow_text
        self._allow_text = False

        try:
            if group.title:
                self._serialise_titled_group(section, group.title,
                                             group.members)
            else:
                self._serialise_untitled_group(section, group.members)
        finally:
            self._allow_text = allow_text


    def _serialise_table(self, table):
        """
        Outputs a table.
        """
        if not table.has_data_rows:
            return

        section = ET.SubElement(self._current, 'table',
                                attrib={"style":  "min-width:50%"})
        section.tail = '\n'
        for row in table.all_rows_for_adaptor:
            table_row = ET.SubElement(section, 'tr')
            table_row.tail = '\n  '
            for cur_item in row.cells:
                self._serialise_tablecell(table_row, cur_item)

    def _serialise_line_graph(self, graph):
        """
        Outputs a line graph. Or at least, it will when it is finished.

        At the moment, it doesn't know how to, so just outputs
        a table instead.
        """

        # Graphs-are-tables, so we can helpfully output the graph as a table.
        self._serialise_table(graph)

    def _serialise_cluster_table(self, table):
        """
        Outputs a Cluster that's already been converted to a table.
        """
        if not table.has_data_rows:
            return

        section = ET.SubElement(self._current, 'table',
                                attrib={"class":  "cluster",
                                        "border": "0"})
        section.tail = '\n'
        for row, rowname in zip([row.cells for row in
                                 table.all_rows_for_adaptor],
                                ["top", "middle", "bottom"]):
            table_row = ET.SubElement(section, 'tr')
            table_row.tail = '\n  '
            for cell, colname in zip(row, ["left", "center", "right"]):
                classes = []
                if rowname != "middle":
                    classes.append("cluster-" + rowname)
                if colname != "center" or not classes:
                    classes.append("cluster-" + colname)
                self._serialise_tablecell(table_row, cell,
                                          {'class': " ".join(classes)})

    def _serialise_protocol_tree_table(self, table):
        """
        Outputs a ProtocolTree that's already been converted to a table.
        """
        if not table.has_data_rows:
            return

        section = ET.SubElement(self._current, 'table',
                                attrib={"class":  "protocol-tree",
                                        "border": "0"})
        section.tail = '\n'
        for row in [row.cells for row in table.all_rows_for_adaptor]:
            table_row = ET.SubElement(section, 'tr')
            table_row.tail = '\n  '
            self._serialise_tablecell(table_row, row[0])
            self._serialise_tablecell(table_row, row[1],
                                      {"class": "protocol_key"})
            for cell in row[2:]:
                css_class = "protocol"
                if cell.contents == "":
                    css_class = "protocol-gap"
                self._serialise_tablecell(table_row, cell,
                                          {"class": css_class})

    def _serialise_tablecell(self, row, cell, attrib=None):
        """
        Outputs a cell from a table.
        """
        if attrib is None:
            attrib = {}

        attrib["align"] = cell.align
        if cell.nowrap:
            attrib["nowrap"] = "nowrap"
        if cell.columns > 1:
            attrib["colspan"] = str(cell.columns)

        tag = ET.SubElement(row, 'th' if cell.is_header else 'td', attrib)

        current = self._current
        allow_text = self._allow_text

        self._current = tag
        self._allow_text = True

        try:
            self._serialise(cell.contents)
        finally:
            self._current = current
            self._allow_text = allow_text

        tag.tail = "\n  "

