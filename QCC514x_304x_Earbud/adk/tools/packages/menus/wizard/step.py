# Copyright (c) 2020 - 2021 Qualcomm Technologies International, Ltd.

import sys
import logging
import itertools
from threading import Thread, Event

from menus.wizard import gui
from menus.wizard.exceptions import WizardThreadException
from menus.wizard.action import ActionsList


class Step(gui.Frame, object):
    def __init__(self, parent_wizard, previous_step, cli_args=None):
        super(Step, self).__init__(parent_wizard.content_frame)

        self.wizard = parent_wizard
        self.cli_args = cli_args
        # Get logger with class name of child class inheriting from Step
        self.log = logging.getLogger('wizard.step.{}'.format(type(self).__name__))

    def show(self, title="This is a step", *args, **kwargs):
        gui.Label(self, text=title).pack(**gui.PACK_DEFAULTS)

    def next(self):
        self.log.debug("Next")

    def back(self):
        self.log.debug("Back")


class StepDo(Step):
    actions = ActionsList()

    def __init__(self, *args, **kwargs):
        super(StepDo, self).__init__(*args, **kwargs)
        self.__loop = itertools.cycle(('\\', '|', '/', '-'))
        self.excep_event = Event()

    def show(self):
        super(StepDo, self).show(title="The wizard will now perform the following actions")
        self.progress = dict()
        for a in self.actions:
            var = gui.StringVar()
            var.set("[   ]")
            self.progress[a] = var
            action_frame = gui.Frame(self)
            action_frame.pack(**gui.PACK_DEFAULTS)
            progress_lbl = gui.Label(action_frame, textvariable=var, anchor=gui.W, font='TkFixedFont')
            progress_lbl.pack(anchor=gui.W, side=gui.LEFT, fill=gui.Y)
            description_lbl = gui.Label(action_frame, text=a.description, anchor=gui.W)
            description_lbl.pack(anchor=gui.W, side=gui.LEFT, fill=gui.BOTH, expand=True)

        self.proceed_btn = gui.Button(self, text="Proceed", command=self._run_actions)
        self.proceed_btn.pack(anchor=gui.E, side=gui.TOP, fill=gui.Y, expand=True)
        self.tasks_done = gui.BooleanVar()
        self.tasks_done.trace("w", self._check_exception)
        self.wizard.next_requires = [self.tasks_done]

    def _run_actions(self):
        self.wizard.back_button['state'] = gui.DISABLED
        self.proceed_btn['state'] = gui.DISABLED
        self.actions_thread = Thread(name="Actions thread", target=self._execute)
        self.actions_thread.daemon = True
        self.actions_thread.start()
        self.wizard.root.after(10, self._check_done)

    def _execute(self):
        while self.actions:
            a = self.actions[0]
            self.log.info("Executing action: {}".format(a.description))
            try:
                a.callable()
            except Exception:
                self.excep_event.set()
                self.actions_thread._excep = sys.exc_info()
                break
            self.progress[a].set("[ X ]")
            self.actions.popleft()

    def _check_done(self):
        if self.actions_thread.is_alive():
            self.progress[self.actions[0]].set("[ {} ]".format(next(self.__loop)))
            self.wizard.root.after(10, self._check_done)
        elif self.excep_event.is_set():
            self.tasks_done.set(False)
        else:
            self.tasks_done.set(True)

    def _check_exception(self, *args):
        if self.excep_event.is_set():
            raise WizardThreadException(self.actions_thread._excep)

    def back(self):
        self.tasks_done.set(False)

    def next(self):
        self.wizard.back_button['state'] = gui.NORMAL


class StepDone(Step):
    def show(self, title="The wizard is now done", *args, **kwargs):
        super(StepDone, self).show(title=title)
        self.wizard.back_button['state'] = gui.DISABLED