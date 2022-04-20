# Copyright (c) 2020 Qualcomm Technologies International, Ltd.

import argparse
import json
import glob
import os
import shutil
import sys
from shutil import copyfile
import zipfile
from zipfile import ZipFile

default_pyron_lite_api_version = 'PRL1000'
dest_lib_filename = 'libpryon_lite'
dest_libs_basename = 'libs'
models_dest_basename = models_src_basename = 'models'
localeToModels_filename = 'localeToModels.json'

def parse_args():
    """ parse the command line arguments """
    parser = argparse.ArgumentParser(description='Extract AMA files')

    parser.add_argument('-z', '--zip_file',
                        required=True,
                        help='Complete path to the AMA zip package (REQUIRED)')

    parser.add_argument('-d', '--dest',
                        required=True,
                        help='Destination directory where files will be saved (REQUIRED)')

    parser.add_argument('-c', '--chip',
                        required=True,
                        help='Target chip variant. Valid options are qcc515x, qcc516x, qcc514x, qcc512x-rom-v21, qcc512x-rom-v02 (REQUIRED)')

    parser.add_argument('-av', '--api_version',
                        required=False,
                        help='API Version of Pyron-Lite. Default is "PRL1000" (OPTIONAL)')
    args = parser.parse_args()

    if(args.api_version == None):
        args.api_version = default_pyron_lite_api_version

    return args

def export_lib_files(libs_path_dest, pyron_lite_libs_path_src):
    """ export library files """
    if(os.path.exists(libs_path_dest) == False):
        os.makedirs(libs_path_dest)
    pyron_lite_libs_path_dest = {"reloc": os.path.join(libs_path_dest, dest_lib_filename + os.path.splitext(pyron_lite_libs_path_src["reloc"])[1]),
                                "archive": os.path.join(libs_path_dest,  dest_lib_filename + os.path.splitext(pyron_lite_libs_path_src["archive"])[1])}
    copyfile(pyron_lite_libs_path_src["reloc"], pyron_lite_libs_path_dest["reloc"])    
    copyfile(pyron_lite_libs_path_src["archive"], pyron_lite_libs_path_dest["archive"])
    print('%s exported as %s' % (pyron_lite_libs_path_src["reloc"], pyron_lite_libs_path_dest["reloc"]))
    print('%s exported as %s' % (pyron_lite_libs_path_src["archive"], pyron_lite_libs_path_dest["archive"]))

def export_file(model, locale, src_root_dir, models_dest_dir):
    """ export single model file """
    if(len(model) > 1):
        print_error_and_exit("Multiple models defined for locale %s" % locale)

    model = model[0]
    print('locale:%s model:%s' % (locale, model))
    model += '.bin'
    start_index_of_locale = model.find('.') + 1
    end_index_of_locale = model.find('.alexa')
    if(locale in model):
        model_src = os.path.join(src_root_dir, models_src_basename, model)
        model_dest = os.path.join(models_dest_dir, locale)
        copyfile(model_src, model_dest)
        print('%s exported as %s' % (model_src, model_dest))
    else:
        locale_in_model = model[start_index_of_locale:end_index_of_locale]
        print('locale %s model not required, as locale is supported in %s model (%s) ' % (locale, locale_in_model, os.path.join(models_dest_dir, locale_in_model)))

def export_model_files(models_dest_dir, src_root_dir, json_file_path):
    """ export all model files """
    if(os.path.exists(models_dest_dir) == False):
        os.makedirs(models_dest_dir)
    with open(json_file_path) as f:
        data = json.load(f)
        for locale in data['alexa'].keys():
            model = data['alexa'].get(locale)
            export_file(model, locale, src_root_dir, models_dest_dir)

def cleanup(unzipped_already, src_dir):
    """ remove interim unzipped files """
    if((unzipped_already == False) and (len(src_dir))):
        print('Cleaning...')
        shutil.rmtree(src_dir)

def unzip(arg_zip_file):
    """ unzip the AMA zip package """
    src_dir = arg_zip_file.replace('.zip', '')
    unzipped_already = os.path.exists(src_dir)
    if(unzipped_already == False):
        if(zipfile.is_zipfile(arg_zip_file)):
            with ZipFile(arg_zip_file, 'r') as zip_ref:
                print('Preparing...')
                zip_ref.extractall(src_dir)
    return unzipped_already

def print_error_and_exit(message):
    """ print message and terminate script """
    print("Error: %s\r\nExiting script" % message)
    exit()

def check_path(path):
    """ check path is valid. Print error message if not """
    if(os.path.exists(path) == False):
        print_error_and_exit("Cannot find %s" % path)

def get_unzipped_src_chip_dir(src_dir, chip):
    """ extract path to target chip directory """
    src_dir_chip_path = [ name for name in glob.glob(os.path.join(src_dir, '*%s*' % chip)) if os.path.isdir(name) ]
    num_dirs = len(src_dir_chip_path)
    if(num_dirs != 1):
        print_error_and_exit('Incorrect number of target chip directories found (%d): %s' % (num_dirs, src_dir_chip_path))
    return src_dir_chip_path[0]

def get_pyron_lite_lib_paths(src_dir_chip_path, api_version):
    """ export path to python_lite libs """
    pyron_lite_libs_dir = os.path.join(src_dir_chip_path, api_version)
    check_path(pyron_lite_libs_dir)
    
    reloc_file = glob.glob(os.path.join(pyron_lite_libs_dir, '*.reloc'))
    archive_file = glob.glob(os.path.join(pyron_lite_libs_dir, '*.a'))
    if(len(reloc_file) != 1 or len(archive_file) != 1):
        print_error_and_exit('Incorrect number of .reloc or .a files')
    pyron_lite_libs_paths = {"reloc": reloc_file[0], "archive" : archive_file[0]}
    return pyron_lite_libs_paths

def get_json_file_path(src_dir_chip_path):
    """ export path to json to model file """
    json_file = os.path.join(src_dir_chip_path, localeToModels_filename)
    check_path(json_file)
    return json_file

if __name__ == '__main__':
    args = parse_args()

    # Unzip source archive
    unzipped_already = unzip(args.zip_file)

    # Assemble & validate required paths
    src_dir = args.zip_file.replace('.zip', '')
    src_dir_chip_path = get_unzipped_src_chip_dir(src_dir, args.chip)
    pyron_lite_libs_paths = get_pyron_lite_lib_paths(src_dir_chip_path, args.api_version)
    json_file_path = get_json_file_path(src_dir_chip_path)
    dest_libs_path = os.path.join(args.dest, args.chip, dest_libs_basename)
    dest_models_path = os.path.join(args.dest, args.chip, models_dest_basename)

    # Export files
    export_lib_files(dest_libs_path, pyron_lite_libs_paths)
    export_model_files(dest_models_path, src_dir, json_file_path)

    # Remove any interim files
    cleanup(unzipped_already, src_dir)
