# Copyright (c) 2020 - 2021 Qualcomm Technologies International, Ltd.

# Python 2 and 3
from __future__ import print_function
import logging
import shutil
import zipfile
import re
import json
import os

from menus.wizard import (
    WizardError,
    StepDo,
    StepDone,
    Action,
    gui
)

from menus.va_wizard.common import (
    VaStepLocale, VaStepOptions, VaStepVendorFile,
    installed_addons,
)


log = logging.getLogger('va_wizard.ama')

Ama = {
    'name': 'ama',
    'steps': None,
    'requires_addon': False,
    'display_opts': {
        'text': "AMA",
    }
}

class AmaWakeWord(object):
    pass


class AmaOptions(VaStepOptions):
    def __init__(self, *args, **kwargs):
        super(AmaOptions, self).__init__(*args, **kwargs)
        self.accessory_addon_name = 'iap2'
        self.include_accessory_option = 'include_{}'.format(self.accessory_addon_name)

    def show(self):
        super(AmaOptions, self).show(title="Configure AMA")

        available_options = ['include_wuw']

        accessory_addon_installed = any(a == self.accessory_addon_name for a in installed_addons(self.workspace))
        if accessory_addon_installed:
            available_options.append(self.include_accessory_option)

        self.show_options(available_options)

    def accessory_addon_selected(self):
        return self.selected_options[self.include_accessory_option].get()

    def next(self):
        AmaDo.actions.clear()
        AmaDo.actions.append(Action("Add DEFINES to application project", self.__add_defs_to_app_project))
        if self.accessory_addon_selected():
            AmaDo.actions.append(Action("Import {} addon...".format(self.accessory_addon_name), self.__import_accessory_addon))

        self.filter_steps(Ama['steps'], AmaWakeWord, self.wuw_enabled)

    def __import_accessory_addon(self):
        self.import_addon(self.accessory_addon_name, self.selected_options)

    def __add_defs_to_app_project(self):
        defines = ['INCLUDE_AMA', 'INCLUDE_KYMERA_AEC']

        if self.wuw_enabled:
            defines.append('INCLUDE_WUW')

        self.add_defines_to_project(defines, self.app_project)


class AmaVendorFile(VaStepVendorFile, AmaWakeWord):

    @property
    def capability_project_name(self):
        return 'download_apva.x2p'

    def show(self):
        super(AmaVendorFile, self).show(title="Select AMA WakeWord vendor file")

    def next(self):
        AmaDo.actions.append(Action("Extract vendor package file", self.extract_vendor_package))
        AmaDo.actions.append(Action("Build wakeword engine capability", self.build_capability))
        AmaDo.actions.append(Action("Add capability to RO filesystem", self.add_files_to_ro_project))


