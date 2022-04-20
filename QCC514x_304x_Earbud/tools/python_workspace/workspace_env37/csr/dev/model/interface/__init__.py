############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2013 - 2020 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
"""
Debugging Model Interface Classes

The classes are used to build the public interface of Model components.

As in Model-Adaptor-View.

They are converted to Views (e.g. xhtml, commandline, gui) by Adaptors

Used to decouple logical debugging resources from presentation.

Initially only used to represent static reports - but the plan is world
domination (including interactive guis).

Nb. Only Logical elements are represented here. If you feel tempted to add
anything presentation specific (e.g. "folds") - DONT

Similarly, although the Table object can be used to perform complex layouts
don't make the reports generate complex layouts by abusing Table objects.
Instead, make a new class and then you may choose (in BaseStreamAdaptor or in
individual adaptors) to generate the table object on the fly and pass that on
to the table renderer. This allows adaptors to perform more specialised
rendering for complex layouts rather than being restricted to a table.

Nb. Only application agnostic elements are allowed here. If you feel
tempted to add anything application/device specific - DONT.

Presentation _hints_ (e.g. preferred graph style for DataSeries) might be
acceptable if Adaptors really really can't make a good enough choice - and they
can't be stylised based on specific class/subclass.
"""

#pylint: disable=too-many-lines
import re
import itertools
from operator import itemgetter
from csr.wheels.bitsandbobs import TypeCheck

# Maximum length used for repr truncation
MAX_REPR_LEN = 40

def strll(text, max_len=MAX_REPR_LEN):
    """
    This is for use by the __repr__ method of an Interface object.
    Limits output length to max_len. It uses repr to
    escape the supplied text string.
    """

    # Truncate then escape. The length will be slightly variable but the exact
    # length was never important, this will prevent stopping part-way through
    # an escape sequence and it will ensure the closing quote is preserved.
    if len(text) > max_len:
        text = text[:max_len] + "..."
    return repr(text)

def as_strll(obj, max_len=MAX_REPR_LEN):
    """
    This is for optional use by the __repr__ method of an Interface object.
    Limits output length to max_len. It uses repr to
    escape the supplied interface object rendered as a string.
    """

    return strll(str(obj), max_len)

def sized(text):
    """
    This is for use by the __repr__ method of an Interface object.
    Return string containing size information for the interface object,
    when rendered as a string text, of the form "(N characters, M lines)".
    """

    chars = len(text)
    lines = len(text.splitlines())
    return "(chars:{}, lines:{})".format(chars, lines)

def as_sized_strll(obj):
    """
    This is for optional use by the __repr__ method of an Interface object.
    Escapes the supplied interface object rendered as a string.
    Limits output length.
    Appends size information: (N characters, M lines)
    """

    text = str(obj)
    escaped = as_strll(text)
    return escaped + ' ' + sized(text)

class Interface(object): #pylint: disable=too-few-public-methods
    """
    Provides an interface between rendering adaptors and components.
    This is an abstract base class, construct one of the derived classes.
    """
    def __str__(self):
        """
        Renders object to a string, e.g. for use at the interactive shell
        """

        from csr.dev.adaptor.text_adaptor import StringTextAdaptor
        return StringTextAdaptor(self)

    def  _simple_str(self):
        """
        method that defaults to str(self) but can be overridden as desired
        to suit its use in __repr__.
        """
        return str(self)

    def _extra_repr_info(self, textll):
        """
        Derived classes can override this to provide extra formatting for
        outputting in the __repr__ method without the need necessarily
        to completely override the default __repr__.
        It returns a list of strings,
        containing by default just the supplied textll,
        (which is the textual representation of limited length),
        which __repr__ can by default join with commas.
        """

        # pylint: disable=no-self-use
        return [textll]

    def __repr__(self):
        """
        Shortened representation for use in REPL.
        This gets truncated at a maximum length of MAX_REPR_LEN chars with
        ellipsis added.
        Adds a count of chars and lines in the representation of str(self).

        When overriding the __repr__ method of a derived class, don't go
        and call __repr__ of a component object because that will cause
        double-escaping of things like '\n' and quotes, etc., instead call
        the __str__ method, if you need component information.  But given the
        output length is limited, you may not find such a need anyway.
        """

        text = self._simple_str()
        textll = strll(text)
        return "<{}: {} {}>".format(
            self.__class__.__name__,
            ", ".join(self._extra_repr_info(textll)),
            sized(str(self)))

