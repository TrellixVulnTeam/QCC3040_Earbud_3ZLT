############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2013 - 2020 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
from csr.dev.adaptor.base_stream_adaptor import BaseStreamAdaptor
from csr.wheels.bitsandbobs import colorama_enabled, ColoramaAnsiToWin32, \
                                   AnsiColourCodes
from csr.wheels import gstrm
import sys
import re
try:
    # Py2
    from StringIO import StringIO
except ImportError:
    # Py3
    from io import StringIO

from io import IOBase, BytesIO

if sys.version_info >= (3, 0):
    # Python 3 no longer has the file built-in
    type_file = IOBase
else:
    type_file = file

import re

# We need to borrow the visitor framework. All the other adaptors do all
# their work in their __init__ method so let's copy that pattern so we
# don't get broken by subtle assumptions.
class TableCellLayoutHints(BaseStreamAdaptor):
    """
    Return a series of hints to help us vary layout algorithms based on
    the model.

    The dictionaries of hints returned by these functions are meaningful
    only to TextAdaptor.
    """
    def __init__(self, model):
        if model is None:
            model = ""
        self._layout = None
        self._serialise(model)
        if self._layout is None:
            raise ValueError("Don't know table layout hints for type %s" %
                             type(model))

    def _serialise_text(self, text):
        self._layout = {"align_as_block": False}

    def _serialise_code(self, code, colour=None):
        # Note we currently don't handle colour support for this, colour arguement
        # is to provide a consistent interface
        self._layout = {"align_as_block": True}

    def _serialise_group(self, group):
        self._layout = {"align_as_block": True}

    def _serialise_table(self, table):
        self._layout = {"align_as_block": True}

    @property
    def align_as_block(self):
        """
        If this object renders to multiple lines, should each line be aligned
        indepedently (False) or should the whole block be aligned but the
        relative alignment within the block preserved (True).
        """
        return self._layout["align_as_block"]


