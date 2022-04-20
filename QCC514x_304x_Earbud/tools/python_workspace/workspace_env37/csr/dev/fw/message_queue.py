############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2021 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
"""
@file
Message Router Firmware Component file.

@section Description
Implements decoding of the message router and the
individual message queue entries

@section Usage
Use apps1.fw.message_router.info()
Use apps1.fw.message_router.msg_queues_dict[i].info()
"""
from csr.wheels import gstrm
from csr.dev.fw.firmware_component import FirmwareComponent
from csr.dev.model import interface
from csr.dev.adaptor.text_adaptor import TextAdaptor
from csr.dev.env.env_helpers import InvalidDereference
from csr.dev.fw.debug_log import TrapLogDecoder

MaxDestinationEntries = 16


def display_task(task, decoder):
    """
    Display tasks according to it's value
    1. If task is NULL display 0.
    2. If task is less than or equal to the number of queues,
    task is a message queue
    3. Otherwise, task is a function pointer, display it's name
    """
    if task == 0:
        # NULL task, will be thrown away, display anyway
        task_desc = str(task) + " (NULL)"
    elif task <= MaxDestinationEntries:
        # Task is a message queue
        task_desc = str(task) + " (Message Queue)"
    else:
        # Task is a function pointer, possibly NULL
        # Display it's name instead
        task_desc = decoder.get_task_name(task)
    return task_desc