class Group(Interface):
    """\
    Logical group of sub models.

    Renderings:
    - xhtml: <section> [/ <h1>]
    - html+js: <section> [/ <h1>] , foldable
    - wxWidgets: Panel
    - text: indent [title.upper()]
    - ansi: ....nuf
    """
    def __init__(self, title=None):

        self._title = title
        self._members = []

    @property
    def title(self):
        """The title of the group"""
        return self._title

    @property
    def members(self):
        """The list of members in the group"""
        return self._members

    def append(self, new_member):
        """
        Add new_member to the end of the list of members in the group.
        Returns None.
        """
        return self._members.append(new_member)

    def extend(self, new_members):
        """
        Extends the existing members with those in new_members.
        If new_members is None or len(new_members) == 0 then does nothing.
        If new_members is not iterable or len(new_members) is 1 then
        just appends it as is.
        """
        if new_members:
            try:
                _ = iter(new_members)
            except TypeError: # not iterable
                self._members.append(new_members)
            else:
                for new_member in new_members:
                    self._members.append(new_member)
        return self._members

    def __repr__(self):
        """
        Shortened representation for use in REPL.
        """

        return "<Group:{} members:{}, {}>".format(
            '' if self.title else " <anonymous>,",
            len(self.members),
            as_sized_strll(str(self)))

class Warning(Interface): # pylint: disable=redefined-builtin
    """\
    Information that indicates a warning. Something that may indicate a
    problem.

    In gui this will be presented as standard text preceded by the
    text Warning - but may also be pulled to the front of the section/output.

    In Web, should be highlighted and will be pulled to the front
    of the output
    """

    _label = 0

    def __init__(self, text=''):

        self._text = text
        self._new_label_()

    def _new_label_(self):
        """
        Assign this instance of Warning, a new label that
        can be used to link warnings in a presentation layer.
        """
        Warning._label = Warning._label + 1
        self._label = Warning._label

    @property
    def text(self):
        """Returns the Warning object's text string"""
        return self._text

    @property
    def label(self):
        """Returns a unique label for this Warning instance"""
        return "_Warning%d"%self._label

    def  _simple_str(self):
        """
        method that provides textual content of Warning for use by __repr__
        """
        return self.text

class Error(Interface):
    """\
    Information that indicates an error. Something that is incorrect.

    In gui this will be presented as standard text preceded by the
    text Error- but may also be pulled to the front of the section/output.

    In Web, should be highlighted and pulled to the front of the section
    and / or output
    """
    _label = 0

    def __init__(self, text=''):

        self._text = text
        self._new_label_()

    def _new_label_(self):
        """
        Assign this instance of Error, a new label that
        can be used to link warnings in a presentation layer.
        """
        Error._label = Error._label + 1
        self._label = Error._label

    @property
    def text(self):
        """Returns the Error object's text string"""
        return self._text

    @property
    def label(self):
        """Returns a unique label for this Error instance"""
        return "_Error%d"%self._label

    def  _simple_str(self):
        """
        method that provides textual content of Error for use by __repr__
        """
        return self.text

class Code(Interface): # pylint: disable=too-few-public-methods
    """\
    Code source or rawtext.

    Presented as fixed width font in web/gui, line ends preserved.
    """
    def __init__(self, text=''):

        self._text = text

    @property
    def text(self):
        """Returns the Code object's text string"""
        return self._text

class OrderedSet(Interface): # pylint: disable=too-few-public-methods
    """
    Code source or rawtext from a set of "parallel" sources.  Similar to a
    group, but doesn't necessarily represent a logical unit - hence no title.
    """
    def __init__(self, items, item_colours=None,):

        self.items = list(items)
        if item_colours is None and len(self.items) == 2:
            # We only default colours if there are exactly two items
            self.item_colours = ["brightgreen", "brightmagenta"]
        elif not item_colours:
            self.item_colours = [None]*len(items)
        else:
            self.item_colours = item_colours

    def __repr__(self):
        """
        Shortened representation for use in REPL.
        """

        return "<OrderedSet: {} {} of {} {}>".format(
            len(self.items),
            "members" if  len(self.items) != 1 else "member",
            len(self.item_colours) if self.item_colours else "no",
            "colours" if len(self.item_colours) != 1 else "colour")

class Text(Interface): # pylint: disable=too-few-public-methods
    """\
    General information.
    Presented using a variable width font in web. Line ends (may be) lost.
    """
    def __init__(self, text=''):

        self._text = text

    @property
    def text(self):
        """Returns the Text object's text string"""
        return self._text

