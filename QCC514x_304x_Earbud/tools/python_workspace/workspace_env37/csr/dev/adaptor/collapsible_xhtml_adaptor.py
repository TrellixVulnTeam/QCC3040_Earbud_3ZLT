############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2013 - 2020 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
"""Generates a collapsible HTML view of a Model, to HTML5 standards."""

import platform
from xml.etree import ElementTree as ET
from csr.dev.adaptor.xhtml_adaptor import XHTMLAdaptor
from csr.dev.model import interface

if platform.python_version_tuple() >= ('3',):
    unichr = chr

# Much of the menu display and section folding originates from BlueCore
# CoreTools at //depot/bc/core_tools/CoreTools/Document/Document.pm

class CollapsibleXHTMLAdaptor(XHTMLAdaptor):
    """\
    Generates a collapsible XHTML view of a Model.

    Does this by

    1. writing a small set of Javascript functions into the report
    which can modify for a given group whether the "expanded element" (which is
    a span containing the group's child tree) and a collapse control (a "[-]"
    hyperlink) or the "collapsed element" (a text element "[]") and an expand
    control (a "[+]" hyperlink) are displayed;
    2. creating these elements in the body of the HTML.

    The whole thing is of course recursive: the "expanded element" becomes the
    parent of all inner markup.

    The JS functions must be passed a unique identifier for each call to ensure
    that the right element is collapsed/expanded.  These are automatically
    generated in a simple numeric sequence.  The class also keeps track of the
    recursion level

    """
    # pylint:disable=too-few-public-methods, too-many-instance-attributes

    def __init__(self, model, out_stream, default_expand_level=0):
        # pylint:disable=super-init-not-called
        # The base class __init__ is not called because we need initialisation
        # of _menu and other properties.

        self._name = 0
        self._level = 1
        self._default_expand_level = default_expand_level

        # We have to make a choice. Are we writing out an entire document
        # or an XHTML fragment for embedding in a larger document.
        #
        # If we were doing the latter, it would make it difficult for us to
        # use anything in a global namespace, most notably Javascript
        # functions, as we might clash with other definitions in the
        # document in which we're embedded.
        #
        # Also, some browsers have been known not to accept Javascript in the
        # body of a document accepting it only in the head.
        #
        # So, we'll take the decision to write out a whole document suitable
        # for dumping directly into a .html file.

        # The root element of an HTML document is <html>.
        self._root = ET.Element('html', attrib={"lang" : "en"})
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
        self._body = ET.SubElement(self._root, 'body',
                                   attrib={"onload": "loaded()"})
        self._body.tail = '\n'

        menu_c = ET.SubElement(self._body, 'div',
                               attrib={"class": "menu_container",
                                       "id":    "menu",
                                       "style": "display: none"})
        menu_c.tail = '\n'
        menu_s = ET.SubElement(menu_c, 'div',
                               attrib={"class": "menu_shadow"})
        menu_s.tail = '\n'
        menu = ET.SubElement(menu_s, 'div', attrib={"class" : "menu"})
        menu.tail = '\n'
        menu_b = ET.SubElement(menu, 'a',
                               attrib={"class": "menu_button_right",
                                       "href" : "javascript:menu_flip()"})
        menu_b.text = unichr(8596)
        menu_b.tail = '\n'
        ind = ET.SubElement(menu, 'span')
        ind.text = 'Index'
        ind.tail = '\n'

        self._menu = ET.SubElement(menu, 'ul')
        self._menu.tail = '\n'

        for model_item in to_render:
            self._current = self._body
            self._serialise(model_item)

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
        if platform.python_version_tuple() < ('3',):
            ET.ElementTree(self._root).write(out_stream, method="html")
        else:
            ET.ElementTree(self._root).write(out_stream, method="html",
                                             encoding="unicode")

    def _get_next_name(self):
        self._name += 1
        return str(self._name)

    def _down_level(self):
        self._level += 1

    def _up_level(self):
        self._level -= 1

    @property
    def _default_expanded(self):
        return self._level < self._default_expand_level

    def _collapse_expand_functions(self):
        script_el = ET.SubElement(self._current, "script",
                                  attrib={"type" : "text/javascript"})
        script_el.text = """
function change_display(id, state) {
var el = document.getElementById(id);
if (el)
el.style.display=state;
}
function hide(id) {
change_display(id,'none');
}
function show(id, state) {
change_display(id, '');
}
function expand(id) {
show(id + "-expanded");
show(id + "-collapse");
hide(id + "-expand");
hide(id + "-collapsed");
}
function collapse(id) {
hide(id + "-expanded");
hide(id + "-collapse");
show(id + "-expand");
show(id + "-collapsed");
}
function unfold_and_goto(target) {
  var targets = document.getElementsByName(target);
  if (targets.length != 0)
  {
    for(var node = targets[0]; node; node = node.parentNode)
      if (node.id) {
        if (node.id.length > 5 &&
            node.id.substr(node.id.length-9) == '-expanded')
          expand(node.id.substr(0, node.id.length-9));
        else if (node.id.length > 6 &&
          node.id.substr(node.id.length-10) == '-collapsed')
          collapse(node.id.substr(0, node.id.length-10));
    }
    expand(target);
  }
  window.location.href = '#' + target;
}
function menu_flip() {
 var el = document.getElementById("menu") ;
 if (el.style.right == 'auto') {
   el.style.right = '0.5em';
   el.style.left = 'auto';
 } else {
   el.style.right = 'auto';
   el.style.left = '0.5em';
 }
}
function menu_button(el)
{
  el.className = (el.className == "open") ? "closed" : "open";

  /* IE8 needs a hint to recalculate styles. */
  var menu = document.getElementById("menu");
  old_class = menu.className;
  menu.className = "repaint";
  menu.className = old_class;
}
function add_menu_folds(el)
{
  if (!el)
    return;

  for(var c = el.firstChild; c; c = c.nextSibling) {
    add_menu_folds(c);
    if (c.tagName == "UL") {
      var button = document.createElement("A");
      button.className = "closed";
      button.onclick = function() { menu_button(button) };
      el.insertBefore(button, c);
    }
  }
}
function loaded() {
  /* The menu technique we're using doesn't work in IE7, detect it. */
  if (document.querySelector) {
    var menu = document.getElementById("menu");
    if (menu) {
      add_menu_folds(menu);
      menu.style.display = "";
    }
  }
}
"""
        script_el.tail = '\n'

    @property
    def _document_css(self):
        """
        The CSS for this style of document, as opposed to the CSS needed to
        render common element.
        """
        return """
table {
  border-spacing: 0em;
}
td th {
  padding-left: 0.2em;
  padding-right: 0.2em;
}
.menu_container {
  position: fixed;
  top: 0.5em;
  right: 0.5em;
  max-height: 95%;
  overflow: auto;
  overflow-x: hidden;
  z-index: 10;
  display: none
}
@media screen {
  .menu_container {
    display: block;
  }
}
.menu {
  background: #ffa;
  border: thin solid #f84;
  padding: 0.25em;
  position: relative;
  left: -0.25em;
  top: -0.25em;
  font-size: small;
  min-width: 6em;
}
.menu_shadow {
  position: relative;
  left: 0.25em;
  top: 0.25em;
  background: #888;
  margin-right: 0.25em;
  margin-bottom: 0.25em;
}
.menu_button_right, .open:before, .closed:before {
  width: 1em;
  height: 1em;
  background: #fc8;
  border: thin outset #f84;
  margin: 0.1em;
  padding: 0.1em;
  text-align: center;
  text-decoration: none;
  font-family: sans-serif;
  font-size: x-small;
  font-weight: bold;
  color: black;
}
.menu_button_right, .open, .closed {
  cursor: pointer;
}
.menu_button_right {
  display: block;
}
.menu_button_right {
  float: right;
  margin-left: 0.25em;
}
.menu_button_right:active, .open:active:before, .closed:active:before {
  border: thin inset #f84;
  color: black;
}
.menu ul {
  list-style: none;
  margin-left: 0.5em;
  padding: 0em;
}
.menu > ul {
  margin: 0em;
  clear: both;
  min-width: 30em;
}
.open:before {
  content: "^";
}
.closed:before {
  content: "v";
}
.open + ul {
  display: block;
}
.closed + ul {
  display: none;
}
"""

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
        self._collapse_expand_functions()

    def _make_folding(self, element, name, suffix, show):
        # pylint: disable=no-self-use
        elem = ET.SubElement(element, 'span',
                             attrib={"id" : name + "-" + suffix})
        if not show:
            elem.set('style', "display: none")
        elem.tail = '\n'
        return elem

    def _make_fold_control(self, element, name, action, show, text):
        # pylint: disable=too-many-arguments
        fold = self._make_folding(element, name, action, show)
        but = ET.SubElement(fold, "a",
                            attrib={"href" :
                                    "javascript:%s('%s')" % (action, name)})
        but.text = text

    def _collapsible_section(self, element, contents, name):
        """
        Make a collapsible section with associated collapse/expand controls,
        returning the parent element of the expanded content.
        The collapsed content is 'Section folded'.
        """

        errors = contents.findall(".//p[@class='error']")
        warnings = contents.findall(".//p[@class='warning']")
        expand = True if warnings or errors else self._default_expanded

        self._make_fold_control(element, name, "collapse", expand, '[-]')
        self._make_fold_control(element, name, "expand", not expand, '[+]')

        collapsed = self._make_folding(element, name, "collapsed", not expand)
        collapsed.text = "Section folded."

        contents.set('id', name + "-expanded")
        if not expand:
            contents.set('style', "display: none")

        element.append(contents)
        return expand

    def _add_heading_at_name_level(self, parent, text, name):
        """
        Add a heading of level self._level
        """
        heading = ET.SubElement(parent, 'h%d' % self._level)
        anchor = ET.SubElement(heading, 'a', attrib={"name" : name})
        anchor.text = text
        heading.tail = '\n'


    def _serialise_titled_group(self, section, title, members):
        name = 'sec_' + self._get_next_name()

        self._add_heading_at_name_level(section, title, name)

        menuitem = ET.SubElement(self._menu, 'li')
        menuitem.tail = '\n'
        menutitle = ET.SubElement(
            menuitem, 'a',
            attrib={"href": "javascript:unfold_and_goto('%s')" % name})
        menutitle.text = title

        parentmenu = self._menu
        self._menu = ET.Element('ul')

        contents, worth_folding = self._serialise_group_members(members)

        if self._menu.find('li') is not None:
            menuitem.append(self._menu)
        self._menu = parentmenu

        if list(contents):
            if worth_folding:
                self._collapsible_section(section, contents, name)
            else:
                section.append(contents)
        else:
            # If a section has a title and is empty the put in a message to
            # say that it's an empty session, otherwise things are confusing.
            # Then, since the expanded part of the section is as small as the
            # message saying it's folded, don't bother folding it.
            contents.text = "Nothing to report."
            section.append(contents)

        # Because we have a title and at least one line of output, we're
        # always worth folding.
        return True


    def _serialise_untitled_group(self, section, members):
        contents, worth_folding = self._serialise_group_members(members)

        section.append(contents)

        return worth_folding


    def _serialise_group_members(self, members):
        contents = ET.Element('div')
        contents.tail = '\n'

        worth_folding = False

        self._down_level()

        for member in members:
            self._current = contents
            if self._serialise_wf(member):
                worth_folding = True

        self._up_level()

        # If we contain more than one output item then this group is worth
        # folding even if each item on its own was not worth folding.
        if len(list(contents)) > 1:
            worth_folding = True

        return contents, worth_folding


    def _serialise_group(self, group):
        """\
        Map Group to <section> with optional <h1> title and render all members
        (recursively).  If (and only if) a group has members, make its
        title element collapsible with the members descended from the
        "expanded" element
        """
        self._serialise_group_wf(group)


    # The _wf (worth_folding) variants of the serialisation functions return a
    # boolean to indicate whether the contents are so short they're not worth
    # folding.

    def _serialise_group_wf(self, group):
        """
        As _serialise_group but returns a boolean to indicate whether the
        contents are sufficiently long to be worth folding.
        """
        section = ET.SubElement(self._current, 'section')
        section.tail = '\n'

        if group.title:
            return self._serialise_titled_group(section, group.title,
                                                group.members)

        return self._serialise_untitled_group(section, group.members)


    def _serialise_code_wf(self, code):
        """
        As _serialise_code but returns a boolean to indicate whether the
        contents are sufficiently long to be worth folding.
        """
        self._serialise_code(code)
        # The test on the length of the string is primarily to avoid having to
        # search long strings just to find out that they do, indeed, contain
        # non-trivial newlines.
        return len(code.text) > 132 or '\n' in code.text.rstrip()


    def _serialise_text_wf(self, text):
        """
        As _serialise_text but returns a boolean to indicate whether the
        contents are sufficiently long to be worth folding.
        """
        self._serialise_text(text)
        # Deciding whether a text string is so long it's worth folding is
        # a bit harder than for code. We want to allow a little more as the
        # text will be being displayed in a proportionally spaced font.
        #
        # At the moment, we don't do anything special to newlines in Text
        # objects but in future we might so we'll trigger on that too.
        return len(text.text) > 200 or '\n' in text.text.rstrip()


    def _serialise_wf(self, model, depth=0):
        """
        As _serialise but returns a boolean to indicate whether the contents
        are sufficiently long to be worth folding.
        """

        # In general, it would be better if the analyses could tell us
        # explicitly whether it was worth folding their report. This would
        # give better consistency when a section has multiple subsections
        # some of which are small and some of which are long.
        #
        # The analyses could then give us some text to use when we fold their
        # reports.

        if isinstance(model, interface.Code):
            return self._serialise_code_wf(model)
        if isinstance(model, interface.Text):
            return self._serialise_text_wf(model)
        if isinstance(model, interface.Group):
            return self._serialise_group_wf(model)

        # If it's a class where we don't have explicit code them assume
        # that if there's any output then it's worth folding.
        #
        # To deal with the common case of serialisers that either output
        # nothing our output a long response, check the number of elements
        # in self._current before and after this call. If it's not grown
        # then assume we added nothing interesting.
        #
        # In general, this is not a great technique, but it avoids having
        # to rewrite all the serialisers.
        #
        # Tables are deliberately included here. Because of the borders
        # they're always quite large even if they're a single row without
        # a header. If we want to change that decision, it's fairly easy

        old_len = len(list(self._current))
        self._serialise(model, depth)
        return len(list(self._current)) > old_len
