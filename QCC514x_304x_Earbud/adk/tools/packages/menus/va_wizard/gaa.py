# Copyright (c) 2020 - 2021 Qualcomm Technologies International, Ltd.

# Python 2 and 3
from __future__ import print_function
import os
import zipfile
import fnmatch
import re

from menus.addon_importer import UI

from menus.wizard import (
    StepDo,
    StepDone,
    gui,
    Action
)

from menus.va_wizard.common import (
    VaStepOptions, VaStepLocale, VaStepVendorFile,
    get_addon_dir,
    addon_utils
)

Gaa = {
    'name': 'gaa',
    'steps': None,
    'requires_addon': True,
    'display_opts': {
        'text': "GAA",
    }
}


class GaaHotword(object):
    pass


class GaaOptions(VaStepOptions):
    def __init__(self, *args, **kwargs):
        super(GaaOptions, self).__init__(*args, **kwargs)

        self.addon_name = 'gaa'
        self.gaa_addon_dir = get_addon_dir(self.workspace, self.addon_name)
        self.gaa_project = os.path.join(self.gaa_addon_dir, 'projects', self.app_name, self.chip_type, 'gaa.x2p')

    def show(self):
        super(GaaOptions, self).show(title="Configure GAA")

        available_addon_options = UI.get_available_options_for_addon(self.gaa_addon_dir)

        self.show_options(available_addon_options)

    def next(self):
        GaaDo.actions.clear()
        GaaDo.actions.append(Action("Import GAA addon", self.__import_gaa_addon))

        self.filter_steps(Gaa['steps'], GaaHotword, self.wuw_enabled)

    def __import_gaa_addon(self):
        self.import_addon(self.addon_name, self.selected_options)

class GaaVendorFile(VaStepVendorFile, GaaHotword):
    def __init__(self, *args, **kwargs):
        super(GaaVendorFile, self).__init__(*args, **kwargs)

    def show(self):
        super(GaaVendorFile, self).show(title="Select GAA Hotword vendor file")

    @property
    def capability_project_name(self):
        return 'download_gva.x2p'

    def next(self):
        GaaDo.actions.append(Action("Extract vendor package file", self.extract_vendor_package))
        GaaDo.actions.append(Action("Build hotword engine capability", self.build_capability))
        GaaDo.actions.append(Action("Add capability to RO filesystem", self.add_files_to_ro_project))


class GaaLocale(VaStepLocale, GaaHotword):
    def __init__(self, parent_wizard, previous_step, *args, **kwargs):
        super(GaaLocale, self).__init__(parent_wizard, previous_step, *args, **kwargs)
        self.addon_name = 'gaa'
        self.gaa_addon_dir = get_addon_dir(self.workspace, self.addon_name)
        self.gaa_project = os.path.join(self.gaa_addon_dir, 'projects', self.app_name, self.chip_type, 'gaa.x2p')
        self.va_files_dir = os.path.join(self.va_files_dir, 'gaa')
        self.needs_default_locale_selection = False
        self.can_skip_locale_selection = True

    def show(self):
        super(GaaLocale, self).show(title="Select model files to pre-load (optional)")

    def next(self):
        if self.selected_locales:
            GaaDo.actions.append(Action("Add preloaded models to filesystem", self.__add_gaa_preloaded_model_files))

    def __add_gaa_preloaded_model_files(self):
        self.add_preloaded_model_files(self.va_fs_project)
        self.__add_project_to_workspace()

    def __add_project_to_workspace(self):
        patch = (
            '<workspace>'
            '<project default="" name="gaa" path=""><dependencies><project name="va_fs"/></dependencies></project>'
            '<project name="va_fs" path="{}" default="no"/>'
            '</workspace>'
        ).format(self.va_fs_project)

        self.log.info("Adding project: {} to workspace: {}".format(self.va_fs_project, self.workspace))
        addon_utils.patchFile(patch, self.workspace)

    def get_locale_from_model_file(self, model_file):
        m = re.match(r"(.+)/(.+.ota)", model_file)
        captures = m.groups()
        model_type = captures[0]
        locale = captures[1]
        return "{}/{}".format(model_type, locale)

    def show_locale_options(self):
        if not self.vendor_package_file.get():
            return

        for model in self.__get_available_models(self.vendor_package_file.get()):
            self._locales_listbox.insert(gui.END, model)

    def __get_available_models(self, vendor_package_file):
        zipObj = zipfile.ZipFile(vendor_package_file)
        for f in zipObj.namelist():
            if f.startswith("Hotword_Models") and fnmatch.fnmatch(f, '*kalimba_*_map.txt'):
                model_map = zipObj.read(f)
                matches = re.finditer(r".*'(300.)': '((ok|x)\/[a-zA-Z_]+\.ota)'", model_map)

                for match in matches:
                    display_locale = match.group(0)
                    model_code = match.group(1)
                    model_file = match.group(2)

                    self.model_file_prefix = os.path.dirname(f)

                    self.locales_map[display_locale] = {
                        'file': model_file,
                        'code': model_code
                    } 
                    yield display_locale


class GaaDo(StepDo):
    pass


Gaa['steps'] = (GaaOptions, GaaVendorFile, GaaLocale, GaaDo)