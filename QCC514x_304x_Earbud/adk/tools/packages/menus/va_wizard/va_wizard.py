#!/usr/bin/env python
# Copyright (c) 2020 Qualcomm Technologies International, Ltd.

# Python 2 and 3
import argparse
import os

from menus.wizard import (
    logger,
    Wizard,
    Step,
    StepDone,
    gui
)

from menus.va_wizard.common import (
    installed_addons
)

from menus.va_wizard.ama import Ama
from menus.va_wizard.gaa import Gaa

PROVIDERS = [Ama, Gaa]


class Select(Step):
    def show(self):
        super(Select, self).show(title="Select VA providers to setup:")

        addons = installed_addons(self.cli_args.workspace)

        self.providers = dict()

        for p in PROVIDERS:
            if p['requires_addon']:
                if any(a == p['name'] for a in addons):
                    self.__show_provider(p)
            else:
                self.__show_provider(p)

        self.selected_provider = gui.StringVar(self)

        for provider_name, provider in self.providers.items():
            radio = gui.Radiobutton(self, value=provider_name, variable=self.selected_provider, **provider['display_opts'])
            radio.pack(anchor=gui.W, expand=True)

            if not self.selected_provider.get():
                radio.select()  # Select the first in the group

    def __show_provider(self, provider):
        self.providers[provider['name']] = provider

    def next(self):
        self.log.debug("Selected VA provider: {}".format(self.selected_provider.get()))
        self.__add_steps_for_selected_provider()
        self.__remove_steps_for_deselected_providers()
        self.log.debug("VA Wizard steps: {}".format(self.wizard.steps))

    def __add_steps_for_selected_provider(self):
        for step in reversed(self.providers[self.selected_provider.get()]['steps']):
            if step not in self.wizard.steps:
                self.wizard.steps.insert(1, step)

    def __remove_steps_for_deselected_providers(self):
        for provider_name, provider in self.providers.items():
            if provider_name != self.selected_provider.get():
                for step in provider['steps']:
                    if step in self.wizard.steps:
                        self.wizard.steps.remove(step)


class Finished(StepDone):
    def show(self):
        super(Finished, self).show(title="Voice Assistant has been setup.")


parser = argparse.ArgumentParser()
parser.add_argument("-k", "--kit", default="")
parser.add_argument("-w", "--workspace")


def run(args):
    wizard = Wizard(args, "Voice Assistant setup wizard", [Select, Finished])
    logger.log_to_file(os.path.join(os.path.dirname(args.workspace), 'va_setup_wizard.log'))
    wizard.run()


def main():
    args, _ = parser.parse_known_args()
    run(args)


if __name__ == "__main__":
    main()
