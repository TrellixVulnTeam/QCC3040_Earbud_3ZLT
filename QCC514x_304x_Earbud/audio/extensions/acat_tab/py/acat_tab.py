############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2016 - 2018 Qualcomm Technologies, Inc. and/or its
# subsidiaries. All rights reserved.
#
############################################################################
"""ACAT entry point for an ADK.

Forms a wrapper around ACAT allowing it to be started by
passing a QMDE workspace where the paths to the Kymera ELF
image is obtained.
This is primarily intended for use in the QMDE tabs.

Usage:
> python acat_tab.py -p <path_to_acat> -d <debug_interface> \
      [-w <workspace_file>]
where
<path_to_acat> is full path to ACAT.
<debug_interface> trb/scar/0 or equivalent.
<workspace_file> Workspace file for current workspace.
"""
from __future__ import print_function

import argparse
import copy
import importlib
import os
import re
import subprocess
import sys
import warnings
import xml.etree.cElementTree as ET
from xml.etree import ElementTree

import ACAT


# -----------------------------
# ---- Start of Aliases library
# -----------------------------
class AliasWarning(RuntimeWarning):
    """Base class for warnings in the Aliases processing."""
    pass


class AliasError(RuntimeError):
    """Base class for errors in the Aliases processing."""
    pass


class AliasDuplicateError(AliasError):
    """Duplicate Error.
    
    Raised when a duplicate definition has been found for the same alias.
    """
    def __init__(self, alias, values=None):
        msg = "Duplicated alias: {}".format(alias)
        if values:
            msg += ", values={}, {}".format(*values)
        super(AliasDuplicateError, self).__init__(msg)


class AliasUndefinedError(AliasError):
    """Undefined Error.
    
    Raised if an alias without a definition is found when processing a file.
    """
    def __init__(self, alias):
        msg = "Undefined alias: {}".format(alias)
        super(AliasUndefinedError, self).__init__(msg)


class AliasCyclicInclusionError(AliasError):
    """Cyclic Inclusion Error.

    Alias files can't include themselves at any level. If a cyclic inclusion is found
    this exception is raised.
    """
    def __init__(self, filename):
        msg = "Cyclic inclusion of alias file: {}".format(filename)
        super(AliasCyclicInclusionError, self).__init__(msg)


STOP = "://"
SDK_PREFIX = 'sdk' + STOP
WSROOT_PREFIX = 'wsroot' + STOP
PATTERN = re.compile(r'(\w+' + STOP + r')')
ALIAS_XPATH = ".//alias"
INCLUDE_XPATH = ".//include"
WORKSPACE_EXTENSION = '.x2w'
WSROOT_EXTENSION = '.wsroot'


