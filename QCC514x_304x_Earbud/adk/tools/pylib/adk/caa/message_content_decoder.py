############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2020 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################

from csr.dwarf.read_dwarf import DwarfNoSymbol

class MessageContentDecoder(object):
    ''' Helper class for decoding message content '''

    @classmethod
    def message_to_struct(cls, env, message_name, struct_address):
        ''' Attempts to find the typed message structure for a message given its
            name and the address of the message.

            The function attempts the following conversions:
                1. Append _T to the message name.
                2. Append _t to the message name.
                3. Convert the message named to camel case.
            If those conversions fails a custom conversion is attempted using
            the map in the class. If that fails, None is returned.
        '''

        if message_name:
            candidate_types = [ message_name + '_T',
                                message_name + '_t',
                                message_name.title().replace('_', '')]
            for candidate in candidate_types:
                if candidate in env.types:
                    return env.cast(struct_address, candidate)
