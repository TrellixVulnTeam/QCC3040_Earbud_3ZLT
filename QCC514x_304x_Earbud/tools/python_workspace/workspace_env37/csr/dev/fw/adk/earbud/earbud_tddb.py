############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2021 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################

from csr.dev.fw.firmware_component import FirmwareComponent
from csr.dev.model import interface
from csr.dev.fw.psflash import Psflash

class EarbudTrustedDeviceList(FirmwareComponent):
    ''' Class providing trusted device list summary information about the Earbud application.
    '''
    TDDB_DEVICES_MAX = 6
    TDDB_LIST_MAX = 8

    TDDB_KEYCOUNT_SC = 2
    TDDB_KEYCOUNT_GATT = 1
    TDDB_KEYCOUNT_APP = 1

    PSKEY_CONNLIB_BASE = 0x260e
    PSKEY_TDDB_SYSTEM = PSKEY_CONNLIB_BASE
    PSKEY_TDDB_INDEX = PSKEY_TDDB_SYSTEM + 1
    PSKEY_TDDB_BASE = PSKEY_TDDB_INDEX + 1

    PSKEY_TDDB_BASE_SC = PSKEY_TDDB_BASE + (2 * TDDB_LIST_MAX)
    PSKEY_TDDB_BASE_GATT = PSKEY_TDDB_BASE_SC + (TDDB_KEYCOUNT_SC * TDDB_LIST_MAX)
    PSKEY_TDDB_BASE_APP = PSKEY_TDDB_BASE_GATT + (TDDB_KEYCOUNT_GATT * TDDB_LIST_MAX)

    TDDB_FILE_BASE = 0xFFFF0000
    TDDB_VER_BIT_LE  = (1 << 0)
    TDDB_VER_BIT_SIGN = (1 << 1)
    TDDB_VER_BIT_PRIVACY = (1 << 2)
    TDDB_VER_BIT_CACHING = (1 << 4)
    TDDB_FILE_VERSION = TDDB_FILE_BASE | TDDB_VER_BIT_LE | TDDB_VER_BIT_PRIVACY | TDDB_VER_BIT_CACHING

    PSKEY_TRUSTED_DEVICE_LIST = TRUSTED_DEVICE_INDEX = PSKEY_CONNLIB_BASE + 1
    TRUSTED_DEVICE_LIST = TRUSTED_DEVICE_INDEX

    TDDB_SYSTEM_OFFSET_VERSION = 0
    TDDB_SYSTEM_OFFSET_ER = 4
    TDDB_SYSTEM_OFFSET_IR = 20
    TDDB_SYSTEM_OFFSET_DIV = 36
    TDDB_SYSTEM_OFFSET_HASH = 40

    TDDB_INDEX_OFFSET_COUNT = 0
    TDDB_INDEX_OFFSET_ELEM = 4
    TDDB_INDEX_ELEM_OFFSET_ADDR = 0
    TDDB_INDEX_ELEM_OFFSET_ADDR_TYPE = 8
    TDDB_INDEX_ELEM_OFFSET_RANK = 9
    TDDB_INDEX_ELEM_OFFSET_PRIORITY = 10

    TDDB_INDEX_ELEM_ADDR_OFFSET_LAP = 0
    TDDB_INDEX_ELEM_ADDR_OFFSET_UAP = 4
    TDDB_INDEX_ELEM_ADDR_OFFSET_NAP = 6

    TDDB_INDEX_ELEM_SIZE = 12

    TDDB_BREDRKEY_OFFSET_TYPE = 0
    TDDB_BREDRKEY_OFFSET_LEN = 1
    TDDB_BREDRKEY_OFFSET_LINKKEY = 2
    TDDB_BREDRKEY_OFFSET_AUTHORISED = 18

    TDDB_LEKEY_OFFSET_SECREQ = 2
    TDDB_LEKEY_OFFSET_ENC = 4
    TDDB_LEKEY_OFFSET_DIV = 30
    TDDB_LEKEY_OFFSET_ID = 32
    TDDB_LEKEY_OFFSET_SIGN = 48

    TDDB_LEKEY_ENCCENTRAL_OFFSET_EDIV = 0
    TDDB_LEKEY_ENCCENTRAL_OFFSET_RAND = 2
    TDDB_LEKEY_ENCCENTRAL_OFFSET_LTK = 10

    KEY_TYPE_AUTH_P192 = 5
    KEY_TYPE_UNAUTH_P256 = 7
    KEY_TYPE_AUTH_P256 = 8

    def __init__(self, env, core, parent=None):
        FirmwareComponent.__init__(self, env, core, parent=parent)

        self._psflash = Psflash(env, core)
        try:
            # Check for Synergy ADK build
            env.vars["synergy_service_inst"]
        except KeyError:
            self._is_synergyadk = False
        else:
            self._is_synergyadk = True


    def _psread(self, ps_key):
        """ Read PS Keys value from PsFlash based on ps_key."""
        key_value = str(self._psflash.keys(show_data=False, report=True ,keys=ps_key))
        # Parse the return string for PS Keys value
        key_value = key_value[key_value.find(':') + 1:]
        key_value = key_value.split()
        output = [int(x,16) for x in key_value]
        return output

    def parse_tddb_list(self):
        """
        Get the CONNLIB1 data from PS that describes the number of devices in trusted device
        database and device info [address,rank,priority] of each devices.
        :return: device count, list of devices with device info
        """
        devices = [None] * self.TDDB_DEVICES_MAX
        index = self._psread(self.PSKEY_TDDB_INDEX)
        # Number of devices present in Trusted Device database in PS
        device_count = index[self.TDDB_INDEX_OFFSET_COUNT]
        # Retrieve device address, rank and priority for the Trusted devices
        indexElemBase = self.TDDB_INDEX_OFFSET_ELEM
        for i in range(self.TDDB_DEVICES_MAX):
            indexElem = index[indexElemBase : indexElemBase + self.TDDB_INDEX_ELEM_SIZE]
            rank = indexElem[self.TDDB_INDEX_ELEM_OFFSET_RANK]

            if rank < self.TDDB_DEVICES_MAX and rank >= 0:
                device = {}
                addr = {}

                addr["lap"] = (indexElem[self.TDDB_INDEX_ELEM_ADDR_OFFSET_LAP] +
                              (indexElem[self.TDDB_INDEX_ELEM_ADDR_OFFSET_LAP + 1] << 8) +
                              (indexElem[self.TDDB_INDEX_ELEM_ADDR_OFFSET_LAP + 2] << 16))
                addr["uap"] = indexElem[self.TDDB_INDEX_ELEM_ADDR_OFFSET_UAP]
                addr["nap"] = (indexElem[self.TDDB_INDEX_ELEM_ADDR_OFFSET_NAP] +
                              (indexElem[self.TDDB_INDEX_ELEM_ADDR_OFFSET_NAP + 1] << 8))
                addr["type"] = indexElem[self.TDDB_INDEX_ELEM_OFFSET_ADDR_TYPE]
                priority = indexElem[self.TDDB_INDEX_ELEM_OFFSET_PRIORITY]

                device["addr"] = addr
                device["rank"] = rank
                device["priority"] = priority

                devices[i] = device

            indexElemBase = indexElemBase + self.TDDB_INDEX_ELEM_SIZE

        devices = list(filter(None, devices))

        return device_count, devices


    def _swap_adjacent_elem(self, link_key):
        """Method to swap the adjacent elements present in the list."""
        link_key[::2], link_key[1::2] = link_key[1::2], link_key[::2]
        return link_key

    def _extract_bredr_key(self, devices):
        """Method to extract the bredr information for each of the devices in the list."""
        for i,device in enumerate(devices):
            bredr = {}
            ps_key = self.PSKEY_TDDB_BASE_SC + i
            key_val = self._psread(ps_key)

            if key_val is not None and len(key_val) > 0:
                linkKeyType = key_val[self.TDDB_BREDRKEY_OFFSET_TYPE] & 0xFF
                if linkKeyType != 0:
                    linkKeyLen = key_val[self.TDDB_BREDRKEY_OFFSET_LEN]
                    linkKey = self._swap_adjacent_elem(key_val[self.TDDB_BREDRKEY_OFFSET_LINKKEY :
                        self.TDDB_BREDRKEY_OFFSET_LINKKEY + 16])
                    authorised = key_val[self.TDDB_BREDRKEY_OFFSET_AUTHORISED]

                    bredr["type"] = linkKeyType
                    bredr["len"] = linkKeyLen
                    bredr["linkkey"] = linkKey
                    bredr["authorised"] = authorised
                    device["bredr"] = bredr


    def _extract_le_key(self, devices):
        """Method to extract the Le key information for each of the devices in the list."""
        for i,device in enumerate(devices):
            le = {}
            ps_key = self.PSKEY_TDDB_BASE_SC + self.TDDB_LIST_MAX + i
            key_val = self._psread(ps_key)

            if key_val is not None and len(key_val) > 0:
                keyValid = key_val[0] & 0xFF

                if keyValid != 0:
                    le["secReq"] = key_val[self.TDDB_LEKEY_OFFSET_SECREQ]

                    if keyValid & 0x01:
                        encCentral = key_val[self.TDDB_LEKEY_OFFSET_ENC:self.TDDB_LEKEY_OFFSET_DIV]

                        le["ediv"] = (encCentral[self.TDDB_LEKEY_ENCCENTRAL_OFFSET_EDIV] +
                            (encCentral[self.TDDB_LEKEY_ENCCENTRAL_OFFSET_EDIV + 1] << 8))
                        le["rand"] = self._swap_adjacent_elem(
                            encCentral[self.TDDB_LEKEY_ENCCENTRAL_OFFSET_RAND :
                            self.TDDB_LEKEY_ENCCENTRAL_OFFSET_RAND + 8])
                        le["ltk"] = self._swap_adjacent_elem(
                            encCentral[self.TDDB_LEKEY_ENCCENTRAL_OFFSET_LTK :
                            self.TDDB_LEKEY_ENCCENTRAL_OFFSET_LTK + 16])
                        le["keySize"] = key_val[1]

                    if keyValid & 0x02:
                        le["id"] = self._swap_adjacent_elem(key_val[self.TDDB_LEKEY_OFFSET_ID :
                            self.TDDB_LEKEY_OFFSET_SIGN])

                    if keyValid & 0x04:
                        sign = key_val[self.TDDB_LEKEY_OFFSET_SIGN : -1]
                        le["csrk"] = self._swap_adjacent_elem(sign[ : 16])
                        le["counter"] = (sign[8] + (sign[9] << 8) +
                            (sign[10] << 16) + (sign[11] << 24))

                    if keyValid & 0x08:
                        le["div"] = key_val[self.TDDB_LEKEY_OFFSET_DIV]

                    if keyValid & 0x10:
                        le["rpaOnly"] = True
                    else:
                        le["rpaOnly"] = False
                    device["le"] = le

    def _getDeviceStrData(self, device):
        """Method that returns a string with all the printable values for the device."""
        if "bredr" in device:
            bredr = device["bredr"]
            key_type = bredr["type"]
            mitm = False
            sc = False

            if key_type == self.KEY_TYPE_AUTH_P192 or key_type == self.KEY_TYPE_AUTH_P256:
                mitm = True

            if key_type == self.KEY_TYPE_UNAUTH_P256 or key_type == self.KEY_TYPE_AUTH_P256:
                sc = True

            result = "".join(["Trusted device : ", "Yes" if bredr["authorised"] else "No","\n"])
            result = "".join([result, "Encryption key size : ", str(bredr["len"]),"\n"])
            result = "".join([result, "MITM protection : ", "Yes" if mitm else "No","\n"])
            result = "".join([result, "Secure connections : ", "Yes" if sc else "No","\n"])
            result = "".join([result, "LE Bonding : ", "Yes" if "le" in device else "No","\n"])
            result = "".join([result, "Priority device : ", "Yes" if device["priority"] else "No","\n"])

            if key_type:
                header = "BR/EDR key ({})".format(bredr["type"])
                value = "".join(["{:02X}".format(n) for n in bredr["linkkey"]])
                result = "".join([result, header, " : ", value,"\n"])

        if "le" in device:
            le = device["le"]

            if "ltk" in le:
                header = "LE central keys"

                result = "".join([result, header, "\n"])
                result = "".join([result, "\tEDIV : ", "".join(["{:04X}".format(le["ediv"]), "\n"])])
                result = "".join([result, "\tRAND : ", "".join(["{:02X}".format(n) for n in le["rand"]]), "\n"])
                result = "".join([result, "\tLTK : ", "".join(["{:02X}".format(n) for n in le["ltk"]]), "\n"])

            if "div" in le:
                result = "".join([result, "Diversifier : ", "{:02X}".format(le["div"]), "\n"])

            if "id" in le:
                result = "".join([result, "ID IRK : ", "".join(["{:02X}".format(n) for n in le["id"]]), "\n"])

        return result


    def _generate_report_body_elements(self):
        if self._is_synergyadk:
            grp = interface.Group("Summary")

            device_count, devices = self.parse_tddb_list()

            if device_count != len(devices):
                tbl = interface.Error("Trusted device database is corrupted : Device count is {} "
                    "and number of devices in database is {}.\n".format(count,len(devices)))
                grp.append(tbl)

            keys_in_summary = ['bdaddr', 'value']
            tbl = interface.Table(keys_in_summary)

            self._extract_bredr_key(devices)
            self._extract_le_key(devices)

            for device in devices:
                row = []
                bdaddr = "{:04X}:{:02X}:{:06X}".format(device["addr"]["nap"],
                    device["addr"]["uap"], device["addr"]["lap"])
                tbl.add_row([bdaddr,self._getDeviceStrData(device)])

            grp.append(tbl)
        else:
            grp = interface.Group("Note :- Supported only for Synergy ADK build")

        return [grp]