class Aliases(object):
    """Handle aliases from MDE projects and workspaces."""
    def __init__(self, xml_filename, sdk=None, wsroot=None):
        """Reads an x2w or x2p file, extracts the aliases definitions and
        constructs an Aliases object.

        Args:
            xml_filename (string): Path to the x2w or x2p file to load
            sdk (string): Path to the SDK installation (default: {os.environ['ADK_ROOT']})
            wsroot (string): Path to the wsroot:// location (default: {None})
        """
        self.xml_filename = xml_filename
        self.processed_files = set([])
        self.alias_dict = {
            SDK_PREFIX: self._get_sdk(sdk),
            WSROOT_PREFIX: self._get_wsroot(wsroot)
        }
        self._update_aliases(xml_filename)
        self._check_undefined_aliases()
        self._normalize_aliases()

    def expand(self, path):
        """Expand aliases in the specified path.

        Args:
            path (str): Path with aliases.

        Returns:
            str: Path with aliases expanded and normalized.
        """
        return os.path.normpath(self.__expand(path))

    def __expand(self, path):
        try:
            found_aliases = PATTERN.search(path).groups()
        except AttributeError:
            return path
        for alias in found_aliases:
            expanded_alias = self.alias_dict.get(alias, alias)
            expanded_alias = expanded_alias if expanded_alias.endswith(('\\', '/')) else expanded_alias + os.path.sep
            path = path.replace(alias, expanded_alias)
        return path

    def expand_all(self, paths_with_aliases):
        """ Expand aliases in the specified paths.

        Args:
            paths_with_aliases: Object containing paths, this can be dict, list or tuple.

        Returns:
            Returns a new object of the same type as the input with aliases
            expanded in each path and normalized.
        """
        return {key: os.path.normpath(value) for (key, value) in self.__expand_all(paths_with_aliases).items()}

    def __expand_all(self, paths_with_aliases):
        return {key: self.__expand(paths_with_aliases[key]) for key in self._seq_iter(paths_with_aliases)}

    @property
    def wsroot(self):
        return self.alias_dict[WSROOT_PREFIX]

    @property
    def sdk(self):
        return self.alias_dict[SDK_PREFIX]

    def _seq_iter(self, obj):
        return obj if isinstance(obj, dict) else range(len(obj))

    def _get_sdk(self, sdk):
        if not sdk:
            sdk = os.environ.get('ADK_ROOT', '')
            if not sdk:
                sdk = os.path.abspath('.')

        return sdk

    def _get_wsroot(self, wsroot):
        if not wsroot:
            if self._is_workspace():
                try:
                    wsroot_file = os.path.splitext(self.xml_filename)[0] + WSROOT_EXTENSION
                    self._add_to_processed_files(wsroot_file)
                    wsroot_dict = self._get_direct_children(ET.parse(wsroot_file))
                    wsroot = wsroot_dict[WSROOT_PREFIX]
                    wsroot = self._make_path_absolute(wsroot, wsroot_file)
                    if len(wsroot_dict) > 1:
                        warnings.warn("Additional entries found in {} ignored"
                                      .format(wsroot_file), AliasWarning)
                except (IOError, KeyError):
                    wsroot = os.path.dirname(self.xml_filename)
            else:
                raise AliasUndefinedError(WSROOT_PREFIX)
        return os.path.normpath(wsroot)

    def _normalize_aliases(self):
        for key, value in self.alias_dict.items():
            self.alias_dict[key] = os.path.normpath(value)

    def _make_path_absolute(self, path, anchor_file):
        if not os.path.isabs(path):
            return os.path.join(os.path.dirname(anchor_file), path)
        return path

    def _is_workspace(self):
        return self.xml_filename.endswith(WORKSPACE_EXTENSION)

    def _update_aliases(self, xml_filename):
        self._add_to_processed_files(xml_filename)
        xml_tree = ET.parse(xml_filename)

        aliases = self._get_direct_children(xml_tree)
        self._check_all_duplicates(aliases, self.alias_dict)
        self.alias_dict.update(aliases)
        self.alias_dict = self.__expand_all(self.alias_dict)

        for i in xml_tree.findall(INCLUDE_XPATH):
            include_path = self.__expand(i.attrib['src'])
            include_path = self._make_path_absolute(include_path, xml_filename)
            self._update_aliases(include_path)

    def _add_to_processed_files(self, filename):
        if filename in self.processed_files:
            raise AliasCyclicInclusionError(filename)
        self.processed_files.add(filename)

    def _get_direct_children(self, aliases_root):
        aliases = {}
        for a in aliases_root.findall(ALIAS_XPATH):
            key = a.attrib['name'] + STOP
            value = a.text
            self._check_duplicate(key, value, aliases)
            aliases[key] = value
        return aliases

    def _check_all_duplicates(self, aliases_a, aliases_b):
        for key in aliases_a:
            if key in aliases_b:
                values = (aliases_b[key], aliases_a[key])
                raise AliasDuplicateError(key, values=values)

    def _check_duplicate(self, key, value, aliases):
        if key in aliases:
            raise AliasDuplicateError(key, values=(aliases[key], value))

    def _check_undefined_aliases(self):
        for expanded in self.alias_dict.values():
            m = PATTERN.search(expanded)
            if m:
                raise AliasUndefinedError(m.group(1))

    def __iter__(self):
        return iter(self.alias_dict)

    def __getitem__(self, key):
        return self.alias_dict[key]

    def __len__(self):
        return len(self.alias_dict)

# ---------------------------
# ---- End of Aliases library
# ---------------------------


class ACATPackages(object):
    """Handle dependencies for ACAT."""

    REQUIRED_PACKAGES = {
        'graphviz': 'graphviz==0.8.4'
    }

    def check_only(self):
        """Checks the installation of required packages.

        It prints a simple complaint for each missing package.
        """
        for pkg, pkg_version in self.REQUIRED_PACKAGES.items():
            if self._is_installed(pkg) is not True:
                print(
                    "Package {} is missing. Not all the ACAT features "
                    "will work!".format(pkg_version)
                )

    @staticmethod
    def _is_installed(package):
        """Checks whether the given package is already installed.

        Args:
            package (str): The package name that `import` use.

        Returns:
            Bool: True if the package is already installed, False otherwise.
        """
        try:
            importlib.import_module(package)
            return True
        except ImportError:
            return False


