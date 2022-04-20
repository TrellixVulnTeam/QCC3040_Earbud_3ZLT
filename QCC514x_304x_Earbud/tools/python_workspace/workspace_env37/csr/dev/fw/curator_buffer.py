############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2015 - 2016 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
from csr.dev.hw.mmu import MMUPagedBuffer

class CurBuffer():
    '''
    Class that interfaces with and has the same methods as the firmware 
    BUFFER structure in buffer/buffer.h.
    '''
    def __init__(self, core, buffer):
        self.buffer = buffer
        self.size_mask = self.buffer["size_mask"].value
        self.size = self.size_mask + 1
        self.mmu_handle = self.buffer["handle"].value
       
        self.mmu_rec = core.subsystem.mmu.handleTable.records[self.mmu_handle]
        self.mmu_data = MMUPagedBuffer(core.subsystem.mmu, self.mmu_rec.pageTable)
        self._page_size = core.subsystem.mmu.page_size

    def raw_write(self, data):
        index = self.buffer["index"].value
        tail = self.buffer["tail"].value
        bytes = len(data)
        if index < tail and index + bytes > tail:
            raise self.BufferFull
        if index > tail and index + bytes >= self.size and \
                                            index + bytes - self.size > tail:
            raise self.BufferFull
        self.mmu_data[index:index+bytes] = data
        
    def raw_write_update(self, length, update_mmu_handle=False):
        index = self.buffer["index"].value
        new_index = (index + length) & self.size_mask
        self.buffer["index"].value = new_index
        if update_mmu_handle:
            self.mmu_rec.bufferOffset = new_index

    def raw_read(self, length):
        outdex = self.buffer["outdex"].value
        return self.mmu_data[outdex:outdex+length]
        
    def raw_read_update(self, length):
        outdex = self.buffer["outdex"].value
        self.buffer["outdex"].value = (outdex + length) & self.size_mask
        
    def update_tail(self, length):
        tail = self.buffer["tail"].value
        self.buffer["tail"].value = (tail + length) & self.size_mask
        
    class BufferFull(RuntimeWarning):
        pass
        
        
class CurMsgBuffer(CurBuffer):
    '''
    Class that interfaces with and has the same methods as the firmware 
    BUFFER_MSG in buffer/buffer_msg.h. This is the base class for
    variants that is specialised either for reading or writing by
    CurBufferReader or CurBufferWriter.
    '''
    def __init__(self, core, name):
        self.buf_msg = core.fw.env.globalvars[name]
        self.memory_address = self.buf_msg.address
        self.members= self.buf_msg.members
        self.ring_entry_mask = self.buf_msg["msg_lengths"].num_elements - 1
        CurBuffer.__init__(self, core, self.buf_msg["buf"])


class CurBufferReader(CurMsgBuffer):
    '''
    Class that interfaces with and has the same methods as the firmware 
    BUFFER_MSG in buffer/buffer_msg.h. This is the class used for reading
    from a BUFFER_MSG.
    '''
    def any_msgs_to_send(self):
        return self.buf_msg["front"].value != self.buf_msg["back"].value

    def get_back_msg(self):
        if not self.any_msgs_to_send():
            return None
        back = self.buf_msg["back"].value
        length = self.buf_msg["msg_lengths"][back].value
        data = self.raw_read(length)
        self.update_back(back, length)
        self.update_behind(back, length)
        return data
    
    def update_back(self, back, length):
        self.raw_read_update(length)       
        self.buf_msg["back"].value = (back + 1) & self.ring_entry_mask

    def update_behind(self, behind, length):
        self.update_tail(length)
        self.buf_msg["behind"].value = (behind + 1) & self.ring_entry_mask

class CurBufferWriter(CurMsgBuffer):
    '''
    Class that interfaces with and has the same methods as the firmware 
    BUFFER_MSG in buffer/buffer_msg.h. This is the class used for writing
    to a BUFFER_MSG.
    '''
    def any_msgs_available(self):
        return self.buf_msg["behind"].value != (
                        self.buf_msg["front"].value + 1) & self.ring_entry_mask

    def add_front_msg(self, msg):
        if not self.any_msgs_available():
            raise self.BufferFull
        front = self.buf_msg["front"].value
        length = len(msg)
        self.raw_write(msg)
        self.add_to_front(front, length)
        
    def add_to_front(self, front, length):
        self.raw_write_update(length)
        self.buf_msg["msg_lengths"][front].value = length
        self.buf_msg["front"].value = (front + 1) & self.ring_entry_mask
    
