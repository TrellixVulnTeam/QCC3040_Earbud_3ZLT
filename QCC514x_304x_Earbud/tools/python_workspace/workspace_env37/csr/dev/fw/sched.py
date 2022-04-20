############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2015 - 2016 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
""" This module provides API for the Oxygen and FreeRTOS Scheduler
"""
# pylint: disable=no-self-use, too-many-branches

from csr.wheels import gstrm
from csr.wheels.global_streams import iprint
from csr.dev.fw.firmware_component import FirmwareComponent
import itertools
import sys
import time
from csr.wheels.bitsandbobs import timeout_clock
from csr.dev.model import interface
from csr.dev.adaptor.text_adaptor import TextAdaptor
from csr.dev.hw.address_space import AddressSpace
from csr.dwarf.read_dwarf import DwarfNoSymbol
from csr.dev.fw.thread import FreeRTOSTask


class SchedOxygen(FirmwareComponent):
    """ Provide access to Oxygen Scheduler
    """
    identifier = "Oxygen"

    class SchedTimeout(RuntimeError):
        pass

    def __init__(self, fw_env, core, parent=None):
        FirmwareComponent.__init__(self, fw_env, core, parent)
        try:
            self.bg = SchedOxygen.SchedBG(self.env, self._core)
            self.tasks = SchedOxygen.SchedTasks(self.env, self._core)
        except DwarfNoSymbol:
            raise self.NotDetected()

        self.current_stack_start = self._core.fields.STACK_START_ADDR.read()

    def _generate_report_body_elements(self):
        report = interface.Group("Sched")
        report.append(self.bg.info(report=True))
        report.append(self.tasks.info(report=True))
        report.append(self.info(report=True))
        return [report]
    
    def _on_reset(self):
        pass

    def wait_for_runlevel(self, level, timeout=2, quiet=False):
        """ Wait for the given runlevel of the kalimba scheduler. 
        """
        run_lvl = 0 # scheduler run level RUNLEVEL_BOOT (0)
        ts = 0.01
        tc = 0
        started_at =  timeout_clock()
        while (run_lvl != level) and (tc < timeout) :
            sched_oxygen = self.env.cus["sched_oxygen.c"]
            try:
                run_lvl = sched_oxygen.localvars["sched_flags"]["current_runlevel"].value
            except AddressSpace.ReadFailure:
                pass
            if run_lvl != level:
                time.sleep(ts)
                tc = timeout_clock() - started_at
        ended_at = timeout_clock()
        diff = ended_at - started_at
        if (run_lvl != level):
            if not quiet:
                iprint("Kalimba scheduler run level %d" % run_lvl)
            raise self.SchedTimeout("Kalimba Scheduler timed out after %1.2fs" % diff)
        elif not quiet:
            iprint("Kalimba scheduler run level %d after %1.2fs" % (run_lvl, diff))

    def info(self, report=False):
        """
        Print or return a report of the general scheduler state
        """
        output = interface.Group("sched.general")
        output_table = interface.Table(["name", "value"])
        output_table.add_row(["background_work_pending",
                              self.env.gbl.background_work_pending.value])
        output.append(output_table)
        if report is True:
            return output
        TextAdaptor(output, gstrm.iout)

    class SchedBG(object):
        """
        Provide access to background interrupts.
        """

        def __init__(self, fw_env, core):
            """
            Saves a reference to the core object.
            """
            self.core = core
            self._fw_env = fw_env

        def state(self, pause_cpu=False):
            """
            Returns a dictionary containing information about the background
            interrupts.

            pause_cpu is False by default so the chip's behaviour is not
            modified unless explicitly asked to.
            """
            ret = dict()
            core = self.core
            bg_int_ids = self._fw_env.enums["bg_int_ids"]
            bg_ints_in_priority = self._fw_env.globalvars["bg_ints_in_priority"]
            fns = self._fw_env.functions

            if pause_cpu and core.is_running:
                core.pause()
                unpause = True
            else:
                unpause = False
            
            for i in range(bg_ints_in_priority.num_elements):
                _bg_int = bg_ints_in_priority[i]["first"]
                while _bg_int.value:
                    bg_int = _bg_int.deref
                    id = bg_int["id"].value
                    ret[id] = dict()
                    
                    ret[id]["priority"] = i
                    ret[id]["raised"] = bg_int["raised"].value
                    try:
                        ret[id]["name"] = bg_int_ids[id]
                    except KeyError:
                        ret[id]["name"] = None
                    if bg_int["prunable"].value:
                        ret[id]["prunable"] = True
                    else:
                        ret[id]["prunable"] = False
                    ret[id]["handler"] = fns[bg_int["handler"].value]
                    
                    _bg_int = bg_int["next"]
            
            if unpause:
                core.run()
            return ret
    
        def info(self, report=False):
            """
            Print or return a report of the background interrupts state. This
            is just a text formatting wrapper around the data from 
            self.state().
            """
            state = self.state()
            output = interface.Group("sched.bg")
            output_table = interface.Table(["name", "id", "handler", "raised", 
                                            "priority", "prunable"])
            for id, bg_int in state.items():
                output_table.add_row([bg_int["name"], "0x%08x" % id, 
                                      bg_int["handler"], bg_int["raised"],
                                      bg_int["priority"], bg_int["prunable"]])
            output.append(output_table)
            if report is True:
                return output
            TextAdaptor(output, gstrm.iout)

    class SchedTasks(object):
        """
        Provide access to tasks and queues.
        """

        def __init__(self, fw_env, core):
            """
            Saves a reference to the core object.
            """
            self.core = core
            self._fw_env = fw_env
            self.priority_str = dict([(v,k.split("_")[0]) for k,v in 
                                  self._fw_env.enums["PRIORITY"].items()])

        def state(self, pause_cpu=False):
            """
            Returns a dictionary containing information about the tasks.

            pause_cpu is False by default so the chip's behaviour is not
            modified unless explicitly asked to.
            """
            ret = dict()
            core = self.core
            tasks_in_priority = self._fw_env.globalvars["tasks_in_priority"]
            fns = self._fw_env.functions

            if pause_cpu and core.is_running:
                core.pause()
                unpause = True
            else:
                unpause = False

            for i in range(tasks_in_priority.num_elements):
                _task = tasks_in_priority[i]["first"]
                while _task.value:
                    task = _task.deref
                    id = task["id"].value
                    ret[id] = dict()
                    ret[id]["run_level"] = task.runlevel.value
                    ret[id]["priority"] = i
                    ret[id]["nqueues"] = task.nqueues
                    ret[id]["mqueues_pending"] = task.mqueues.deref.first.value
                    if task["prunable"].value:
                        ret[id]["prunable"] = True
                    else:
                        ret[id]["prunable"] = False
                    ret[id]["handler"] = fns[task["handler"].value]

                    _task = task["next"]

            if unpause:
                core.run()
            return ret

        def info(self, report=False):
            """
            Print or return a report of the sched tasks state. This
            is just a text formatting wrapper around the data from
            self.state().
            """
            state = self.state()
            output = interface.Group("sched.tasks")
            output_table = interface.Table(["id", "handler", "mqueues_pending",
                                            "run_level", "priority", "prunable"])
            for id, task in state.items():
                if task["mqueues_pending"]:
                    pending_flag = True
                else:
                    pending_flag = False
                output_table.add_row(["0x%08x" % id,
                                      task["handler"], pending_flag,
                                      task["run_level"],
                                      self.priority_str.get(task["priority"],
                                                            task["priority"]),
                                      task["prunable"]])
            output.append(output_table)
            if report is True:
                return output
            TextAdaptor(output, gstrm.iout)