class TableCell(Interface):
    """
    Class used to hold a cell in a table so it can be tagged with per-cell
    metadata such as the number of columns it spans.
    """
    def __init__(self, contents):
        self._contents = contents
        self._header = False
        self._align = None
        self._nowrap = False
        self._columns = 1

    # Because we often need to set many parameters, we'll implement a fluent
    # interface. This allows code like:
    #
    #    x = TableCell("Three is the magic number").set_columns(3).set_nowrap()

    def set_header(self, header=True):
        """
        Mark whether this cell is a header. The default is that cells are not
        header cells. Calling this method with no arguments will make the cell
        a header cell.

        Using the add_header_row() method to add a row to a table object will
        automatically call set_header() on all the elements.

        Similarly, when adding a data row to a table object, the add_row()
        method has an optional parameter to make the first column a header
        as that's a common case.

        Headers are only guaranteed to be supported for entire rows at the top
        of the table and entire columns at the left of the table. Headers in
        other places may or may not be supported by all adaptors.

        To get a header row to render correctly on all adaptors, it must be
        added to the table using the add_header_row() method.
        """
        self._header = header
        return self

    def set_align(self, align):
        """
        Set the preferred horizontal alignment of this cell. Acceptable values
        are "left", "right", "center" and "centre" (the last two are
        equivalent).

        The default alignment is left for single column cells and center for
        multi-column cells. The default behaviour can be restored by
        specifying an alignment of None.
        """
        if align is not None:
            if align == "centre":
                align = "center" # Make life easy for HTML adapters
            if align not in ('left', 'right', 'center'):
                raise ValueError("Unknown alignment \"%s\"" % align)
        self._align = align
        return self

    def set_nowrap(self, nowrap=True):
        """
        Hint to the adaptor whether the contents of this cell should be
        word-wrapped.  The default is to word wrap. Calling this method with
        no arguments will turn off word wrapping. Otherwise, the parameter
        indicates whether wrapping should be surpressed.

        Not all adaptors support word wrapping. All adaptors support
        suppression of wrapping.
        """
        self._nowrap = nowrap
        return self

    def set_columns(self, cols):
        """
        Set the number of columns spanned by this cell.
        """
        self._columns = cols
        return self

    @property
    def is_header(self):
        """
        Returns if this is a header cell. See set_header().
        """
        return self._header

    @property
    def align(self):
        """
        Returns this cell's preferred alignment as the string "left", "right" or
        "center". See set_align().
        """
        if self._align is None:
            return "center" if self.columns > 1 else "left"
        return self._align

    @property
    def nowrap(self):
        """
        Returns if wrapping should be suppressed for this cell. See
        set_nowrap().
        """
        return self._nowrap

    @property
    def columns(self):
        """
        Returns the number of columns spanned by this cell. See set_columns().
        """
        return self._columns

    @property
    def contents(self):
        """
        Returns the contents of this cell. This is whatever the caller of
        Table passed to add_row() or the constructor.
        """
        return self._contents

    def __repr__(self):
        """
        Shortened representation for use in REPL.
        Note a TableCell itself cannot be rendered outside a table,
        but one could be inspected using print(repr(some_cell)).
        """

        out = "<TableCell: " + as_strll(self.contents)
        if self.is_header: # boolean
            out += ", header"
        if self._align: # Don't display if it's set to automatic
            out += ", " + self.align
        if self.columns > 1:
            out += ", columns:{}".format(self.columns)
        if self.nowrap: # boolean
            out += ", nowrap"
        return out + ">"

def as_table_cell(contents):
    """
    Return contents in a TableCell object. If contents is already a TableCell
    object then just return it unmodified (so we can later access properties
    set by the caller) otherwise wrap the contents in a new TableCell object.
    """
    if isinstance(contents, TableCell):
        return contents
    return TableCell(contents)