class MessageRouter(FirmwareComponent):
    """
    Report the content of all message queues in Free RTOS
    """

    def __init__(self, fw_env, core, parent=None):
        self.decoder = TrapLogDecoder(fw_env, core)
        self.fw_env = fw_env
        self.core = core

        try:
            # Free RTOS has a message router that contains
            # data about message queues.
            self.fw_env.var.message_router
        except AttributeError:
            raise FirmwareComponent.NotDetected
        else:
            # Collect data for VM queue and message queues from the message router
            self.vm_queue = self.fw_env.var.message_router.default_destination
            self.msg_queues = self.fw_env.var.message_router.destination_table
            self.msg_delivered_not_freed = (self.fw_env.var.
                                            messages_delivered_but_not_freed)
            self.msg_queues_dict = self.queues()
            self.vm_queue_info = self.MessageQueue(self.vm_queue, "n/a",
                                                   self.decoder)

    def info(self, report=False):
        output = interface.Group("Message Router")
        router_tbl = interface.Table(["No. of queues", self.queue_count])

        # Display message queue data
        queue_output = interface.Group("Message Queue")
        queue_data = interface.Table(["Message Queue Handle",
                                      "Message Queue",
                                      "No. of messages"])

        for key, value in self.msg_queues_dict.items():
            queue_data.add_row([key, value.queue, value.msg_count()])

        # Display VM queue data
        vm_output = interface.Group("VM Message Queue")
        vm_data = interface.Table(["Pointer to VM Queue",
                                   "No. of messages", "Messages"])
        vm_data.add_row([self.vm_queue,
                         self.vm_queue_info.msg_count(),
                         interface.Code("\n".join(
                             self.vm_queue_info.msg_data()))])

        output.append(router_tbl)
        queue_output.append(queue_data)
        vm_output.append(vm_data)

        # Display messages that have been delivered but not freed.
        if not self.msg_delivered_not_freed:
            if report:
                return [output, queue_output, vm_output]

            TextAdaptor(output, gstrm.iout)
            TextAdaptor(queue_output, gstrm.iout)
            TextAdaptor(vm_output, gstrm.iout)
        else:
            msg_not_freed_output = interface.Group(
                "Messages delivered but not freed")
            msg_not_freed_tbl = interface.Table(
                ["Pointer to message", "Message"])
            count = 1

            try:
                ptr = self.msg_delivered_not_freed
                while True:
                    entry_desc = []
                    entry_desc.append("Message {}:".format(count))
                    count += 1
                    entry_desc.append("  task: %s " %
                                      display_task(ptr.deref.task.value,
                                                   self.decoder))
                    entry_desc.append("  id: %s " %
                                      ptr.deref.message.deref.id.value)
                    if ptr.deref.message.deref.app_message:
                        element_str = ("  message pointer: %s" %
                                       ptr.deref.message.deref.app_message)
                        entry_desc.append(element_str)
                    if ptr.deref.message.deref.condition_addr:
                        entry_desc.append("  condition_addr: %s" %
                                          ptr.deref.message.deref.condition_addr)
                        entry_desc.append("  c_width: %s" %
                                          ptr.deref.message.deref.condition_width)
                    element_str = "  due: %s (ms)" % \
                                  int(ptr.deref.message.deref.due_ms.value)
                    entry_desc.append(element_str)
                    msg_not_freed_tbl.add_row([ptr,
                                               interface.Code("\n".join(
                                                   entry_desc))])
                    ptr = ptr.deref.next
            except InvalidDereference:
                pass
            msg_not_freed_output.append(msg_not_freed_tbl)
            if report:
                return [output, queue_output, vm_output, msg_not_freed_output]
            TextAdaptor(output, gstrm.iout)
            TextAdaptor(queue_output, gstrm.iout)
            TextAdaptor(vm_output, gstrm.iout)
            TextAdaptor(msg_not_freed_output, gstrm.iout)

    def queues(self):
        """
        Dictionary containing all the message queues in the router
        plus the VM queue
        """
        # Iterate through the message queues
        queue_dict = {}
        handle = 0
        for queue_ptr in self.msg_queues:
            try:
                queue_ptr.deref
            except InvalidDereference:
                handle += 1
                continue
            else:
                # The message queue handle starts from 1.
                # See app_ss/main/fw/src/customer/core/trap_api/message_router.c
                handle += 1
                queue_dict[handle] = self.MessageQueue(queue_ptr, handle,
                                                       self.decoder)

        return queue_dict

    @property
    def queue_count(self):
        """
        Number of queues in the message router +  VM queue
        """
        return len(self.msg_queues_dict) + 1

    class MessageQueue:
        def __init__(self, ptr, handle, decoder):
            self.queue = ptr
            self.handle = handle
            self.decoder = decoder

        @property
        def messages(self):
            """
            Returns a list of Message objects containing
            data about each message in the queue
            """
            messages = []
            try:
                ptr = self.queue.deref.queued
                while ptr.deref.next != 0:
                    messages.append(self.Message(ptr.deref))
                    ptr = ptr.deref.next
            except InvalidDereference:
                # End of message queue or message queue is empty
                pass
            return messages

        def msg_count(self):
            return len(self.messages)

        def info(self, report=False):
            """
            Display info about message queue and it's messages in a table
            """
            msg_desc = self.msg_data()
            queue_output = interface.Group("Message Queue")
            q_tbl = interface.Table(["Pointer to queue", "Handle",
                                     "Message count", "Message Details"])
            q_tbl.add_row([self.queue, self.handle, self.msg_count(),
                           interface.Code("\n".join(msg_desc))])
            queue_output.append(q_tbl)
            if report:
                return [queue_output]
            TextAdaptor(queue_output, gstrm.iout)

        def msg_data(self):
            if self.msg_count() != 0:
                first_due_ms = None
                queue_desc = []
                count = 1
                for msg in self.messages:
                    entry_desc = []
                    entry_desc.append("Message {}:".format(count))
                    count += 1
                    entry_desc.append("  task: %s " %
                                      display_task(msg.task.value, self.decoder))
                    entry_desc.append("  id: %s " % msg.message_id.value)
                    if msg.app_message:
                        element_str = "  message pointer: %s" % msg.app_message
                        entry_desc.append(element_str)
                    if msg.condition_addr:
                        entry_desc.append("  condition_addr: %s" % msg.condition_addr)
                        entry_desc.append("  c_width: %s" % msg.condition_width)
                    element_str = "  due: %s (ms)" % int(msg.due_ms.value)
                    if first_due_ms is not None:
                        element_str += " (  head + %g s)" % (
                                (msg.due_ms - first_due_ms) / 1000.0)
                    entry_desc.append(element_str)
                    queue_desc.append("\n".join(entry_desc))
                return queue_desc
            else:
                return []

        class Message:
            """
            Implements an interface for accessing message data
            """

            def __init__(self, entry):
                self.task = entry.task
                self.due_ms = entry.message.deref.due_ms
                self.app_message = entry.message.deref.app_message
                self.condition_addr = entry.message.deref.condition_addr
                self.message_id = entry.message.deref.id
                self.condition_width = entry.message.deref.condition_width
                self.ref_count = entry.message.deref.refcount
