############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2014 - 2016 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
"""
Interface to the contents of an assembly listing file.  Currently fairly simple:
can spot and parse instructions and functions in XAPGCC and KCC listing files.
"""

import os
import re
import sys
from csr.wheels.bitsandbobs import PureVirtualError, display_hex, HexDisplayInt

try:
    # Python 2
    int_type = (int, long)
except NameError:
    # Python 3
    int_type = int


class AsmOutOfSyncError(RuntimeError):
    """
    Indicates that the ASM file isn't consistent with the ELF
    """

class AsmInstruction(object):
    """
    Interface to properties of a specific instruction from the disassembly.
    This class can be specialised both to override certain platform-specific
    aspects of individual disassembled instructions and to add custom features.
    """
    
    def __init__(self, enc, text):
        self._enc = enc
        self._text = text
        
    def __repr__(self):
        return "%s    %s" % (self._enc, self._text)
        
    @property
    def encoding(self):
        """
        Return the integer or list of integers representing the opcode of the
        instruction
        """
        raise PureVirtualError
    
    @property
    def text(self):
        """
        Return the disassembly text itself in full
        """
        return self._text
    
    @property
    def length(self):
        """
        Return the size of the encoded instruction in native units: it's either
        a single integer or a list of bytes
        """
        if isinstance(self.encoding, int_type):
            return 1
        return len(self.encoding)
    
    @property
    def is_potential_branch(self):
        """
        Check whether the instruction looks like it might branch (by checking
        it for a custom set of substrings)
        """
        text = self.text
        return any(ins in text for ins in self._branch_instructions)
    
    @property
    def _branch_instruction(self):
        """
        Platform-specific list of substrings in disassembly text that indicate
        a potential branch
        """
        raise PureVirtualError
    

class XapAsmInstruction(AsmInstruction):
    """
    AsmInstruction specialisation for the XAP: knows which XAP instructions are
    branches, converts the opcode to an integer, and allows the instruction and
    its operands to be distinguished
    """
    @property
    def _branch_instructions(self):
        return ("bcc", "bcs", "beq", "blt", "bmi", "bne", "bpl", "bra",
                "bra2", "bra3", "brxl", "bsr", "rts", "rti")

    @property
    @display_hex
    def encoding(self):
        """
        XAP opcodes are a single hex integer
        """
        return int(self._enc, 16)

    @property
    def operator(self):
        """
        The operator is the first word of the disassembled text
        """
        return self.text.split()[0]
    
    @property
    def operands(self):
        """
        The operands are whatever's left when you take the operator off
        """
        return self.text.lstrip(self.operator).lstrip()
        

class KalAsmInstruction(AsmInstruction):
    """
    AsmInstruction specialisation for Kalimba: knows which Kalimba instructions
    are branches and splits the opcode into a list of bytes
    """
    @property
    def _branch_instructions(self):
        return ("call", "jump", "rts", "rti")
    
    @property
    @display_hex
    def encoding(self):
        """
        Kalimba (32) opcodes are a sequence of bytes
        """
        return [int(e, 16) for e in self._enc.split()]

    def extend(self, instruction_tail):
        """
        Append extra bytes to the encoding for multiline instructions
        """
        self._enc += instruction_tail
    
class AsmListing(object):
    """
    Generic assembly listing parser.  Can detect instructions and functions,
    pulling out the salient information.
    """
    @classmethod
    def get_asm_listing(cls, listing_file):
        if os.path.exists(listing_file):
            asm_listing = {}
            function_listing = {}
            if sys.version_info < (3, 0):
                open_file = open(listing_file)
            else:
                open_file = open(listing_file, encoding="utf_8", errors="surrogateescape")
            with open_file as listing:
                for line in listing:
                    asm_line = cls.ASM_RE.match(line)
                    if asm_line:
                        addr = int(asm_line.group("addr"),16)
                        asm = asm_line.group("asm")
                        code = asm_line.group("code")
                        # Store the string of disassembly plus the length of
                        # the instruction in bytes
                        asm_listing[addr] = cls.AsmInstructionType(code, asm)

                    asm_multiline = cls.ASM_MULTILINE_RE.match(line)
                    if asm_multiline:
                        # Instructions over 4 bytes take multiple lines
                        # append them to the last created instruction
                        asm_listing[addr].extend(asm_multiline.group("code"))

                    asm_func = cls.ASM_FUNC_RE.match(line)
                    if asm_func:
                        func = asm_func.group("func")
                        addr = asm_func.group("addr")
                        function_listing[func] = HexDisplayInt(int(addr, 16))
        

        else:
            asm_listing = None
            function_listing = None
            
        return {"instructions": asm_listing, "functions": function_listing}


    def __init__(self, asm_path):
        
        self._asm_path = asm_path
        
    @property
    def instructions(self):
        """
        Lazily evaluate the dictionaries of instructions and functions if
        necessary, and return the instructions
        """
        try:
            self._instructions
        except AttributeError:
            d = self.get_asm_listing(self._asm_path)
            self._instructions = d["instructions"]
            self._functions = d["functions"]
        return self._instructions

    @property
    def functions(self):
        """
        Lazily evaluate the dictionaries of instructions and functions if
        necessary, and return the functions
        """
        try:
            self._functions
        except AttributeError:
            d = self.get_asm_listing(self._asm_path)
            self._instructions = d["instructions"]
            self._functions = d["functions"]
        return self._functions

    
class XapAsmListing(AsmListing):
    """
    Specialisation for XAP GCC's proc.lst file
    """
    ASM_RE = re.compile(r"\s+(?P<addr>[A-F0-9]{1,6}):\s+(?P<code>[0-9A-F]{4})\s+(?P<asm>[\w\*].*)")
    ASM_MULTILINE_RE = re.compile(r"$a") # Never match anything
    ASM_FUNC_RE = re.compile(r"(?P<addr>0x[0-9A-F]{8})\s+\<(?P<func>\w+)\>:\s*")
    AsmInstructionType = XapAsmInstruction

class KalAsmListing(AsmListing):
    """
    Specialisation for KCC's <>.lst file
    """
    addr = r"8(?P<addr>[a-f0-9]{7})"
    code = r"(?P<code>(([0-9a-f]{2}\s){2}){1,2})"
    asm = r"((?P<asm>\w.*);)"
    func = r"\<\$_(?P<func>\w+)\>:"

    ASM_RE = re.compile(addr + ":\s+" + code + "\s*" + asm)
    ASM_MULTILINE_RE = re.compile(addr + ":\s+" + code + "$")
    ASM_FUNC_RE = re.compile(addr + "\s+" + func)

    AsmInstructionType = KalAsmInstruction
