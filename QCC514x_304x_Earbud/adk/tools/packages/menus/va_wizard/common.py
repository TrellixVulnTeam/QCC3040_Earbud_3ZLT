# Copyright (c) 2020 - 2021 Qualcomm Technologies International, Ltd.

# Python 2 and 3
from __future__ import print_function
import argparse
import os
import zipfile
import subprocess
import shutil
import glob

import capsign

from menus.addon_importer import Importer, AddonUtils

from menus.wizard import (
    Step,
    gui,
    WizardError
)


def get_addons_dir(workspace):
    app_name, _ = os.path.splitext(os.path.basename(workspace))
    head, tail = os.path.split(workspace)
    while tail != app_name:
        head, tail = os.path.split(head)
    addons_dir = os.path.join(head, 'addons')
    return addons_dir


def get_addon_dir(workspace, addon_name):
    return os.path.join(get_addons_dir(workspace), addon_name)


def installed_addons(workspace):
    addons_dir = get_addons_dir(workspace)
    return (d for d in os.listdir(addons_dir) if os.path.isdir(os.path.join(addons_dir, d)))


addon_utils = AddonUtils()


class VaStep(Step):
    def __init__(self, *args, **kwargs):
        super(VaStep, self).__init__(*args, **kwargs)
        self.workspace = self.cli_args.workspace
        self.app_name = os.path.splitext(os.path.basename(self.workspace))[0]
        self.app_project = os.path.splitext(os.path.normpath(self.workspace))[0] + '.x2p'
        self.ro_fs_project = os.path.join(os.path.dirname(self.workspace), 'filesystems', 'ro_fs.x2p')
        try:
            self.chip_type = addon_utils.readAppProjectProperty("CHIP_TYPE", self.app_project)
        except Exception as e:
            gui.tkMessageBox.showerror(title="Error getting CHIP_TYPE", message=str(e))

    def add_defines_to_project(self, defines, project):
        patch = (
            '<project><configurations><configuration name="" options="">'
            '<property name="DEFS">{defines}</property>'
            '</configuration></configurations></project>'
        ).format(defines=" ".join(defines))

        self.log.info("Adding defines to project:\nProject path:{}\nDefines:\n{}".format(project, "\n".join(defines)))

        addon_utils.patchFile(patch, project)


class VaStepOptions(VaStep):
    def __init__(self, *args, **kwargs):
        super(VaStepOptions, self).__init__(*args, **kwargs)
        self.selected_options = dict()

    def show_options(self, available_options):
        gui.Label(self, text="Select provider options:").pack(anchor=gui.W)
        frame = gui.Frame(self, relief=gui.GROOVE, borderwidth=2)
        frame.pack(**gui.PACK_DEFAULTS)
        if available_options:
            for opt in available_options:
                var = self.selected_options.get(opt, gui.StringVar())
                chk = gui.Checkbutton(frame, text=opt, variable=var, offvalue='', onvalue=opt)
                chk.pack(anchor=gui.W, side=gui.LEFT)
                self.selected_options[opt] = var
        else:
            lbl = gui.Label(self, text="No options available")
            lbl.pack(anchor=gui.W)

    @property
    def wuw_enabled(self):
        try:
            return bool(self.selected_options['include_wuw'].get())
        except KeyError:
            return False

    def import_addon(self, addon_name, addon_options):
        args = argparse.Namespace(
            workspace=self.workspace,
            project=self.app_project,
            kit=self.cli_args.kit,
            addon_path=get_addon_dir(self.workspace, addon_name),
            addon_options=[o.get() for o in addon_options.values()],
        )
        self.log.info("Importing addon: {}".format(addon_name))
        self.log.info("Addon options: {!s}".format(", ".join(opt for opt in addon_options)))
        addon_importer = Importer(args)
        addon_importer.selectAddon()
        addon_import_successful = addon_importer.importAddon()
        if addon_import_successful:
            self.log.info("Import succesful for addon: {}".format(addon_name))
        else:
            self.log.error("Import failed for addon: {}".format(addon_name))

    def filter_steps(self, steps, klass=object, condition=True):
        """ Filter steps of a specific class based on a condition
        """
        for s in reversed(steps):
            if issubclass(s, klass):
                if condition:
                    if s not in self.wizard.steps:
                        self.wizard.steps.insert(1, s)
                elif s in self.wizard.steps:
                    self.wizard.steps.remove(s)