class AmaLocale(VaStepLocale):
    def __init__(self, parent_wizard, previous_step, *args, **kwargs):
        super(AmaLocale, self).__init__(parent_wizard, previous_step, *args, **kwargs)
        self.va_files_dir = os.path.join(self.va_files_dir, 'ama')
        self._ama_locale_to_models_override = dict()
        self.needs_default_locale_selection = True
        self.can_skip_locale_selection = False
        self.locale_prompts_src_dir = gui.StringVar()
        self.locale_prompts_src_dir.trace('w', self.__show_locales_from_prompts)

    def show(self):
        prompts_frame = gui.Frame(self, relief=gui.GROOVE, borderwidth=2)
        prompts_frame.pack(side=gui.TOP, anchor=gui.SW, fill=gui.BOTH, expand=True)
        btn_frame = gui.Frame(prompts_frame)
        btn_frame.pack(**gui.PACK_DEFAULTS)
        gui.Label(btn_frame, text="Locale prompts folder").pack(side=gui.LEFT, anchor=gui.W)
        self.prompts_btn = gui.Button(btn_frame, text="Browse...", command=self.__select_prompts)
        self.prompts_btn.pack(side=gui.LEFT, anchor=gui.W)
        gui.Label(prompts_frame, text="Path:").pack(side=gui.LEFT)
        lbl = gui.Label(prompts_frame, relief=gui.GROOVE, borderwidth=1, textvariable=self.locale_prompts_src_dir)
        lbl.pack(side=gui.LEFT, anchor=gui.W, fill=gui.X, expand=True)

        super(AmaLocale, self).show(title="Select supported locales")
        self.wizard.next_requires = [self.locale_prompts_src_dir]

    def __select_prompts(self):
        selected = gui.tkFileDialog.askdirectory(title="Select folder containing AMA locale prompts")

        if selected:
            self.locale_prompts_src_dir.set(selected)

    def show_locale_options(self):
        self._locales_listbox.delete(0, gui.END)
        if self.vendor_package_file:
            locales = self.__get_locales_from_vendor_package()
        else:
            locales = []

        for locale in locales:
            self._locales_listbox.insert(gui.END, locale)

    def __get_locales_from_vendor_package(self):
        zipObj = zipfile.ZipFile(self.vendor_package_file.get())
        for f in zipObj.namelist():
            if ("kalimba" in f) and (f.endswith("localeToModels.json")):
                locale_to_models = json.loads(zipObj.read(f))['alexa']
                for locale, model in locale_to_models.items():
                    if len(model) > 1:
                        log.warning("More than one model file found for locale: {}".format(locale))
                    self.model_file_prefix, model_file = model[0].split('/')
                    self.model_file_prefix = "models/{}".format(self.model_file_prefix)
                    model_file = model_file + ".bin"
                    model_file_code = self.__get_locale_from_file(model_file)
                    self.locales_map[locale] ={
                        'file': model_file,
                        'code': model_file_code
                    }
                    if locale not in model_file:
                        self._ama_locale_to_models_override[locale] = model_file_code
                    yield locale

    def __show_locales_from_prompts(self, *args):
        if self.vendor_package_file:
            return

        for locale in self.__get_locales_from_prompts():
            self._locales_listbox.insert(gui.END, locale)


    def __get_locales_from_prompts(self):
        prompts_dir = self.locale_prompts_src_dir.get()
        found_locales = set()
        for localized_prompt in os.listdir(prompts_dir):
            if localized_prompt.endswith(".sbc"):
                found_locales.add(self.__get_locale_from_file(localized_prompt))

        if not found_locales:
            msg = "No prompts found in folder: {}".format(prompts_dir)
            gui.tkMessageBox.showerror(title="Error getting CHIP_TYPE", message=msg)

        return found_locales

    def next(self):
        PROMPTS_PER_LOCALE = 2

        prompts_to_add = self.prompts_to_add

        expected_number_of_prompts = len(self.selected_locales) * PROMPTS_PER_LOCALE
        if expected_number_of_prompts != len(prompts_to_add):
            message = ("Expected num_prompts={} (num_locales={} * prompts_per_locale={}), found num_prompts={}"
                .format(expected_number_of_prompts, len(self.selected_locales), PROMPTS_PER_LOCALE, len(prompts_to_add)))

            self.log.warning(message)
            message += ("\nYou can proceed but setup must be finished later by adding the missing prompts to the"
                        "Read Only filesystem project (ro_fs)")
            if not gui.tkMessageBox.askokcancel(title="Unexpected number of prompts", message=message):
                raise WizardError()

            self.log.warning("Continuing with incomplete localized prompts setup")

        if self.selected_locales:
            if self.vendor_package_file:
                AmaDo.actions.append(Action("Add preloaded models to RO filesystem", self.__add_ama_preloaded_model_files))
            AmaDo.actions.append(Action("Add localized prompts to RO filesystem", self.__add_locale_prompts))
            AmaDo.actions.append(Action("Add model DEFINES to application project", self.__add_model_defs_to_projects))

    def __add_ama_preloaded_model_files(self):
        self.add_preloaded_model_files(self.ro_fs_project)

    def __add_locale_prompts(self):
        self.add_files_to_project(self.ro_fs_project, self.prompts_to_add)

    @property
    def prompts_to_add(self):
        wanted_locale_prompts = []
        for locale in self.selected_locales:
            try:
                wanted_locale_prompts.append(self._ama_locale_to_models_override[locale])
            except KeyError:
                wanted_locale_prompts.append(locale)

        prompts_to_add = []
        prompts_src_dir = self.locale_prompts_src_dir.get()
        for localized_prompt in os.listdir(prompts_src_dir):
            for loc in wanted_locale_prompts:
                if loc in localized_prompt:
                    if not os.path.isdir(self.prompts_dir):
                        os.makedirs(self.prompts_dir)

                    src = os.path.join(prompts_src_dir, localized_prompt)
                    dst = os.path.join(self.prompts_dir, localized_prompt)
                    shutil.copy(src, dst)
                    prompts_to_add.append(dst)
        return prompts_to_add

    def __add_model_defs_to_projects(self):
        default_model = self.default_model.get()
        if not default_model:
            return

        available_locales = ','.join('\\"{}\\"'.format(l) for l in self._locales_listbox.get(0, gui.END))

        defines = [
            'AMA_DEFAULT_LOCALE=\\"{}\\"'.format(default_model),
            'AMA_AVAILABLE_LOCALES={}'.format(available_locales)
        ]

        overrides = []
        for locale, override in self._ama_locale_to_models_override.items():
            overrides.append('{{\\"{}\\",\\"{}\\"}}'.format(locale, override))

        if overrides:
            defines.append('AMA_LOCALE_TO_MODEL_OVERRIDES={}'.format(",".join(overrides)))

        self.add_defines_to_project(defines, self.app_project)

    def __get_locale_from_file(self, model_file):
        m = re.match(r".*([a-z]{2}-[A-Z]{2})", model_file)
        locale = m.groups()[0]
        return "{}".format(locale)




class AmaDo(StepDo):
    pass


class AmaDone(StepDone):
    def show(self):
        super(AmaDone, self).show(title="Ama Voice Assistant configured")

        config_instructions = (
            "For additional Ama Voice assistant options please see:\n"
            "adk/src/services/voice_ui/ama/ama_config.h"
        )
        further_config = gui.Label(self, text=config_instructions)
        further_config.pack(**gui.PACK_DEFAULTS)


Ama['steps'] = (AmaOptions, AmaVendorFile, AmaLocale, AmaDo, AmaDone)
