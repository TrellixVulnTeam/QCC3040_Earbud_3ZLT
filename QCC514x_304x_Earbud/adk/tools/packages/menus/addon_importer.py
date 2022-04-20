#!/usr/bin/env python
# Copyright (c) 2018-2020 Qualcomm Technologies International, Ltd.

# Python 2 and 3
from __future__ import print_function

import os
import re
import sys
from shutil import copyfile
import argparse
try:
    import tkFileDialog
except ImportError:
    from tkinter import filedialog as tkFileDialog
try:
    import tkMessageBox
except ImportError:
    from tkinter import messagebox as tkMessageBox
try:
    import Tkinter as tkinter
except ImportError:
    import tkinter

from collections import OrderedDict

addons_dir = os.path.join('..', '..', 'addons')
templates_dir = os.path.join('tools', 'templates')
adk_tools_dir = os.path.join('adk', 'tools')
options_templates_dir = os.path.join(templates_dir, 'options')
app_templates_project_file = 'addon_ext_appsp1_x2p.xml'
app_templates_workspace_file = 'addon_ext_appsp1_x2w.xml'
ro_fs_templates_project_file = 'addon_ext_ro_fs_x2p.xml'
addon_templates_project_file = 'addon_ext_addon_x2p.xml'
addon_utils_script = os.path.join(adk_tools_dir, 'addon_utils', 'addon_utils.py')
addon_utils_makefile = os.path.join(adk_tools_dir, 'addon_utils', 'Makefile')
ro_fs_project_file = os.path.join('filesystems', 'ro_fs.x2p')
appsp1_x2p_relative_paths2root = os.path.join('..', '..', '..')
relative_path2addon_utils = os.path.join("..", "..", 'addon_utils')

addon_select_info_message = 'About to import addon: {0}.\n\nThis operation will modify {1} and {2} files.\n\nProceed?'
addon_import_success_message = 'Addon {} operation successful'
addon_import_cancel_message = 'Addon {} operation cancelled'

addon_variant_tokens = {
    "USE_SYNERGY": "/CONFIG_SYNERGY"
}

addon_dir_prerequisites = [
    addons_dir,
    'notices',
    'projects',
    'src',
    'tools',
    templates_dir,
    os.path.join(templates_dir, app_templates_project_file),
    os.path.join(templates_dir, app_templates_workspace_file)
]

adk_dir_prerequisites = [
    addon_utils_script,
    addon_utils_makefile
]

ADDON_FILE = 'adk.addon'