class VaStepVendorFile(VaStep):
    def __init__(self, parent_wizard, previous_step, *args, **kwargs):
        super(VaStepVendorFile, self).__init__(parent_wizard, previous_step, *args, **kwargs)
        self.python2 = os.path.join(self.cli_args.kit, 'tools', 'python27', 'python')
        self.python3 = os.path.join(self.cli_args.kit, 'tools', 'pyenv37', 'Scripts', 'python')
        self.audio_dir = os.path.join(os.path.dirname(self.workspace), '..', '..', '..', 'audio', self.chip_type)
        self.kymera_dir = os.path.join(self.audio_dir, 'kalimba', 'kymera')
        self.capability_project = os.path.join(self.kymera_dir, 'tools', 'KCSMaker', self.capability_project_name)

        self.vendor_package_file = gui.StringVar()

        self.selected_options = previous_step.selected_options
        self.wuw_enabled = previous_step.wuw_enabled

    @property
    def capability_project_name(self):
        raise NotImplementedError("VA providers must implement this method")

    def show(self, *args, **kwargs):
        super(VaStepVendorFile, self).show(*args, **kwargs)
        self.wizard.next_requires = [self.vendor_package_file]

        self.show_options()

    def show_options(self):
        def __select_file():
            if self.vendor_package_file.get():
                initialdir = os.path.dirname(self.vendor_package_file.get())
            else:
                initialdir = os.path.dirname(self.workspace)

            selected = gui.tkFileDialog.askopenfilename(
                initialdir=initialdir,
                title="Select vendor package file",
                filetypes=(("zip files", "*.zip"),))
            if selected:
                self.vendor_package_file.set(os.path.normpath(selected))
                truncated_path.set(self.vendor_package_file.get()[0:30]+"...")

        options_frame = gui.Frame(self, relief=gui.GROOVE, borderwidth=2)
        options_frame.pack(**gui.PACK_DEFAULTS)
        btn_frame = gui.Frame(options_frame)
        btn_frame.pack(**gui.PACK_DEFAULTS)
        gui.Label(btn_frame, text="Select vendor package file").pack(side=gui.LEFT, anchor=gui.W)
        self.partner_pkg_btn = gui.Button(btn_frame, text="Browse...", command=__select_file)
        self.partner_pkg_btn.pack(side=gui.LEFT, anchor=gui.W)

        gui.Label(options_frame, text="Path:").pack(side=gui.LEFT)
        truncated_path = gui.StringVar()
        lbl = gui.Label(options_frame, relief=gui.GROOVE, borderwidth=1, textvariable=truncated_path)
        lbl.pack(side=gui.LEFT, anchor=gui.W, fill=gui.X, expand=True)

    def extract_vendor_package(self):
        prerun_script = os.path.join(self.kymera_dir, 'build', addon_utils.readAppProjectProperty("PRERUN_SCRIPT", self.capability_project))
        prerun_script_args = addon_utils.readAppProjectProperty("PRERUN_PARAMS", self.capability_project)
        prerun_script_args = prerun_script_args.split()
        audio_bin_dir = None
        vendor_package_filename = os.path.basename(self.vendor_package_file.get())
        for i, arg in enumerate(prerun_script_args):
            if (arg == '-a') or (arg == '--audio_bin'):
                audio_bin_dir = os.path.normpath(os.path.join(os.path.dirname(self.capability_project), prerun_script_args[i + 1]))
            elif (arg == '-f') or (arg == '--file_name'):
                prerun_script_args[i + 1] = vendor_package_filename
            elif (arg == '-b') or (arg == '--build_config'):
                prerun_script_args[i + 1] = 'streplus_rom_release'

        if audio_bin_dir is None:
            raise ValueError("Audio bin dir is invalid: audio_bin_dir={}".format(audio_bin_dir))

        extracted_vendor_file = os.path.join(audio_bin_dir, vendor_package_filename)

        extracted_dir = os.path.splitext(extracted_vendor_file)[0]
        if os.path.isdir(extracted_dir):
            shutil.rmtree(extracted_dir)

        if self.vendor_package_file.get() != extracted_vendor_file:
            shutil.copy(self.vendor_package_file.get(), extracted_vendor_file)

        cmd = [self.python3, prerun_script] + prerun_script_args
        self.log.info("Running Audio extractor script: {!s}".format(cmd))
        out = subprocess.check_output(cmd, cwd=os.path.dirname(self.capability_project))
        self.log.info(out)

    def build_capability(self):
        ubuild_path = os.path.join(self.cli_args.kit, 'tools', 'ubuild', 'ubuild.py')
        cmd = [self.python2, ubuild_path,
               '-k', self.cli_args.kit,
               '-w', self.workspace,
               '-p', self.capability_project,
               '-b', 'build', '-c', 'debug', '--verbose', '--build_system', 'make', '--special', '"flash=nvscmd"']
        out = subprocess.check_output(cmd, cwd=os.path.dirname(self.capability_project))
        self.log.info(out)

        if self._needs_edkcs():
            self._sign_capability()

    def _needs_edkcs(self):
        return 'QCC30' in os.path.basename(os.path.dirname(self.workspace))

    def _sign_capability(self):
        output_elf_relpath = addon_utils.readAppProjectProperty("OUTPUT", self.capability_project)
        output_bundle = os.path.abspath(os.path.dirname(os.path.join(os.path.dirname(self.capability_project), output_elf_relpath)))
        prebuilt_dir = os.path.abspath(glob.glob(os.path.join(self.audio_dir, 'kalimba_ROM_*', 'kymera', 'prebuilt_dkcs', '*_rom_release'))[0])
        result = capsign.sign(output_bundle, prebuilt_dir, self.cli_args.kit)
        if result.is_error:
            raise WizardError("Error signing dkcs file\n{}".format(result.err))

    def add_files_to_ro_project(self):
        elf_file = addon_utils.readAppProjectProperty("OUTPUT", self.capability_project)
        
        cap_file_ext = '.edkcs' if self._needs_edkcs() else ".dkcs"
        cap_file = os.path.splitext(os.path.normpath(os.path.join(os.path.dirname(self.capability_project), elf_file)))[0] + cap_file_ext
        
        patch = '<project><file path="{}"/></project>'.format(os.path.relpath(cap_file, os.path.dirname(self.ro_fs_project)))

        addon_utils.patchFile(patch, self.ro_fs_project)


