############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2020 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################

from csr.dev.fw.firmware_component import FirmwareComponent
from csr.dev.model import interface
from csr.dev.adaptor.text_adaptor import TextAdaptor
from csr.dev.env.env_helpers import var_address
from csr.wheels import gstrm
from csr.dwarf.read_dwarf import DwarfNoSymbol
from .caa_trap_log_decoder import CaaTrapLogDecoder
from .message_content_decoder import MessageContentDecoder

class VmMessageQueue(FirmwareComponent):
    ''' This class reports the vm message queue using context from the CAA trap
        log decoder. Subclasses may set a different _decoder to extend the
        decode context (e.g. for a particular application) '''

    # Number of bytes to report per line for binary message content
    BYTES_PER_LINE = 16
    # Value #defined in firmware to indicate a multicast message has been cancelled
    INVALIDATED_TASK = 1
    # The depth to display decoded structures in the report. Limited to 1 by
    # default so the table is a summary (e.g. not decoding unions or linked lists)
    DISPLAY_DEPTH = 1

    def __init__(self, env, core, parent=None):
        FirmwareComponent.__init__(self, env, core, parent=parent)
        self._decoder = CaaTrapLogDecoder(env, core)

    def _get_tasks(self, message):
        ''' Decode standard and multicast messages. Return the list of the tasks
            the message is sent.
        '''
        task_list = []
        if message.multicast.value == 0:
            task_list.append(message.t.task.value)
        else:
            # NULL terminated of list of tasks
            tlist = message.t.tlist
            index = 0
            task = tlist[0].value
            while task != 0:
                # Don't append 'invalidated' tasks
                if task != self.INVALIDATED_TASK:
                    task_list.append(task)
                index += 1
                task = tlist[index].value

        return task_list

    def _get_task_handler(self, task):
        ''' Get a task handler's function name '''
        try:
            return  self._decoder.get_task_handler(task)
        except TypeError:
            return None

    def _get_task_handlers(self, tasks):
        ''' Return a list of tasks handler function names given a list of tasks '''
        return list(map(self._get_task_handler, tasks))

    def _condition_address_to_string(self, condition_addr):
        ''' Attempts to find the name and value of the message's conditional
        variable. Returns a string variable=value if found, or the address of
        the condition variable if the address could not be decoded.
          '''
        var_tuple = self.env.vars.get_var_at_address(condition_addr)
        if var_tuple:
            # This returns a tuple of (address, name, dwarf obj). The variable name
            # returned can either be real conditional variable, or it can be a
            # structure whose member is the conditional variable.
            var_name = var_tuple[1]
            var = self.env.vars[var_name]
            if var_address(var) == condition_addr:
                return var_name
            # Search the variable's members for one at the condition address
            try:
                members = var.members
            except AttributeError:
                # No members
                pass
            else:
                for member_name, member_value in members.items():
                    if var_address(member_value) == condition_addr:
                        return "{}.{}={}".format(var_name, member_name, member_value.value_string)
        return condition_addr

    def _message_id_to_name(self, task, id):
        ''' Convert a numeric message id into a named message '''
        return self._decoder.get_id_name(task, id)

    def _message_id_name_to_struct(self, id_name, message_address):
        ''' Try to convert a named message to a typed object. Returns None if
            the conversion failed '''
        if id_name:
            msg_struct = MessageContentDecoder.message_to_struct(self.env, id_name, message_address)
            if msg_struct:
                return msg_struct.display_depth(self.DISPLAY_DEPTH, show=False)
            else:
                return None

    def _message_generator(self):
        ''' Iterates through the messages in the queue yielding a dict of message
            parameters. '''
        queue = self.env.vars["vm_message_queue"]
        for message in queue:
            with message.footprint_prefetched():
                tasks = self._get_tasks(message)
                handlers = self._get_task_handlers(tasks)
                id_name = self._message_id_to_name(tasks[0], message.id.value)
                msg_ptr = message.message.value
                msg_raw = None
                msg_struct = None
                if msg_ptr != 0:
                    msg_raw = self._core.fw.pmalloc.memory_block_mem(msg_ptr)
                    msg_struct = self._message_id_name_to_struct(id_name, msg_ptr)
                yield { 'message': message,
                        'tasks': tasks,
                        'handlers': handlers,
                        'id': message.id,
                        'id_name': id_name,
                        'condition_str': self._condition_address_to_string(message.condition_addr.value),
                        'due': message.due.value,
                        'msg_ptr': msg_ptr,
                        'msg_raw': msg_raw,
                        'msg_struct': msg_struct }

    def _add_report_row(self, tbl, entry):
        ''' Generate a report table row from a message dict returned by the
            _message_generator function '''
        task_handlers_str = ",\n".join(["0x{:X} ({})".format(t, h) for t, h in zip(entry['tasks'], entry['handlers'])])
        if len(entry['tasks']) > 1:
            task_handlers_str = '[' + task_handlers_str + ']'
        id_str = "{} ({})".format(entry['id'], entry['id_name'])

        element_message = None
        if entry['msg_ptr']:
            lines = []
            lines.append("0x{:X}".format(entry['msg_ptr']))
            if entry['msg_struct']:
                lines.append(entry['msg_struct'])
            elif entry['msg_raw']:
                length = len(entry['msg_raw'])
                for start in range(0, length, self.BYTES_PER_LINE):
                    end = min(start + self.BYTES_PER_LINE, length)
                    lines.append(" ".join(["%02x" % a for a in entry['msg_raw'][start:end]]))
            element_message = '\n'.join(lines)

        condition = entry['condition_str']
        due = str(entry['due'])

        tbl.add_row([id_str, task_handlers_str, element_message, condition, due])

    def _generate_report_body_elements(self):
        ''' Report the message queue in a table.
            The function attempts to decode:
            1) Message ids -> message names
            2) Task -> task handler
            3) Message -> Typed structure
            4) Condition -> Condition variable and value
            The CaaTrapLogDecoder and MessageContentDecoder classes are used
            to perform the decoding.
        '''
        return self.filter_report()

    def duplicate_report(self, min_duplicates_to_report=2):
        ''' Report any duplicate messages in the queue. min_duplicates_to_report
            may be used to control the minimum number of duplicate messages
            reported.
        '''

        grp = interface.Group("Duplicates")
        tbl = interface.Table(["Id", "Task (handler)", "Occurences"])
        histogram = dict()
        for message in self._message_generator():
            for task, handler in zip(message['tasks'], message['handlers']):
                key = "{},{},{},{}".format(message['id'], message['id_name'], task, handler)
                try:
                    histogram[key] += 1
                except KeyError:
                    histogram[key] = 1

        for key, occurrences in sorted(histogram.items(), key=lambda dvalue: dvalue[1], reverse=True):
            if occurrences >= min_duplicates_to_report:
                message_id, message_name, task, handler = key.split(',')
                id_str = "{} ({})".format(message_id, message_name)
                task_str = "{} ({})".format(task, handler)
                tbl.add_row([id_str, task_str, occurrences])

        grp.append(tbl)
        TextAdaptor(grp, gstrm.iout)

    def filter_report(self, message_id=None, message_name=None, task=None, handler=None):
        ''' Report a filtered message queue in a table.
            Any of the optional arguments may be specified.
            Messages matching all the specified arguments will be reported.
        '''
        grp = interface.Group("VM Message Queue")
        tbl = interface.Table(["Id", "Task (handler)", "Message", "Condition", "Due"])

        for entry in self._message_generator():
            if message_id and entry['id'].value != message_id:
                continue
            if message_name and entry['id_name'] != message_name:
                continue
            if task and task not in entry['tasks']:
                continue
            if handler and handler not in entry['handlers']:
                continue
            self._add_report_row(tbl, entry)

        grp.append(tbl)
        TextAdaptor(grp, gstrm.iout)
