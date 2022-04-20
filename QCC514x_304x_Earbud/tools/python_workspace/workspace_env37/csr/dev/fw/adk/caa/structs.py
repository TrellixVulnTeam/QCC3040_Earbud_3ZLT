############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2020 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################

from csr.wheels.bitsandbobs import display_hex
from csr.dev.fw.structs import IAppsStructHandler, HandledStructure

class IAdkStructHandler(IAppsStructHandler):
    """
    Base type for StructHandlers that are specific to the ADK software
    """
    pass

class Bdaddr(IAdkStructHandler, HandledStructure):
    """
    Structure handler for bdaddr
    """
    @staticmethod
    def handles():
        return ("bdaddr", )

    def __repr__(self):
        return repr(self._st)

    def __str__(self):
        return "{:04x}:{:02x}:{:06x}".format(self.nap, self.uap, self.lap)

    def __eq__(self, other):
        if not isinstance(other, Bdaddr):
            return False
        return self.lap == other.lap and self.uap == other.uap and self.nap == other.nap

    def __ne__(self, other):
        return not self == other

    def is_zero(self):
        return self.nap == 0 and self.uap == 0 and self.lap == 0

    @property
    @display_hex
    def uap(self):
        return self._st.uap.value

    @property
    @display_hex
    def lap(self):
        return self._st.lap.value

    @property
    @display_hex
    def nap(self):
        return self._st.nap.value

class TypedBdaddr(IAdkStructHandler, HandledStructure):
    """
    Structure handler for typed_bdaddr
    """
    # Defined in bdaddr_.h
    _types_dict = {
        0x00 : 'TYPED_BDADDR_PUBLIC',
        0x01 : 'TYPED_BDADDR_RANDOM',
        0xFF : 'TYPED_BDADDR_INVALID',
    }

    @staticmethod
    def handles():
        return ("typed_bdaddr", )

    def __repr__(self):
        return repr(self._st)

    def __str__(self):
        return str(self.bdaddr) + "," + self._types_dict[self.type]

    def __eq__(self, other):
        if not isinstance(other, TypedBdaddr):
            return False
        return self.bdaddr == other.bdaddr and self.type == other.type

    def __ne__(self, other):
        return not self == other

    @property
    def bdaddr(self):
        return Bdaddr(self._core, self._st.addr)

    @property
    def type(self):
        return self._st.type.value

class TpBdaddr(IAdkStructHandler, HandledStructure):
    """
    Structure handler for tp_bdaddr
    """
    @staticmethod
    def handles():
        return ("tp_bdaddr", )

    def __repr__(self):
        return repr(self._st)

    def __str__(self):
        return str(self.typed_bdaddr) + "," + self._transport_enum[self.transport]

    def __eq__(self, other):
        if not isinstance(other, TpBdaddr):
            return False
        return self.typed_bdaddr == other.typed_bdaddr and self.transport == other.transport

    def __ne__(self, other):
        return not self == other

    @property
    def _transport_enum(self):
        return self._core.fw.env.enums['TRANSPORT_T']

    def is_bredr(self):
        ''' Query if this address is a BREDR ACL '''
        return self.transport == self._transport_enum.TRANSPORT_BREDR_ACL

    def is_ble(self):
        ''' Query if this address is a BLE ACL '''
        return self.transport == self._transport_enum.TRANSPORT_BLE_ACL

    @property
    def typed_bdaddr(self):
        return TypedBdaddr(self._core, self._st.taddr)

    @property
    def transport(self):
        return self._st.transport.value

class BitfieldDecoder(object):
    ''' Class to decode bitfield objects. Subclasses must implement the "handles" method

        A dictionary mapping bitfield name to bitfield value is provided to this
        class to allow pretty-printing of the fields set in the value.
    '''

    def __init__(self, core, value, prefix):
        ''' Value is the numeric value of a bitfield instance.
            Prefix is the string prefixing the names of subclass attributes that
            define the names of the bitfield bits.
        '''
        self._core = core
        self._value = value
        self._prefix = prefix

    def __repr__(self):
        return repr(self._value)

    def _get_set_bits(self):
        lookup = self.__class__.__dict__
        return [key for key, value in lookup.items() if key.startswith(self._prefix) and value in self]

    def __str__(self):
        return '\n|'.join(self._get_set_bits())

    def __contains__(self, bit):
        return (0 != (self._value.value & bit))

class DeviceProfiles(BitfieldDecoder):
    ''' Decoder for device profiles. Flags are #defined in the code, so are
        redefined and manually decoded in this class.
    '''

    DEVICE_PROFILE_HFP =       (1 << 0)
    DEVICE_PROFILE_A2DP =      (1 << 1)
    DEVICE_PROFILE_AVRCP =     (1 << 2)
    DEVICE_PROFILE_SCOFWD =    (1 << 3)
    DEVICE_PROFILE_PEERSIG =   (1 << 4)
    DEVICE_PROFILE_HANDOVER =  (1 << 5)
    DEVICE_PROFILE_MIRROR =    (1 << 6)
    DEVICE_PROFILE_AMA =       (1 << 7)
    DEVICE_PROFILE_GAA =       (1 << 8)
    DEVICE_PROFILE_GAIA =      (1 << 9)
    DEVICE_PROFILE_PEER =      (1 << 10)
    DEVICE_PROFILE_ACCESSORY = (1 << 11)

    def __init__(self, core, value):
        BitfieldDecoder.__init__(self, core, value, "DEVICE_PROFILE_")

class DeviceFlags(BitfieldDecoder):
    ''' Decoder for device flags. Flags are #defined in the code, so are
        redefined and manually decoded in this class
    '''

    # Note 1<<5 is missing in the code
    DEVICE_FLAGS_HANDSET_LINK_KEY_TX_REQD =        (1 << 0)
    DEVICE_FLAGS_HANDSET_ADDRESS_FORWARD_REQD =    (1 << 1)
    DEVICE_FLAGS_JUST_PAIRED =                     (1 << 2)
    DEVICE_FLAGS_PRE_PAIRED_HANDSET =              (1 << 3)
    DEVICE_FLAGS_IS_PTS =                          (1 << 4)
    DEVICE_FLAGS_MIRRORING_ME =                    (1 << 6)
    DEVICE_FLAGS_MIRRORING_C_ROLE =                (1 << 7)
    DEVICE_FLAGS_PRIMARY_ADDR =                    (1 << 8)
    DEVICE_FLAGS_SECONDARY_ADDR =                  (1 << 9)
    DEVICE_FLAGS_KEY_SYNC_PDL_UPDATE_IN_PROGRESS = (1 << 10)
    DEVICE_FLAGS_NOT_PAIRED =                      (1 << 11)

    def __init__(self, core, value):
        BitfieldDecoder.__init__(self, core, value, "DEVICE_FLAGS_")
