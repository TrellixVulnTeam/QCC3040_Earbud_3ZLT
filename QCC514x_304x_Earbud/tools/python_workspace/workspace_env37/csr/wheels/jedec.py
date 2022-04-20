############################################################################
# CONFIDENTIAL
#
# %%fullcopyright(2017)
#
############################################################################
"""
Helper classes for JEDEC manufacturers 
"""

#import sys

class JedecLookup:
    """
    Map a JEDEC manufacturer ID to a name.

    Includes the top level table only, no extension IDs, with the table
    derived from JEDEC document. JEP106AR

    If we have a shortlist of supported IDs this can be supplied during 
    creation of class and updates the list used.

    NOTE that full JEDEC support would require tiered IDs. Not yet 
    implemented / known to be used.
    """
    JEDEC_MFR_LOOKUP = {
                0x01 : "AMD",
                0x02 : "AMI",
                0x04 : "Fujitsu",
                0x07 : "Hitachi",
                0x08 : "Inmos",
                0x0B : "Intersil",
                0x0D : "Mostek",
                0x0E : "Freescale",
                0x10 : "NEC",
                0x13 : "Conexant",
                0x15 : "NXP",
                0x16 : "Synertek",
                0x19 : "Xicor",
                0x1A : "Zilog",
                0x1C : "Mitsubishi",
                0x1F : "Atmel",
                0x20 : "STMicroelectronics",
                0x23 : "Wafer Scale Integration",
                0x25 : "Tristar",
                0x26 : "Visic",
                0x29 : "MicrochipTechnology",
                0x2A : "Ricoh Ltd.",
                0x2C : "Micron Technology",
                0x2F : "ACTEL",
                0x31 : "Catalyst",
                0x32 : "Panasonic",
                0x34 : "Cypress",
                0x37 : "Zarlink",
                0x38 : "UTMC",
                0x3B : "Integrated CMOS (Vertex)",
                0x3D : "Tektronix",
                0x3E : "Oracle Corporation",
                0x40 : "ProMos/Mosel Vitelic",
                0x43 : "Xerox",
                0x45 : "SanDisk Corporation",
                0x46 : "Elan Circuit Tech.",
                0x49 : "Xilinx",
                0x4A : "Compaq",
                0x4C : "SCI",
                0x4F : "I3 Design System",
                0x51 : "Crosspoint Solutions",
                0x52 : "Alliance Semiconductor",
                0x54 : "Hewlett-Packard",
                0x57 : "New Media",
                0x58 : "MHS Electronic",
                0x5B : "Kawasaki Steel",
                0x5D : "TECMAR",
                0x5E : "Exar",
                0x61 : "Northern Telecom",
                0x62 : "Sanyo",
                0x64 : "Crystal Semiconductor",
                0x67 : "Asparix",
                0x68 : "Convex Computer",
                0x6B : "Transwitch",
                0x6D : "Cannon",
                0x6E : "Altera",
                0x70 : "Qualcomm",
                0x73 : "AMS(Austria Micro)",
                0x75 : "Aster Electronics",
                0x76 : "Bay Networks (Synoptic)",
                0x79 : "Thesys",
                0x7A : "Solbourne Computer",
                0x7C : "Dialog Semiconductor",
                0x7F : "<extended ID>",
                0x83 : "Fairchild",
                0x85 : "GTE",
                0x86 : "Harris",
                0x89 : "Intel",
                0x8A : "I.T.T.",
                0x8C : "Monolithic Memories",
                0x8F : "National",
                0x91 : "RCA",
                0x92 : "Raytheon",
                0x94 : "Seeq",
                0x97 : "Texas Instruments",
                0x98 : "Toshiba",
                0x9B : "Eurotechnique",
                0x9D : "Lucent",
                0x9E : "Exel",
                0xA1 : "Lattice Semi.",
                0xA2 : "NCR",
                0xA4 : "IBM",
                0xA7 : "Intl. CMOS Technology",
                0xA8 : "SSSI",
                0xAB : "VLSI",
                0xAD : "SK Hynix",
                0xAE : "OKI Semiconductor",
                0xB0 : "Sharp",
                0xB3 : "IDT",
                0xB5 : "DEC",
                0xB6 : "LSI Logic",
                0xB9 : "Thinking Machine",
                0xBA : "Thomson CSF",
                0xBC : "Honeywell",
                0xBF : "Silicon Storage Technology",
                0xC1 : "Infineon",
                0xC2 : "Macronix",
                0xC4 : "Plus Logic",
                0xC7 : "European Silicon Str.",
                0xC8 : "Apple Computer",
                0xCB : "Protocol Engines",
                0xCD : "Seiko Instruments",
                0xCE : "Samsung",
                0xD0 : "Klic",
                0xD3 : "Tandem",
                0xD5 : "Integrated Silicon Solutions",
                0xD6 : "Brooktree",
                0xD9 : "Performance Semi.",
                0xDA : "Winbond Electronic",
                0xDC : "Bright Micro",
                0xDF : "PCMCIA",
                0xE0 : "LG Semi",
                0xE3 : "Array Microsystems",
                0xE5 : "Analog Devices",
                0xE6 : "PMC-Sierra",
                0xE9 : "Quality Semiconductor",
                0xEA : "Nimbus Technology",
                0xEC : "Micronas",
                0xEF : "NEXCOM",
                0xF1 : "Sony",
                0xF2 : "Cray Research",
                0xF4 : "Vitesse",
                0xF7 : "Zentrum/ZMD",
                0xF8 : "TRW",
                0xFB : "Allied-Signal",
                0xFD : "Media Vision",
                0xFE : "Numonyx Corporation",
        }

    def __init__(self,preferred_names=None):
        self._preferred_names = preferred_names
        self._lookup = self.JEDEC_MFR_LOOKUP.copy()
        if self._preferred_names:
            self._lookup.update(self._preferred_names)

    def dict(self):
        return self._lookup

    def get_mfr_string(self,numeric_id):
        try:
            return self._lookup[numeric_id]
        except KeyError:
            return "UNKNOWN JEDEC MFR %d (0x%x)"%(numeric_id,numeric_id)

    def get_known_mfr_string(self,numeric_id):
        """
        Lookup ID from short list.
        Raises KeyError if not known
        """
        try:
            return self._preferred_names[numeric_id]
        except TypeError:
            raise KeyError("No preferred names supplied. ID requested 0x%x"%numeric_id)