class TextAdaptor (BaseStreamAdaptor):
    """\
    Generates plain text view of a Model.
    
    Example view:
    
    AMBER-LAB-OR-SIMILAR DEVICE
      AMBER-R01-V0 CHIP
        CURATOR SUBSYSTEM
          CPU CORE
            FIRMWARE
          MMU
            BUFFER SUMMARY
              bh size  used  offs   bh size  used  offs   bh size  used  offs   bh size  used  offs
              01 01000 01000 00852  02 00200 00200 00144  03 00100 00100 00048  04 00040 00040 00020  
              05 00100 00100 -----  07 00080 00080 0002c  
            BUFFER 1
              Buffer 0x1 | Offset 0x852 | Size 0x6 | Pages 64
              0000 :  f4 17 00 8a 99 dd f0 07 14 35 00 16 10 19 00 ff
              0010 :  00 47 9c 82 00 8a 19 d3 00 8a 1a 1a 16 28 12 27
              0020 :  36 26 42 25 00 8a 35 dc 00 8a 41 db e0 14 00 ff
    """
    def __init__(self, model, out_stream, indent_spaces=2):

        self._indent = indent_spaces
        self._depth = 0
        self._out_stream = out_stream
        # Define a flag we can interrogate to determine if output is destined for terminal
        # as sys.stdouts tends to change...
        self._terminal_output = False
        self._strip_codes = AnsiColourCodes().strip_codes
        if (out_stream is sys.stdout or out_stream is sys.stderr) and out_stream.isatty():
            self._terminal_output = True
            write_to_stdout = out_stream is sys.stdout
            # colorama
            with colorama_enabled():
                # Now select the wrapped one to serialise to
                self._strip_codes = False
                self._out = sys.stdout if write_to_stdout else sys.stderr
                self._serialise(model)
        elif isinstance(out_stream, (type_file, StringIO, BytesIO)):
            self._out = out_stream
            self._serialise(model)
        else:
            # Can't use colorama directly because it's fixated on stdout and 
            # stderr.  But we can still use the AnsiToWin32 class manually.  The
            # downside is that it's slower to instantiate one of these every
            # time.  Theoretically we could cache these against stream object
            # IDs but that feels like premature optimisation.

            # Colorama AnsiToWin32 assumes the stream has a flush method, so
            # if this one doesn't we can't use colorama
            try:
                out_stream.flush
            except AttributeError:
                self._out = out_stream
            else:
                self._out = ColoramaAnsiToWin32(out_stream).stream
                self._strip_codes = False

            self._serialise(model)
    
    # BaseStreamAdaptor compliance
    
    def _serialise_code(self, code, colour=None):
        # Although code is already raw text we need to write it line by line to
        # get the correct indent in front of each of them.

        if colour and self._terminal_output:
            try:
                colour_code = AnsiColourCodes().code[colour]
                reset_code = AnsiColourCodes().reset_code
            except KeyError:
                raise ValueError("{} is not a valid. Choose from {}".format(colour,
                                                                            AnsiColourCodes().names))
        else:
            colour_code = reset_code = ""

        for code_line in code.text.splitlines():
            code_line = colour_code + code_line + reset_code
            self._writeln(code_line)

    def _serialise_group(self, group):
        """\
        Outputs group title in upper case and render members indented a bit.
        """
        title = group.title
        if title:
            self._writeln('%s' % title.upper())
            self._depth += 1
        for member in group.members:
            self._serialise(member)
        if title:
            self._depth -= 1

    def _separator_line(self, sep, colsep, colrowmark, prev_row_widths,
                        next_row_widths):
        """
        Output a table separator line. sep is the basic character such
        as "-" or "=" or None for no separator. colsep is the normal column
        separator such as " | ". colrowmark is the character to use when
        a row and column meet such as "+". prev_row_widths and next_row_widths
        are the widths of the columns in rows either side of this separator.
        These may be different if there are cells that span multiple columns
        such as:

             --------------+-----
             Span two rows | cell
             -----+--------+-----
             cell | Span two rows
             -----+--------------

        sep will be used for space characters in colsep and colrowmark for
        non-space characters.

        One of prev_row_widths and next_row_widths may be None to indicate
        (respectively) the start and end of the table. Setting them both to
        None doesn't make sense.

        If both prev_row_widths and next_row_widths are not None then the two
        row widths must have the same overall length, that is sum(row_widths)
        + (len(row_widths)-1) * len(colsep) must be the same for the two lists
        of widths.
        """
        if sep is None:
            return

        markposns = [
            ind for ind, char in enumerate(list(colsep)) if char != " "]

        bwid = len(colsep) # column border width

        # Pick one non-None row width to calculate the overall length
        row_widths = prev_row_widths
        if row_widths is None:
            row_widths = next_row_widths

        sepline = [sep] * (sum(row_widths) + (len(row_widths)-1) * bwid)

        for row_widths in [prev_row_widths, next_row_widths]:
            if row_widths is not None:
                pos = 0
                for col in row_widths[:-1]:
                    pos += col
                    for mark in markposns:
                        sepline[pos + mark] = colrowmark
                    pos += bwid

        self._writeln("".join(sepline))

    _align_funcs = {
        "left": str.ljust,
        "right": str.rjust,
        "center": str.center
        }

    def _serialise_table(self, table):
        """
        Outputs a table with or without headings or rows using default
        settings.
        """

        # At the moment, the default values for _serialise_table_internal
        # give something that matches older versions of this adaptor and
        # are generally suitable or most tables. This may change.
        self._serialise_table_internal(table)

    def _serialise_cluster_table(self, cluster_table):
        """
        Output a Cluster that has already been turned into a table.

        Clusters are used in ProtocolTrees so we try to minimise the width.
        """
        self._serialise_table_internal(
            cluster_table, colsep=" ", topbotsep=None, headsep=None,
            rowsep=None)

    def _serialise_protocol_tree_table(self, protocol_tree_table):
        """
        Output a ProtocolTree that has already been turned into a table.

        Protocol trees can be very wide so we try to minimise the horizontal
        width.
        """
        self._serialise_table_internal(
            protocol_tree_table, colsep="|", topbotsep="-", rowsep="-",
            colrowmark="+")

    def _serialise_table_internal(self, table, colsep=" | ", topbotsep="-",
                                  headsep="=", rowsep=None, colrowmark="+"):
        """
        Output a table with configuration of the columne and row separators.
        The parameters are:

          colsep:     The separator between columns
          topbotsep:  The separator at the top and bottom of the table
          headsep:    The separator between headers and non-headers
          rowsep:     The separator between normal rows
          colrowmark: When column and row separators meet

        The defaults for these reproduce the behavior from older versions of
        this adaptor which was to have separators between data columns but not
        between data rows. Note that the default for HTML tables is different.

        See the documentation of _separator_line for how these interact.
        """

        # There doesn't appear to be an obvious way to highlight header
        # columns (at the start of rows), at least not without supporting
        # Unicode or using ANSI escape sequences and opening the can of worms
        # described later.

        bwid = len(colsep) # The width of the border between columns

        # Note that Table ensures all rows have the same number of columns
        # (that is, the sum of the column spans not the lengths of the lists).
        rows = table.all_rows_for_adaptor
        if not rows:
            # Table is empty
            return

        # Really the column width is the number of character cells that will
        # be taken up by the contents of a line. For pure ASCII that's easy.
        # For Unicode (where, even for fixed width fonts, characters can take
        # zero, one or two cells), or in the presence of control codes (such
        # as ANSI colour escape sequences or tabs) this is really nasty.
        #
        # For Unicode, if we can avoid CJK (Eastern) characters and right-to-
        # left languages, we might be able to count grapheme clusters
        # (assuming we can handle zero width characters). For ANSI colour, the
        # correct answer is one of: to hand off the colouring in the same way
        # we hand off CSS classes in HTML adaptors so we apply the control
        # sequences here after measuring the length; to provide two rendering
        # methods, one with control sequences and one without; strip ANSI
        # sequences explicitly, or, to provide an explicit width method on all
        # models.
        #
        # For the moment, we go old-school by assuming pure ASCII so we can
        # use len() and also assuming that for anything complex the user can
        # go to an HTML adaptor.

        col_widths = [0] * table.num_cols  # The width of each column

        # In the code that follows "cind" means the index of the left-most
        # column of this cell, numbering from 0

        cellvals = [] # Cached information about each cell

        for row in rows:
            rowvals = []
            cind = 0
            for cell in row.cells:
                text = StringTextAdaptor(cell.contents, indent_spaces=0)
                layout = TableCellLayoutHints(cell.contents)
                linetxts = re.sub("\n+$", "", text).split("\n")
                width = max(len(x) for x in linetxts)
                if layout.align_as_block:
                    linetxts = [x.ljust(width) for x in linetxts]
                rowvals.append([cell, linetxts, width, cind])
                cind += cell.columns
            cellvals.append((row.is_header, rowvals))

        # Start by finding all the cells that span one column and use those
        # to initialise col_widths.
        for _, row in cellvals:
            for cell, linetxts, width, cind in row:
                cols = cell.columns
                if cols == 1:
                    col_widths[cind] = max(col_widths[cind], width)

        # Now find all the multi-column cells, see if we have enough space
        # for them (not forgetting we can use the border space) and grow the
        # columns if there's not enough space.
        for _, row in cellvals:
            cind = 0
            for cell, linetxts, width, cind in row:
                cols = cell.columns
                if cols != 1:
                    # The current total widths of the columns we span
                    spanwidth = bwid * (cols-1) + \
                        sum(col_widths[cind:cind+cols])
                    # While we don't have enough space, find the smallest
                    # column in the group we're spanning and grow it by one
                    # space.
                    while spanwidth < width:
                        # Index of narrowest (left-most if multiple matches)
                        _, smallest = min(
                            (w, i+cind) for i, w in
                            enumerate(col_widths[cind:cind+cols]))
                        col_widths[smallest] += 1
                        spanwidth += 1

        # In theory, we now have enough information to output the table and
        # we could do it in one loop. However, for the sanity of those reading
        # this code, let's do it in a couple of steps.

        # For every row, make sure all cells in that row have the same number
        # of lines and that each line has the same number of characters and
        # that the number of characters is correct for the column.
        #
        # If we ever wanted to implement vertical alignment (top, middle or
        # bottom) then this is the place to do it. For the moment we're just
        # doing top alignment.
        for _, row in cellvals:
            rowlines = max(len(celldata[1]) for celldata in row)
            for celldata in row:
                cell, linetxts, width, cind = celldata

                # This is where we would do vertical alignment
                linetxts += [""] * (rowlines - len(linetxts))

                cols = cell.columns
                width = bwid * (cols-1) + sum(col_widths[cind:cind+cols])

                align = self._align_funcs[cell.align]
                celldata[1] = [align(txt, width) for txt in linetxts]
                celldata[2] = width

        # We use a big header separator line (===) between header and
        # non-header (that is, data) rows. So, we need to know whether
        # the previous row was a header or not.
        prev_was_header = None

        # We need to know the column widths for the previous row so we can
        # insert row/column intersections at the right points. See
        # _separator_line() for an example. Also, if two consecutive header
        # lines change column spans then we'll insert a header separator.
        prev_row_widths = None

        # If the table contains a header but no data then older versions of
        # this adaptor would insert a header separator line and an end of
        # table line so it was clear that there was a header and no data. We
        # want to preserve this behaviour. See the unit tests for an example.
        seen_header = False
        seen_data = False

        # Finally, we can output the table.
        for is_header, row in cellvals:
            if is_header:
                seen_header = True
            else:
                seen_data = True

            row_widths = [celldata[2] for celldata in row]

            # Decide what sort of row separator to output
            if prev_was_header is None:
                # Start of table gets a normal separator
                sep = topbotsep
            elif prev_was_header != is_header:
                # Header separator between header and non-header rows
                sep = headsep
            elif prev_was_header and is_header and \
                    row_widths != prev_row_widths:
                # We've got different column spans in two consecutive header
                # rows so insert a separator.
                sep = headsep
            else:
                # We're between two header rows with the same structure or
                # we're between two consecutive data rows.
                sep = rowsep

            self._separator_line(sep, colsep, colrowmark, prev_row_widths,
                                 row_widths)

            prev_was_header = is_header
            prev_row_widths = row_widths

            # Start off with a list of cells each with a list of lines.
            rowtxts = [celldata[1] for celldata in row]
            # We want to work through this a line at a time, joining the
            # lines for the cells with the appropriate column separator.
            # The zip() call here is transposing the list of lists so we now
            # have a list of lines each containing a list of cells.
            for tableline in [colsep.join(x) for x in zip(*rowtxts)]:
                self._writeln(tableline)

        # Extra separator for a table with only headers. See earlier comments.
        if seen_header and not seen_data:
            self._separator_line(headsep, colsep, colrowmark, prev_row_widths,
                                 prev_row_widths)

        self._separator_line(topbotsep, colsep, colrowmark, prev_row_widths,
                             None)

    # Private
    def _writeln(self, text):
        """\
        Write the text indented according to current logical depth.
        """
        if self._strip_codes:
            text = self._strip_codes(text)
        out_str = "{}{}\n".format(' ' * (self._depth * self._indent), text)
        try:
            # Py2
            self._out.write(out_str.decode())
        except (AttributeError, TypeError):
            # Py3
            self._out.write(out_str)
        