class UI(object):
    def __init__(self, args):
        self.theme = {
            'pady': 5,
            'padx': 5,
            'background': 'white'
        }
        self.root = None
        self.args = args

    def _configure_root_window(self):
        self.root = tkinter.Tk()
        self.root.config(**self.theme)
        self.selected_addon = tkinter.StringVar()
        self.selected_addon.trace('w', self._show_options)  # Set-up an observer

        self.selected_addons_dir = tkinter.StringVar()
        self.selected_addons_dir.trace('w', self._show_addons)  # Set-up an observer

        try:
            qmde_icon = os.path.join(self.args.kit, 'res', 'qmde_allsizes.ico')
            self.root.iconbitmap(bitmap=qmde_icon)
        except tkinter.TclError:
            pass

        # "Show" window again and lift it to top so it can get focus,
        # otherwise dialogs will end up behind other windows
        self.root.deiconify()
        self.root.lift()
        self.root.focus_force()

    @staticmethod
    def _listDirs(folder):
        """Returns a list of dirs in 'folder'
        """
        if not os.path.isdir(folder):
            return []
        return [d for d in os.listdir(folder) if os.path.isdir(os.path.join(folder, d))]

    @staticmethod
    def _listAddons(folder):
        """Returns a list of addons in the selected folder

            Addons must have a "adk.addon" file with a description
        """
        candidates = UI._listDirs(folder)
        addons = OrderedDict()
        for name in candidates:
            addon_file_path = os.path.join(folder, name, ADDON_FILE)
            if os.path.isfile(addon_file_path):
                with open(addon_file_path) as f:
                    description = f.read()
                addons[name] = description

        return addons

    def _show_options(self, *args):
        for child in self.options_frame.children.values():
            child.destroy()

        selected_addon_dir = os.path.join(self.selected_addons_dir.get(), self.selected_addon.get())
        available_options = UI.get_available_options_for_addon(selected_addon_dir)

        self.selected_options = []
        if available_options:
            lbl = tkinter.Label(self.options_frame, text="Select optional addon features:", background='white')
            lbl.pack(anchor=tkinter.W)

            for i, opt in enumerate(available_options):
                var = tkinter.StringVar()
                chk = tkinter.Checkbutton(self.options_frame, text=opt, variable=var, offvalue='', onvalue=opt, background='white')
                chk.pack(anchor=tkinter.W, expand=True)
                self.selected_options.append(var)
        else:
            lbl = tkinter.Label(self.options_frame, text="No additional features available for selected addon", background='white')
            lbl.pack(anchor=tkinter.W)

    @staticmethod
    def get_available_options_for_addon(selected_addon_dir):
        selected_options_dir = os.path.join(selected_addon_dir, options_templates_dir)
        available_options = UI._listDirs(selected_options_dir)
        return available_options

    def _show_addons(self, *args):
        for child in self.addons_frame.children.values():
            child.destroy()

        addons_directory = self.selected_addons_dir.get()
        available_addons = self._listAddons(addons_directory)

        if available_addons:
            lbl = tkinter.Label(self.addons_frame, text="Available addons:", background='white')
            lbl.pack(anchor=tkinter.W)

            for addon, description in available_addons.items():
                text = "{} - {}".format(addon, description)
                radio = tkinter.Radiobutton(self.addons_frame, text=text, variable=self.selected_addon, value=addon, background='white')
                radio.pack(anchor=tkinter.W, expand=True)

            self.selected_addon.set(available_addons.keys()[0])
        else:
            lbl = tkinter.Label(self.addons_frame, text="No addons found in selected folder", background='white')
            lbl.pack(anchor=tkinter.W)
            self.selected_addon.set("")

    def selectAddon(self, addons_directory, callback):
        self._configure_root_window()
        self.addons_directory = addons_directory

        window = self.root
        window.title("Addon Importer")

        self.addons_frame = tkinter.Frame(window, **self.theme)
        explorer_frame = tkinter.Frame(window, **self.theme)
        self.options_frame = tkinter.Frame(window, **self.theme)
        buttons_frame = tkinter.Frame(window, **self.theme)

        self.selected_addons_dir.set(addons_directory)

        browse_lbl = tkinter.Label(explorer_frame, text="Select a different addons folder location:", background='white')
        browse_lbl.pack(side=tkinter.LEFT, anchor=tkinter.W)
        browse_btn = tkinter.Button(explorer_frame, text='Browse', command=self._selectFolder)
        browse_btn.pack(side=tkinter.RIGHT, anchor=tkinter.E)

        def cancel_cmd():
            self.cancel('import')

        def next_cmd():
            window.quit()
            addon_dir = os.path.join(self.selected_addons_dir.get(), self.selected_addon.get())
            selected_options = [opt.get() for opt in self.selected_options if opt.get() != '']
            callback(addon_dir, selected_options)

        cancel_btn = tkinter.Button(buttons_frame, text='Cancel', command=cancel_cmd)
        cancel_btn.pack(anchor=tkinter.SW, side=tkinter.LEFT)

        next_btn = tkinter.Button(buttons_frame, text='Next', command=next_cmd)
        next_btn.pack(anchor=tkinter.SE, side=tkinter.RIGHT)

        self.addons_frame.pack(fill=tkinter.X)
        explorer_frame.pack(fill=tkinter.X)
        self.options_frame.pack(fill=tkinter.X)
        buttons_frame.pack(side=tkinter.BOTTOM, fill=tkinter.X)

        window.after(10, lambda: self._center(window))
        window.protocol("WM_DELETE_WINDOW", cancel_cmd)
        window.mainloop()

    def _selectFolder(self):
        """
        Show a folder selection window for the user to import
        an addon
        """
        selected = tkFileDialog.askdirectory(parent=self.root, initialdir=self.selected_addons_dir.get())
        self.selected_addons_dir.set(selected)

    def success(self, action):
        message = addon_import_success_message.format(action.lower())
        print(message)
        if self.args.addon_path is None:
            tkMessageBox.showinfo(message=message)

    def cancel(self, action):
        message = addon_import_cancel_message.format(action.lower())
        print(message)
        if self.args.addon_path is None:
            tkMessageBox.showinfo(message=message)
        sys.exit(0)

    def fail(self, msg):
        print(msg)
        if self.args.addon_path is None:
            tkMessageBox.showerror(message=msg)
        sys.exit(1)

    def confirmAction(self, addon_name):
        info = addon_select_info_message.format(addon_name, self.args.project, self.args.workspace)
        print(info)
        proceed = tkMessageBox.askokcancel('Attention!', info)
        print(proceed)
        return proceed

    @staticmethod
    def _center(win):
        width = max(win.winfo_width(), 300)
        height = win.winfo_height()
        x_cordinate = int((win.winfo_screenwidth() / 2) - (width / 2))
        y_cordinate = int((win.winfo_screenheight() / 2) - (height / 2))
        win.geometry("{}x{}+{}+{}".format(width, height, x_cordinate, y_cordinate))