class SchedFreeRTOS(FirmwareComponent):
    """ Provide access to the FreeRTOS Scheduler.
    """
    identifier = "FreeRTOS"

    class SchedTimeout(RuntimeError):
        pass

    def __init__(self, fw_env, core, parent=None):
        FirmwareComponent.__init__(self, fw_env, core, parent)

        # Determine whether the firmware is running FreeRTOS.
        # The current TCB pointer seems suitable to test. It's part of the
        # FreeRTOS Kernel, can not be changed as it's used by existing ports,
        # and uses a different naming convention to us.
        try:
            fw_env.globalvars["pxCurrentTCB"]
        except KeyError:
            raise self.NotDetected()

        self.tasks = self.SchedTasks(self.env, self._core)

    def _generate_report_body_elements(self):
        report = interface.Group("Sched")
        report.append(self.tasks.info(report=True))
        report.append(self.info(report=True))
        return [report]

    def _on_reset(self):
        pass

    def wait_for_runlevel(self, level, timeout=2, quiet=False):
        """ Wait for the given runlevel of the kalimba scheduler.
        """

        # FreeRTOS doesn't have the concept of runlevels, run level 2
        # (RUNLEVEL_FINAL) is used to determine if the scheduler has started
        # i.e. MessageLoop() / sched() has been called.
        RUNLEVEL_FINAL = 2

        if level >= RUNLEVEL_FINAL:
            ts = 0.01
            tc = 0
            started_at = timeout_clock()

            # Test for not equal to 1 rather than 0 as the CRT may not have
            # zero initialised variables yet.
            while (self.env.vars["xSchedulerRunning"].value != 1) and (tc < timeout):
                time.sleep(ts)
                tc = timeout_clock() - started_at
            ended_at = timeout_clock()
            diff = ended_at - started_at


            if self.env.vars["xSchedulerRunning"].value == 1:
                if not quiet:
                    iprint("Kalimba scheduler running after {:.2f}s".format(diff))
            else:
                if not quiet:
                    iprint("Kalimba scheduler not running")
                raise self.SchedTimeout(
                    "Kalimba scheduler timed out after {:.2f}s".format(diff))

    @property
    def current_stack_start(self):
        """
        Get the stack start address for the current task.

        This is the location of the first frame on the stack rather than the
        start of the memory allocation.
        """
        p_current_tcb = self.env.var.pxCurrentTCB
        if p_current_tcb.value is None:
            raise AddressSpace.NoAccess("pxCurrentTCB is not available in the memory data source")

        if p_current_tcb.value != 0:
            # The stack end address is in the first 32 bits of the stack.
            return p_current_tcb.deref.pxStack.value + 4
        
        # If pxCurrentTCB isn't initialised the scheduler hasn't started yet.
        return self._core.fields.STACK_START_ADDR.read()

    def info(self, report=False):
        """
        Print or return a report of the general scheduler state
        """
        output = interface.Group("sched.general")
        if report is True:
            return output
        TextAdaptor(output, gstrm.iout)

    class SchedTasks(object):
        """
        Provides data on scheduler tasks.
        """
        def __init__(self, fw_env, core):
            self.core = core
            self._fw_env = fw_env

        def state(self, pause_cpu=False):
            """
            Returns a list of all tasks and data on their current state.

            pause_cpu is False by default so the chip's behaviour is not
            modified unless explicitly asked to.
            """
            tasks = []
            core = self.core

            if pause_cpu and core.is_running:
                core.pause()
                unpause = True
            else:
                unpause = False

            # There isn't one location that lists all tasks in FreeRTOS.
            # See vTaskList in tasks.c for how FreeRTOS finds all of its tasks.

            def tasklist(lis):
                """
                Generator for looping over all tasks in a FreeRTOS task list (List_t).
                """
                # FreeRTOS lists are circular, loop until we get to the start item, ignoring the end node
                final = lis.pxIndex
                i = final.deref.pxNext
                while i.value != final.value:
                    if i.value != lis.xListEnd.address:
                        yield self._fw_env.cast(i.deref.pvOwner, "TCB_t")
                    i = i.deref.pxNext

            # pxCurrentTCB contains the currently running task
            tasks.append(FreeRTOSTask(self.core, self._fw_env, self._fw_env.var.pxCurrentTCB.deref,
                                      "Running", desc=self.task_description(
                                          self._fw_env.var.pxCurrentTCB.deref)))

            # The pxReadyTasksLists array contains all ready tasks ordered by priority
            for task_list in self._fw_env.var.pxReadyTasksLists:
                tasks += [FreeRTOSTask(self.core, self._fw_env, t, "Ready", desc=self.task_description(t))
                          for t in tasklist(task_list)]

            # pxDelayedTaskList and pxOverflowDelayedTaskList contains all blocked tasks
            tasks += [FreeRTOSTask(self.core, self._fw_env, t, "Blocked", desc=self.task_description(t))
                      for t in tasklist(self._fw_env.var.pxDelayedTaskList.deref)]
            tasks += [FreeRTOSTask(self.core, self._fw_env, t, "Blocked", desc=self.task_description(t))
                      for t in tasklist(self._fw_env.var.pxOverflowDelayedTaskList.deref)]

            # xSuspendedTaskList contains suspended tasks.
            # Only present if INCLUDE_vTaskSuspend == 1
            try:
                tasks += [FreeRTOSTask(self.core, self._fw_env, t, "Suspended", desc=self.task_description(t))
                          for t in tasklist(self._fw_env.var.xSuspendedTaskList)]
            except AttributeError:
                pass

            # xTasksWaitingTermination contains tasks waiting to be deleted.
            # Only present if INCLUDE_vTaskDelete == 1
            try:
                tasks += [FreeRTOSTask(self.core, self._fw_env, t, "Deleted", desc=self.task_description(t))
                          for t in tasklist(self._fw_env.var.xTasksWaitingTermination)]
            except AttributeError:
                pass

            if unpause:
                core.run()
            return tasks

        def task_description(self, tcb):
            """
            Return a string description of the task given a pointer to the TCB.

            The generic FreeRTOS tasks class doesn't know anything about the
            tasks that are being run so this just returns an empty string.
            Derived classes are expected to override it.
            """
            return ""

        def info(self, report=False):
            """
            Print or return a report of the tasks state. This is just a text formatting wrapper
            around the data from self.state().
            """
            output = interface.Group("sched.tasks")
            output_table = interface.Table(["Name", "State",
                                            "Priority",
                                            "TCB Address",
                                            "Stack Address",
                                            "Stack Size (bytes)",
                                            "Description",
                                            "High water mark"])
            for task in self.state():
                output_table.add_row([task.name, task.task_state,
                                      task.priority,
                                      hex(task.address),
                                      hex(task.stack_address),
                                      task.stack_size_bytes,
                                      task.description,
                                      task.high_water_mark])

            output.append(output_table)
            if report is True:
                return output
            TextAdaptor(output, gstrm.iout)


class AppsP1SchedFreeRTOS(SchedFreeRTOS):
    """ Provide access to the Apps P1 FreeRTOS Scheduler.
    """
    class SchedTasks(SchedFreeRTOS.SchedTasks):
        """
        Provides data on Apps P1 FreeRTOS scheduler tasks.
        """
        def task_description(self, tcb):
            """
            Return a string description for the provided task.
            """
            if tcb.address == self._fw_env.var.vm.task.value:
                return "Runs the message loop for all VM Tasks"
            if tcb.address == self._fw_env.var.ipc_data.recv_task.value:
                return "Handles IPC messages received from Apps P0"
            if tcb.address == self._fw_env.var.xIdleTaskHandle.value:
                return "Run when there are no other runnable tasks"
            return "Application created task"
