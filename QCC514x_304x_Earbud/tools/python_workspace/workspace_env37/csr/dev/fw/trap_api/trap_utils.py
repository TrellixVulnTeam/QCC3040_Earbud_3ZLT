############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2015 - 2016 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################

from csr.wheels.global_streams import iprint
from csr.dev.model.base_component import BaseComponent
from csr.dev.env.env_helpers import _Variable
from csr.dev.fw.call import CallFwMallocFailed
import time
from csr.wheels.polling_loop import add_poll_function, remove_poll_function
from csr.wheels.bitsandbobs import bytes_to_words, timeout_clock
from .bluestack_utils import BluestackUtils

class TrapApiMissingTaskHandler(RuntimeError):
    """
    Raised to indicate that a message was received without a Python-level task
    handler registered
    """    
    def __init__(self, task, id):
        report = str()
        super(TrapApiMissingTaskHandler, self).__init__("No handler registered "
                                "for task 0x%x (msg id was 0x%x)" % (task, id))

class UnexpectedMsgType(RuntimeError):
    """
    Raised to indicate that a handler saw a message of an unexpected type
    """

class TrapUtils(BaseComponent):
    """
    Support code for interfacing to the trap API via Python.  There should just
    be one instance of this class for a given P1 core; the AppsP1Firmware object
    supports this via the standard lazy construction idiom.
    
    Note on memory management: the methods in this class for accessing messages
    ensure that the list structure that is used to store individual messages is
    freed, but any memory allocated for the message body is passed to the caller
    It is recommended that this memory is only passed beyond the immediate 
    clients of this class when absolutely necessary: otherwise the risk of 
    memory leakage is very high since Python programmers never normally have to 
    think about memory deallocation.
    """

    # Initialisation
    #------------------------------------------------------
    def __init__(self, fw_env, apps1):
        
        self._fw_env = fw_env
        self._apps1 = apps1
        self._apps0 = apps1.subsystem.p0
        self._call1 = apps1.fw.call
        self._on_reset()
        
    def _generate_report_body_elements(self):
        pass
        
    def _on_reset(self):
        """
        Clear out internal caches to reflect the fact that the firmware has
        reset and therefore all pointers we had cached have become invalid
        """
        self._task_hdlrs = {}
        self._msg_task = {}
        self._sink_task = {}
        self._pending_msg = []
        self._source_task = {}
        
    def reset_test_support(self):
        """
        Reset the state of the trap api test support code.  Not to be confused
        with clearing state to handle a device reset under our feet
        """
        for msg in self._pending_msg:
            self._call1.pfree(msg["m"])
        for msg_type in list(self._msg_task.keys()):
            self.remove_msg_type_task(msg_type)
        for sink in list(self._sink_task.keys()):
            self.remove_sink_msg_task(sink)
        for source in list(self._source_task.keys()):
            self.remove_source_msg_task(source)
        for task in list(self._task_hdlrs.keys()):
            self.delete_task(task)
        assert(self._task_hdlrs == {})

        
        
    # Utility accessors
    #------------------------------------------------------
    @property
    def apps1(self):
        return self._apps1
    
    @property
    def apps0(self):
        return self._apps0
    
    @property
    def call1(self):
        return self._call1


    # Basic Variable management
    #------------------------------------------------------

    def build_var(self, type_dict, address):
        """
        Create a _Variable object from a given type dictionary based on a given
        address in P1's data address space
        """
        return _Variable.create_from_type(type_dict, address, self._apps1,
                                          self._apps1.info.layout_info)

    def create_prim(self, prim_dict):
        """
        Create a primitive object given its type dictionary
        """
        prim_size = prim_dict["byte_size"]
        prim_raw = self._call1.xzpmalloc_trace(prim_size, 0)
        if prim_raw == 0:
            raise CallFwMallocFailed("Failed to allocate prim")
        return self.build_var(prim_dict, prim_raw)

            
    def free_var_mem(self, var):
        """
        Counterpart to build_var: simply pfrees the memory which the
        supplied variable is based on
        """
        self._call1.pfree(var.address)
    

    # Basic Task and message handling support
    #------------------------------------------------------
    
            
    def create_task(self, handler=None):
        """
        Create a new Task in the firmware, registering the handler in the Python
        if supplied
        """
        tdata = self._call1.pnew("TaskData")
        task = tdata.address
        tdata.handler.value = self._fw_env.functions.get_call_address(
                                                  "trap_api_test_task_handler")
        if handler is not None:
            if not self._task_hdlrs:
                self._add_poll_fn()
            self._task_hdlrs[task] = handler
        return task

    def delete_task(self, task):
        """
        Destroy a task in the firmware and remove it from this object's list of
        handlers, which may cause removal of the polling function too
        """
        # Throw away any pending messages in the firmware
        self.call1.MessageFlushTask(task)
        # Throw away any pending messages in the test support layer
        self.flush_task_msgs(task)
        self.call1.pfree(task)
        if task in self._task_hdlrs:
            del self._task_hdlrs[task]
            if not self._task_hdlrs:
                self._remove_poll_fn()

    def send_msg(self, task, id):
        """
        Place a message on the given queue (message bodies are not supported)
        """
        if task is None:
            raise ValueError("Attempting to send msg to unspecified task!")
        self._call1.MessageSendLater(task, id, 0, 0)
    
    def reset_max_msg_size(self, size):
        """
        Resets the firmware's allocation size for copying message bodies.  The
        default is 32.
        """
        return self._call1.trap_api_test_reset_max_message_body_bytes(size) 
        
    def handle_msg_type(self, msg_type, task=None, handler=None):
        """
        Create a Task or extend a Task's scope to cover the given msg_type.
        The msg_type must match a trap API function as Message<msg_type>Task, 
        e.g. msg_type="BlueStack".  This function should only be called once for        
        a given msg type: if a msg type is already handled, this function will
        do nothing.
        """
        if msg_type in self._msg_task:
            # Already handled: nothing to do
            return False
        # If we haven't been passed a Task, create one 
        if task is None:
            task = self.create_task()

        # Register the task with the firmware
        trap_name = "Message%sTask" % msg_type
        self._call1(trap_name, task)

        # Register the task with the Python
        self._msg_task[msg_type] = task
        # Register the handler with the Python
        if handler is not None:
            if not self._task_hdlrs:
                self._add_poll_fn()
            self._task_hdlrs[task] = handler
        return task

    def remove_msg_type_task(self, msg_type):
        """
        Remove the task that handles this msg type in the firmware clearing out
        any pending messages and removing any task handler
        
        WARNING: there is a race here with the firmware: it is possible for the
        fw to receive a message for a registered task before it is de-registered
        and then deliver it between deregistration and task flushing.  It's
        impossible to avoid this when calling from Python, but the firmware is
        gracious and simply reports that it is dropping the message.
        """
        if msg_type not in self._msg_task:
            return
        task = self._msg_task[msg_type]
        # Remove the task handler before the flush (flush removes it)
        task_fw = self.call1("Message%sTask" % msg_type, 0)
        # Flush any pending messages now to try to minimise the race hazard
        self.call1.MessageFlushTask(task)
        # Call delete_task immediately to try to minimise the race hazard
        self.delete_task(task)
        # extract list of msg_type which had the deleted task as handler
        types_for_task = [msg_type for msg_type,reg_task
                             in self._msg_task.items() if reg_task == task]
        # delete extracted list
        for msg_type in types_for_task:
            del self._msg_task[msg_type]
        assert(task_fw == task)
    
    def handle_sink_msgs(self, id, handler=None):
        """
        Register the test support handle as handler for the given stream ID.
        """
        if id in self._sink_task:
            # Already handled: nothing to do
            return

        # Each sink requires a separate task, so create a new one
        task = self.create_task()
            
        # Register the task with the firmware
        self._call1.MessageStreamTaskFromSink(id, task)
        
        # Register the task with the Python
        self._sink_task[id] = task

        # Register the handler with the Python
        if handler is not None:
            if not self._task_hdlrs:
                self._add_poll_fn()
            self._task_hdlrs[task] = handler
        return task

    def remove_sink_msg_task(self, id):
        """
        Remove the handler for the given sink task.  There is a race hazard:
        see remove_msg_type_task.
        """
        if id not in self._sink_task:
            iprint("Sink ID 0x%x not found in %s" % (id, self._sink_task))
            return
        task = self._sink_task[id]
        # Remove the task handler before the flush (flush removes it)
        task_fw = self.call1.MessageStreamTaskFromSink(id, 0)
        # Flush any pending messages now to try to minimise the race hazard
        self.call1.MessageFlushTask(task)
        self.delete_task(task)
        del self._sink_task[id]
        assert(task_fw == task)
    
    def handle_source_msgs(self, id, handler=None):
        """
        Register the test support handle as handler for the given source stream ID.
        """
        if id in self._source_task:
            # Already handled: nothing to do
            return

        # Each sink requires a separate task, so create a new one
        task = self.create_task()
            
        # Register the task with the firmware
        self._call1.MessageStreamTaskFromSource(id, task)
        
        # Register the task with the Python
        self._source_task[id] = task

        # Register the handler with the Python
        if handler is not None:
            if not self._task_hdlrs:
                self._add_poll_fn()
            self._task_hdlrs[task] = handler
        return task

    def remove_source_msg_task(self, id):
        """
        Remove the handler for the given source task.  There is a race hazard:
        see remove_msg_type_task.
        """
        if id not in self._source_task:
            iprint("Source ID 0x%x not found in %s" % (id, self._source_task))
            return
        task = self._source_task[id]
        # Remove the task handler before the flush (flush removes it)
        task_fw = self.call1.MessageStreamTaskFromSource(id, 0)
        # Flush any pending messages now to try to minimise the race hazard
        self.call1.MessageFlushTask(task)
        self.delete_task(task)
        del self._source_task[id]
        assert(task_fw == task)

    def remove_sink_source_from_task_list(self, id_src, id_sink):
        """
        Remove the handler for the given sink task.  There is a race hazard:
        see remove_msg_type_task.
        """

        if id_src not in self._source_task:
            iprint("Source ID 0x%x not found in %s" % (id, self._source_task))
        else:
            task_src = self._source_task[id_src]
            self.delete_task(task_src)
            del self._source_task[id_src]

        if id_sink not in self._sink_task:
            iprint("Sink ID 0x%x not found in %s" % (id, self._sink_task))
        else:
            task_sink = self._sink_task[id_sink]
            self.delete_task(task_sink)
            del self._sink_task[id_sink]

    # Message loop implementation
    #------------------------------------------------------

    def try_get_core_msg(self):
        """
        Read a message from the test handler's queue, returning its elements
        by value and freeing the firmware memory.  (But note that the message
        body, if any, is handed over to the caller).
        """
        next_rsp = self._call1.trap_api_test_get_next()
        if next_rsp == 0:
            return None
        msg = self._fw_env.cast(next_rsp, "TEST_MESSAGE_LIST")
        res_dict = {"t" : msg.t.value,
                    "id" : msg.id.value,
                    "m" : msg.m.value}
        self._call1.pfree(next_rsp)
        return res_dict
            
    def get_core_msg(self, id=None, task=None, timeout=None):
        """
        Read messages from the internal cache of previously-read messages and the
        firmware's cache of received messages until one is received with the
        indicated ID or the timeout is reached (a value of None makes this a 
        one-shot check, and a value of 0 disables the timeout).
        Messages read from the firmware cache in the mean time are
        saved into the internal cache.
        """
        
        # Is there a message with this ID on the pending list?
        for i, pending in enumerate(self._pending_msg):
            if ((id is None or pending["id"] == id) and 
                                (task is None or pending["t"] == task)):
                # Found one.  Remove it from the list and return it
                self._pending_msg.pop(i)
                return pending["m"]
        
        start = timeout_clock()
        while True:
            next_rsp = self.try_get_core_msg()
            if next_rsp:
                if ((id is None or next_rsp["id"] == id) and 
                                    (task is None or next_rsp["t"] == task)):
                    return next_rsp["m"]
                else:
                    # Restart the timer in case the message is present but we're
                    # slow churning through prior messages on the list
                    start = timeout_clock()
                    self._pending_msg.append(next_rsp)

            if timeout is None:
                return False
            elif timeout > 0 and (timeout_clock() - start > timeout):
                iprint("Timed out waiting for message")
                return False
    
    def flush_task_msgs(self, task, timeout=0.1):
        """
        Keep reading and deleting messages for the given task until there
        aren't any left in the queue.  This function should only be called
        when it is known that no more messages will be arriving for this task,
        i.e. because it has been deleted in the firmware.  
        
        The timeout is how long we keep checking the list for after the last 
        time we saw a message for this task.  This is to avoid an over-long loop
        if some other task is being flooded with messages as we search the queue
        for more messages to flush.  The default seems like a reasonable amount
        of time to trawl the messages that were already delivered on entry
        """
        new_pending = []
        for i, pending in enumerate(self._pending_msg):
            if pending["t"] == task:
                # Found one.  Free it and throw it away.
                self._call1.free(pending["m"])
            else:
                # Otherwise keep it
                new_pending.append(pending)
        self._pending_msg = new_pending
        start = timeout_clock()
        while timeout_clock() - start < 0.1:
            next_rsp = self.try_get_core_msg()
            if next_rsp:
                if next_rsp["t"] == task:
                    self._call1.pfree(next_rsp["m"])
                    start = timeout_clock() # restart the timer
                else:
                    self._pending_msg.append(next_rsp)

    def fw_msg_list(self):
        """
        Convenience function for displaying the test support task list, which
        contains messages that the firmware has delivered to the trap API but
        which the Python support layer hasn't yet consumed
        """
        listptr = self._fw_env.cus["trap_api_test_support.c"].localvars["list"]
        if listptr.value != 0:
            return listptr.deref
        return None

    @property
    def _unique_name(self):
        """
        Use the built-in id function to construct a unique name for this object
        to ensure we don't have a name clash in the global poll loop
        """
        return "%s_%s" % (self.__class__.__name__, str(id(self)))

    def _remove_poll_fn(self):
        """
        Can be called by a handler function to stop the message loop (but note
        that the Pydbg event loop will only exit if *all* its functions are
        removed)
        """
        remove_poll_function(self._unique_name)

    def _add_poll_fn(self):
        """
        Add the core message dispatch function to the Pydbg event loop 
        """
        add_poll_function(self._unique_name, self.msg_dispatch)
        
    def msg_dispatch(self):
        """
        Single attempt to retrieve a message and dispatch the corresponding 
        handler
        """
        msg = self.try_get_core_msg()
        if msg is not None:
            try:
                handler = self._task_hdlrs[msg["t"]]
            except KeyError:
                # There's no handler for this task.  Whoops.
                self._call1.pfree(msg["m"])
                raise TrapApiMissingTaskHandler(msg["t"], msg["id"])
            
            handler(msg)
            self._call1.pfree(msg["m"])

    # Utility functions
    #------------------------------------------------------

    @property
    def bluestack(self):
        try:
            self._bluestack_utils
        except AttributeError:
            self._bluestack_utils = BluestackUtils(self)
        return self._bluestack_utils

    def copy_bytes_to_device(self, dst, byte_data):
        """Copy a Python byte list to device memory.
        """
        src = self.call1.pmalloc_trace(len(byte_data), 0)
        self.apps1.dm[src:src + len(byte_data)] = byte_data
        self.call1.memmove(dst, src, len(byte_data))
        self.call1.pfree(src);

    def pystr_to_charptr(self, s):
        """Copy a Python string into a char pointer in device memory.
           The returned pointer should be freed with pfree.
        """
        chars = self.call1.pnew("char", len(s) + 1)
        for i in range(len(s)):
            chars[i].value = ord(s[i])
        chars[len(s)].value = ord('\0')
        return chars
