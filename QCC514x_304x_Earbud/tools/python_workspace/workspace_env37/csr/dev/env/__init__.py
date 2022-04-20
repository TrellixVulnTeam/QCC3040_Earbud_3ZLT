############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2013 - 2016 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
"""\
CSR Device Debugging Framework Environment 

Contains Abstract debugging environment Interface definitions and 
implementations/adaptors for various debug environments.

Example environments:-
- embedded in xide + project
- on top of coredump + elf
- directly on spi, trb... + elf

The goal is to enable python device models (hardware and firmware)
and views/analysers that are reusable in multiple debugging environments.

For example:-
- Code to model & view hydra logs should work in xide or coredump or standalone
on SPI/TRB.  
- A graphical view of the mmu state should work from live device or coredump.
- Anomoly analysers should work on coredumps and attached devices.
"""