def StringTextAdaptor(model, indent_spaces=2):
    """
    Simple wrapper around TextAdaptor to return a string containing the text
    report
    """
    class StringStream(object):
        def __init__(self):
            self._str = ""
        def write(self, s):
            self._str += s
        @property
        def str(self):
            return self._str
            
    s = StringStream()
    TextAdaptor(model, s, indent_spaces=indent_spaces)
    # Strip off trailing newline if that's the only one
    
    out_string = s.str.rstrip() if s.str.find("\n") == len(s.str) - 1  else s.str
    # Handle ensure out_string is a str type
    if sys.version_info < (3,) and not isinstance(s.str, str):
        return out_string.encode()
    else:
        return out_string


def derender_simple_table(table_string, colsep=" | ", topbotsep="-",
                                  headsep="=", rowsep=None, colrowmark="+",
                                  leave_as_strings=True):
    """
    Helper function to make it safer to compare tabular test results against a 
    golden table by stripping out the table formatting.
    """
    heading_sep_re = re.compile(r"%s+(\%s\%s+)*" % (headsep, colrowmark, headsep))

    
    table_lines = [row.strip() for row in table_string.split("\n")]
    table_lines = [t for t in table_lines if t]
    
    def reconstruct_entries(row_list):
        """
        If the entries in the table have been split across multiple lines, rebuild
        the lines
        """
        
        return [" ".join(row[col].strip() for row in row_list) for col in range(len(row_list[0]))]
            
    
    heading_sep_rows = [(i, r) for (i,r) in enumerate(table_lines) if heading_sep_re.match(r)]
    
    if len(heading_sep_rows) > 1:
        raise ValueError("%d rows matched the headings separator row!" % len(heading_sep_rows))
    
    if heading_sep_rows:
        heading_start_line = 1
        heading_end_line = heading_sep_rows[-1][0]
        heading_row_list = [row.split(colsep) for row in table_lines[1:heading_end_line]]
        
        headings = reconstruct_entries(heading_row_list)
    else:
        headings = None
        heading_end_line = 0

    
    if rowsep is None:
        # We have to assume every row is a single line
        if topbotsep is None:
            rows = [row.split(colsep) for row in table_lines[heading_end_line+1:]]
        else:
            # Don't include the bottom row
            rows = [row.split(colsep) for row in table_lines[heading_end_line+1:-1]]
    
    else:
        raise NotImplementedError("Haven't implemented multiline stripping yet")
    
    # Now process the rows: convert anything that looks like a number into a number
    # and leave the rest as strings.
    
    clean_rows = []
    for row in rows:
        clean_row = [entry.strip() for entry in row]
        if not leave_as_strings:
            converted_row = []
            for entry in clean_row:
                try:
                    entry = int(entry,0)
                except ValueError:
                    pass
            converted_row.append(entry)
            clean_rows.append(converted_row)
        else:
            clean_rows.append(clean_row)
        
    return clean_rows

def text_adaptor(model):
    TextAdaptor(model, gstrm.iout)