class AddonUtils(object):
    def __init__(self):
        sys.path.append(os.path.join(os.path.dirname(__file__), relative_path2addon_utils))
        from addon_utils import XPathCommand
        self.addon_utils_cmd_line = XPathCommand("addon")

    def util_script_execute(self, cmds):
        try:
            from StringIO import StringIO
        except ImportError:
            from io import StringIO
        old_stdout = sys.stdout
        result = StringIO()
        sys.stdout = result
        status = self.addon_utils_cmd_line.execute(cmds)
        sys.stdout = old_stdout
        if status == -1:
            print("Script Error")
        return result.getvalue()

    def executePatchScript(self, input_file, templates_file):
        self.util_script_execute(["--input", input_file, "--merge", templates_file, "--output", input_file])

    def patchFile(self, merge_buffer, output_file):
        temp_addon_templates_file = os.path.join(os.path.dirname(output_file), 'addon_ext_temp.xml')
        with open(temp_addon_templates_file, "w+") as f:
            f.write(merge_buffer)

        self.executePatchScript(output_file, temp_addon_templates_file)
        os.remove(temp_addon_templates_file)

    def readAppProjectProperty(self, config_path_type, project_file):
        config_items = self.util_script_execute(["--input", project_file, "--configs", config_path_type])
        config_items = config_items.replace('-D', '')
        config_items = config_items.split('\n')[0]
        return config_items

    def getVariant(self, project_defs):
        variants = [addon_variant_tokens[x] for x in project_defs.split() if x in addon_variant_tokens]
        if len(variants) > 1:
            print("Error: Multiple build variants found " + str(variants))
            sys.exit(1)

        return ''.join(variants)

    def readAppProjectTypedefs(self, project_file):
        all_files = self.util_script_execute(["--input", project_file, "--file_paths"])
        #create list of typedef files under adk/
        adk_typedefs = re.findall('.*adk.*.typedef',all_files)
        seperator = '\n'
        typedef_list = seperator.join(['    <file visible="no" path="../../{}"/>'.format(f) for f in adk_typedefs])
        return typedef_list

    def readAppProjectDevkitGroup(self, project_file):
        return self.util_script_execute(["--input", project_file, "--devkitgroup"]).strip()


