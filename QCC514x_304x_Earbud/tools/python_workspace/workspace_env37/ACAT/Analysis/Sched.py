############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2013 - 2020 Qualcomm Technologies, Inc. and/or its
# subsidiaries. All rights reserved.
#
############################################################################
"""Scheduler Analysis.

Module used to analyse Kymera scheduler.
"""
import numbers

from ACAT.Analysis import Analysis
from ACAT.Core import Arch
from ACAT.Core import CoreUtils as cu
from ACAT.Core.exceptions import (
    BundleMissingError, DebugInfoNoVariableError, OutdatedFwAnalysisError
)
from ACAT.Core.CoreTypes import ChipVarHelper as ch

try:
    from future_builtins import hex
except ImportError:
    pass

VARIABLE_DEPENDENCIES = {
    'strict': (
        '$_strict_events_queue', '$_casual_events_queue',
        '$_CurrentPriorityMask', '$_current_id', '$_tasks_in_priority',
        '$_bg_ints_in_priority', 'L_pCachedMessage'
    )
}
ENUM_DEPENDENCIES = {'not_strict': ('panicid',)}
TYPE_DEPENDENCIES = {
    'tEventsQueue': ('first_event',),
    'TASK': ('next', 'handler', 'id', 'mqueue', 'priv'),
    'BG_INTQ': ('first',),
    'BGINT': ('handler', 'raised', 'id'),
    'MSG': ('next',),
    'tTimerStruct': (
        'timer_id', 'TimedEventFunction', 'data_pointer', 'variant'
    )
}

# Timer constants.
MAX_EVENT_TIME_PAST = 50000  # See pl_timers.c


class Timer(object):
    """A structure for storing information about a timer."""

    def __init__(self):
        # Members are filled in by something else; need access to Analysis
        # functions.
        self.raw_data = None  # tTimerStruct-shaped variable
        self.id = 0
        self.timed_event_function = None
        self.type = ""  # Should be set to 'strict' or 'casual'
        self.is_periodic = False
        self.period = 0
        self.data_pointer = None
        self.earliest_time = 0  # Only for casual events
        self.latest_time = 0  # Used for casual and strict events

    def __str__(self):
        """Returns a tidy string representation of a timer object."""
        tf_str = "Timer " + hex(self.id) + "\n"
        tf_str = tf_str + "Type: " + self.type + "\n"

        if self.type == 'strict':
            tf_str = tf_str + "Expiry time: " + hex(self.latest_time) + "\n"
        else:
            tf_str = tf_str + "Earliest expiry: " + hex(self.earliest_time)
            tf_str = tf_str + ", latest expiry: " + \
                hex(self.latest_time) + "\n"

        tf_str = tf_str + "Data pointer: " + hex(self.data_pointer) + "\n"

        if self.timed_event_function is not None:
            tf_str = tf_str + "EventFunction: \n"
            temp_str = '    ' + str(self.timed_event_function)  # Indent
            tf_str = tf_str + temp_str.replace('\n', '\n    ')  # Indent
            tf_str = tf_str[:-4]  # Remove indent from final newline

        return tf_str


