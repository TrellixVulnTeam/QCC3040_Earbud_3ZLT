# -*- coding: utf-8 -*-
#------------------------------------------------------------------------------
# Automatically-generated memory-mapped registers IO file
#------------------------------------------------------------------------------

class c_enum(object):
   def __init__(self, parent, value, text, reg):
      self.parent = parent
      self.value  = value
      self.text   = text
      self.reg    = reg

class c_value(object):
   def __init__(self, parent, name, value, text):
      self.parent = parent
      self.name   = name
      self.value  = value
      self.text   = text

class c_bits(object):
   def __init__(self, parent, lsb, msb, mask, width, rw_flags, text):
      self.parent   = parent
      self.lsb      = lsb
      self.msb      = msb
      self.mask     = mask
      self.rw_flags = rw_flags
      self.width    = width
      self.text     = text

class c_reg(object):
   def __init__(self, addr, r, w, m, rw_flags, width, reset, local, ext_read, mod_name, group, text, locked_by, unlock_value, typedefd_io, safe_to_load, is_bank_ctrl, bank_ctrl_reg, constant):
      self.addr          = addr
      self.r             = r            # Readable
      self.w             = w            # Writeable
      self.m             = m            # Mixed register type
      self.rw_flags      = rw_flags     # RW flags
      self.width         = width
      self.local         = local
      self.ext_read      = ext_read
      self.mod_name      = mod_name
      self.text          = text
      self.group         = group
      self.reset         = reset
      self.locked_by     = locked_by
      self.unlock_value  = unlock_value
      self.safe_to_load  = safe_to_load
      self.typedefd_io   = typedefd_io
      self.is_bank_ctrl  = is_bank_ctrl
      self.bank_ctrl_reg = bank_ctrl_reg
      self.constant      = constant

class c_regarray(object):
   def __init__(self, addr, num_elements, element):
      self.addr = addr
      self.num_elements  = num_elements
      self.element = element




































