class Table(Interface):
    """
    Class used to represent a table.

    Most commonly, pass a row of cells to the constructor to create a header
    row at the top of the table and then call add_row() repeatedly to add the
    data. See the add_row() method for some more options.

    """
    def __init__(self, headings=None):
        self._rows = []
        self._has_data_rows = False

        if headings is not None:
            self.add_header_row(headings)

    class TableRow(object):
        """
        Class used to represent a row in a table. This is used internally
        within the Table object but instances are also returned to adaptors.
        """
        def __init__(self, is_header, cells):
            self._is_header = is_header
            self._cells = cells

        @property
        def is_header(self):
            """
            Is this row a header row?
            """
            return self._is_header

        @property
        def cells(self):
            """
            The cells in this row.
            """
            return self._cells

        @property
        def num_cols(self):
            """
            The total number of columns spanned by all the cells in this
            row.
            """
            return sum(cell.columns for cell in self.cells)

        def padded_row(self, desired_cols):
            """
            Return a copy of this row but padded with blank cells at the end
            so it has the desired number of columns.
            """
            is_header = self.is_header
            padded = self.cells[:]
            needed = desired_cols - self.num_cols
            if needed > 0:
                padded += [TableCell("").set_header(is_header)] * needed
            elif needed < 0:
                raise ValueError("Can't pad a table row to %d columns when "
                                 "it already has %d columns." %
                                 (desired_cols, self.num_cols))
            return type(self)(is_header, padded)

    def _add_row_internal(self, new_row, is_header):
        """
        Internal function to add a new row into self._rows, preserving any
        class invariants. The cells in new_row must have been turned into
        TableCell objects before calling this function.
        """
        self._rows.append(self.TableRow(is_header, new_row))

    def add_row(self, new_row, make_first_header=False):
        """
        Add a new row to a table. The parameter "make_first_header" specifies
        whether the first column in the new row should be marked as a header
        row. The default is False, that is, that the first column is not a
        header.

        The elements of the new_row can either be interface objects or plain
        strings. To apply fine-grained control the elements can be TableCell
        objects. For example, to make a row with two header columns, you could
        write:

            table.add_row(["header 1", TableCell("header 2").set_header(),
                           "data"], make_first_header=True)

        Tables where headers appear only as zero or more whole rows at the top
        and as zero or more whole columns at the left are guaranteed to be
        rendered correctly by all adaptors. Headers in other positions may not
        render as cleanly so you'll need to check the output.

        Note that adding a row where every cell is a header is not the same
        as adding a header row. If you're adding a header row then use the
        add_header_row method.
        """

        # In theory we could get rid of the restriction that header rows needed
        # to be added with add_header_row() by getting adaptors that care to
        # look if all cells on a given row are headers. The only thing that
        # fails is the degenerate case of a table with a header column and a
        # variable number of data columns collapsing to zero data columns with
        # the text adaptor:
        #
        #              Headers                        Auto     Explicit
        #              vvvvvv                         detect   add_header_row
        #
        #              -------+-------+------         ------   ------
        #  Headers->   Animal | Run 1 | Run 2         Animal   Animal
        #              =======+=======+======   -->   Birds    ======
        #              Birds  |    37 |    22         Bees     Birds
        #              Bees   |    19 |     4         ======   Bees
        #              -------+-------+------         ------   ------
        #
        # If the text adaptor used a special separator after header columns
        # then there would be a similar problem there too.

        wrapped = [as_table_cell(cell) for cell in new_row]
        if make_first_header:
            wrapped[0].set_header()
        self._add_row_internal(wrapped, False)
        self._has_data_rows = True

    def add_header_row(self, new_row):
        """
        Add a new header row to a table. See add_row() for the restrictions on
        header placements.
        """
        self._add_row_internal(
            [as_table_cell(cell).set_header() for cell in new_row],
            True)

    @property
    def has_data_rows(self):
        """
        Indicate whether this table has any data in it.

        Rows that are marked as header rows do not count.
        """
        return self._has_data_rows

    @property
    def num_cols(self):
        """
        Returns the number of columns in this table. This might not be the same
        as the number of cells in each row of the table as individual cells
        might span multiple columns.

        Note that this calculation is expensive so callers should cache the
        result if they know the table can't change.
        """
        return max([0] + [row.num_cols for row in self._rows])

    @property
    def all_rows_for_adaptor(self):
        """
        Provide the data rows in a format we've agreed with the adaptor
        objects. This Table object and the adaptor objects can change the
        format of this with no notice to anything else. This call returns
        both header and data rows.

        DO NOT USE THIS METHOD OUTSIDE ADAPTOR OBJECTS.

        At the moment, this method returns a class that supplied at least
        two properties. The is_header property indicates whether this row
        is a header row. The cells property returns a list of the cells in
        this row. The cells are guaranteed to be TableCell objects.

        Some adaptors use the header indication in the sub-list/tuple to render
        header rows differently. Others just use the header property on the
        table cells.

        Extra padding cells will be added to the right of any row which does
        not contain as many columns as the longest row. Note that unlike
        earlier implementations, padding of all rows to have the same number
        of columns is done in this function rather than when a row is
        added. This makes the function potentially expensive so callers should
        cache the result.
        """
        num_cols = self.num_cols

        return [row.padded_row(num_cols) for row in self._rows]

    @property
    def _data_rows(self):
        """
        Return just the data rows for this table. This is a private function
        used by this Table object.
        """
        return [row.cells for row in self.all_rows_for_adaptor
                if not row.is_header]

    @staticmethod
    def _test_compare_contents(expected, actual, check_length):
        """
        Compare table data and expected contents in a manner suitable for
        test_headings_are() and test_data_row_is().

        """
        if check_length and len(actual) != len(expected):
            return False

        if len(actual) < len(expected):
            return False

        for data, exp in zip(actual, expected):
            if exp is not None:
                if isinstance(exp, int):
                    if int(data.contents) != exp:
                        return False
                else:
                    if str(data.contents) != str(exp):
                        return False

        return True

    def test_headings_are(self, expected_headings, check_length=True):
        """
        Sometimes unit tests want to make sure that some code they're testing
        has written the right headings into a table. This function allows
        them to test the header contents.

        expected_headings should be a list where each element is a string or
        integer giving the test equivalent of the expected contents or None if
        this element should not be tested. The Table object will cope with
        doing enough unwrapping of internal structures to find the appropriate
        values to compare (converting ths stored value to an integer if
        the expected value is an integer).

        If check_length is False then the real table headings are allowed to be
        longer than the contents being checked (if the caller just wants to
        check the first few items). Otherwise, the real table headings must be
        exactly the same length as expected_headings.

        This function will work only with simple tables that have one row
        of headings and no complex formatting.

        Unit tests for the Table object itself will need to test both this
        and all_rows_for_adaptor().
        """
        headings = []
        rows = self.all_rows_for_adaptor
        if rows and rows[0].is_header:
            headings = rows[0].cells
        return self._test_compare_contents(expected_headings, headings,
                                           check_length)

    def test_data_rows_are(self, expected_rows, check_num_rows=True,
                           check_num_columns=True):
        """
        Sometimes unit tests want to make sure that some code they're testing
        has written the right contents into a table. This function allows
        them to test the data in the table (that is, after any header rows).

        expected_rows is a list of lists with one sub-list for each row in
        the table. Each entry is tested against the appropriate table row. A
        sub-list can be replaced with None to mean that the given row should
        not be tested.

        The sub-lists should each be a list where each element is a string or
        integer giving the test equivalent of the expected contents or None if
        this element should not be tested. The Table object will cope with
        doing enough unwrapping of internal structures to find the appropriate
        values to compare (converting ths stored value to an integer if the
        expected value is an integer).

        If check_num_rows is False then the table is allowed to have more
        data rows than expected. Otherwise the table must have exactly the
        expected number of rows.

        Unit tests for the Table object itself will need to test this,
        all_rows_for_adaptor() and has_data_rows().
        """
        rows = self._data_rows

        if check_num_rows and len(rows) != len(expected_rows):
            return False

        if len(rows) < len(expected_rows):
            return False

        for row, exp in zip(rows, expected_rows):
            if exp is not None:
                if not self._test_compare_contents(exp, row, check_num_columns):
                    return False

        return True

    def append_table_data(self, other):
        """
        Take another table and append all the data rows from that table onto
        the end of this table. Header rows are not copied.

        It is up to the caller to ensure that the header is still valid.

        This is a semi-shallow copy. Most changes to the source table made
        after the data has been appended will not be reflected in this
        table. However, some changes will:

          * Rows added to the other table will not be reflected here.

          * Rows deleted from the other table will not be reflected here
            (there's no official way to do this).

          * Changing the number of elements in a row in the other table
            will not be reflected here (there's no official way to do this
            externally but adding a row to the other table can cause this
            to happen when columns are balanced).

          * Deleting or replacing an element from a row in the other table
            will not be reflected here (there's no official way to do this).

          * Manipulating the objects in the elements in rows in the other
            table (such as setting properties on an object) will be reflected
            here.
        """
        for row in other._data_rows: # pylint:disable=protected-access
            # row is an iterable delivering each cell in turn
            self.add_row(row)

    def _extra_repr_info(self, textll):
        extra = ""
        if self._rows:
            if not self.has_data_rows:
                extra = ", no data rows"
        else:
            extra = "empty"

        info = ["{}x{}, {}{}".format(
            self.num_cols, len(self._rows),
            textll if textll != "''" else '',
            extra)]
        return info

    def _simple_str(self):
        """
        Shortened representation for use in REPL.
        """

        if self._rows:
            as_str = str(self)
            # Shorten separator lines
            desc = re.sub(r"^[- +|=]{4,}$", "---", as_str, 0, re.MULTILINE)
            # Squish spaces
            desc = re.sub(r" {2,}", " ", desc)
        else:
            desc = ""
        return desc

