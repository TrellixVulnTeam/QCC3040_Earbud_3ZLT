#
# Copyright (c) 2019-2020 Qualcomm Technologies, Inc. and/or its
# subsidiaries.  All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""Pretty Printer Tools."""


# BUG: B-299464
# pylint: disable=too-few-public-methods
class PrettyDictionaryPrinter:
    """A pretty printer of a dictionary.

    It assumes all the values are either a Dictionary or a String. Keys can be
    either Integer or String.

    Args:
        left_margin (int): Number of spaces to allow on the left hand side.
        dictionary (dict): A str -> str or int -> str key-value dictionary.
        title (str): When given, it prints the title before the content.
    """
    def __init__(self, left_margin, dictionary, title=None):
        self._left_margin = left_margin
        self._title = title
        self._configuration = dictionary

        self._margin = self._left_margin

    def pprint(self):
        """Pretty prints the title (when configured) and the data."""
        if self._title:
            print(self._title)

        self._print_dict(self._configuration)

    def _print_dict(self, dictionary):
        if not dictionary:
            # The dictionary is empty, nothing to print.
            return

        key_max_width = self._get_max_width(dictionary.keys())
        print_string_formatter = '{0}{1:<%s}: {2}' % int(key_max_width)

        for key in sorted(dictionary.keys(), key=str):
            value = dictionary[key]
            if isinstance(value, dict):
                print(
                    print_string_formatter.format(' ' * self._margin, key, '')
                )
                # Extra margin on the left and print out.
                self._margin = self._margin + self._left_margin
                self._print_dict(value)
                # Revert the extra margin to its normal value.
                self._margin = self._margin - self._left_margin
            else:
                print(
                    print_string_formatter.format(
                        ' ' * self._margin, key, value
                    )
                )

    @staticmethod
    def _get_max_width(words):
        max_width = 0
        for word in words:
            word = str(word)
            if len(word) > max_width:
                max_width = len(word)

        return max_width