def get_audio_elf_path_from_workspace(workspace_file, isSimTarget):
    """Get the path to the audio elf file from the workspace.

    Given a workspace file (x2w) will return the Path to the kymera_audio
    project contained within.
    """
    aliases = Aliases(workspace_file)

    workspace_tree = ElementTree.parse(workspace_file)
    workspace_root = workspace_tree.getroot()
    workspace_path = os.path.dirname(workspace_file)
    project_dict = {}
    workspace_name = os.path.basename(workspace_file)
    workspace_name, dont_care = os.path.splitext(workspace_name)
    main_proj_name = workspace_name + ".x2p"
    kymera_image_path = None
    dkcs_path = None
    bundle_image_list = []
    projects_visited = []

    for project in workspace_root.iter('project'):
        project_path = project.get('path')

        # Skip project items without a path attribute.
        if project_path is None:
            continue

        project_path = aliases.expand(project_path)

        if project_path not in projects_visited:
            projects_visited.append(project_path)
            project_path = os.path.abspath(os.path.join(workspace_path, project_path))
            if "kymera_audio.x2p" in project_path:
                is_audio_proj, kymera_image_path = get_output_value_from_audio_proj(project_path, isSimTarget, False)
                if kymera_image_path:
                    kymera_project_path = os.path.dirname(project_path)
                    kymera_image_path = os.path.join(kymera_project_path, kymera_image_path)
                    kymera_image_path = os.path.abspath(kymera_image_path)
                    dkcs_path = get_dkcs_path_value_from_audio_proj(project_path)
                    if dkcs_path:
                        dkcs_path = os.path.abspath(os.path.join(kymera_project_path, dkcs_path))
                        if os.path.exists(dkcs_path):
                            for bundle_name in os.listdir(dkcs_path):
                                bundle_name = os.path.join(os.path.abspath(dkcs_path), bundle_name)
                                if os.path.splitext(bundle_name)[1] == ".elf":
                                    bundle_image_list.append(bundle_name)
            else:
                is_audio_proj, bundle_image_path = get_output_value_from_audio_proj(project_path, isSimTarget, True)
                if is_audio_proj:
                    kymera_project_path = os.path.dirname(project_path)
                    bundle_image_path = os.path.join(kymera_project_path, bundle_image_path)
                    bundle_image_path = os.path.abspath(bundle_image_path)
                    bundle_image_list.append(bundle_image_path)
    return kymera_image_path, bundle_image_list


def get_output_value_from_audio_proj(project_file, isSimTarget, isBundle):
    """Get the output value form the audio project.

    Given an audio project file (x2p) will return the value of the property
    'OUTPUT' if that field exists.
    """
    project_tree = ElementTree.parse(project_file)
    project_root = project_tree.getroot()
    subsys = ""
    for configuration in project_root.iter('configuration'):
        if (configuration.get('name') == "kse" and isSimTarget) or (configuration.get('name') != "kse" and not isSimTarget):
            for property in configuration.iter('property'):
                if 'SUBSYSTEM_NAME' in property.get('name'):
                    subsys = property.text
            if subsys != "audio":
                return False, None
            for property in configuration.iter('property'):
                if isSimTarget and not isBundle:
                    if 'KALSIM_FIRMWARE' in property.get('name'):
                        return True, property.text
                else:
                    if 'OUTPUT' in property.get('name'):
                        return True, property.text
    return False, None

def get_dkcs_path_value_from_audio_proj(project_file):
    """Get the DKCS path from the audio project.

    Given an audio project file (x2p) will return the value of the property
    'DKCS_PATH' if that field exists.
    """
    project_tree = ElementTree.parse(project_file)
    project_root = project_tree.getroot()
    for configuration in project_root.iter('configuration'):
        for property in configuration.iter('property'):
            if 'DKCS_PATH' in property.get('name'):
                return property.text
    return None


if __name__ == "__main__":
    parser = argparse.ArgumentParser()

    parser.add_argument(
        "-d",
        "--debug_interface",
        help="Debug interface eg. trb/scar/0",
        dest="debug",
        default="trb/scar/0"
    )

    parser.add_argument(
        "-w",
        "--workspace",
        help="The workspace file that is being debugged",
        dest="workspace",
        required=True,
    )

    args = parser.parse_args()
    env = os.environ.copy()

    print('ACAT {}'.format(ACAT.__version__))

    # Check ACAT dependencies. If anything missing, a suitable message
    # will appear on the screen.
    ACATPackages().check_only()

    if args.debug.split("/")[-2] == "sim":
        isSimTarget = True
    else:
        isSimTarget = False

    try:
        audio_path, bundles_list = get_audio_elf_path_from_workspace(args.workspace, isSimTarget)
    except KeyError:
        audio_path = None

    if audio_path is not None:
        # Setup the calling string for ACAT
        cmd_parameters = ['-i']

        print("Kalaccess transport string is: {}".format(args.debug))

        cmd_parameters.extend(['-s', args.debug])

        # Kalsim doesn't support dual-core
        if not isSimTarget:
            # Dual-core
            cmd_parameters.append('-d')

        # Do not fail to launch if chip is not enabled
        cmd_parameters.append('-q')

        cmd_parameters.extend(['-b', os.path.splitext(audio_path)[0]])

        for bundle in bundles_list:
            if os.path.isfile(os.path.splitext(bundle)[0] + ".elf"):
                cmd_parameters.extend(['-j', os.path.splitext(bundle)[0]])

        print(' '.join(cmd_parameters))
        ACAT.main(cmd_parameters)

