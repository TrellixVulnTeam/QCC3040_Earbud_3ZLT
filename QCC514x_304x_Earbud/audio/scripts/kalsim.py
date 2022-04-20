#
# Copyright (c) 2020 Qualcomm Technologies International, Ltd.
# All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""KSE support functions"""

import os
import subprocess

# TODO remove KALSIM_KSE_PATH
# TODO rename KALSIM_LOGLEVEL to KALSIM_LOG_LEVEL
# TODO rename KALSIM_NAME to KALSIM_PATH


KATS_WORKSPACE = 'KATS_WORKSPACE'


def launch_kse(runpy_path, chipcode_root, **kwargs):
    """Launch kse_shell

    Args:
        runpy_path (str): runpy executable path
        chipcode_root (str): Chipcode checkout root directory
        platform (str): Chip platform
        name (str): Path to kalsim binary
        fw_path (str): Full path to firmware including kymera_xxxx_audio.elf
        loglevel (int): Log level
        timer_update (int): Timer update period
        enable_debugger (str): 'true' or 'false'
        kse_path (str): Path to KSE workspace
        bundle_path (str): Path to downloadable
        patch_path (str): Path to patches
        scripts (str): Comma separated list of scripts to invoke
        pstore_use (str): Persistent Storage use enable 'true' or 'false'
        pstore_filename (str): Persistent Storage config filename

    Returns:
        bool: Operation successful
    """
    platform = kwargs.pop('platform')
    ks_path = kwargs.pop('name')
    fw_path = kwargs.pop('fw_path')
    log_level = kwargs.pop('loglevel', 20)
    timer_update_period = kwargs.pop('timer_update', 6400)
    debug = True if kwargs.pop('enable_debugger', 'false') == 'true' else False
    bundle_path = kwargs.pop('bundle_path', '')
    patch_path = kwargs.pop('patch_path', '')
    script = kwargs.pop('scripts', '')
    pstore_use = True if kwargs.pop('pstore_use', 'false') == 'true' else False
    pstore_filename = kwargs.pop('pstore_filename', '')

    print("Kalsim mode selected (KALSIM_MODE project property is set to true)")

    # add KATS_WORKSPACE to environment
    env = os.environ.copy()
    env[KATS_WORKSPACE] = os.path.abspath(os.path.join(chipcode_root, 'audio', 'kse', 'workspace'))

    # compute kse_shell command line
    cmd = runpy_path + ' -m kse.kalsim.kalsim_shell'
    cmd += ' --platform ' + platform
    cmd += ' --ks_path ' + ks_path
    cmd += " --ks_firmware {}".format(os.path.splitext(fw_path)[0])
    cmd += ' --log_level ' + str(log_level)
    cmd += ' --ks_timer_update_period ' + str(timer_update_period)
    cmd += ' --acat_use'
    if debug:
        cmd += ' --ks_debug --ka_skip'
    if bundle_path:
        cmd += ' --acat_bundle ' + os.path.splitext(bundle_path)[0]
    if patch_path:
        cmd += ' --hydra_ftp_server_directory ' + os.path.dirname(patch_path)
    if script:
        script = script.replace(',', ' ')
        cmd += " --script \"{}\"".format(script)
    if pstore_use:
        cmd += " --pstore_use"
    if pstore_filename:
        cmd += " --pstore_filename {}".format(pstore_filename)

    cmd = 'start cmd.exe /C "%s & pause"' % (cmd)

    print("KSE launch command:{cmd} directory:{dir}".format(cmd=cmd, dir=chipcode_root))
    subprocess.Popen(cmd, env=env, shell=True, cwd=chipcode_root)
    return True


def launch_sim(working_dir, kalsim_exe, kapfile):
    """Emulates deploying to a physical target device by using a simulator instead.
    Kills any instances of Kalsim currently running and then starts a fresh one.
    Loads the new Kalsim with the supplied kap file.

    Args:
        working_dir (str): Working directory
        kalsim_exe (str): Full path to kalsim binary
        kapfile (str): Full path to kap file
    """
    killallkal()

    cmd_line = "%s %s -d --initial-pc 0x4000000" % (kalsim_exe, kapfile)
    print("KALSIM CMD %s" % (cmd_line))

    si = subprocess.STARTUPINFO()
    si.dwFlags = subprocess.STARTF_USESTDHANDLES | subprocess.STARTF_USESHOWWINDOW | subprocess.CREATE_NEW_CONSOLE
    process_h = subprocess.Popen(cmd_line, startupinfo=si, cwd=working_dir)
    print('New Kalsim PID = %s' % (process_h.pid))


def killallkal():
    """Kills any instances of Kalsim currently running"""
    print('Killing all old instances of Kalsim')
    _ = subprocess.call('taskkill /F /IM kalsim*', shell=True)