class VaStepLocale(VaStep):
    def __init__(self, parent_wizard, previous_step, *args, **kwargs):
        super(VaStepLocale, self).__init__(parent_wizard, previous_step, *args, **kwargs)

        self.va_fs_project = os.path.join(os.path.dirname(self.workspace), 'filesystems', 'va_fs.x2p')
        self.va_files_dir = os.path.join(os.path.dirname(self.cli_args.workspace), 'filesystems', 'va')

        self.selected_options = previous_step.selected_options
        if previous_step.wuw_enabled:
            self.vendor_package_file = previous_step.vendor_package_file
        else:
            self.vendor_package_file = ''

        self.model_file_prefix = None
        self.locales_map = dict()
        self.default_model_selected = False
        self.needs_default_locale_selection = True
        self.can_skip_locale_selection = False

    @property
    def model_files_dir(self):
        return os.path.join(self.va_files_dir, 'models')

    @property
    def prompts_dir(self):
        return os.path.join(self.va_files_dir, 'prompts')

    def get_locale_from_model_file(self, model_file):
        raise NotImplementedError("VA providers must implement this method")

    def show_locale_options(self):
        raise NotImplementedError("VA providers must implement this method")

    def show(self, *args, **kwargs):
        super(VaStepLocale, self).show(*args, **kwargs)
        self.show_options()
        self.show_locale_options()

    def show_options(self):
        self.options_frame = gui.Frame(self, relief=gui.GROOVE, borderwidth=2)
        self.options_frame.pack(side=gui.TOP, anchor=gui.SW, fill=gui.BOTH, expand=True)

        self._show_locale_options()

    def add_preloaded_model_files(self, project_file):
        model_files = self._extract_model_files()
        self.add_files_to_project(project_file, model_files)

    def add_files_to_project(self, project_file, files_list):
        template = '<file path="{}"/>'
        patch = [template.format(os.path.relpath(f, os.path.dirname(project_file))) for f in files_list]

        addon_utils.patchFile('<project>{}</project>'.format('\n'.join(patch)), project_file)

    def _extract_model_files(self):
        zipObj = zipfile.ZipFile(self.vendor_package_file.get())

        if not os.path.isdir(self.model_files_dir):
            os.makedirs(self.model_files_dir)

        selected_model_files = []
        for locale in self._available_locales.get():
            locale_file = self.locales_map[locale]['file']
            zip_path = "{}/{}".format(self.model_file_prefix, locale_file)
            extracted_path = os.path.normpath(os.path.join(self.model_files_dir, self.locales_map[locale]['code']))

            if not os.path.isdir(os.path.dirname(extracted_path)):
                os.mkdir(os.path.dirname(extracted_path))

            with zipObj.open(zip_path) as src, open(extracted_path, 'wb') as dst:
                shutil.copyfileobj(src, dst)

            if locale in self.selected_locales:
                selected_model_files.append(extracted_path)

        return selected_model_files

    def update_default_locale(self):
        menu = self.default_lang_dropdown["menu"]
        menu.delete(0, gui.END)
        for model in self.selected_locales:
            menu.add_command(label=model, command=lambda x=model: self.default_model.set(x))

        if not self.default_model.get() or (self.default_model.get() not in self.selected_locales):
            menu.invoke(0)

    def import_addon(self, addon_name, addon_options):
        args = argparse.Namespace(
            workspace=self.cli_args.workspace,
            project=self.app_project,
            kit=self.cli_args.kit,
            addon_path=get_addon_dir(self.cli_args.workspace, addon_name),
            addon_options=[o.get() for o in addon_options.values()],
        )
        self.log.info("Importing addon: {}".format(addon_name))
        self.log.info("Addon options: {!s}".format(", ".join(opt for opt in addon_options)))
        addon_importer = Importer(args)
        addon_importer.selectAddon()
        addon_import_successful = addon_importer.importAddon()
        if addon_import_successful:
            self.log.info("Import succesful for addon: {}".format(addon_name))
        else:
            self.log.error("Import failed for addon: {}".format(addon_name))

    def _show_locale_options(self):
        self.selected_locales = []

        def __get_selected_languages(event):
            self.selected_locales = [event.widget.get(i) for i in event.widget.curselection()]
            if self.needs_default_locale_selection:
                self.update_default_locale()

            if not self.selected_locales and self.can_skip_locale_selection:
                self.wizard.next_button['text'] = self.wizard.SKIP
            else:
                self.wizard.next_button['text'] = self.wizard.NEXT

        # gui.Label(self.options_frame, text="Select models to pre-load").pack(**gui.PACK_DEFAULTS)
        if self.can_skip_locale_selection:
            self.wizard.next_button['text'] = self.wizard.SKIP

        yscrollbar = gui.Scrollbar(self.options_frame)
        yscrollbar.pack(side=gui.RIGHT, fill=gui.Y)
        self._available_locales = gui.Variable()
        self._locales_listbox = gui.Listbox(self.options_frame, listvariable=self._available_locales, selectmode=gui.MULTIPLE, yscrollcommand=yscrollbar.set)
        self._locales_listbox.bind("<<ListboxSelect>>", __get_selected_languages)
        self._locales_listbox.pack(side=gui.TOP, anchor=gui.NW, fill=gui.BOTH, expand=True)

        yscrollbar.config(command=self._locales_listbox.yview)

        self.default_model = gui.StringVar()
        if self.needs_default_locale_selection:
            default_frame = gui.Frame(self)
            default_frame.pack(side=gui.TOP, fill=gui.BOTH, expand=True)
            default_lbl = gui.Label(default_frame, text="Default locale:")
            default_lbl.pack(side=gui.LEFT, anchor=gui.E, fill=gui.BOTH, expand=False)
            self.default_model.trace('w', self.__default_model_selected)
            self.default_lang_dropdown = gui.OptionMenu(default_frame, self.default_model, '')
            self.default_lang_dropdown.pack(side=gui.RIGHT, anchor=gui.W, fill=gui.BOTH, expand=True)

    def __default_model_selected(self, *args):
        self.default_model_selected = bool(self.default_model.get())