class Importer(object):
    def __init__(self, args):
        self.args = args
        self.ui = UI(args)
        self.addon_utils = AddonUtils()
        self.addon_dir = None

        # Set base paths (relativeness will be set after folder has been selected)
        self.app_templates_project_file = app_templates_project_file
        self.app_templates_workspace_file = app_templates_workspace_file
        self.ro_fs_templates_project_file = ro_fs_templates_project_file
        self.addon_templates_project_file = addon_templates_project_file
        self.addon_utils_script = addon_utils_script
        self.ro_fs_project_file = os.path.join(os.path.dirname(self.args.project), ro_fs_project_file)
        self.addon_x2p_file = ''
        self.addon_name = ''
        self.selected_options = []
        self.project_templates = []
        self.workspace_templates = []
        self.ro_fs_templates = []
        self.addon_templates = []

    def selectAddon(self):
        if self.args.addon_path is None:
            project_path = os.path.dirname(self.args.project)
            start_directory = os.path.join(project_path, appsp1_x2p_relative_paths2root, "addons")

            self.ui.selectAddon(start_directory, self._storeSelections)
        else:
            self.addon_dir = self.args.addon_path
            self.selected_options = self.args.addon_options

    def _storeSelections(self, selected_addon_dir, selected_options):
        self.addon_dir = selected_addon_dir
        self.selected_options = selected_options

    def importAddon(self):
        if not self.addon_dir:
            self._cancel()

        self._createImportToolFilePaths()
        self.addon_name = os.path.basename(self.addon_dir)
        self._checkPrerequisites()

        result = False
        if self._confirmAction():
            self._loadAddonIntoWorkspace()
            self._success()
            result = True
        else:
            self._cancel()
            result = False

        return result

    def _cancel(self):
        self.ui.cancel('import')

    def _success(self):
        self.ui.success('import')

    def _checkPrerequisiteExists(self, prerequisite):
        if not os.path.exists(prerequisite):
            self.ui.fail('no ' + prerequisite + ' found')

    def _translatePaths(self, app_paths):
        app_project_dir = os.path.dirname(self.args.project)

        paths = app_paths.split()
        # Get all paths from the application x2p, excluding other addon paths
        # These are relative to the app project dir so make them absolute by joining them to it
        addon_paths = [os.path.join(app_project_dir, path) for path in paths if ('addons' not in path)]

        # Modify paths relative to addon x2p
        addon_paths = [os.path.relpath(p, os.path.dirname(self.addon_x2p_file)) for p in addon_paths]

        return ' '.join(addon_paths).replace(os.path.sep, '/')

    def _patchAddonProjectFile(self, replace_pairs):
        with open(self.addon_x2p_file) as f:
            template = f.read()

        with open(self.addon_x2p_file, 'w') as f:
            f.write(template.format(**replace_pairs))

    def _patchLoadedWorkspaceFile(self, chip_type, product, template):
        with open(template, "r") as f:
            workspace_patch = f.read()

        workspace_patch = workspace_patch.replace('{CHIP_TYPE}', chip_type)
        workspace_patch = workspace_patch.replace('{PRODUCT_TYPE}', product)
        self.addon_utils.patchFile(workspace_patch, self.args.workspace)

    def _createAddonProjectFile(self, chip_type, product):
        # Create addon project file
        addon_x2p_template_file_path = os.path.join(self.addon_dir, templates_dir, self.addon_name + '.x2p')
        self.addon_x2p_file = os.path.join(self.addon_dir, 'projects', product, chip_type, self.addon_name + '.x2p')
        addon_x2p_chip_specific_dir = os.path.dirname(self.addon_x2p_file)

        if not os.path.exists(addon_x2p_chip_specific_dir):
            os.makedirs(addon_x2p_chip_specific_dir)

        copyfile(addon_x2p_template_file_path, self.addon_x2p_file)

    def _createMakefile(self):
        src_addon_makefile = os.path.join(self.addon_dir, '..', '..', addon_utils_makefile)
        dest_addon_makefile = os.path.join(self.addon_dir, 'Makefile')
        copyfile(src_addon_makefile, dest_addon_makefile)

    def _loadAddonIntoWorkspace(self):
        # Create makefile
        self._createMakefile()
        # Need to identify the correct project file by reading the chip type from the loaded application x2p
        chip_type = self.addon_utils.readAppProjectProperty("CHIP_TYPE", self.args.project)

        # Identify product type (e.g. earbud, headset etc.)
        product = os.path.splitext(os.path.basename(self.args.project))[0]

        defs = self.addon_utils.readAppProjectProperty("DEFS", self.args.project)
        
        # Create addon project file
        self._createAddonProjectFile(chip_type, product)

        # typedef paths        
        app_typedef_files = self.addon_utils.readAppProjectTypedefs(self.args.project)
        app_incpaths = self.addon_utils.readAppProjectProperty("INCPATHS", self.args.project)
        app_libpaths = self.addon_utils.readAppProjectProperty("LIBPATHS", self.args.project)

        replace_pairs = {
            "VARIANT": self.addon_utils.getVariant(defs),
            "TYPEDEF_FILES":app_typedef_files,
            "DEFS": defs,
            "INCPATHS": self._translatePaths(app_incpaths),
            "LIBPATHS": self._translatePaths(app_libpaths),
            "CHIP_TYPE": chip_type,
            "CHIP_NAME": self.addon_utils.readAppProjectProperty("CHIP_NAME", self.args.project),
            "devkitGroup": self.addon_utils.readAppProjectDevkitGroup(self.args.project)
        }

        self._patchAddonProjectFile(replace_pairs)
        
        for template in self.workspace_templates:
            self._patchLoadedWorkspaceFile(chip_type, product, template)

        for template in self.project_templates:
            self.addon_utils.executePatchScript(self.args.project, template)

        # Merge ro_fs project templates
        for template in self.ro_fs_templates:
            self.addon_utils.executePatchScript(self.ro_fs_project_file, template)

        for template in self.addon_templates:
            self.addon_utils.executePatchScript(self.addon_x2p_file, template)

    def _checkPrerequisites(self):
        # Addon directory prerequisites
        for prereq in addon_dir_prerequisites:
            self._checkPrerequisiteExists(os.path.join(self.addon_dir, prereq))

        # Adk tools directory prerequisites
        for prereq in adk_dir_prerequisites:
            self._checkPrerequisiteExists(os.path.join(self.addon_dir, '..', '..', prereq))

    def _createImportToolFilePaths(self):
        self.addon_utils_script = os.path.join(self.addon_dir, '../..', self.addon_utils_script)

        self.project_templates = self._getTemplates(self.app_templates_project_file)
        self.workspace_templates = self._getTemplates(self.app_templates_workspace_file)
        self.ro_fs_templates = self._getTemplates(self.ro_fs_templates_project_file)
        self.addon_templates = self._getTemplates(self.addon_templates_project_file)

    def _getTemplates(self, template_type):
        """Gets the all the addon templates of a specific type

        E.g.:
            self.getTemplates(self.app_templates_project_file)

            will return all templates for the application project file.
            The main addon template is always returned first, any template from
            additional options is returned after.
            Optional templates are in random order
        """
        templates = []
        main_template = os.path.join(self.addon_dir, templates_dir, template_type)

        if os.path.isfile(main_template):
            templates.append(main_template)

        templates.extend(self._getOptionalTemplates(template_type))
        return templates

    def _getOptionalTemplates(self, template_type):
        options_path = os.path.join(self.addon_dir, options_templates_dir)
        potential_templates = [os.path.join(options_path, opt, template_type) for opt in self.selected_options]
        return [t for t in potential_templates if os.path.isfile(t)]

    def _confirmAction(self):
        if self.args.addon_path is None:
            confirm = self.ui.confirmAction(self.addon_name)
        else:
            confirm = True
        return confirm


def parse_args():
    """ parse the command line arguments """
    parser = argparse.ArgumentParser(description='Import an Addon')

    parser.add_argument('-w', '--workspace',
                        required=True,
                        help='Specifies the workspace to use')

    parser.add_argument('-p', '--project',
                        required=True,
                        help='Specifies the project file to use')

    parser.add_argument('-k', '--kit',
                        required=True,
                        help='Specifies the kit to use')

    parser.add_argument('-ap', '--addon_path',
                        required=False,
                        help='Specifies the addon path (for non-GUI driven CI)')

    parser.add_argument('-o', '--addon_options',
                        required=False,
                        default=[],
                        nargs="*",
                        help='Specifies addon optional features (for non-GUI driven CI).\n'
                             'If this argument is used, it MUST be use in combination with --addon_path')

    args = parser.parse_args()

    if args.addon_options and args.addon_path is None:
        print("ERROR: -o/--addon_options argument requires the -ap/--addon_path argument as well")
        sys.exit(1)

    return args


if __name__ == '__main__':
    args = parse_args()
    importer = Importer(args)
    importer.selectAddon()
    importer.importAddon()
