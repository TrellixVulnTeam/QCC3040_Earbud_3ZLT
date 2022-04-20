############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2015 - 2016 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
"""
@file
Trap Message Queue Firmware Component file.

@section Description
Implements decoding of the vm message queue entries

@section Usage
Use apps1.fw.trap_message_queue.report()
"""
from csr.dev.fw.firmware_component import FirmwareComponent
from csr.dev.model import interface
from csr.dev.fw.debug_log import TrapLogDecoder
from csr.dev.env.env_helpers import InvalidDereference

class TrapMessageQueue(FirmwareComponent):
    '''
    Reports the contents of the VM message queue.
    '''
    @property
    def messages(self):
        '''
        Iterate over each message in the VM message queue
        '''
        try:
            ptr = self.vm_message_queue
            while True:
                yield self.Message(ptr.deref)
                ptr = ptr.deref.next
        except InvalidDereference:
            # We've reached the end of the message queue or
            # the message queue was NULL.
            pass

    def _generate_report_body_elements(self):
        output = interface.Group("message queue")
        decoder = TrapLogDecoder(self.env, self._core)
        queue_desc = []
        first_due_ms = None
        for message in self.messages:
            entry_desc = []
            element_task = message.task
            element_str = "task %s" % element_task
            try:
                element_str += " (%s)" % decoder.get_task_name(element_task)
            except TypeError:
                pass
            entry_desc.append(element_str)
            entry_desc.append("id: %s (%s)" % (message.id, 
                       decoder.get_id_name(element_task, message.id)))
            if message.app_message:
                element_str = "message: %s" % message.app_message
                msg_mem = self._core.fw.pmalloc.memory_block_mem(message.app_message)
                if msg_mem:
                    element_str += " = " + " ".join(["%02x" % a for a in msg_mem])
                entry_desc.append(element_str)
            if message.condition_addr:
                entry_desc.append("condition_addr: %s" % message.condition_addr)
                entry_desc.append("c_width: %s" % message.condition_width)
            element_str = "due: %s" % message.due_ms
            if first_due_ms is not None: 
                element_str += " (head + %g s)" % (
                                            (message.due_ms - first_due_ms)/1000.0) 
            entry_desc.append(element_str)
            queue_desc.append("\n".join(entry_desc))
            if first_due_ms is None:
                first_due_ms = message.due_ms
        output.append(interface.Code("\n\n".join(queue_desc)))
        return [output]

class TrapMessageQueueSingletask(TrapMessageQueue):
    '''
    Trap message queue class for OSs with a single global queue.
    i.e. Apps P1 Oxygen builds.
    '''
    def __init__(self, fw_env, core, parent=None):
        TrapMessageQueue.__init__(self, fw_env, core, parent)

        try:
            fw_env.gbl.vm_message_queue
        except AttributeError:
            raise self.NotDetected()

    @property
    def vm_message_queue(self):
        '''
        Returns a pointer to the first message in the VM message queue.
        '''
        return self.env.gbl.vm_message_queue

    class Message():
        '''
        Implements a common interface for accessing message data.
        '''
        def __init__(self, entry):
            try:
                self.task = entry.t.task.value
            except AttributeError:
                self.task = entry.task.value
            self.id = entry.id.value
            self.app_message = entry.message.value
            self.condition_addr = entry.condition_addr.value
            self.condition_width = entry.c_width.value
            self.due_ms = entry.due.value

class TrapMessageQueueMultitask(TrapMessageQueue):
    '''
    Trap message queue class for OSs with multiple queues.
    i.e. Apps P1 FreeRTOS builds.
    '''
    def __init__(self, fw_env, core, parent=None):
        TrapMessageQueue.__init__(self, fw_env, core, parent)

        try:
            fw_env.var.vm
        except AttributeError:
            raise self.NotDetected()

    @property
    def vm_message_queue(self):
        '''
        Returns a pointer to the first message in the VM message queue.
        '''
        return self.env.var.vm.queue.deref.queued

    class Message():
        '''
        Implements a common interface for accessing message data.
        '''
        def __init__(self, entry):
            self.task = entry.task.value
            self.id = entry.message.deref.id.value
            self.app_message = entry.message.deref.app_message.value
            self.condition_addr = entry.message.deref.condition_addr.value
            self.condition_width = entry.message.deref.condition_width.value
            self.due_ms = entry.message.deref.due_ms.value
