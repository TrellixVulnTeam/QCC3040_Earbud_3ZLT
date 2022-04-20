############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2018 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
from csr.dev.adaptor.base_stream_adaptor import BaseStreamAdaptor
from csr.dev.adaptor.text_adaptor import StringTextAdaptor
import csv


class CSVAdaptor(BaseStreamAdaptor):
    """\
    Generates csv view of a Model.

    Example view:

    CHIPMATE_WRITE_CUSTOM_3,,
    field,bits,value
    CHIPMATE_WRITE_CUSTOM_3,[0:15],0x0
    -- END --,,
    CHIPMATE_WRITE_CUSTOM_2,,
    field,bits,value
    CHIPMATE_WRITE_CUSTOM_2,[0:15],0x0
    -- END --,,
    CHIPMATE_WRITE_CUSTOM_1,,
    field,bits,value
    CHIPMATE_WRITE_CUSTOM_1,[0:15],0x0
    -- END --,,
    """

    def __init__(self, model, file_path):

        self._depth = 0
        self._out_stream = open(file_path, 'w')
        self._out = csv.writer(self._out_stream,
                               delimiter=',')
        self._serialise(model)
        self._out_stream.close()

    def _serialise_table(self, table):
        """
        Outputs a table with or without headings or rows
        """

        for row in table.all_rows_for_adaptor:
            cols = []
            for element in row.cells:
                cols += self._serialise_tablecell(element)
            self._out.writerow(tuple(cols))

    def _serialise_tablecell(self, cell):
        """
        Outputs a cell from a table.
        """
        txt = StringTextAdaptor(cell.contents, indent_spaces=0)
        if "\n" in txt:
            raise ValueError("Tables written as csv cannot contain "
                             "multi-line text in cells")

        # We'll deal with multi-column cells by putting blank cells after
        # the data. This will keep columns aligned and also means that if the
        # user loads the data into Excel, and if the user knows where the
        # multi-column cells are, then they'll be able to select the cells and
        # do merge-and-center.
        return [txt] + [""] * (cell.columns - 1)

    def _serialise_group(self, group):
        """\
        Outputs group title in upper case and render members indented a bit.
        """
        title = group.title
        if title:
            self._out.writerow([title])
            self._depth += 1
        for member in group.members:
            self._serialise(member)
        if title:
            self._depth -= 1

    def _serialise_text(self, text):
        for text_line in text.text.splitlines():
            self._out.writerow([text_line])

    def _serialise_warning(self, warning):
        raise NotImplemented(self)

    def _serialise_error(self, error):
        raise NotImplemented(self)

    def _serialise_code(self, code, colour=None):
        raise NotImplemented(self)