class ListLine(Interface):
    """\
    A list of (effectively) key, value pairs that will normally be rendered
    as a table with one header row and one data row but which you can ask to
    summarise itself as a single line of text.
    """
    def __init__(self):
        self._headings = []
        self._row = []
        self._summary = ""

    def add(self, header, value, summary_header=None, summary_value=None):
        """
        Add an element to this list. header is the column header for the table
        and value is the data value.

        By default, the summary will use the same header and value as the
        table. However, these can be overridden with the summary_header
        and summary_value parameters.

        If the header ends with a "(" then it will be omitted from the table
        header but the summary header will use it and place a ")" after the
        value instead of placing a space after the header.

        A summary header that's the empty string ("") means no header in
        the summary line (presumably because the value is self-describing).

        This method is equivalent to calling both add_table and add_summary
        with appropriate arguments. If you want to add something to just the
        table or just the summary then call those methods directly.
        """
        if summary_header is None:
            summary_header = header
        if summary_value is None:
            summary_value = value
        self.add_table(header, value)
        self.add_summary(summary_header, summary_value)

    def add_table(self, header, value):
        """
        Add an element to this list but just for the table output. header is
        the column header for the table and value is the data value.
        """
        if header.endswith("("):
            header = header[:-1]
        self._headings.append(header)
        self._row.append(value)

    def add_summary(self, header, value):
        """
        Add an element to this list but just for the summary output. header is
        the column header for the table and value is the data value.

        If the header is None or the empty string then no header will be
        output (presumably because the value is self-describing).

        If the header ends with "(" then a ")" will be placed after the value
        and there will be no space between the header and value.
        """
        if header is not None and header != "":
            if header.endswith("("):
                text = header + str(value) + ")"
            else:
                text = header + " " + str(value)
        else:
            text = value
        self.add_summary_text(text, ", ")

    def add_summary_text(self, text, separator=" "):
        """
        Add some text directly to the end of the summary.

        If the summary is not empty then the separator is added first.

        The difference between this and add_summary("", text) is that it gives
        you control of the separator and hence much more scope for being
        inconsistent.
        """
        if self._summary != "":
            self._summary += separator
        self._summary += str(text)

    @property
    def table(self):
        """
        Return a table containing the information in this ListLine.
        """
        table = Table(self._headings)
        table.add_row(self._row)
        return table

    @property
    def summary(self):
        """
        Return a one-line string summarising this ListLine.
        """
        return self._summary

    def _simple_str(self):
        """
        Uses summary as its representation for use in REPL.
        """
        return self.summary

    def _extra_repr_info(self, textll):
        return [textll, "count:{}".format(len(self._row))]

