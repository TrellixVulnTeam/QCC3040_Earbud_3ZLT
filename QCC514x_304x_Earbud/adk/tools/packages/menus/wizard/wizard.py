# Copyright (c) 2020 - 2021 Qualcomm Technologies International, Ltd.

import os

from menus.wizard.exceptions import WizardError

from menus.wizard.step import (
    Step,
    gui
)


def _slide(parent, w, x):
    w.place(in_=parent, relx=0.5, rely=0.5, relwidth=1, relheight=1, anchor=gui.CENTER, x=x)
    w.after(10, w.update())


def _slide_in(parent, w):
    start = int(parent.winfo_width() * 0.75)
    for x in range(start, 0, -10):
        _slide(parent, w, x)


def _slide_out(parent, w):
    end = parent.winfo_width()
    for x in range(0, end, 10):
        _slide(parent, w, x)


class Wizard(object):
    BACK = "<< Back"
    NEXT = "Next >>"
    SKIP = "Skip >>"
    FINISH = "Finish"

    def __init__(self, args, title="ADK Wizard", steps=None):

        self.args = args
        self.root = gui.Root
        self.root.title(title)
        self._configure_root_window()

        self.button_frame = gui.Frame(self.root)
        self.content_frame = gui.Frame(self.root)

        self.__next_requires = list()

        self.back_button = gui.Button(self.button_frame, text=self.BACK, command=self._back)
        self.next_button = gui.Button(self.button_frame, text=self.NEXT, command=self._next)
        self.finish_button = gui.Button(self.button_frame, text=self.FINISH, command=self._finish)

        self.button_frame.pack(side=gui.BOTTOM, fill=gui.X)
        self.content_frame.pack(side=gui.TOP, fill=gui.X, expand=True)

        self.current_step_idx = 0
        self.current_step = None

        if steps is None:
            self.steps = [Step, Step]
        else:
            self.steps = steps

        self.done_steps = []

        self.show_step(self.current_step_idx)
        self.root.after(10, lambda: self._center(self.root))
        # self.root.protocol("WM_DELETE_WINDOW", self.cancel)

    def run(self):
        self.root.mainloop()

    def _back(self):
        self.next_button['text'] = self.NEXT
        self.next_button['state'] = gui.NORMAL
        del self.next_requires

        self.current_step.back()
        done_step = self.done_steps.pop()
        self.show_step(self.current_step_idx - 1)
        done_step.destroy()

    @property
    def next_requires(self):
        return self.__next_requires

    @next_requires.setter
    def next_requires(self, value):
        if not isinstance(value, (list, tuple, set)):
            value = [value]

        for v in value:
            assert isinstance(v, gui.Variable)
            v.next_requires_trace_id = v.trace('w', self.__next_button_enable)

        self.__next_requires = value

    @next_requires.deleter
    def next_requires(self):
        for v in self.__next_requires:
            v.trace_vdelete('w', v.next_requires_trace_id)
        self.__next_requires = []

    def __next_button_enable(self, *args):
        self.next_button['state'] = gui.NORMAL if all(n.get() for n in self.next_requires) else gui.DISABLED

    def __next_button_disable(self):
        self.next_button['state'] = gui.DISABLED

    def _next(self):
        try:
            self.current_step.next()
        except WizardError:
            return

        self.next_button['text'] = self.NEXT
        del self.next_requires
        self.show_step(self.current_step_idx + 1)

    def _finish(self):
        self.root.quit()
        exit(0)

    def _configure_root_window(self):
        qmde_icon = os.path.join(self.args.kit, 'res', 'qmde_allsizes.ico')
        if os.path.isfile(qmde_icon):
            self.root.iconbitmap(bitmap=qmde_icon, default=qmde_icon)

        # "Show" window again and lift it to top so it can get focus,
        # otherwise dialogs will end up behind other windows
        self.root.deiconify()
        self.root.lift()
        self.root.focus_force()

    @staticmethod
    def _center(win):
        width = max(win.winfo_width(), 300)
        height = win.winfo_height()
        x_cordinate = int((win.winfo_screenwidth() / 2) - (width / 2))
        y_cordinate = int((win.winfo_screenheight() / 2) - (height / 2))
        win.geometry("{}x{}+{}+{}".format(width, height, x_cordinate, y_cordinate))
        win.geometry("")
        win.minsize(width, height)

    def show_step(self, step_idx):
        if self.current_step is not None:
            self.current_step.pack_forget()

        try:
            new_step = self.done_steps[step_idx]
        except IndexError:
            previous_step = self.done_steps[step_idx - 1:step_idx]
            previous_step = previous_step[0] if len(previous_step) == 1 else None
            new_step = self.steps[step_idx](self, previous_step, self.args)
            new_step.show()

        self.__next_button_disable()  # Disable button during animation

        if step_idx > self.current_step_idx:
            _slide_in(self.content_frame, new_step)
        elif step_idx < self.current_step_idx:
            _slide_out(self.content_frame, self.current_step)

        new_step.pack(fill=gui.BOTH, expand=True)

        self.__next_button_enable()

        self.current_step_idx = step_idx
        if new_step not in self.done_steps:
            self.done_steps.append(new_step)

        self.current_step = new_step

        if step_idx == 0:
            self.back_button.pack_forget()
            self.next_button.pack(side=gui.RIGHT)
            self.finish_button.pack_forget()

        elif step_idx == len(self.steps) - 1:
            self.back_button.pack(side=gui.LEFT)
            self.next_button.pack_forget()
            self.finish_button.pack(side=gui.RIGHT)

        else:
            self.back_button.pack(side=gui.LEFT)
            self.next_button.pack(side=gui.RIGHT)
            self.finish_button.pack_forget()
