############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2013 - 2016 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
import re
from csr.dev.model import interface
from csr.wheels.bitsandbobs import PureVirtualError

class BaseStreamAdaptor(object):
    """Common base for serialising model adaptors"""

    # Protected / Provided

    def _preamble(self):
        """
        Write any necessary header guff (e.g. javascript functions) to the
        output stream
        """
        pass

    def _serialise(self, model, depth=0, colour=None):
        """\
        Decode model sub-type and dispatch to specific serialise methods
        """
        if isinstance(model, interface.Code):
            self._serialise_code(model, colour)
        elif isinstance(model, interface.OrderedSet):
            for item, colour in zip(model.items, model.item_colours):
                self._serialise(item, colour=colour)
        elif isinstance(model, interface.Text):
            self._serialise_text(model)
        elif isinstance(model, interface.Warning):
            self._serialise_warning(model)
        elif isinstance(model, interface.Error):
            self._serialise_error(model)
        elif isinstance(model, interface.Group):
            self._serialise_group(model)
        # LineGraph is an instance of Table, so we need to check it first.
        elif isinstance(model, interface.LineGraph):
            self._serialise_line_graph(model)
        elif isinstance(model, interface.Table):
            self._serialise_table(model)
        elif isinstance(model, interface.ListLine):
            self._serialise_list_line(model)
        elif isinstance(model, interface.Cluster):
            self._serialise_cluster(model)
        elif isinstance(model, interface.ProtocolTree):
            self._serialise_protocol_tree(model)
        elif isinstance(model, interface.TableCell):
            raise ValueError("TableCells can be rendered only within tables, "
                             "not as stand-alone objects")
        elif isinstance(model, type(None)):
            # None is allowed as a convenience to indicate there'ws nothing to
            # serialise. This is handy for commands that do something but have
            # nothing to say.
            pass
        elif isinstance(model, str):
            # Treat bare strings and simple numbers as text. As well as making
            # coding easier it also helps when making updates that allow
            # things that were previously bare strings to now become objects.
            #
            # The downside is that people might get lazy and just output
            # variables instead of thinking about how they want them
            # presented.
            #
            # We need to catch strings here as we don't want them to fire
            # accidentally on the regular expression match in the fallback
            # code that comes next.
            #
            # We don't really need to catch numbers as the fallback code will
            # work for them but should we get problems in the fallback code
            # and have to turn it off, we'd still like numbers to work.
            self._serialise_string(model)
        elif isinstance(model, int):
            # See the comment for strings
            self._serialise_int(model)
        elif isinstance(model, float):
            # See the comment for strings
            self._serialise_float(model)
        else:
            # The story continues in _serialise_other()
            self._serialise_other(model)

    # Text/Code are similar. By default treat Text as Code,
    # but allow adaptation.
    def _serialise_text(self, text):
        self._serialise_code(text)

    # Warning and Errors should receive special treatment in terms of
    # presentation. When serialised they can simply be output.
    # Web adaptors should add formatting.
    def _serialise_warning(self, warning):
        self._serialise_text(interface.Text("Warning: " + warning.text))

    def _serialise_error(self, error):
        self._serialise_text(interface.Text("Error: " + error.text))

    # The default behaviour for a list line is to turn it into a table
    # and let the adaptor render that.
    def _serialise_list_line(self, listline):
        self._serialise_table(listline.table)

    # The default behaviour for strings is to turn them into text and let the
    # adaptor render that.
    def _serialise_string(self, string):
        wrapped = interface.Text(string)
        self._serialise_text(wrapped)

    # The default behaviour for ints is to turn them into strings and let the
    # adaptor render that.
    def _serialise_int(self, number):
        self._serialise_string(str(number))

    # The default behaviour for floats is to turn them into strings and let the
    # adaptor render that.
    def _serialise_float(self, number):
        self._serialise_string(str(number))

    # The default behaviour for other types is to turn them into strings and
    # let the adaptor render that.
    def _serialise_other(self, other):
        # We could try to convert any type to a string but that always succeeds
        # and often doesn't provide anything useful. We might not find out
        # we've converted an inappropriate type until a human is reading the
        # value buried deep in some report. So, let's catch the less useful
        # cases and treat them as errors. This is a bit brute force.
        string = str(other)
        if re.match(r"<.* at 0x[0-9a-fA-F]+>$", string):
            raise ValueError("BaseStreamAdaptor: Unknown model type %s" %
                             type(other))
        self._serialise_string(string)

    # The default behaviour for a cluster is to convert it into a table and
    # render that but redirect through a helper method so that subclasses
    # that want the table conversion but then want to tweak the table
    # rendering don't need to repeat the conversion
    def _serialise_cluster(self, cluster):
        table = interface.Table()
        for row in cluster.as_row_list:
            cells = [interface.TableCell(cell).set_nowrap() for cell in row]
            table.add_row([cells[0].set_align("right"),
                           cells[1].set_align("center"),
                           cells[2].set_align("left")])
        self._serialise_cluster_table(table)

    def _serialise_cluster_table(self, cluster_table):
        self._serialise_table(cluster_table)

    # The default behaviour for a protocol tree is to turn it into a table and
    # let the adaptor render that but redirect through a helper method so that
    # subclasses that want the table conversion but then want to tweak the
    # table rendering don't need to repeat the conversion
    def _serialise_protocol_tree(self, tree):
        table = interface.Table()

        # Reversed because the highest protocol layer is on the first row
        for layer, key, els in reversed(tree.grid):
            row = [layer, interface.TableCell(key).set_align("center")]
            next_col = 0
            for contents, column, width in els:
                padding = column - next_col
                if padding > 0:
                    row.append(interface.TableCell("").set_columns(padding))
                if contents is None:
                    contents = ""
                row.append(interface.TableCell(contents).set_columns(width).
                           set_align("center"))
                next_col = column + width
            table.add_row(row, make_first_header=True)

        self._serialise_protocol_tree_table(table)

    def _serialise_protocol_tree_table(self, protocol_tree_table):
        self._serialise_table(protocol_tree_table)

    def _serialise_line_graph(self, graph):
        """
        Outputs a line graph, or at least what we can easily do as text,
        which is the same as a table by default.
        """

        self._serialise_table(graph)

    # Protected / Required

    def _serialise_code(self, code, colour=None):
        raise PureVirtualError(self)

    def _serialise_group(self, group):
        raise PureVirtualError(self)

    def _serialise_table(self, table):
        raise PureVirtualError(self)