class Cluster(Interface):
    """
    This models an object consisting of some central text decorating it in all
    eight adjacent directions.

    If you have a small number of arguments, calling code will look simpler
    using argument names. Arguments have two letter names.The first
    letter indicates the row with "t" for top, "m" for middle and "b" for
    bottom. The second letter indicates the column with "l" for left,
    "c" for centre and "r" for right.

    With a large number of arguments, calling code may look simpler by laying
    out the arguments at the call-site and adjusting the white-space to
    make the layout clearer (although this will trigger a pylint bad-whitespace
    message. To make this work, the nine arguments are passed in order working
    from top to bottom from left to right, so given the arguments 1, 2, 3, 4,
    5, 6, 7, 8, 9 it will generate the display as follows (argument names are
    also given for reference):

      1 2 3           tl tc tr
      4 5 6           ml mc mr
      7 8 9           bl bc br

    In HTML all the decorations (that is, everything except the central text -
    "5" in the example) are reduced in size.

    """
    # pylint: disable=invalid-name,too-many-arguments
    def __init__(self,
                 tl="", tc="", tr="",
                 ml="", mc="", mr="",
                 bl="", bc="", br=""):
        self._top = [tl, tc, tr]
        self._mid = [ml, mc, mr]
        self._bot = [bl, bc, br]

    @property
    def top_row(self):
        """
        The top row of the cluster.
        """
        return self._top

    @property
    def middle_row(self):
        """
        The middle row of the cluster.
        """
        return self._mid

    @property
    def bottom_row(self):
        """
        The bottom row of the cluster.
        """
        return self._bot

    @property
    def as_flat_list(self):
        """
        The entire cluster as an array enumerated from top to bottom then
        from left to right, that is [tl, tc, tr, ml, mc, mr, bl, bc, br]
        where t, m, b = top, middle, bottom and l, c, r = left, centre, right.
        """
        return self.top_row + self.middle_row + self.bottom_row

    @property
    def as_row_list(self):
        """
        The entire cluster as an list per row from top to bottom where each
        list element is a list of the elements on that row from left to right,
        that is [[tl, tc, tr], [ml, mc, mr], [bl, bc, br]] where t, m, b =
        top, middle, bottom and l, c, r = left, centre, right.
        """
        return [self.top_row, self.middle_row, self.bottom_row]