ARITHMETIC_MODE                                                                  = c_reg(0xffffe014, 1, 1, 0, "", 5, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
ARITHMETIC_MODE.ADDSUB_SATURATE_ON_OVERFLOW                                      = c_bits(ARITHMETIC_MODE, 0, 0, 0x1, 1, "", "")
ARITHMETIC_MODE.ARITHMETIC_16BIT_MODE                                            = c_bits(ARITHMETIC_MODE, 1, 1, 0x2, 1, "", "")
ARITHMETIC_MODE.DISABLE_UNBIASED_ROUNDING                                        = c_bits(ARITHMETIC_MODE, 2, 2, 0x4, 1, "", "")
ARITHMETIC_MODE.DISABLE_FRAC_MULT_ROUNDING                                       = c_bits(ARITHMETIC_MODE, 3, 3, 0x8, 1, "", "")
ARITHMETIC_MODE.DISABLE_RMAC_STORE_ROUNDING                                      = c_bits(ARITHMETIC_MODE, 4, 4, 0x10, 1, "", "")
BITREVERSE_ADDR                                                                  = c_reg(0xffffe038, 1, 0, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
BITREVERSE_ADDR.BITREVERSE_ADDR                                                  = c_bits(BITREVERSE_ADDR, 0, 31, 0xffffffff, 32, "", "")
BITREVERSE_DATA                                                                  = c_reg(0xffffe030, 1, 0, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
BITREVERSE_DATA.BITREVERSE_DATA                                                  = c_bits(BITREVERSE_DATA, 0, 31, 0xffffffff, 32, "", "")
BITREVERSE_DATA16                                                                = c_reg(0xffffe034, 1, 0, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
BITREVERSE_DATA16.BITREVERSE_DATA16                                              = c_bits(BITREVERSE_DATA16, 0, 31, 0xffffffff, 32, "", "")
BITREVERSE_VAL                                                                   = c_reg(0xffffe02c, 1, 1, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
BITREVERSE_VAL.BITREVERSE_VAL                                                    = c_bits(BITREVERSE_VAL, 0, 31, 0xffffffff, 32, "", "")
DBG_COUNTERS_EN                                                                  = c_reg(0xffffe050, 1, 1, 0, "", 1, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
DBG_COUNTERS_EN.DBG_COUNTERS_EN                                                  = c_bits(DBG_COUNTERS_EN, 0, 0, 0x1, 1, "", "")
FRAME_POINTER                                                                    = c_reg(0xffffe028, 1, 1, 0, "", 32, 0, 1, 1, "", "", "", "", "", 0, 1, "", "", 0)
FRAME_POINTER.FRAME_POINTER                                                      = c_bits(FRAME_POINTER, 0, 31, 0xffffffff, 32, "", "")
MM_DOLOOP_END                                                                    = c_reg(0xffffe004, 1, 1, 0, "", 32, 0, 1, 1, "", "", "", "", "", 0, 1, "", "", 0)
MM_DOLOOP_END.MM_DOLOOP_END                                                      = c_bits(MM_DOLOOP_END, 0, 31, 0xffffffff, 32, "", "")
MM_DOLOOP_START                                                                  = c_reg(0xffffe000, 1, 1, 0, "", 32, 0, 1, 1, "", "", "", "", "", 0, 1, "", "", 0)
MM_DOLOOP_START.MM_DOLOOP_START                                                  = c_bits(MM_DOLOOP_START, 0, 31, 0xffffffff, 32, "", "")
MM_QUOTIENT                                                                      = c_reg(0xffffe008, 1, 1, 0, "", 32, 0, 1, 1, "", "", "", "", "", 0, 1, "", "", 0)
MM_QUOTIENT.MM_QUOTIENT                                                          = c_bits(MM_QUOTIENT, 0, 31, 0xffffffff, 32, "", "")
MM_REM                                                                           = c_reg(0xffffe00c, 1, 1, 0, "", 32, 0, 1, 1, "", "", "", "", "", 0, 1, "", "", 0)
MM_REM.MM_REM                                                                    = c_bits(MM_REM, 0, 31, 0xffffffff, 32, "", "")
MM_RINTLINK                                                                      = c_reg(0xffffe010, 1, 1, 0, "", 32, 0, 1, 1, "", "", "", "", "", 0, 1, "", "", 0)
MM_RINTLINK.MM_RINTLINK                                                          = c_bits(MM_RINTLINK, 0, 31, 0xffffffff, 32, "", "")
NUM_CORE_STALLS                                                                  = c_reg(0xffffe044, 1, 0, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
NUM_CORE_STALLS.NUM_CORE_STALLS                                                  = c_bits(NUM_CORE_STALLS, 0, 31, 0xffffffff, 32, "", "")
NUM_INSTRS                                                                       = c_reg(0xffffe040, 1, 0, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
NUM_INSTRS.NUM_INSTRS                                                            = c_bits(NUM_INSTRS, 0, 31, 0xffffffff, 32, "", "")
NUM_INSTR_EXPAND_STALLS                                                          = c_reg(0xffffe04c, 1, 0, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
NUM_INSTR_EXPAND_STALLS.NUM_INSTR_EXPAND_STALLS                                  = c_bits(NUM_INSTR_EXPAND_STALLS, 0, 31, 0xffffffff, 32, "", "")
NUM_MEM_ACCESS_STALLS                                                            = c_reg(0xffffe048, 1, 0, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
NUM_MEM_ACCESS_STALLS.NUM_MEM_ACCESS_STALLS                                      = c_bits(NUM_MEM_ACCESS_STALLS, 0, 31, 0xffffffff, 32, "", "")
NUM_RUN_CLKS                                                                     = c_reg(0xffffe03c, 1, 0, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
NUM_RUN_CLKS.NUM_RUN_CLKS                                                        = c_bits(NUM_RUN_CLKS, 0, 31, 0xffffffff, 32, "", "")
PC_STATUS                                                                        = c_reg(0xffffe054, 1, 0, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
PC_STATUS.PC_STATUS                                                              = c_bits(PC_STATUS, 0, 31, 0xffffffff, 32, "", "")
STACK_END_ADDR                                                                   = c_reg(0xffffe01c, 1, 1, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
STACK_END_ADDR.STACK_END_ADDR                                                    = c_bits(STACK_END_ADDR, 0, 31, 0xffffffff, 32, "", "")
STACK_OVERFLOW_PC                                                                = c_reg(0xffffe024, 1, 0, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
STACK_OVERFLOW_PC.STACK_OVERFLOW_PC                                              = c_bits(STACK_OVERFLOW_PC, 0, 31, 0xffffffff, 32, "", "")
STACK_POINTER                                                                    = c_reg(0xffffe020, 1, 1, 0, "", 32, 0, 1, 1, "", "", "", "", "", 0, 1, "", "", 0)
STACK_POINTER.STACK_POINTER                                                      = c_bits(STACK_POINTER, 0, 31, 0xffffffff, 32, "", "")
STACK_START_ADDR                                                                 = c_reg(0xffffe018, 1, 1, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
STACK_START_ADDR.STACK_START_ADDR                                                = c_bits(STACK_START_ADDR, 0, 31, 0xffffffff, 32, "", "")
TEST_REG_0                                                                       = c_reg(0xffffe058, 1, 1, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
TEST_REG_0.TEST_REG_0                                                            = c_bits(TEST_REG_0, 0, 31, 0xffffffff, 32, "", "")
TEST_REG_1                                                                       = c_reg(0xffffe05c, 1, 1, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
TEST_REG_1.TEST_REG_1                                                            = c_bits(TEST_REG_1, 0, 31, 0xffffffff, 32, "", "")
TEST_REG_2                                                                       = c_reg(0xffffe060, 1, 1, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
TEST_REG_2.TEST_REG_2                                                            = c_bits(TEST_REG_2, 0, 31, 0xffffffff, 32, "", "")
TEST_REG_3                                                                       = c_reg(0xffffe064, 1, 1, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
TEST_REG_3.TEST_REG_3                                                            = c_bits(TEST_REG_3, 0, 31, 0xffffffff, 32, "", "")



DEBUG                                                                            = c_reg(0xfffffe30, 1, 1, 0, "", 19, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
DEBUG.DEBUG_RUN                                                                  = c_bits(DEBUG, 0, 0, 0x1, 1, "", "")
DEBUG.DEBUG_STEP                                                                 = c_bits(DEBUG, 1, 1, 0x2, 1, "", "")
DEBUG.DEBUG_DUMMY                                                                = c_bits(DEBUG, 2, 2, 0x4, 1, "", "")
DEBUG.DEBUG_PM_BREAK0                                                            = c_bits(DEBUG, 3, 3, 0x8, 1, "", "")
DEBUG.DEBUG_PM_BREAK1                                                            = c_bits(DEBUG, 4, 4, 0x10, 1, "", "")
DEBUG.DEBUG_PM_BREAK2                                                            = c_bits(DEBUG, 5, 5, 0x20, 1, "", "")
DEBUG.DEBUG_PM_BREAK3                                                            = c_bits(DEBUG, 6, 6, 0x40, 1, "", "")
DEBUG.DEBUG_PM_BREAK4                                                            = c_bits(DEBUG, 7, 7, 0x80, 1, "", "")
DEBUG.DEBUG_PM_BREAK5                                                            = c_bits(DEBUG, 8, 8, 0x100, 1, "", "")
DEBUG.DEBUG_PM_BREAK6                                                            = c_bits(DEBUG, 9, 9, 0x200, 1, "", "")
DEBUG.DEBUG_PM_BREAK7                                                            = c_bits(DEBUG, 10, 10, 0x400, 1, "", "")
DEBUG.DEBUG_DM_WRITE_BREAK0                                                      = c_bits(DEBUG, 11, 11, 0x800, 1, "", "")
DEBUG.DEBUG_DM_READ_BREAK0                                                       = c_bits(DEBUG, 12, 12, 0x1000, 1, "", "")
DEBUG.DEBUG_DM_WRITE_BREAK1                                                      = c_bits(DEBUG, 13, 13, 0x2000, 1, "", "")
DEBUG.DEBUG_DM_READ_BREAK1                                                       = c_bits(DEBUG, 14, 14, 0x4000, 1, "", "")
DEBUG.DEBUG_DM_BYTE_MATCH_EN_BREAK0                                              = c_bits(DEBUG, 15, 15, 0x8000, 1, "", "")
DEBUG.DEBUG_DM_DATA_MATCH_EN_BREAK0                                              = c_bits(DEBUG, 16, 16, 0x10000, 1, "", "")
DEBUG.DEBUG_DM_BYTE_MATCH_EN_BREAK1                                              = c_bits(DEBUG, 17, 17, 0x20000, 1, "", "")
DEBUG.DEBUG_DM_DATA_MATCH_EN_BREAK1                                              = c_bits(DEBUG, 18, 18, 0x40000, 1, "", "")
DEBUG_PM_BREAK7_ENABLED                                                          = c_reg(0xfffffe60, 1, 1, 0, "", 7, 0, 1, 1, "", "", "", "", "", 0, 1, "", "", 0)
DEBUG_PM_BREAK7_ENABLED.DEBUG_PM_BREAK7_ENABLED                                  = c_bits(DEBUG_PM_BREAK7_ENABLED, 0, 6, 0x7f, 7, "", "")
DEBUG_PM_BREAK7_ENABLES                                                          = c_reg(0xfffffe5c, 1, 1, 0, "", 7, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
DEBUG_PM_BREAK7_ENABLES.DEBUG_PM_BREAK7_ENABLES                                  = c_bits(DEBUG_PM_BREAK7_ENABLES, 0, 6, 0x7f, 7, "", "")
DM_BREAK0_BYTE_SELECT                                                            = c_reg(0xfffffe40, 1, 1, 0, "", 4, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
DM_BREAK0_BYTE_SELECT.DM_BREAK0_BYTE_SELECT                                      = c_bits(DM_BREAK0_BYTE_SELECT, 0, 3, 0xf, 4, "", "")
DM_BREAK0_DATA_MASK                                                              = c_reg(0xfffffe48, 1, 1, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
DM_BREAK0_DATA_VALUE                                                             = c_reg(0xfffffe44, 1, 1, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
DM_BREAK0_DATA_VALUE.DM_BREAK0_DATA_VALUE                                        = c_bits(DM_BREAK0_DATA_VALUE, 0, 31, 0xffffffff, 32, "", "")
DM_BREAK0_END_ADDR                                                               = c_reg(0xfffffe04, 1, 1, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
DM_BREAK0_END_ADDR.DM_BREAK0_END_ADDR                                            = c_bits(DM_BREAK0_END_ADDR, 0, 31, 0xffffffff, 32, "", "")
DM_BREAK0_START_ADDR                                                             = c_reg(0xfffffe00, 1, 1, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
DM_BREAK0_START_ADDR.DM_BREAK0_START_ADDR                                        = c_bits(DM_BREAK0_START_ADDR, 0, 31, 0xffffffff, 32, "", "")
DM_BREAK1_BYTE_SELECT                                                            = c_reg(0xfffffe4c, 1, 1, 0, "", 4, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
DM_BREAK1_BYTE_SELECT.DM_BREAK1_BYTE_SELECT                                      = c_bits(DM_BREAK1_BYTE_SELECT, 0, 3, 0xf, 4, "", "")
DM_BREAK1_DATA_MASK                                                              = c_reg(0xfffffe54, 1, 1, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
DM_BREAK1_DATA_VALUE                                                             = c_reg(0xfffffe50, 1, 1, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
DM_BREAK1_DATA_VALUE.DM_BREAK1_DATA_VALUE                                        = c_bits(DM_BREAK1_DATA_VALUE, 0, 31, 0xffffffff, 32, "", "")
DM_BREAK1_END_ADDR                                                               = c_reg(0xfffffe0c, 1, 1, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
DM_BREAK1_END_ADDR.DM_BREAK1_END_ADDR                                            = c_bits(DM_BREAK1_END_ADDR, 0, 31, 0xffffffff, 32, "", "")
DM_BREAK1_START_ADDR                                                             = c_reg(0xfffffe08, 1, 1, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
DM_BREAK1_START_ADDR.DM_BREAK1_START_ADDR                                        = c_bits(DM_BREAK1_START_ADDR, 0, 31, 0xffffffff, 32, "", "")
EXTERNAL_BREAK                                                                   = c_reg(0xfffffe38, 1, 1, 0, "", 2, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
EXTERNAL_BREAK.EXTERNAL_BREAK_RECEIVE_EN                                         = c_bits(EXTERNAL_BREAK, 0, 0, 0x1, 1, "", "")
EXTERNAL_BREAK.EXTERNAL_BREAK_SEND_EN                                            = c_bits(EXTERNAL_BREAK, 1, 1, 0x2, 1, "", "")
EXTERNAL_BREAK_STATUS                                                            = c_reg(0xfffffe58, 1, 1, 0, "", 1, 0, 1, 0, "", "", "", "", "", 0, 1, "", "", 0)
EXTERNAL_BREAK_STATUS.EXTERNAL_BREAK_STATUS                                      = c_bits(EXTERNAL_BREAK_STATUS, 0, 0, 0x1, 1, "", "")
INTERPROC_BREAK                                                                  = c_reg(0xfffffe3c, 1, 1, 0, "", 2, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
INTERPROC_BREAK.INTERPROC_BREAK_RECEIVE_EN                                       = c_bits(INTERPROC_BREAK, 0, 0, 0x1, 1, "", "")
INTERPROC_BREAK.INTERPROC_BREAK_SEND_EN                                          = c_bits(INTERPROC_BREAK, 1, 1, 0x2, 1, "", "")
PM_BREAK0_ADDR                                                                   = c_reg(0xfffffe10, 1, 1, 0, "", 32, 0, 1, 1, "", "", "", "", "", 0, 1, "", "", 0)
PM_BREAK0_ADDR.PM_BREAK0_ADDR                                                    = c_bits(PM_BREAK0_ADDR, 0, 31, 0xffffffff, 32, "", "")
PM_BREAK1_ADDR                                                                   = c_reg(0xfffffe14, 1, 1, 0, "", 32, 0, 1, 1, "", "", "", "", "", 0, 1, "", "", 0)
PM_BREAK1_ADDR.PM_BREAK1_ADDR                                                    = c_bits(PM_BREAK1_ADDR, 0, 31, 0xffffffff, 32, "", "")
PM_BREAK2_ADDR                                                                   = c_reg(0xfffffe18, 1, 1, 0, "", 32, 0, 1, 1, "", "", "", "", "", 0, 1, "", "", 0)
PM_BREAK2_ADDR.PM_BREAK2_ADDR                                                    = c_bits(PM_BREAK2_ADDR, 0, 31, 0xffffffff, 32, "", "")
PM_BREAK3_ADDR                                                                   = c_reg(0xfffffe1c, 1, 1, 0, "", 32, 0, 1, 1, "", "", "", "", "", 0, 1, "", "", 0)
PM_BREAK3_ADDR.PM_BREAK3_ADDR                                                    = c_bits(PM_BREAK3_ADDR, 0, 31, 0xffffffff, 32, "", "")
PM_BREAK4_ADDR                                                                   = c_reg(0xfffffe20, 1, 1, 0, "", 32, 0, 1, 1, "", "", "", "", "", 0, 1, "", "", 0)
PM_BREAK4_ADDR.PM_BREAK4_ADDR                                                    = c_bits(PM_BREAK4_ADDR, 0, 31, 0xffffffff, 32, "", "")
PM_BREAK5_ADDR                                                                   = c_reg(0xfffffe24, 1, 1, 0, "", 32, 0, 1, 1, "", "", "", "", "", 0, 1, "", "", 0)
PM_BREAK5_ADDR.PM_BREAK5_ADDR                                                    = c_bits(PM_BREAK5_ADDR, 0, 31, 0xffffffff, 32, "", "")
PM_BREAK6_ADDR                                                                   = c_reg(0xfffffe28, 1, 1, 0, "", 32, 0, 1, 1, "", "", "", "", "", 0, 1, "", "", 0)
PM_BREAK6_ADDR.PM_BREAK6_ADDR                                                    = c_bits(PM_BREAK6_ADDR, 0, 31, 0xffffffff, 32, "", "")
PM_BREAK7_ADDR                                                                   = c_reg(0xfffffe2c, 1, 1, 0, "", 32, 0, 1, 1, "", "", "", "", "", 0, 1, "", "", 0)
PM_BREAK7_ADDR.PM_BREAK7_ADDR                                                    = c_bits(PM_BREAK7_ADDR, 0, 31, 0xffffffff, 32, "", "")
REGFILE_PC                                                                       = c_reg(0xffffff00, 1, 1, 0, "", 32, 0, 1, 1, "", "", "", "", "", 0, 1, "", "", 0)
REGFILE_PC.REGFILE_PC                                                            = c_bits(REGFILE_PC, 0, 31, 0xffffffff, 32, "", "")
STATUS                                                                           = c_reg(0xfffffe34, 1, 0, 0, "", 17, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
STATUS.STATUS_RUNNING                                                            = c_bits(STATUS, 0, 0, 0x1, 1, "", "")
STATUS.STATUS_PM_BREAK                                                           = c_bits(STATUS, 1, 1, 0x2, 1, "", "")
STATUS.STATUS_DM_BREAK0                                                          = c_bits(STATUS, 2, 2, 0x4, 1, "", "")
STATUS.STATUS_DM_BREAK1                                                          = c_bits(STATUS, 3, 3, 0x8, 1, "", "")
STATUS.STATUS_DUMMY                                                              = c_bits(STATUS, 4, 4, 0x10, 1, "", "")
STATUS.STATUS_PM_WAIT_IN                                                         = c_bits(STATUS, 5, 5, 0x20, 1, "", "")
STATUS.STATUS_DM1_WAIT_IN                                                        = c_bits(STATUS, 6, 6, 0x40, 1, "", "")
STATUS.STATUS_DM2_WAIT_IN                                                        = c_bits(STATUS, 7, 7, 0x80, 1, "", "")
STATUS.STATUS_RETRYPMREAD                                                        = c_bits(STATUS, 8, 8, 0x100, 1, "", "")
STATUS.STATUS_DLYRUNNING                                                         = c_bits(STATUS, 9, 9, 0x200, 1, "", "")
STATUS.STATUS_SINGSTEPCOMP                                                       = c_bits(STATUS, 10, 10, 0x400, 1, "", "")
STATUS.STATUS_PROCESSING                                                         = c_bits(STATUS, 11, 11, 0x800, 1, "", "")
STATUS.STATUS_EXECUTING                                                          = c_bits(STATUS, 12, 12, 0x1000, 1, "", "")
STATUS.STATUS_STALLEDPM                                                          = c_bits(STATUS, 13, 13, 0x2000, 1, "", "")
STATUS.STATUS_EXCEPTION_BREAK                                                    = c_bits(STATUS, 14, 14, 0x4000, 1, "", "")
STATUS.STATUS_EXTERNAL_BREAK                                                     = c_bits(STATUS, 15, 15, 0x8000, 1, "", "")
STATUS.STATUS_INTERPROC_BREAK                                                    = c_bits(STATUS, 16, 16, 0x10000, 1, "", "")



REGFILE_B0                                                                       = c_reg(0xffffffa4, 1, 1, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
REGFILE_B0.REGFILE_B0                                                            = c_bits(REGFILE_B0, 0, 31, 0xffffffff, 32, "", "")
REGFILE_B1                                                                       = c_reg(0xffffffa8, 1, 1, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
REGFILE_B1.REGFILE_B1                                                            = c_bits(REGFILE_B1, 0, 31, 0xffffffff, 32, "", "")
REGFILE_B4                                                                       = c_reg(0xffffffac, 1, 1, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
REGFILE_B4.REGFILE_B4                                                            = c_bits(REGFILE_B4, 0, 31, 0xffffffff, 32, "", "")
REGFILE_B5                                                                       = c_reg(0xffffffb0, 1, 1, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
REGFILE_B5.REGFILE_B5                                                            = c_bits(REGFILE_B5, 0, 31, 0xffffffff, 32, "", "")
REGFILE_FP                                                                       = c_reg(0xffffffb4, 1, 1, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
REGFILE_FP.REGFILE_FP                                                            = c_bits(REGFILE_FP, 0, 31, 0xffffffff, 32, "", "")
REGFILE_I0                                                                       = c_reg(0xffffff4c, 1, 1, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
REGFILE_I0.REGFILE_I0                                                            = c_bits(REGFILE_I0, 0, 31, 0xffffffff, 32, "", "")
REGFILE_I1                                                                       = c_reg(0xffffff50, 1, 1, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
REGFILE_I1.REGFILE_I1                                                            = c_bits(REGFILE_I1, 0, 31, 0xffffffff, 32, "", "")
REGFILE_I2                                                                       = c_reg(0xffffff54, 1, 1, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
REGFILE_I2.REGFILE_I2                                                            = c_bits(REGFILE_I2, 0, 31, 0xffffffff, 32, "", "")
REGFILE_I3                                                                       = c_reg(0xffffff58, 1, 1, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
REGFILE_I3.REGFILE_I3                                                            = c_bits(REGFILE_I3, 0, 31, 0xffffffff, 32, "", "")
REGFILE_I4                                                                       = c_reg(0xffffff5c, 1, 1, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
REGFILE_I4.REGFILE_I4                                                            = c_bits(REGFILE_I4, 0, 31, 0xffffffff, 32, "", "")
REGFILE_I5                                                                       = c_reg(0xffffff60, 1, 1, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
REGFILE_I5.REGFILE_I5                                                            = c_bits(REGFILE_I5, 0, 31, 0xffffffff, 32, "", "")
REGFILE_I6                                                                       = c_reg(0xffffff64, 1, 1, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
REGFILE_I6.REGFILE_I6                                                            = c_bits(REGFILE_I6, 0, 31, 0xffffffff, 32, "", "")
REGFILE_I7                                                                       = c_reg(0xffffff68, 1, 1, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
REGFILE_I7.REGFILE_I7                                                            = c_bits(REGFILE_I7, 0, 31, 0xffffffff, 32, "", "")
REGFILE_L0                                                                       = c_reg(0xffffff7c, 1, 1, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
REGFILE_L0.REGFILE_L0                                                            = c_bits(REGFILE_L0, 0, 31, 0xffffffff, 32, "", "")
REGFILE_L1                                                                       = c_reg(0xffffff80, 1, 1, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
REGFILE_L1.REGFILE_L1                                                            = c_bits(REGFILE_L1, 0, 31, 0xffffffff, 32, "", "")
REGFILE_L4                                                                       = c_reg(0xffffff84, 1, 1, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
REGFILE_L4.REGFILE_L4                                                            = c_bits(REGFILE_L4, 0, 31, 0xffffffff, 32, "", "")
REGFILE_L5                                                                       = c_reg(0xffffff88, 1, 1, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
REGFILE_L5.REGFILE_L5                                                            = c_bits(REGFILE_L5, 0, 31, 0xffffffff, 32, "", "")
REGFILE_M0                                                                       = c_reg(0xffffff6c, 1, 1, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
REGFILE_M0.REGFILE_M0                                                            = c_bits(REGFILE_M0, 0, 31, 0xffffffff, 32, "", "")
REGFILE_M1                                                                       = c_reg(0xffffff70, 1, 1, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
REGFILE_M1.REGFILE_M1                                                            = c_bits(REGFILE_M1, 0, 31, 0xffffffff, 32, "", "")
REGFILE_M2                                                                       = c_reg(0xffffff74, 1, 1, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
REGFILE_M2.REGFILE_M2                                                            = c_bits(REGFILE_M2, 0, 31, 0xffffffff, 32, "", "")
REGFILE_M3                                                                       = c_reg(0xffffff78, 1, 1, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
REGFILE_M3.REGFILE_M3                                                            = c_bits(REGFILE_M3, 0, 31, 0xffffffff, 32, "", "")
REGFILE_NUM_CORE_STALLS                                                          = c_reg(0xffffff94, 1, 0, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
REGFILE_NUM_CORE_STALLS.REGFILE_NUM_CORE_STALLS                                  = c_bits(REGFILE_NUM_CORE_STALLS, 0, 31, 0xffffffff, 32, "", "")
REGFILE_NUM_INSTRS                                                               = c_reg(0xffffff90, 1, 0, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
REGFILE_NUM_INSTRS.REGFILE_NUM_INSTRS                                            = c_bits(REGFILE_NUM_INSTRS, 0, 31, 0xffffffff, 32, "", "")
REGFILE_NUM_RUNCLKS                                                              = c_reg(0xffffff8c, 1, 0, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
REGFILE_NUM_RUNCLKS.REGFILE_NUM_RUNCLKS                                          = c_bits(REGFILE_NUM_RUNCLKS, 0, 31, 0xffffffff, 32, "", "")
REGFILE_R0                                                                       = c_reg(0xffffff14, 1, 1, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
REGFILE_R0.REGFILE_R0                                                            = c_bits(REGFILE_R0, 0, 31, 0xffffffff, 32, "", "")
REGFILE_R1                                                                       = c_reg(0xffffff18, 1, 1, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
REGFILE_R1.REGFILE_R1                                                            = c_bits(REGFILE_R1, 0, 31, 0xffffffff, 32, "", "")
REGFILE_R10                                                                      = c_reg(0xffffff3c, 1, 1, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
REGFILE_R10.REGFILE_R10                                                          = c_bits(REGFILE_R10, 0, 31, 0xffffffff, 32, "", "")
REGFILE_R2                                                                       = c_reg(0xffffff1c, 1, 1, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
REGFILE_R2.REGFILE_R2                                                            = c_bits(REGFILE_R2, 0, 31, 0xffffffff, 32, "", "")
REGFILE_R3                                                                       = c_reg(0xffffff20, 1, 1, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
REGFILE_R3.REGFILE_R3                                                            = c_bits(REGFILE_R3, 0, 31, 0xffffffff, 32, "", "")
REGFILE_R4                                                                       = c_reg(0xffffff24, 1, 1, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
REGFILE_R4.REGFILE_R4                                                            = c_bits(REGFILE_R4, 0, 31, 0xffffffff, 32, "", "")
REGFILE_R5                                                                       = c_reg(0xffffff28, 1, 1, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
REGFILE_R5.REGFILE_R5                                                            = c_bits(REGFILE_R5, 0, 31, 0xffffffff, 32, "", "")
REGFILE_R6                                                                       = c_reg(0xffffff2c, 1, 1, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
REGFILE_R6.REGFILE_R6                                                            = c_bits(REGFILE_R6, 0, 31, 0xffffffff, 32, "", "")
REGFILE_R7                                                                       = c_reg(0xffffff30, 1, 1, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
REGFILE_R7.REGFILE_R7                                                            = c_bits(REGFILE_R7, 0, 31, 0xffffffff, 32, "", "")
REGFILE_R8                                                                       = c_reg(0xffffff34, 1, 1, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
REGFILE_R8.REGFILE_R8                                                            = c_bits(REGFILE_R8, 0, 31, 0xffffffff, 32, "", "")
REGFILE_R9                                                                       = c_reg(0xffffff38, 1, 1, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
REGFILE_R9.REGFILE_R9                                                            = c_bits(REGFILE_R9, 0, 31, 0xffffffff, 32, "", "")
REGFILE_RFLAGS                                                                   = c_reg(0xffffff44, 1, 1, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
REGFILE_RFLAGS.N_FLAG                                                            = c_bits(REGFILE_RFLAGS, 0, 0, 0x1, 1, "", "")
REGFILE_RFLAGS.Z_FLAG                                                            = c_bits(REGFILE_RFLAGS, 1, 1, 0x2, 1, "", "")
REGFILE_RFLAGS.C_FLAG                                                            = c_bits(REGFILE_RFLAGS, 2, 2, 0x4, 1, "", "")
REGFILE_RFLAGS.V_FLAG                                                            = c_bits(REGFILE_RFLAGS, 3, 3, 0x8, 1, "", "")
REGFILE_RFLAGS.UD_FLAG                                                           = c_bits(REGFILE_RFLAGS, 4, 4, 0x10, 1, "", "")
REGFILE_RFLAGS.SV_FLAG                                                           = c_bits(REGFILE_RFLAGS, 5, 5, 0x20, 1, "", "")
REGFILE_RFLAGS.BR_FLAG                                                           = c_bits(REGFILE_RFLAGS, 6, 6, 0x40, 1, "", "")
REGFILE_RFLAGS.UM_FLAG                                                           = c_bits(REGFILE_RFLAGS, 7, 7, 0x80, 1, "", "")
REGFILE_RFLAGS.INT_N_FLAG                                                        = c_bits(REGFILE_RFLAGS, 8, 8, 0x100, 1, "", "")
REGFILE_RFLAGS.INT_Z_FLAG                                                        = c_bits(REGFILE_RFLAGS, 9, 9, 0x200, 1, "", "")
REGFILE_RFLAGS.INT_C_FLAG                                                        = c_bits(REGFILE_RFLAGS, 10, 10, 0x400, 1, "", "")
REGFILE_RFLAGS.INT_V_FLAG                                                        = c_bits(REGFILE_RFLAGS, 11, 11, 0x800, 1, "", "")
REGFILE_RFLAGS.INT_UD_FLAG                                                       = c_bits(REGFILE_RFLAGS, 12, 12, 0x1000, 1, "", "")
REGFILE_RFLAGS.INT_SV_FLAG                                                       = c_bits(REGFILE_RFLAGS, 13, 13, 0x2000, 1, "", "")
REGFILE_RFLAGS.INT_BR_FLAG                                                       = c_bits(REGFILE_RFLAGS, 14, 14, 0x4000, 1, "", "")
REGFILE_RFLAGS.INT_UM_FLAG                                                       = c_bits(REGFILE_RFLAGS, 15, 15, 0x8000, 1, "", "")
REGFILE_RFLAGS.UNUSED                                                            = c_bits(REGFILE_RFLAGS, 16, 31, 0xffff0000, 16, "", "")
REGFILE_RLINK                                                                    = c_reg(0xffffff40, 1, 1, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
REGFILE_RLINK.REGFILE_RLINK                                                      = c_bits(REGFILE_RLINK, 0, 31, 0xffffffff, 32, "", "")
REGFILE_RMAC0                                                                    = c_reg(0xffffff0c, 1, 1, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
REGFILE_RMAC0.REGFILE_RMAC0                                                      = c_bits(REGFILE_RMAC0, 0, 31, 0xffffffff, 32, "", "")
REGFILE_RMAC1                                                                    = c_reg(0xffffff08, 1, 1, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
REGFILE_RMAC1.REGFILE_RMAC1                                                      = c_bits(REGFILE_RMAC1, 0, 31, 0xffffffff, 32, "", "")
REGFILE_RMAC2                                                                    = c_reg(0xffffff04, 1, 1, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
REGFILE_RMAC2.REGFILE_RMAC2                                                      = c_bits(REGFILE_RMAC2, 0, 31, 0xffffffff, 32, "", "")
REGFILE_RMAC24                                                                   = c_reg(0xffffff10, 1, 1, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
REGFILE_RMAC24.REGFILE_RMAC24                                                    = c_bits(REGFILE_RMAC24, 0, 31, 0xffffffff, 32, "", "")
REGFILE_RMACB0                                                                   = c_reg(0xffffffa0, 1, 1, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
REGFILE_RMACB0.REGFILE_RMACB0                                                    = c_bits(REGFILE_RMACB0, 0, 31, 0xffffffff, 32, "", "")
REGFILE_RMACB1                                                                   = c_reg(0xffffff9c, 1, 1, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
REGFILE_RMACB1.REGFILE_RMACB1                                                    = c_bits(REGFILE_RMACB1, 0, 31, 0xffffffff, 32, "", "")
REGFILE_RMACB2                                                                   = c_reg(0xffffff98, 1, 1, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
REGFILE_RMACB2.REGFILE_RMACB2                                                    = c_bits(REGFILE_RMACB2, 0, 31, 0xffffffff, 32, "", "")
REGFILE_RMACB24                                                                  = c_reg(0xffffff48, 1, 1, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
REGFILE_RMACB24.REGFILE_RMACB24                                                  = c_bits(REGFILE_RMACB24, 0, 31, 0xffffffff, 32, "", "")
REGFILE_SP                                                                       = c_reg(0xffffffb8, 1, 1, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
REGFILE_SP.REGFILE_SP                                                            = c_bits(REGFILE_SP, 0, 31, 0xffffffff, 32, "", "")



DOLOOP_CACHE_CONFIG                                                              = c_reg(0xffffe130, 1, 1, 0, "", 2, 3, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
DOLOOP_CACHE_CONFIG.DOLOOP_CACHE_CONFIG_DOLOOP_EN                                = c_bits(DOLOOP_CACHE_CONFIG, 0, 0, 0x1, 1, "", "")
DOLOOP_CACHE_CONFIG.DOLOOP_CACHE_CONFIG_COUNTERS_EN                              = c_bits(DOLOOP_CACHE_CONFIG, 1, 1, 0x2, 1, "", "")
DOLOOP_CACHE_FILL_COUNT                                                          = c_reg(0xffffe138, 1, 0, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
DOLOOP_CACHE_FILL_COUNT.DOLOOP_CACHE_FILL_COUNT                                  = c_bits(DOLOOP_CACHE_FILL_COUNT, 0, 31, 0xffffffff, 32, "", "")
DOLOOP_CACHE_HIT_COUNT                                                           = c_reg(0xffffe134, 1, 0, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
DOLOOP_CACHE_HIT_COUNT.DOLOOP_CACHE_HIT_COUNT                                    = c_bits(DOLOOP_CACHE_HIT_COUNT, 0, 31, 0xffffffff, 32, "", "")


















INTER_PROC_KEYHOLE_ACCESS_CTRL                                                   = c_reg(0xffffa548, 1, 1, 0, "", 4, 15, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
INTER_PROC_KEYHOLE_ACCESS_CTRL.K32_MC_INTER_PROC_KEYHOLE__P0_ACCESS_PERMISSION   = c_bits(INTER_PROC_KEYHOLE_ACCESS_CTRL, 0, 0, 0x1, 1, "", "")
INTER_PROC_KEYHOLE_ACCESS_CTRL.K32_MC_INTER_PROC_KEYHOLE__P0_ACCESS_PERMISSION.K32_MC_INTER_PROC_KEYHOLE__P0_ACCESS_BLOCKED = c_value(INTER_PROC_KEYHOLE_ACCESS_CTRL, "", 0, "")
INTER_PROC_KEYHOLE_ACCESS_CTRL.K32_MC_INTER_PROC_KEYHOLE__P0_ACCESS_PERMISSION.K32_MC_INTER_PROC_KEYHOLE__P0_ACCESS_UNBLOCKED = c_value(INTER_PROC_KEYHOLE_ACCESS_CTRL, "", 1, "")
INTER_PROC_KEYHOLE_ACCESS_CTRL.K32_MC_INTER_PROC_KEYHOLE__P1_ACCESS_PERMISSION   = c_bits(INTER_PROC_KEYHOLE_ACCESS_CTRL, 1, 1, 0x2, 1, "", "")
INTER_PROC_KEYHOLE_ACCESS_CTRL.K32_MC_INTER_PROC_KEYHOLE__P1_ACCESS_PERMISSION.K32_MC_INTER_PROC_KEYHOLE__P1_ACCESS_BLOCKED = c_value(INTER_PROC_KEYHOLE_ACCESS_CTRL, "", 0, "")
INTER_PROC_KEYHOLE_ACCESS_CTRL.K32_MC_INTER_PROC_KEYHOLE__P1_ACCESS_PERMISSION.K32_MC_INTER_PROC_KEYHOLE__P1_ACCESS_UNBLOCKED = c_value(INTER_PROC_KEYHOLE_ACCESS_CTRL, "", 1, "")
INTER_PROC_KEYHOLE_ACCESS_CTRL.K32_MC_INTER_PROC_KEYHOLE__P2_ACCESS_PERMISSION   = c_bits(INTER_PROC_KEYHOLE_ACCESS_CTRL, 2, 2, 0x4, 1, "", "")
INTER_PROC_KEYHOLE_ACCESS_CTRL.K32_MC_INTER_PROC_KEYHOLE__P2_ACCESS_PERMISSION.K32_MC_INTER_PROC_KEYHOLE__P2_ACCESS_BLOCKED = c_value(INTER_PROC_KEYHOLE_ACCESS_CTRL, "", 0, "")
INTER_PROC_KEYHOLE_ACCESS_CTRL.K32_MC_INTER_PROC_KEYHOLE__P2_ACCESS_PERMISSION.K32_MC_INTER_PROC_KEYHOLE__P2_ACCESS_UNBLOCKED = c_value(INTER_PROC_KEYHOLE_ACCESS_CTRL, "", 1, "")
INTER_PROC_KEYHOLE_ACCESS_CTRL.K32_MC_INTER_PROC_KEYHOLE__P3_ACCESS_PERMISSION   = c_bits(INTER_PROC_KEYHOLE_ACCESS_CTRL, 3, 3, 0x8, 1, "", "")
INTER_PROC_KEYHOLE_ACCESS_CTRL.K32_MC_INTER_PROC_KEYHOLE__P3_ACCESS_PERMISSION.K32_MC_INTER_PROC_KEYHOLE__P3_ACCESS_BLOCKED = c_value(INTER_PROC_KEYHOLE_ACCESS_CTRL, "", 0, "")
INTER_PROC_KEYHOLE_ACCESS_CTRL.K32_MC_INTER_PROC_KEYHOLE__P3_ACCESS_PERMISSION.K32_MC_INTER_PROC_KEYHOLE__P3_ACCESS_UNBLOCKED = c_value(INTER_PROC_KEYHOLE_ACCESS_CTRL, "", 1, "")
INTER_PROC_KEYHOLE_ADDR                                                          = c_reg(0xffffa530, 1, 1, 0, "", 32, 0, 1, 0, "", "", "", "", "", 0, 1, "", "", 0)
INTER_PROC_KEYHOLE_ADDR.INTER_PROC_KEYHOLE_ADDR                                  = c_bits(INTER_PROC_KEYHOLE_ADDR, 0, 31, 0xffffffff, 32, "", "")
INTER_PROC_KEYHOLE_CTRL                                                          = c_reg(0xffffa53c, 1, 1, 0, "", 8, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
INTER_PROC_KEYHOLE_CTRL.INTER_PROC_KEYHOLE_CTRL_BYTE_SEL                         = c_bits(INTER_PROC_KEYHOLE_CTRL, 0, 3, 0xf, 4, "", "")
INTER_PROC_KEYHOLE_CTRL.INTER_PROC_KEYHOLE_CTRL_READ_WRITE_SEL                   = c_bits(INTER_PROC_KEYHOLE_CTRL, 4, 4, 0x10, 1, "", "")
INTER_PROC_KEYHOLE_CTRL.INTER_PROC_KEYHOLE_CTRL_READ_WRITE_SEL.INTER_PROC_KEYHOLE_CTRL_READ = c_value(INTER_PROC_KEYHOLE_CTRL, "", 0, "")
INTER_PROC_KEYHOLE_CTRL.INTER_PROC_KEYHOLE_CTRL_READ_WRITE_SEL.INTER_PROC_KEYHOLE_CTRL_WRITE = c_value(INTER_PROC_KEYHOLE_CTRL, "", 1, "")
INTER_PROC_KEYHOLE_CTRL.INTER_PROC_KEYHOLE_CTRL_CPU_SEL                          = c_bits(INTER_PROC_KEYHOLE_CTRL, 5, 6, 0x60, 2, "", "")
INTER_PROC_KEYHOLE_CTRL.INTER_PROC_KEYHOLE_CTRL_PM_DM_SEL                        = c_bits(INTER_PROC_KEYHOLE_CTRL, 7, 7, 0x80, 1, "", "")
INTER_PROC_KEYHOLE_CTRL.INTER_PROC_KEYHOLE_CTRL_PM_DM_SEL.INTER_PROC_KEYHOLE_CTRL_DM = c_value(INTER_PROC_KEYHOLE_CTRL, "", 0, "")
INTER_PROC_KEYHOLE_CTRL.INTER_PROC_KEYHOLE_CTRL_PM_DM_SEL.INTER_PROC_KEYHOLE_CTRL_PM = c_value(INTER_PROC_KEYHOLE_CTRL, "", 1, "")
INTER_PROC_KEYHOLE_MUTEX_LOCK                                                    = c_reg(0xffffa544, 1, 1, 0, "", 4, 15, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
INTER_PROC_KEYHOLE_MUTEX_LOCK.K32_MC_INTER_PROC_KEYHOLE__MUTEX_AVAILABLE         = c_enum(INTER_PROC_KEYHOLE_MUTEX_LOCK, 0, "", "")
INTER_PROC_KEYHOLE_MUTEX_LOCK.K32_MC_INTER_PROC_KEYHOLE__MUTEX_CLAIMED_BY_P0     = c_enum(INTER_PROC_KEYHOLE_MUTEX_LOCK, 1, "", "")
INTER_PROC_KEYHOLE_MUTEX_LOCK.K32_MC_INTER_PROC_KEYHOLE__MUTEX_CLAIMED_BY_P1     = c_enum(INTER_PROC_KEYHOLE_MUTEX_LOCK, 2, "", "")
INTER_PROC_KEYHOLE_MUTEX_LOCK.K32_MC_INTER_PROC_KEYHOLE__MUTEX_CLAIMED_BY_P2     = c_enum(INTER_PROC_KEYHOLE_MUTEX_LOCK, 4, "", "")
INTER_PROC_KEYHOLE_MUTEX_LOCK.K32_MC_INTER_PROC_KEYHOLE__MUTEX_CLAIMED_BY_P3     = c_enum(INTER_PROC_KEYHOLE_MUTEX_LOCK, 8, "", "")
INTER_PROC_KEYHOLE_MUTEX_LOCK.K32_MC_INTER_PROC_KEYHOLE__MUTEX_DISABLED          = c_enum(INTER_PROC_KEYHOLE_MUTEX_LOCK, 15, "", "")
INTER_PROC_KEYHOLE_READ_DATA                                                     = c_reg(0xffffa538, 1, 0, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
INTER_PROC_KEYHOLE_READ_DATA.INTER_PROC_KEYHOLE_READ_DATA                        = c_bits(INTER_PROC_KEYHOLE_READ_DATA, 0, 31, 0xffffffff, 32, "", "")
INTER_PROC_KEYHOLE_STATUS                                                        = c_reg(0xffffa540, 1, 0, 0, "", 1, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
INTER_PROC_KEYHOLE_STATUS.INTER_PROC_KEYHOLE_FREE                                = c_enum(INTER_PROC_KEYHOLE_STATUS, 0, "", "")
INTER_PROC_KEYHOLE_STATUS.INTER_PROC_KEYHOLE_BUSY                                = c_enum(INTER_PROC_KEYHOLE_STATUS, 1, "", "")
INTER_PROC_KEYHOLE_WRITE_DATA                                                    = c_reg(0xffffa534, 1, 1, 0, "", 32, 0, 1, 0, "", "", "", "", "", 0, 1, "", "", 0)
INTER_PROC_KEYHOLE_WRITE_DATA.INTER_PROC_KEYHOLE_WRITE_DATA                      = c_bits(INTER_PROC_KEYHOLE_WRITE_DATA, 0, 31, 0xffffffff, 32, "", "")












ALLOW_GOTO_SHALLOW_SLEEP                                                         = c_reg(0xffffe088, 1, 1, 0, "", 1, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
ALLOW_GOTO_SHALLOW_SLEEP.ALLOW_GOTO_SHALLOW_SLEEP                                = c_bits(ALLOW_GOTO_SHALLOW_SLEEP, 0, 0, 0x1, 1, "", "")
CLOCK_CONT_SHALLOW_SLEEP_RATE                                                    = c_reg(0xffffe098, 1, 1, 0, "", 8, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
CLOCK_CONT_SHALLOW_SLEEP_RATE.CLOCK_CONT_SHALLOW_SLEEP_RATE                      = c_bits(CLOCK_CONT_SHALLOW_SLEEP_RATE, 0, 7, 0xff, 8, "", "")
CLOCK_DIVIDE_RATE                                                                = c_reg(0xffffe070, 1, 1, 0, "", 2, 1, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
CLOCK_DIVIDE_RATE.CLOCK_STOPPED                                                  = c_enum(CLOCK_DIVIDE_RATE, 0, "", "")
CLOCK_DIVIDE_RATE.CLOCK_RATE_MAX                                                 = c_enum(CLOCK_DIVIDE_RATE, 1, "", "")
CLOCK_DIVIDE_RATE.CLOCK_RATE_HALF                                                = c_enum(CLOCK_DIVIDE_RATE, 2, "", "")
CLOCK_DIVIDE_RATE.CLOCK_RATE_RESERVED                                            = c_enum(CLOCK_DIVIDE_RATE, 3, "", "")
CLOCK_STOP_WIND_DOWN_SEQUENCE_EN                                                 = c_reg(0xffffe084, 1, 1, 0, "", 1, 1, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
CLOCK_STOP_WIND_DOWN_SEQUENCE_EN.CLOCK_STOP_WIND_DOWN_SEQUENCE_EN                = c_bits(CLOCK_STOP_WIND_DOWN_SEQUENCE_EN, 0, 0, 0x1, 1, "", "")
DISABLE_MUTEX_AND_ACCESS_IMMUNITY                                                = c_reg(0xffffe094, 1, 1, 0, "", 1, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
DISABLE_MUTEX_AND_ACCESS_IMMUNITY.DISABLE_MUTEX_AND_ACCESS_IMMUNITY              = c_bits(DISABLE_MUTEX_AND_ACCESS_IMMUNITY, 0, 0, 0x1, 1, "", "")
GOTO_SHALLOW_SLEEP                                                               = c_reg(0xffffe08c, 0, 1, 0, "", 1, 0, 1, 0, "", "", "", "", "", 0, 1, "", "", 0)
GOTO_SHALLOW_SLEEP.GOTO_SHALLOW_SLEEP                                            = c_bits(GOTO_SHALLOW_SLEEP, 0, 0, 0x1, 1, "", "")
PMWIN_ENABLE                                                                     = c_reg(0xffffe074, 1, 1, 0, "", 1, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
PMWIN_ENABLE.PMWIN_ENABLE                                                        = c_bits(PMWIN_ENABLE, 0, 0, 0x1, 1, "", "")
PROCESSOR_ID                                                                     = c_reg(0xffffe07c, 1, 0, 0, "", 1, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
PROCESSOR_ID.PROCESSOR_ID                                                        = c_bits(PROCESSOR_ID, 0, 0, 0x1, 1, "", "")
PROC_DEEP_SLEEP_EN                                                               = c_reg(0xffffe078, 1, 1, 0, "", 1, 0, 1, 0, "", "", "", "", "", 0, 1, "", "", 0)
PROC_DEEP_SLEEP_EN.PROC_DEEP_SLEEP_EN                                            = c_bits(PROC_DEEP_SLEEP_EN, 0, 0, 0x1, 1, "", "")
SHALLOW_SLEEP_STATUS                                                             = c_reg(0xffffe090, 1, 0, 0, "", 1, 0, 1, 0, "", "", "", "", "", 0, 1, "", "", 0)
SHALLOW_SLEEP_STATUS.SHALLOW_SLEEP_STATUS                                        = c_bits(SHALLOW_SLEEP_STATUS, 0, 0, 0x1, 1, "", "")



DM1_PROG_EXCEPTION_REGION_END_ADDR                                               = c_reg(0xffffe180, 1, 1, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
DM1_PROG_EXCEPTION_REGION_END_ADDR.DM1_PROG_EXCEPTION_REGION_END_ADDR            = c_bits(DM1_PROG_EXCEPTION_REGION_END_ADDR, 0, 31, 0xffffffff, 32, "", "")
DM1_PROG_EXCEPTION_REGION_START_ADDR                                             = c_reg(0xffffe17c, 1, 1, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
DM1_PROG_EXCEPTION_REGION_START_ADDR.DM1_PROG_EXCEPTION_REGION_START_ADDR        = c_bits(DM1_PROG_EXCEPTION_REGION_START_ADDR, 0, 31, 0xffffffff, 32, "", "")
DM2_PROG_EXCEPTION_REGION_END_ADDR                                               = c_reg(0xffffe188, 1, 1, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
DM2_PROG_EXCEPTION_REGION_END_ADDR.DM2_PROG_EXCEPTION_REGION_END_ADDR            = c_bits(DM2_PROG_EXCEPTION_REGION_END_ADDR, 0, 31, 0xffffffff, 32, "", "")
DM2_PROG_EXCEPTION_REGION_START_ADDR                                             = c_reg(0xffffe184, 1, 1, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
DM2_PROG_EXCEPTION_REGION_START_ADDR.DM2_PROG_EXCEPTION_REGION_START_ADDR        = c_bits(DM2_PROG_EXCEPTION_REGION_START_ADDR, 0, 31, 0xffffffff, 32, "", "")
EXCEPTION_EN                                                                     = c_reg(0xffffe150, 1, 1, 0, "", 2, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
EXCEPTION_EN.EXCEPTION_EN_BREAK                                                  = c_bits(EXCEPTION_EN, 0, 0, 0x1, 1, "", "")
EXCEPTION_EN.EXCEPTION_EN_IRQ                                                    = c_bits(EXCEPTION_EN, 1, 1, 0x2, 1, "", "")
EXCEPTION_PC                                                                     = c_reg(0xffffe18c, 1, 0, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
EXCEPTION_PC.EXCEPTION_PC                                                        = c_bits(EXCEPTION_PC, 0, 31, 0xffffffff, 32, "", "")
EXCEPTION_TYPE                                                                   = c_reg(0xffffe154, 1, 0, 0, "", 4, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
EXCEPTION_TYPE.EXCEPTION_TYPE_NONE                                               = c_enum(EXCEPTION_TYPE, 0, "", "")
EXCEPTION_TYPE.EXCEPTION_TYPE_DM1_UNALIGNED_32BIT                                = c_enum(EXCEPTION_TYPE, 1, "", "")
EXCEPTION_TYPE.EXCEPTION_TYPE_DM1_UNALIGNED_16BIT                                = c_enum(EXCEPTION_TYPE, 2, "", "")
EXCEPTION_TYPE.EXCEPTION_TYPE_DM1_UNMAPPED                                       = c_enum(EXCEPTION_TYPE, 3, "", "")
EXCEPTION_TYPE.EXCEPTION_TYPE_DM2_UNALIGNED_32BIT                                = c_enum(EXCEPTION_TYPE, 4, "", "")
EXCEPTION_TYPE.EXCEPTION_TYPE_DM2_UNALIGNED_16BIT                                = c_enum(EXCEPTION_TYPE, 5, "", "")
EXCEPTION_TYPE.EXCEPTION_TYPE_DM2_UNMAPPED                                       = c_enum(EXCEPTION_TYPE, 6, "", "")
EXCEPTION_TYPE.EXCEPTION_TYPE_PM_UNMAPPED                                        = c_enum(EXCEPTION_TYPE, 7, "", "")
EXCEPTION_TYPE.EXCEPTION_TYPE_PM_PROG_REGION                                     = c_enum(EXCEPTION_TYPE, 8, "", "")
EXCEPTION_TYPE.EXCEPTION_TYPE_DM1_PROG_REGION                                    = c_enum(EXCEPTION_TYPE, 9, "", "")
EXCEPTION_TYPE.EXCEPTION_TYPE_DM2_PROG_REGION                                    = c_enum(EXCEPTION_TYPE, 10, "", "")
EXCEPTION_TYPE.EXCEPTION_TYPE_STACK_OVERFLOW                                     = c_enum(EXCEPTION_TYPE, 11, "", "")
EXCEPTION_TYPE.EXCEPTION_TYPE_OTHER                                              = c_enum(EXCEPTION_TYPE, 12, "", "")
EXCEPTION_TYPE.EXCEPTION_TYPE_PM_OUT_OF_BOUNDS                                   = c_enum(EXCEPTION_TYPE, 13, "", "")
PM_PROG_EXCEPTION_REGION_END_ADDR                                                = c_reg(0xffffe178, 1, 1, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
PM_PROG_EXCEPTION_REGION_END_ADDR.PM_PROG_EXCEPTION_REGION_END_ADDR              = c_bits(PM_PROG_EXCEPTION_REGION_END_ADDR, 0, 31, 0xffffffff, 32, "", "")
PM_PROG_EXCEPTION_REGION_START_ADDR                                              = c_reg(0xffffe174, 1, 1, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
PM_PROG_EXCEPTION_REGION_START_ADDR.PM_PROG_EXCEPTION_REGION_START_ADDR          = c_bits(PM_PROG_EXCEPTION_REGION_START_ADDR, 0, 31, 0xffffffff, 32, "", "")
PROG_EXCEPTION_REGION_ENABLE                                                     = c_reg(0xffffe170, 1, 1, 0, "", 4, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
PROG_EXCEPTION_REGION_ENABLE.PM_PROG_EXCEPTION_REGION_ENABLE                     = c_bits(PROG_EXCEPTION_REGION_ENABLE, 0, 0, 0x1, 1, "", "")
PROG_EXCEPTION_REGION_ENABLE.DM1_PROG_EXCEPTION_REGION_ENABLE                    = c_bits(PROG_EXCEPTION_REGION_ENABLE, 1, 1, 0x2, 1, "", "")
PROG_EXCEPTION_REGION_ENABLE.DM2_PROG_EXCEPTION_REGION_ENABLE                    = c_bits(PROG_EXCEPTION_REGION_ENABLE, 2, 2, 0x4, 1, "", "")
PROG_EXCEPTION_REGION_ENABLE.PM_PROG_EXCEPTION_OOB_ENABLE                        = c_bits(PROG_EXCEPTION_REGION_ENABLE, 3, 3, 0x8, 1, "", "")






PREFETCH_CONFIG                                                                  = c_reg(0xffffe110, 1, 1, 0, "", 1, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
PREFETCH_CONFIG.PREFETCH_CONFIG_COUNTERS_EN                                      = c_bits(PREFETCH_CONFIG, 0, 0, 0x1, 1, "", "")
PREFETCH_DEBUG                                                                   = c_reg(0xffffe124, 1, 0, 0, "", 25, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
PREFETCH_DEBUG.PREFETCH_DEBUG_READ_EN_IN                                         = c_bits(PREFETCH_DEBUG, 0, 0, 0x1, 1, "", "")
PREFETCH_DEBUG.PREFETCH_DEBUG_WAIT_OUT                                           = c_bits(PREFETCH_DEBUG, 1, 1, 0x2, 1, "", "")
PREFETCH_DEBUG.PREFETCH_DEBUG_READ_EN_OUT                                        = c_bits(PREFETCH_DEBUG, 2, 2, 0x4, 1, "", "")
PREFETCH_DEBUG.PREFETCH_DEBUG_WAIT_IN                                            = c_bits(PREFETCH_DEBUG, 3, 3, 0x8, 1, "", "")
PREFETCH_DEBUG.PREFETCH_DEBUG_MEM_REQUEST                                        = c_bits(PREFETCH_DEBUG, 4, 4, 0x10, 1, "", "")
PREFETCH_DEBUG.PREFETCH_DEBUG_NEXT_MEMREQUEST                                    = c_bits(PREFETCH_DEBUG, 5, 5, 0x20, 1, "", "")
PREFETCH_DEBUG.PREFETCH_DEBUG_PMEM_REQUEST                                       = c_bits(PREFETCH_DEBUG, 6, 6, 0x40, 1, "", "")
PREFETCH_DEBUG.PREFETCH_DEBUG_PM_WAIT_IN_PREV                                    = c_bits(PREFETCH_DEBUG, 7, 7, 0x80, 1, "", "")
PREFETCH_DEBUG.PREFETCH_DEBUG_MEM_REQUEST_REG                                    = c_bits(PREFETCH_DEBUG, 8, 8, 0x100, 1, "", "")
PREFETCH_DEBUG.PREFETCH_DEBUG_ALOW_PREFETCHING                                   = c_bits(PREFETCH_DEBUG, 9, 10, 0x600, 2, "", "")
PREFETCH_DEBUG.PREFETCH_DEBUG_MISS_SEQ_T1                                        = c_bits(PREFETCH_DEBUG, 11, 11, 0x800, 1, "", "")
PREFETCH_DEBUG.PREFETCH_DEBUG_MISS_SEQ_T2                                        = c_bits(PREFETCH_DEBUG, 12, 12, 0x1000, 1, "", "")
PREFETCH_DEBUG.PREFETCH_DEBUG_VALID_PREFETCH_DATA                                = c_bits(PREFETCH_DEBUG, 13, 13, 0x2000, 1, "", "")
PREFETCH_DEBUG.PREFETCH_DEBUG_PREFETCH_VALID                                     = c_bits(PREFETCH_DEBUG, 14, 14, 0x4000, 1, "", "")
PREFETCH_DEBUG.PREFETCH_DEBUG_PREFETCH_HIT                                       = c_bits(PREFETCH_DEBUG, 15, 19, 0xf8000, 5, "", "")
PREFETCH_DEBUG.PREFETCH_DEBUG_PREFETCH_VALIDS                                    = c_bits(PREFETCH_DEBUG, 20, 24, 0x1f00000, 5, "", "")
PREFETCH_DEBUG_ADDR                                                              = c_reg(0xffffe128, 1, 0, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
PREFETCH_DEBUG_ADDR.PREFETCH_DEBUG_PMADDRIN                                      = c_bits(PREFETCH_DEBUG_ADDR, 0, 15, 0xffff, 16, "", "")
PREFETCH_DEBUG_ADDR.PREFETCH_DEBUG_PMADDROUT                                     = c_bits(PREFETCH_DEBUG_ADDR, 16, 31, 0xffff0000, 16, "", "")
PREFETCH_FLUSH                                                                   = c_reg(0xffffe114, 1, 1, 0, "", 1, 0, 1, 0, "", "", "", "", "", 0, 1, "", "", 0)
PREFETCH_FLUSH.PREFETCH_FLUSH                                                    = c_bits(PREFETCH_FLUSH, 0, 0, 0x1, 1, "", "")
PREFETCH_PREFETCH_COUNT                                                          = c_reg(0xffffe11c, 1, 0, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
PREFETCH_PREFETCH_COUNT.PREFETCH_PREFETCH_COUNT                                  = c_bits(PREFETCH_PREFETCH_COUNT, 0, 31, 0xffffffff, 32, "", "")
PREFETCH_REQUEST_COUNT                                                           = c_reg(0xffffe118, 1, 0, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
PREFETCH_REQUEST_COUNT.PREFETCH_REQUEST_COUNT                                    = c_bits(PREFETCH_REQUEST_COUNT, 0, 31, 0xffffffff, 32, "", "")
PREFETCH_WAIT_OUT_COUNT                                                          = c_reg(0xffffe120, 1, 0, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
PREFETCH_WAIT_OUT_COUNT.PREFETCH_WAIT_OUT_COUNT                                  = c_bits(PREFETCH_WAIT_OUT_COUNT, 0, 31, 0xffffffff, 32, "", "")






TIMER1_EN                                                                        = c_reg(0xffffe0e4, 1, 1, 0, "", 1, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
TIMER1_EN.TIMER1_EN                                                              = c_bits(TIMER1_EN, 0, 0, 0x1, 1, "", "")
TIMER1_TRIGGER                                                                   = c_reg(0xffffe0ec, 1, 1, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
TIMER1_TRIGGER.TIMER1_TRIGGER                                                    = c_bits(TIMER1_TRIGGER, 0, 31, 0xffffffff, 32, "", "")
TIMER2_EN                                                                        = c_reg(0xffffe0e8, 1, 1, 0, "", 1, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
TIMER2_EN.TIMER2_EN                                                              = c_bits(TIMER2_EN, 0, 0, 0x1, 1, "", "")
TIMER2_TRIGGER                                                                   = c_reg(0xffffe0f0, 1, 1, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
TIMER2_TRIGGER.TIMER2_TRIGGER                                                    = c_bits(TIMER2_TRIGGER, 0, 31, 0xffffffff, 32, "", "")
TIMER_TIME                                                                       = c_reg(0xffffe0e0, 1, 0, 0, "", 32, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
TIMER_TIME.TIMER_TIME                                                            = c_bits(TIMER_TIME, 0, 31, 0xffffffff, 32, "", "")



TRACE_0_CFG                                                                      = c_reg(0xffffa550, 1, 1, 0, "", 10, 256, 0, 0, "", "", "", "", "", 1, 1, "", "", 0)
TRACE_0_CFG.TRACE_CFG_0_ENABLE                                                   = c_bits(TRACE_0_CFG, 0, 0, 0x1, 1, "", "")
TRACE_0_CFG.TRACE_CFG_0_SYNC_INTERVAL                                            = c_bits(TRACE_0_CFG, 1, 3, 0xe, 3, "", "")
TRACE_0_CFG.TRACE_CFG_0_CPU_SELECT                                               = c_bits(TRACE_0_CFG, 4, 5, 0x30, 2, "", "")
TRACE_0_CFG.TRACE_CFG_0_FLUSH_FIFO                                               = c_bits(TRACE_0_CFG, 6, 6, 0x40, 1, "", "")
TRACE_0_CFG.TRACE_CFG_0_STALL_CORE_ON_TRACE_FULL                                 = c_bits(TRACE_0_CFG, 7, 7, 0x80, 1, "", "")
TRACE_0_CFG.TRACE_CFG_0_CLR_STORED_ON_SYNC                                       = c_bits(TRACE_0_CFG, 8, 8, 0x100, 1, "", "")
TRACE_0_CFG.TRACE_CFG_0_FLUSH_BITGEN                                             = c_bits(TRACE_0_CFG, 9, 9, 0x200, 1, "", "")
TRACE_0_DMEM_BASE_ADDR                                                           = c_reg(0xffffa56c, 1, 1, 0, "", 32, 0, 0, 0, "", "", "", "", "", 1, 1, "", "", 0)
TRACE_0_DMEM_BASE_ADDR.TRACE_0_DMEM_BASE_ADDR                                    = c_bits(TRACE_0_DMEM_BASE_ADDR, 0, 31, 0xffffffff, 32, "", "")
TRACE_0_DMEM_CFG                                                                 = c_reg(0xffffa568, 1, 1, 0, "", 13, 0, 1, 0, "", "", "", "", "", 1, 1, "", "", 0)
TRACE_0_DMEM_CFG.TRACE_0_DMEM_EN                                                 = c_bits(TRACE_0_DMEM_CFG, 0, 0, 0x1, 1, "", "")
TRACE_0_DMEM_CFG.TRACE_0_DMEM_CFG_WRAP                                           = c_bits(TRACE_0_DMEM_CFG, 1, 1, 0x2, 1, "", "")
TRACE_0_DMEM_CFG.TRACE_0_DMEM_CFG_LENGTH                                         = c_bits(TRACE_0_DMEM_CFG, 2, 12, 0x1ffc, 11, "", "")
TRACE_0_END_TRIGGER                                                              = c_reg(0xffffa55c, 1, 1, 0, "", 32, 0, 0, 0, "", "", "", "", "", 1, 1, "", "", 0)
TRACE_0_END_TRIGGER.TRACE_0_END_TRIGGER                                          = c_bits(TRACE_0_END_TRIGGER, 0, 31, 0xffffffff, 32, "", "")
TRACE_0_START_TRIGGER                                                            = c_reg(0xffffa558, 1, 1, 0, "", 32, 0, 0, 0, "", "", "", "", "", 1, 1, "", "", 0)
TRACE_0_START_TRIGGER.TRACE_0_START_TRIGGER                                      = c_bits(TRACE_0_START_TRIGGER, 0, 31, 0xffffffff, 32, "", "")
TRACE_0_TBUS_BASE_ADDR                                                           = c_reg(0xffffa564, 1, 1, 0, "", 32, 0, 0, 0, "", "", "", "", "", 1, 1, "", "", 0)
TRACE_0_TBUS_BASE_ADDR.TRACE_0_TBUS_BASE_ADDR                                    = c_bits(TRACE_0_TBUS_BASE_ADDR, 0, 31, 0xffffffff, 32, "", "")
TRACE_0_TBUS_CFG                                                                 = c_reg(0xffffa560, 1, 1, 0, "", 30, 0, 1, 0, "", "", "", "", "", 1, 1, "", "", 0)
TRACE_0_TBUS_CFG.TRACE_0_TBUS_EN                                                 = c_bits(TRACE_0_TBUS_CFG, 0, 0, 0x1, 1, "", "")
TRACE_0_TBUS_CFG.TRACE_0_TBUS_CFG_TRAN_TYPE                                      = c_bits(TRACE_0_TBUS_CFG, 1, 1, 0x2, 1, "", "")
TRACE_0_TBUS_CFG.TRACE_0_TBUS_CFG_WRAP                                           = c_bits(TRACE_0_TBUS_CFG, 2, 2, 0x4, 1, "", "")
TRACE_0_TBUS_CFG.TRACE_0_TBUS_CFG_DEST_SYS                                       = c_bits(TRACE_0_TBUS_CFG, 3, 6, 0x78, 4, "", "")
TRACE_0_TBUS_CFG.TRACE_0_TBUS_CFG_DEST_BLK                                       = c_bits(TRACE_0_TBUS_CFG, 7, 10, 0x780, 4, "", "")
TRACE_0_TBUS_CFG.TRACE_0_TBUS_CFG_SRC_BLK                                        = c_bits(TRACE_0_TBUS_CFG, 11, 14, 0x7800, 4, "", "")
TRACE_0_TBUS_CFG.TRACE_0_TBUS_CFG_TAG                                            = c_bits(TRACE_0_TBUS_CFG, 15, 18, 0x78000, 4, "", "")
TRACE_0_TBUS_CFG.TRACE_0_TBUS_CFG_LENGTH                                         = c_bits(TRACE_0_TBUS_CFG, 19, 29, 0x3ff80000, 11, "", "")
TRACE_0_TRIGGER_CFG                                                              = c_reg(0xffffa554, 1, 1, 0, "", 12, 0, 0, 0, "", "", "", "", "", 1, 1, "", "", 0)
TRACE_0_TRIGGER_CFG.TRACE_TRIGGER_CFG_END_TRIG_EN                                = c_bits(TRACE_0_TRIGGER_CFG, 0, 0, 0x1, 1, "", "")
TRACE_0_TRIGGER_CFG.TRACE_TRIGGER_CFG_START_TRIG_EN                              = c_bits(TRACE_0_TRIGGER_CFG, 1, 1, 0x2, 1, "", "")
TRACE_0_TRIGGER_CFG.TRACE_TRIGGER_CFG_TRIGGER_LENGTH                             = c_bits(TRACE_0_TRIGGER_CFG, 2, 11, 0xffc, 10, "", "")
TRACE_0_TRIGGER_STATUS                                                           = c_reg(0xffffa5a0, 1, 0, 0, "", 6, 0, 0, 0, "", "", "", "", "", 1, 1, "", "", 0)
TRACE_0_TRIGGER_STATUS.TRACE_TRIGGER_STATUS_START_FOUND                          = c_bits(TRACE_0_TRIGGER_STATUS, 0, 0, 0x1, 1, "", "")
TRACE_0_TRIGGER_STATUS.TRACE_TRIGGER_STATUS_START_COMPL                          = c_bits(TRACE_0_TRIGGER_STATUS, 1, 1, 0x2, 1, "", "")
TRACE_0_TRIGGER_STATUS.TRACE_TRIGGER_STATUS_END_FOUND                            = c_bits(TRACE_0_TRIGGER_STATUS, 2, 2, 0x4, 1, "", "")
TRACE_0_TRIGGER_STATUS.TRACE_TRIGGER_STATUS_END_COMPL                            = c_bits(TRACE_0_TRIGGER_STATUS, 3, 3, 0x8, 1, "", "")
TRACE_0_TRIGGER_STATUS.TRACE_TRIGGER_STATUS_SMDBG                                = c_bits(TRACE_0_TRIGGER_STATUS, 4, 5, 0x30, 2, "", "")
TRACE_1_CFG                                                                      = c_reg(0xffffa570, 1, 1, 0, "", 10, 256, 0, 0, "", "", "", "", "", 1, 1, "", "", 0)
TRACE_1_CFG.TRACE_CFG_1_ENABLE                                                   = c_bits(TRACE_1_CFG, 0, 0, 0x1, 1, "", "")
TRACE_1_CFG.TRACE_CFG_1_SYNC_INTERVAL                                            = c_bits(TRACE_1_CFG, 1, 3, 0xe, 3, "", "")
TRACE_1_CFG.TRACE_CFG_1_CPU_SELECT                                               = c_bits(TRACE_1_CFG, 4, 5, 0x30, 2, "", "")
TRACE_1_CFG.TRACE_CFG_1_FLUSH_FIFO                                               = c_bits(TRACE_1_CFG, 6, 6, 0x40, 1, "", "")
TRACE_1_CFG.TRACE_CFG_1_STALL_CORE_ON_TRACE_FULL                                 = c_bits(TRACE_1_CFG, 7, 7, 0x80, 1, "", "")
TRACE_1_CFG.TRACE_CFG_1_CLR_STORED_ON_SYNC                                       = c_bits(TRACE_1_CFG, 8, 8, 0x100, 1, "", "")
TRACE_1_CFG.TRACE_CFG_1_FLUSH_BITGEN                                             = c_bits(TRACE_1_CFG, 9, 9, 0x200, 1, "", "")
TRACE_1_DMEM_BASE_ADDR                                                           = c_reg(0xffffa58c, 1, 1, 0, "", 32, 0, 0, 0, "", "", "", "", "", 1, 1, "", "", 0)
TRACE_1_DMEM_BASE_ADDR.TRACE_1_DMEM_BASE_ADDR                                    = c_bits(TRACE_1_DMEM_BASE_ADDR, 0, 31, 0xffffffff, 32, "", "")
TRACE_1_DMEM_CFG                                                                 = c_reg(0xffffa588, 1, 1, 0, "", 13, 0, 1, 0, "", "", "", "", "", 1, 1, "", "", 0)
TRACE_1_DMEM_CFG.TRACE_1_DMEM_EN                                                 = c_bits(TRACE_1_DMEM_CFG, 0, 0, 0x1, 1, "", "")
TRACE_1_DMEM_CFG.TRACE_1_DMEM_CFG_WRAP                                           = c_bits(TRACE_1_DMEM_CFG, 1, 1, 0x2, 1, "", "")
TRACE_1_DMEM_CFG.TRACE_1_DMEM_CFG_LENGTH                                         = c_bits(TRACE_1_DMEM_CFG, 2, 12, 0x1ffc, 11, "", "")
TRACE_1_END_TRIGGER                                                              = c_reg(0xffffa57c, 1, 1, 0, "", 32, 0, 0, 0, "", "", "", "", "", 1, 1, "", "", 0)
TRACE_1_END_TRIGGER.TRACE_1_END_TRIGGER                                          = c_bits(TRACE_1_END_TRIGGER, 0, 31, 0xffffffff, 32, "", "")
TRACE_1_START_TRIGGER                                                            = c_reg(0xffffa578, 1, 1, 0, "", 32, 0, 0, 0, "", "", "", "", "", 1, 1, "", "", 0)
TRACE_1_START_TRIGGER.TRACE_1_START_TRIGGER                                      = c_bits(TRACE_1_START_TRIGGER, 0, 31, 0xffffffff, 32, "", "")
TRACE_1_TBUS_BASE_ADDR                                                           = c_reg(0xffffa584, 1, 1, 0, "", 32, 0, 0, 0, "", "", "", "", "", 1, 1, "", "", 0)
TRACE_1_TBUS_BASE_ADDR.TRACE_1_TBUS_BASE_ADDR                                    = c_bits(TRACE_1_TBUS_BASE_ADDR, 0, 31, 0xffffffff, 32, "", "")
TRACE_1_TBUS_CFG                                                                 = c_reg(0xffffa580, 1, 1, 0, "", 30, 0, 1, 0, "", "", "", "", "", 1, 1, "", "", 0)
TRACE_1_TBUS_CFG.TRACE_1_TBUS_EN                                                 = c_bits(TRACE_1_TBUS_CFG, 0, 0, 0x1, 1, "", "")
TRACE_1_TBUS_CFG.TRACE_1_TBUS_CFG_TRAN_TYPE                                      = c_bits(TRACE_1_TBUS_CFG, 1, 1, 0x2, 1, "", "")
TRACE_1_TBUS_CFG.TRACE_1_TBUS_CFG_WRAP                                           = c_bits(TRACE_1_TBUS_CFG, 2, 2, 0x4, 1, "", "")
TRACE_1_TBUS_CFG.TRACE_1_TBUS_CFG_DEST_SYS                                       = c_bits(TRACE_1_TBUS_CFG, 3, 6, 0x78, 4, "", "")
TRACE_1_TBUS_CFG.TRACE_1_TBUS_CFG_DEST_BLK                                       = c_bits(TRACE_1_TBUS_CFG, 7, 10, 0x780, 4, "", "")
TRACE_1_TBUS_CFG.TRACE_1_TBUS_CFG_SRC_BLK                                        = c_bits(TRACE_1_TBUS_CFG, 11, 14, 0x7800, 4, "", "")
TRACE_1_TBUS_CFG.TRACE_1_TBUS_CFG_TAG                                            = c_bits(TRACE_1_TBUS_CFG, 15, 18, 0x78000, 4, "", "")
TRACE_1_TBUS_CFG.TRACE_1_TBUS_CFG_LENGTH                                         = c_bits(TRACE_1_TBUS_CFG, 19, 29, 0x3ff80000, 11, "", "")
TRACE_1_TRIGGER_CFG                                                              = c_reg(0xffffa574, 1, 1, 0, "", 12, 0, 0, 0, "", "", "", "", "", 1, 1, "", "", 0)
TRACE_1_TRIGGER_CFG.TRACE_TRIGGER_CFG_END_TRIG_EN                                = c_bits(TRACE_1_TRIGGER_CFG, 0, 0, 0x1, 1, "", "")
TRACE_1_TRIGGER_CFG.TRACE_TRIGGER_CFG_START_TRIG_EN                              = c_bits(TRACE_1_TRIGGER_CFG, 1, 1, 0x2, 1, "", "")
TRACE_1_TRIGGER_CFG.TRACE_TRIGGER_CFG_TRIGGER_LENGTH                             = c_bits(TRACE_1_TRIGGER_CFG, 2, 11, 0xffc, 10, "", "")
TRACE_1_TRIGGER_STATUS                                                           = c_reg(0xffffa5a4, 1, 0, 0, "", 6, 0, 0, 0, "", "", "", "", "", 1, 1, "", "", 0)
TRACE_1_TRIGGER_STATUS.TRACE_TRIGGER_STATUS_START_FOUND                          = c_bits(TRACE_1_TRIGGER_STATUS, 0, 0, 0x1, 1, "", "")
TRACE_1_TRIGGER_STATUS.TRACE_TRIGGER_STATUS_START_COMPL                          = c_bits(TRACE_1_TRIGGER_STATUS, 1, 1, 0x2, 1, "", "")
TRACE_1_TRIGGER_STATUS.TRACE_TRIGGER_STATUS_END_FOUND                            = c_bits(TRACE_1_TRIGGER_STATUS, 2, 2, 0x4, 1, "", "")
TRACE_1_TRIGGER_STATUS.TRACE_TRIGGER_STATUS_END_COMPL                            = c_bits(TRACE_1_TRIGGER_STATUS, 3, 3, 0x8, 1, "", "")
TRACE_1_TRIGGER_STATUS.TRACE_TRIGGER_STATUS_SMDBG                                = c_bits(TRACE_1_TRIGGER_STATUS, 4, 5, 0x30, 2, "", "")
TRACE_ACCESS_CTRL                                                                = c_reg(0xffffa59c, 1, 1, 0, "", 4, 15, 0, 0, "", "", "", "", "", 1, 1, "", "", 0)
TRACE_ACCESS_CTRL.K32_TRACE__P0_ACCESS_PERMISSION                                = c_bits(TRACE_ACCESS_CTRL, 0, 0, 0x1, 1, "", "")
TRACE_ACCESS_CTRL.K32_TRACE__P0_ACCESS_PERMISSION.K32_TRACE__P0_ACCESS_BLOCKED   = c_value(TRACE_ACCESS_CTRL, "", 0, "")
TRACE_ACCESS_CTRL.K32_TRACE__P0_ACCESS_PERMISSION.K32_TRACE__P0_ACCESS_UNBLOCKED = c_value(TRACE_ACCESS_CTRL, "", 1, "")
TRACE_ACCESS_CTRL.K32_TRACE__P1_ACCESS_PERMISSION                                = c_bits(TRACE_ACCESS_CTRL, 1, 1, 0x2, 1, "", "")
TRACE_ACCESS_CTRL.K32_TRACE__P1_ACCESS_PERMISSION.K32_TRACE__P1_ACCESS_BLOCKED   = c_value(TRACE_ACCESS_CTRL, "", 0, "")
TRACE_ACCESS_CTRL.K32_TRACE__P1_ACCESS_PERMISSION.K32_TRACE__P1_ACCESS_UNBLOCKED = c_value(TRACE_ACCESS_CTRL, "", 1, "")
TRACE_ACCESS_CTRL.K32_TRACE__P2_ACCESS_PERMISSION                                = c_bits(TRACE_ACCESS_CTRL, 2, 2, 0x4, 1, "", "")
TRACE_ACCESS_CTRL.K32_TRACE__P2_ACCESS_PERMISSION.K32_TRACE__P2_ACCESS_BLOCKED   = c_value(TRACE_ACCESS_CTRL, "", 0, "")
TRACE_ACCESS_CTRL.K32_TRACE__P2_ACCESS_PERMISSION.K32_TRACE__P2_ACCESS_UNBLOCKED = c_value(TRACE_ACCESS_CTRL, "", 1, "")
TRACE_ACCESS_CTRL.K32_TRACE__P3_ACCESS_PERMISSION                                = c_bits(TRACE_ACCESS_CTRL, 3, 3, 0x8, 1, "", "")
TRACE_ACCESS_CTRL.K32_TRACE__P3_ACCESS_PERMISSION.K32_TRACE__P3_ACCESS_BLOCKED   = c_value(TRACE_ACCESS_CTRL, "", 0, "")
TRACE_ACCESS_CTRL.K32_TRACE__P3_ACCESS_PERMISSION.K32_TRACE__P3_ACCESS_UNBLOCKED = c_value(TRACE_ACCESS_CTRL, "", 1, "")
TRACE_DEBUG_SEL                                                                  = c_reg(0xffffa5a8, 1, 1, 0, "", 4, 0, 0, 0, "", "", "", "", "", 1, 1, "", "", 0)
TRACE_DEBUG_SEL.TRACE_DEBUG_SEL                                                  = c_bits(TRACE_DEBUG_SEL, 0, 3, 0xf, 4, "", "")
TRACE_DMEM_STATUS                                                                = c_reg(0xffffa594, 1, 0, 0, "", 4, 0, 0, 0, "", "", "", "", "", 1, 1, "", "", 0)
TRACE_DMEM_STATUS.TRACE_DMEM_STATUS_CNTL_0_DUMP_DONE                             = c_bits(TRACE_DMEM_STATUS, 0, 0, 0x1, 1, "", "")
TRACE_DMEM_STATUS.TRACE_DMEM_STATUS_CNTL_1_DUMP_DONE                             = c_bits(TRACE_DMEM_STATUS, 1, 1, 0x2, 1, "", "")
TRACE_DMEM_STATUS.TRACE_DMEM_STATUS_CNTL_2_DUMP_DONE                             = c_bits(TRACE_DMEM_STATUS, 2, 2, 0x4, 1, "", "")
TRACE_DMEM_STATUS.TRACE_DMEM_STATUS_CNTL_3_DUMP_DONE                             = c_bits(TRACE_DMEM_STATUS, 3, 3, 0x8, 1, "", "")
TRACE_MUTEX_LOCK                                                                 = c_reg(0xffffa598, 1, 1, 0, "", 4, 15, 0, 0, "", "", "", "", "", 1, 1, "", "", 0)
TRACE_MUTEX_LOCK.K32_TRACE__MUTEX_AVAILABLE                                      = c_enum(TRACE_MUTEX_LOCK, 0, "", "")
TRACE_MUTEX_LOCK.K32_TRACE__MUTEX_CLAIMED_BY_P0                                  = c_enum(TRACE_MUTEX_LOCK, 1, "", "")
TRACE_MUTEX_LOCK.K32_TRACE__MUTEX_CLAIMED_BY_P1                                  = c_enum(TRACE_MUTEX_LOCK, 2, "", "")
TRACE_MUTEX_LOCK.K32_TRACE__MUTEX_CLAIMED_BY_P2                                  = c_enum(TRACE_MUTEX_LOCK, 4, "", "")
TRACE_MUTEX_LOCK.K32_TRACE__MUTEX_CLAIMED_BY_P3                                  = c_enum(TRACE_MUTEX_LOCK, 8, "", "")
TRACE_MUTEX_LOCK.K32_TRACE__MUTEX_DISABLED                                       = c_enum(TRACE_MUTEX_LOCK, 15, "", "")
TRACE_TBUS_STATUS                                                                = c_reg(0xffffa590, 1, 0, 0, "", 4, 0, 0, 0, "", "", "", "", "", 1, 1, "", "", 0)
TRACE_TBUS_STATUS.TRACE_TBUS_STATUS_CNTL_0_DUMP_DONE                             = c_bits(TRACE_TBUS_STATUS, 0, 0, 0x1, 1, "", "")
TRACE_TBUS_STATUS.TRACE_TBUS_STATUS_CNTL_1_DUMP_DONE                             = c_bits(TRACE_TBUS_STATUS, 1, 1, 0x2, 1, "", "")
TRACE_TBUS_STATUS.TRACE_TBUS_STATUS_CNTL_2_DUMP_DONE                             = c_bits(TRACE_TBUS_STATUS, 2, 2, 0x4, 1, "", "")
TRACE_TBUS_STATUS.TRACE_TBUS_STATUS_CNTL_3_DUMP_DONE                             = c_bits(TRACE_TBUS_STATUS, 3, 3, 0x8, 1, "", "")




























































USB2_ENDPOINT_SELECT                                                             = c_reg(0xffffd240, 1, 1, 0, "", 6, 0, 0, 0, "", "", "", "", "", 0, 1, "", "", 0)
USB2_ENDPOINT_SELECT.USB2_ENDPOINT_SELECT_INDEX                                  = c_bits(USB2_ENDPOINT_SELECT, 0, 4, 0x1f, 5, "", "")
USB2_ENDPOINT_SELECT.USB2_ENDPOINT_SELECT_RX_NOT_TX                              = c_bits(USB2_ENDPOINT_SELECT, 5, 5, 0x20, 1, "", "")



AUDIO_PCM_REG_BANK_SELECT                                                        = c_reg(0xffff8de8, 1, 1, 0, "RW", 2, 0, 0, 0, "audio", "", "", "", "", 0, 1, "", "", 0)
AUDIO_SPDIF_REG_BANK_SELECT                                                      = c_reg(0xffff8dec, 1, 1, 0, "RW", 2, 0, 0, 0, "audio", "", "", "", "", 0, 1, "", "", 0)