class Sched(Analysis.Analysis):
    """
    Encapsulates an analysis for the scheduler: task statuses, timers etc.

    Args:
        **kwargs: Arbitrary keyword arguments.
    """

    def __init__(self, **kwargs):
        # Call the base class constructor.
        Analysis.Analysis.__init__(self, **kwargs)
        self._check_kymera_version()

    def _check_kymera_version(self):
        """Checks if the Kymera version is compatible with this analysis.

        Raises:
            OutdatedFwAnalysisError: For outdated Kymera.
        """
        # Preserved data has been removed, if new variables for panic and fault
        # cannot be found raise the outdated firmware exception.

        try:
            self.debuginfo.get_var_strict("$_panic_data")
            self.debuginfo.get_var_strict("$_fault_data")
        except DebugInfoNoVariableError:
            # FALLBACK TO THE OLD IMPLEMENTATION
            raise OutdatedFwAnalysisError()

    def run_all(self):
        """Performs analysis and spew the output to the formatter.

        It analyses the active tasks and any pending messages, the timers
        for the casual and strict events, the time a panic occurred and
        the wake up timer.
        """
        self.formatter.section_start('Scheduler')
        self.analyse_tasks()
        self.analyse_wakeup_timer()
        self.analyse_timers()
        self.formatter.section_end()

    def get_strict_events(self):
        """Returns a tuple of Timer objects."""
        strict_events_q = self.chipdata.get_var_strict('$_strict_events_queue')
        first_event_p = strict_events_q['first_event']
        all_events = []
        for event in ch.parse_linked_list(first_event_p, 'next'):
            timer = self._read_timer(event, 'strict')
            all_events.append(timer)

        return tuple(all_events)

    def get_casual_events(self):
        """Returns a tuple of Timer objects."""
        casual_events_q = self.chipdata.get_var_strict('$_casual_events_queue')
        first_event_p = casual_events_q['first_event']
        all_events = []
        for event in ch.parse_linked_list(first_event_p, 'next'):
            timer = self._read_timer(event, 'casual')
            all_events.append(timer)

        return tuple(all_events)

    def get_timer(self, timer_id):
        """Gets the time object.

        Returns a Timer object encapsulating the timer with timer_id
        provided, or None if no timers with that ID exist.

        Args:
            timer_id
        """
        events = []
        events.extend(self.get_strict_events())
        events.extend(self.get_casual_events())
        results = [e for e in events if e.id == timer_id]
        if results:
            # timer IDs are unique, so assume there's only one in the list!
            return results[0]

    def get_next_timer_trigger_time(self):
        """Get the time of the next timer trigger.

        Reads the value of timer registers and returns the time at which
        the next strict timer will fire, or None if no timer is currently
        set.
        """
        t1_en = self.chipdata.get_reg_strict('$TIMER1_EN').value
        t1_trigger = None
        if t1_en == 1:
            t1_trigger = self.chipdata.get_reg_strict('$TIMER1_TRIGGER').value
            try:
                t1_trigger_ms = self.chipdata.get_reg_strict(
                    '$TIMER1_TRIGGER_MS'
                ).value
                if Arch.addr_per_word == 4:
                    t1_trigger = t1_trigger_ms
                else:
                    t1_trigger = ((t1_trigger_ms << 24) | t1_trigger)
            except DebugInfoNoVariableError:
                # Crescendo Aura and Gordon does not use $TIMER1_TRIGGER_MS
                pass
        return t1_trigger

    #######################################################################
    # Analysis methods - public since we may want to call them individually
    #######################################################################

    def analyse_task(self, task_id_name):
        """Analyse a task by ID or module name.

        Args:
            task_id_name: A parameter than can be a number or a string.
                Number: It is a task ID
                String: Name of the module name.
        """
        if isinstance(task_id_name, numbers.Integral):
            self._analyse_task_by_id(task_id_name)

        elif isinstance(task_id_name, str):
            self._analyse_task_by_module_name(task_id_name)

        else:
            print(
                "The parameter `task_id_name` should be task's ID or its "
                "module name."
            )

    def analyse_tasks(self):
        """Displays the tasks.

        Outputs analysis of all currently-active tasks, as well as any
        pending messages.
        """
        current_task_ptr = self.chipdata.get_var_strict('$_current_id').value
        if current_task_ptr != 0:
            current_task_id = self.chipdata.cast(
                current_task_ptr, 'taskid'
            ).value
        else:
            current_task_id = None

        self.formatter.section_start('Tasks')

        if current_task_id is None:
            self.formatter.output('No task is currently Active.\n')

        # Get the list of task queues. Since this is stored as a simple array,
        # .members contains a list of the individual queues.
        task_queues = self.chipdata.get_var_strict(
            '$_tasks_in_priority'
        )

        matched_active_task = False  # Which task is currently active?
        for pri, queue in enumerate(task_queues):
            if queue['first'].value == 0:
                continue
            first_task = self.chipdata.cast(
                queue['first'].value, 'TASK'
            )
            all_tasks_in_q = [
                t for t in ch.parse_linked_list(first_task, 'next')
            ]
            self.formatter.output(
                str(len(all_tasks_in_q)) + ' tasks found at priority ' +
                str(pri) + "\n"
            )

            for task in all_tasks_in_q:
                handler = task['handler'].value
                if handler != 0:
                    try:
                        module_name = self.debuginfo.get_source_info(
                            handler
                        ).module_name
                    except BundleMissingError:
                        module_name = (
                            "No source information." + "Bundle is missing."
                        )
                    self.formatter.output(
                        'Task ' + str(hex(task['id'].value)) +
                        "\nhandler:" + module_name + "\n"
                    )
                    msg_anchor = task['mqueue']
                    if msg_anchor['first'].value != 0:
                        first_msg = self.chipdata.cast(
                            msg_anchor['first'].value, 'MSG'
                        )
                        all_msgs = [
                            m
                            for m in ch.parse_linked_list(first_msg, 'next')
                        ]
                        if all_msgs:
                            self.formatter.output(
                                '  ' + str(len(all_msgs)) +
                                ' messages pending in message queue'
                            )
                            for i in all_msgs:
                                self.formatter.output('  ' + str(i))

                # List whether or not this is the active task.
                if current_task_id == task['id'].value:
                    matched_active_task = True
                    self.formatter.output('^----- Currently active task \n')

        bg_ints_in_priority = self.chipdata.get_var_strict(
            '$_bg_ints_in_priority'
        )
        for pri, bg_int_g in enumerate(bg_ints_in_priority):
            if bg_int_g['first'].value == 0:
                continue

            first_bg_int = self.chipdata.cast(
                bg_int_g['first'].value, 'BGINT'
            )
            all_bg_int_in_q = [
                t for t in ch.parse_linked_list(first_bg_int, 'next')
            ]
            self.formatter.output(
                str(len(all_bg_int_in_q)) +
                ' Background interrupt found at priority ' + str(pri)
            )

            for bg_int in all_bg_int_in_q:
                handler = bg_int['handler'].value
                if handler != 0:
                    try:
                        module_name = self.debuginfo.get_source_info(
                            handler
                        ).module_name
                    except BundleMissingError:
                        module_name = (
                            "No source information." + "Bundle is missing."
                        )
                    self.formatter.output(
                        'Bg_int ' + str(hex(bg_int['id'].value)) +
                        '\nhandler: ' + module_name
                    )
                    if module_name == "opmgr_operator_bgint_handler":
                        try:
                            # Get the operator id
                            opmgr_analysis = self.interpreter.get_analysis(
                                "opmgr", self.chipdata.processor
                            )
                            oppointer = self.chipdata.get_data(
                                bg_int['ppriv'].value
                            )
                            opdata = self.chipdata.cast(
                                oppointer, "OPERATOR_DATA"
                            )
                            opid = opdata['id'].value
                            operator = opmgr_analysis.get_operator(opid)
                            self.formatter.output(
                                'Operator kick for operator ' +
                                hex(operator.op_ep_id) + " " +
                                operator.cap_data.name
                            )
                        except BaseException as error:
                            self.formatter.output(
                                'Operator not found %s' % str(error)
                            )
                    raised = bg_int['raised'].value
                    if raised != 0:
                        bg_ints = cu.inspect_bitfield(
                            raised,
                            True,
                            self.debuginfo.get_kymera_debuginfo().dm_word_width
                        )
                        self.formatter.output(
                            '  ' + str(len(bg_ints)) + ' BG Interrupts raised'
                        )

                # List whether or not this is the active task.
                if current_task_id == bg_int['id'].value:
                    matched_active_task = True
                    self.formatter.output('^----- Currently active task')
                self.formatter.output('')

        if (current_task_id is not None) and (not matched_active_task):
            self.formatter.alert(
                'No task found whose address matches current_task_id (' +
                hex(current_task_id) + ')'
            )

        self.print_recently_freed_messages()

    def raise_bg_int(self, bg_int_id):
        """Raise a background interrupt.

        This function could be used to kick an operator. Use analyse_tasks to
        get the bg_int id associated with the operator.

        Args:
            bg_int_id (int): Background interrupt ID.
        """
        bg_ints_in_priority = self.chipdata.get_var_strict(
            '$_bg_ints_in_priority'
        )
        for _, bg_int_g in enumerate(bg_ints_in_priority):
            if bg_int_g['first'].value == 0:
                continue

            first_bg_int = self.chipdata.cast(
                bg_int_g['first'].value, 'BGINT'
            )
            all_bg_int_in_q = [
                t for t in ch.parse_linked_list(first_bg_int, 'next')
            ]

            for bg_int in all_bg_int_in_q:
                if bg_int['id'].value == bg_int_id:
                    bg_raised_address = bg_int['raised'].address
                    bg_raised_value = bg_int['raised'].value
                    bg_raised_value += 1
                    self.chipdata.set_data(
                        bg_raised_address, [bg_raised_value]
                    )

    def print_recently_freed_messages(self, max_duplicate_msgs=100):
        """Outputs the recently freed messages.

        Args:
            max_duplicate_msgs (int): Maximum number of duplicate messages
                to print. The default is 100, which is a large and arbitrary
                number.
        """
        cached_msg_p = self.chipdata.get_var_strict('L_pCachedMessage').value
        cached_msg = self.chipdata.cast(cached_msg_p, 'MSG')
        self.formatter.output('Most-recently-freed message: ')
        msg_ids = []
        duplicates = 0
        for element in ch.parse_linked_list(cached_msg, 'next'):
            if duplicates > max_duplicate_msgs:
                self.formatter.error(
                    "Too many duplicate messages! Stopping the printout."
                )
                break
            if element.id.value in msg_ids:
                duplicates += 1

            self.formatter.output(str(element))
            msg_ids.append(element.id.value)

        self.formatter.section_end()

    def analyse_timers(self):
        """Analyse event timers.

        Outputs analysis of strict and casual event timers, plus takes a
        guess at what the time was when we panicked.
        """
        events = []
        events.extend(self.get_strict_events())
        events.extend(self.get_casual_events())

        self.formatter.section_start('Timers')

        # If we panicked, we have an accurate recording of TIMER_TIME at the
        # point something went wrong. Otherwise, we don't really know what the
        # time was.
        sanitycheck_var = self.interpreter.get_analysis(
            "sanitycheck", self.chipdata.processor
        )
        panic_var = sanitycheck_var.get_panic_data()
        panic_id = panic_var['last_id'].value
        panic_time = sanitycheck_var.get_panic_time()
        next_trig_time = self.get_next_timer_trigger_time()

        if panic_time is not None:
            try:
                # Panic 0 should be treated as invalid.
                if panic_id == 0:
                    raise BaseException
                _ = self.debuginfo.get_enum('panicid', panic_id)[0]
                self.formatter.output(
                    'TIMER_TIME at point of (previous?) panic: ' +
                    hex(panic_time)
                )
            except BaseException:
                self.formatter.output(
                    'Panic id is invalid, therefore the chip has not panicked.'
                )
        else:
            if next_trig_time is not None:
                self.formatter.output(
                    'Next timer is set for : ' +
                    hex(next_trig_time) +
                    '; TIMER_TIME was presumably some point ' +
                    'before that when coredump was initiated.'
                )
            else:
                # No timer has ever been set. Hmm.
                self.formatter.output(
                    'No timers were set at the point of the '
                    'coredump. No idea what the time was.'
                )

        self.formatter.output('Timers pending: ' + str(len(events)))
        self.formatter.output('')

        matched_trig_time = False
        for event in events:
            self.formatter.output(event)
            if event.latest_time == next_trig_time:
                matched_trig_time = True
                self.formatter.output('^----- Next timer to expire \n')

            if panic_time is not None:
                # If we know the time at which everything stopped, we can do a
                # lot more analysis than if we don't.
                try:
                    # Call used to catch exception.
                    _ = self.debuginfo.get_enum(
                        'panicid', panic_id
                    )[0]
                    if event.latest_time < (panic_time - MAX_EVENT_TIME_PAST) & 0xFFFFFFFF:
                        self.formatter.alert(
                            'Expiry time for timer ' + hex(event.id) +
                            ' is too far in the past!'
                        )
                        # if DEBUG_KICK_TIMERS is defined, we should have
                        # panicked with PANIC_AUDIO_TIMER_TOO_OLD.
                except BaseException:
                    pass

        if (next_trig_time is not None) and (not matched_trig_time):
            self.formatter.alert(
                'No timer found whose expiry time ' +
                'matches hardware trigger time (' +
                hex(next_trig_time) +
                ') (N.B. This is expected if code ' +
                'execution is servicing a timer handler.)'
            )
        self.formatter.section_end()

    def analyse_wakeup_timer(self):
        """Outputs details of the wakeup timer."""
        self.formatter.section_start('Wakeup Timer')

        wakeup_timer_en = self.chipdata.get_reg_strict('$_TIMER2_EN').value
        wakeup_timer_time = self.chipdata.get_reg_strict(
            '$_TIMER2_TRIGGER'
        ).value
        if wakeup_timer_en == 1:
            self.formatter.output(
                'Wakeup time is: ' +
                hex(wakeup_timer_time))
        else:
            self.formatter.output('No wakeup timer is configured.')

        self.formatter.section_end()

    #######################################################################
    # Private methods - don't call these externally.
    #######################################################################

    def _get_all_queued_tasks(self):
        # Get the list of task queues.
        task_queues = self.chipdata.get_var_strict(
            '$_tasks_in_priority'
        )

        for priority, queue in enumerate(task_queues):
            if queue['first'].value == 0:
                continue
            first_task = self.chipdata.cast(
                queue['first'].value, 'TASK'
            )

            for task in ch.parse_linked_list(first_task, 'next'):
                yield (priority, task)

    def _analyse_task_by_id(self, task_id):
        """Analyse a task by ID."""
        for _, task in self._get_all_queued_tasks():
            if task['id'].value != task_id:
                continue

            # Found the task!
            self.__analyse_task(task)
            break

    def _analyse_task_by_module_name(self, module_name):
        """Analyse a task by ID."""
        for _, task in self._get_all_queued_tasks():
            handler = task['handler'].value
            if handler == 0:
                continue

            source_info = self.debuginfo.get_source_info(handler)
            if module_name != source_info.module_name:
                continue

            # Found the task!
            self.__analyse_task(task)
            break

    def __analyse_task(self, task):
        handler = task['handler'].value
        if handler == 0:
            return

        print(task)

        if not task['priv'].value:
            # When the pointer is null then it shouldn't be dereferenced.
            return

        module_name = self.debuginfo.get_source_info(handler).module_name
        module_type = self.debuginfo.get_sched_task_module_data_type(
            module_name
        )

        if module_type is None:
            # Perhaps this is an old firmware and there is no
            # type not found for this module name.
            return

        print(
            "module_name: {} (type: {})\n".format(
                module_name,
                module_type
            )
        )
        print(self.chipdata.cast(
            task['priv'].value,
            module_type
        ))

    def _read_timer(self, t, timer_type='unknown'):
        """Read timer.

        Takes a raw timer ct.Variable (cast from a linked-list) and turns
        it into a Timer object.

        Args:
            t
            timer_type (str, optional)
        """
        timer = Timer()
        timer.type = timer_type  # should be 'strict' or 'casual'

        # Need to upcast to the correct timer timer_type to get hold of the
        # expiry times. We still need the 'base' timer struct for the
        # other elements though.
        upcast_t = self.chipdata.cast(t.address, 'tTimerStruct')
        if timer_type == 'strict':
            # Timer is 32-bit, so concatenate the two words
            # into one for 24 bit Kalimbas and take the whole
            # word for 32 bit Kalimbas.
            if Arch.addr_per_word == 4:
                timer.latest_time = upcast_t['variant']['event_time'].value
            else:
                timer.latest_time = (
                    (
                        upcast_t['variant']
                        ['event_time'].value[0] << 24
                    ) | upcast_t['variant']
                    ['event_time'].value[1]
                ) & 0xffffffff
        elif timer_type == 'casual':
            # Caveat as above
            if Arch.addr_per_word == 4:
                timer.earliest_time = upcast_t['variant']['casual']['earliest_time'].value
                timer.latest_time = upcast_t['variant']['casual']['latest_time'].value
            else:
                timer.earliest_time = (
                    (
                        upcast_t['variant']['casual']
                        ['earliest_time'].value[0] << 24
                    ) | upcast_t['variant']['casual']
                    ['earliest_time'].value[1]
                ) & 0xffffffff
                timer.latest_time = (
                    (
                        upcast_t['variant']['casual']
                        ['latest_time'].value[0] << 24
                    ) | upcast_t['variant']['casual']
                    ['latest_time'].value[1]
                ) & 0xffffffff
        else:
            raise Exception("Unknown timer type!")

        timer.raw_data = upcast_t
        timer.id = upcast_t['timer_id'].value
        f_ptr = upcast_t['TimedEventFunction'].value
        try:
            timer.timed_event_function = self.debuginfo.get_source_info(f_ptr)
        except BundleMissingError:
            timer.timed_event_function = None
        timer.data_pointer = upcast_t['data_pointer'].value
        return timer