class ProtocolTree(Interface):
    """
    An object representing a protocol stack. It's described as a tree because
    there is a branching relationship. One lower layer can be coupled to
    several upper layers.  For example, one Bluetooth ACL link may be carrying
    several L2CAP channels. One of the L2CAP channels may be an RFCOMM
    connection which in turn can carry several multiplexors.
    """
    def __init__(self):
        self._layer_order = []
        self._layers = {}


    def add_layer(self, layer, key):
        """
        Add a new horizontal layer to a protocol tree. "layer" is the name of
        the layer that will be used to refer to this layer later. "key" is an
        interface object that will be used when rendering the key/legend for
        this layer.

        The order in which layers are declared is important. Declare layers
        from the lowest layer upwards. The declaration order will be the order
        returned from layer_names.
        """
        if layer in self._layers:
            raise ValueError("Layer %s was added to a protocol tree twice." %
                             layer)
        self._layers[layer] = []
        self._layer_order.append((layer, key))


    class DuplicateElement(Exception):
        """
        This exception is thrown from create_element when the element the
        caller is attempting to add will duplicate an element that already
        exists.
        """
        def __init__(self, layer, ident, match):
            super(ProtocolTree.DuplicateElement, self).__init__()
            self.message = "Attempt to insert a duplicate element into " \
                "layer %s with identification attributes %s" % (layer, ident)
            self.match = match


    class MissingElement(Exception):
        """
        This exception is thrown from find_and_link_element when the element
        cannot be found.
        """
        def __init__(self, layer, ident):
            super(ProtocolTree.MissingElement, self).__init__()
            self.message = "Can't find element in layer %s with " \
                "identification attributes %s" % (layer, ident)


    def create_element(self, layer, ident, contents):
        """
        Create an element in a layer. "layer" is the name of the layer as
        previously passed to add_layer(). "id" is a dictionary of key value
        pairs that can be used to identify this element when linking elements
        later, see find_element(). "contents" is an interface object that will
        be used when rendering this element.

        This method returns an identifier for the newly created element. This
        identifier is opaque. The only thing it can be used for is to be passed
        to link_element().
        """
        if ident:
            hnd = self.find_element(layer, ident)
            if hnd:
                raise self.DuplicateElement(layer, ident, hnd)

        # Element records have three values:
        #   [0]: Identification dictionary for linking
        #   [1]: Object for rendering
        #   [2]: Linking record to give the lower layer element this is
        #        connected to. This is either None (for no connection) or an
        #        array of [layer_name, array_index] giving which lower layer
        #        it's connected to and the index in the array of elements for
        #        that layer.
        rec = [ident.copy(), contents, None]

        lyr = self._layers[layer]
        lyr.append(rec)

        return (layer, len(lyr)-1)


    @staticmethod
    def _match(id1, id2):
        """
        Decide if the identifier dictionaries for two element indicate they are
        matched.  Elements are matched if they have no mismatching identifiers.
        """
        for k in itertools.chain(id1.keys(), id2.keys()):
            if k in id1 and k in id2 and id1[k] != id2[k]:
                return False

        return True


    def find_element(self, layer, ident):
        """
        Find an element in a given layer that is compatible with the given
        identifier dictionary. The matching element is the first element in the
        given layer that has no mismatching identifiers.

        The search will also find identifiers in elements to which either
        element has been linked. This is useful if identifiers skip layers. For
        example, the link controller record for a Bluetooth ACL link may have
        both an HCI handle and a Bluetooth address. The Link Manager layer
        might just use the HCI handle. However, a higher layer might use the
        Bluetooth Address. We want the higher layer to be able to identify
        itself with this Link Manager record even though the Link Manager never
        had a Bluetooth Address stored.

        This method returns an identifier for the found element or None if no
        match can be found. In the fomer case this identifier is opaque. The
        only thing it can be used for is to be passed to link_element() or
        tested against None.

        If you want to find an element and immediately link a higher layer
        element to it than try find_and_link_element().
        """
        for i, elem in enumerate(self._layers[layer]):
            if self._match(ident, elem[0]):
                return (layer, i)

        return None


    def link_element(self, upper_el, lower_el):
        """
        Given a two elements in the protocol stack, link them vertically so the
        upper element is now known to be related to the lower element.

        The identification for the upper element will be extended to include
        the non-duplicate identifiers from the lower element.
        """
        up_el = self._layers[upper_el[0]][upper_el[1]]
        lo_el = self._layers[lower_el[0]][lower_el[1]]

        if up_el[2] is not None:
            raise KeyError("Element already linked")

        # For the moment we don't enforce that the "upper" and "lower" elements
        # match the declaration order from add_layer().
        up_el[2] = lower_el

        # Since we copied the dictionary when we created the element, it's
        # easiest to just add the non-duplicating items from the lower element
        # into the identification for the upper element.
        for k in lo_el[0].keys():
            if not k in up_el[0]:
                up_el[0][k] = lo_el[0][k]


    def find_and_link_element(self, upper_el, layer, ident):
        """
        Combine calls to find_element and link_element in the common case
        where you want to link to the element you find.

        Raises a MissingElement exception if the lower layer element
        cannot be found.
        """
        lower_el = self.find_element(layer, ident)
        if lower_el is None:
            raise self.MissingElement(layer, ident)

        self.link_element(upper_el, lower_el)


    @property
    def grid(self):
        """
        Return the protocol tree as a an array of layers starting from the
        lowest layer and working up.

        Each element of the array is a tuple consisting of the key originally
        passed to add layer then a list of (content, column, width)
        tuples. content is the interface object passed to
        create_element(). column is which column this entry belongs to
        (numbered from 0). Width is how many columns this needs to span. For a
        given layer, entries are returned in order of increasing column.
        """

        # Code that could do with refactoring, hence
        # pylint: disable=too-many-branches
        if not self._layer_order:
            return [] # Empty tree

        layer_order = [la[0] for la in self._layer_order]

        # Create a duplicate of layers so we can insert the blanks. This is
        # neither a deep copy nor a shallow copy.
        layers = {}
        for layer in layer_order:
            layers[layer] = [la[:] for la in self._layers[layer][:]]

        # Starting from the top layer and working down. Find each element that
        # hasn't yet been linked to an element in the next layer down. Create a
        # dummy element in that lower layer.
        for layer, prev_layer in reversed(
                list(zip(layer_order[1:], layer_order))):
            for elem in layers[layer]:
                if elem[2] is None:
                    layers[prev_layer].append([None, None, None])
                    elem[2] = [prev_layer, len(layers[prev_layer])-1]

        # At this point the elements in our layers[] array have the same
        # entries as they did in self._layers (see create_element). We're now
        # going to add two more values which will eventually end up being:
        #
        #   [3]: the column this entry will end up in
        #   [4]: the number of highest protocol level entries connected to here
        #
        # [4] is almost the number of columns this entry will take except that
        # an entry with no elements plugged in and an entry with one element
        # plugged in will both take up one column.

        done = []  # Entries we've already processed

        # Start at the lowest layer and assume each entry has nothing plugged
        # in.  Assign the columns.
        for elem_col, elem in enumerate(layers[layer_order[0]]):
            elem.append(elem_col) # Next available column
            elem.append(0)        # Nothing plugged in here yet
            done.append(elem)

        # Now work up the protocol tree layer by layer. We'll assign columns as
        # we go and then move entries to the done list. However, if we need to
        # insert a column we'll go back over the done list and move all
        # processed entries that are in the way one column to the right to make
        # space.
        #
        # Note that because we earlier made sure every higher layer element was
        # connected to a lower layer element (by inserting dummy elements if
        # necessary) we know there will be no unprocessed elements.
        for layer in layer_order[1:]:
            for elem in layers[layer]:
                lwr_loc = elem[2] # Might be dummy element added earlier
                lower_el = layers[lwr_loc[0]][lwr_loc[1]]
                # Find the column we want to place the new element in. This is
                # the column number of the lower element plus the number of
                # elements already plugged in.
                elem_col = lower_el[3] + lower_el[4]
                elem.append(elem_col)
                elem.append(0)  # Nothing plugged in here yet
                if lower_el[4] > 0:
                    # If the lower element already had something plugged in
                    # then we need to insert a column
                    for move in done:
                        if move[3] >= elem_col: # Same column or to the right
                            move[3] += 1  # shift it to the right
                        elif move[3] + move[4] >= elem_col: # Spans column
                            assert move[4] != 0, \
                                "ProtocolTree thinks it needs to resize an " \
                                "element with nothing plugged in."
                            move[4] += 1 # Grow to cover new column
                else:
                    # First thing to be plugged in, no need to move
                    lower_el[4] = 1
                done.append(elem)

        grid = []
        for layer, key in self._layer_order:
            els = [(la[1], la[3], (la[4] if la[4] > 0 else 1))
                   for la in sorted(layers[layer], key=itemgetter(3))]
            grid.append((layer, key, els))

        return grid

class LineGraph(Table):
    """
    Intended to be capable of drawing a graph, but falls back on Table.
    """

    def __init__(self, headings=None, title=None):
        Table.__init__(self, headings=headings)
        self._title = title

    def add_series(self, data, label=None):
        """
        Add a line to the graph, aka a row to the table.
        """
        self.add_row([label] + data)

    @property
    def title(self):
        """
        The title of the group
        """
        return self._title

    @title.setter
    def title(self, value):
        self._title = value
