# Copyright (c) 2020 Qualcomm Technologies International, Ltd.

import argparse
import json
import glob
import os
import fnmatch
import shutil
import sys
from shutil import copyfile
import zipfile
from zipfile import ZipFile

default_pyron_lite_api_version = 'PRL1000'
dest_lib_filename = 'libpryon_lite'
dest_lib_filename_gva = 'libhotword_priv_lib'
models_dest_basename = models_src_basename = 'models'
localeToModels_filename = 'localeToModels.json'
libs_dest_basename = r'kalimba\kymera\lib_release'
headers_dest_basename = r'..\..\capabilities'
header_files_names_list=['pryon_lite.h', 'pryon_lite_error.h', 'pryon_lite_metadata.h', 'pryon_lite_PRL1000.h', 'pryon_lite_vad.h', 'pryon_lite_ww.h']
gva_header_name = 'hotword_dsp_multi_bank_api.h'

def parse_args():
    """ parse the command line arguments """
    parser = argparse.ArgumentParser(description='Extract WWE files')

    parser.add_argument('-a', '--audio_bin',
                        required=True,
                        help=r"Path to the ADK's audio/bin folder (REQUIRED)")

    parser.add_argument('-f', '--file_name',
                        required=True,
                        help='Part or full name of zip pacakge (REQUIRED)')

    parser.add_argument('-b', '--build_config',
                        required=True,
                        help='Target build config. Valid options are streplus_rom_release, streplus_rom_kalsim_kalcmd2_release (REQUIRED)')

    parser.add_argument('-c', '--chip',
                        required=True,
                        help='Target chip variant. Valid options are qcc514x_qcc304x, qcc515x_qcc305x, qcc512x-rom-v21, qcc512x-rom-v02 (REQUIRED)')

    parser.add_argument('-o', '--operator',
                        required=True,
                        help='Target WWE Operator or Capability. Valid options are apva (REQUIRED)')

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
    if((unzipped_already == True) and (len(src_dir))):
        print('Cleaning...')
        shutil.rmtree(src_dir)

def unzip(arg_zip_file):
    """ unzip the zip package """
    src_dir = arg_zip_file.replace('.zip', '')
    unzipped_already = os.path.exists(src_dir)
    if(unzipped_already == False):
        if(zipfile.is_zipfile(arg_zip_file)):
            with ZipFile(arg_zip_file, 'r') as zip_ref:
                print('Preparing...')
                zip_ref.extractall(src_dir)
            unzipped_already = True
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

def export_header_files(dest_headers_path, header_files_path):
    """ export pryon lite API header files """
    if(os.path.exists(dest_headers_path) == False):
        print_error_and_exit("WWE capabiltiy folder is missing")

    for file_name in header_files_names_list:
        copyfile( os.path.join(header_files_path, file_name) , os.path.join(dest_headers_path, file_name) )    
        print('%s exported as %s' % (os.path.join(header_files_path, file_name) , os.path.join(dest_headers_path, file_name)))

def find_hotword_header(src_dir):

    header_file = ""
    for root, dir_name, file_name in os.walk(src_dir):
        if gva_header_name in file_name:
            header_file = os.path.join(root, gva_header_name)

    if (header_file == ""):
        print_error_and_exit("%s file not found in zip package" % (gva_header_name))

    return header_file

def find_hotword_libs(src_dir, build_config):
    for dir_name in os.listdir(src_dir):
        if fnmatch.fnmatch(dir_name, "*Hotword_Models*"):
            hm_dirname = dir_name
    
    hotword_libs_path = os.path.join(src_dir, hm_dirname, build_config)
    return hotword_libs_path

def extract_gva_libs(hotoword_libs_path, dest_libs_path):

    reloc_file = glob.glob(os.path.join(hotoword_libs_path, '*.reloc'))
    archive_file = glob.glob(os.path.join(hotoword_libs_path, '*.a'))
    if(len(reloc_file) != 1 or len(archive_file) != 1):
        print_error_and_exit('Incorrect number of .reloc or .a files')

    copyfile(reloc_file[0], os.path.join(dest_libs_path, dest_lib_filename_gva + ".reloc"))
    print('%s exported as %s' % (reloc_file[0], os.path.join(dest_libs_path, dest_lib_filename_gva + ".reloc")))
    copyfile(archive_file[0], os.path.join(dest_libs_path, dest_lib_filename_gva + ".a"))
    print('%s exported as %s' % (archive_file[0], os.path.join(dest_libs_path, dest_lib_filename_gva + ".a")))

def extract_gva_header(hotword_header_file, dest_header_path):
    dest_header_file = os.path.join(dest_header_path, gva_header_name)
    copyfile(hotword_header_file, dest_header_file)
    print('%s exported as %s' % (hotword_header_file, dest_header_file))

def extract(zip_dir, zip_file_partial_name, build_config, chip_type, operator):
    zip_file_full_name = ""
    pattern = "*" + zip_file_partial_name + "*"
    for file_name in os.listdir(zip_dir):
        if fnmatch.fnmatch(file_name, pattern):
            zip_file_full_name = file_name

    if zip_file_full_name == "":
        print_error_and_exit("Vendor Zip package not found")

    # Unzip source archive
    path_prefix = '\\\\?\\'
    path_zip = os.path.join( zip_dir, zip_file_full_name)
    path_zip = os.path.abspath(path_zip)
    unzipped_already = unzip(path_prefix + path_zip)
    src_dir = os.path.join(zip_dir,zip_file_full_name.replace('.zip', ''))
    dest_libs_path = os.path.join(zip_dir, r'..', chip_type, libs_dest_basename, build_config)
    dest_headers_path = os.path.join(dest_libs_path, headers_dest_basename, operator)

    if(operator == "apva"):
        #We treat StrettoPlusV1.2 the same as StrettoPlusV1.1 for APVA
        if(chip_type == 'qcc515x_qcc305x'):
            chip_type_alias = 'qcc514x_qcc304x'
        else:
            chip_type_alias = chip_type
        # Assemble & validate required paths
        src_dir_chip_path = get_unzipped_src_chip_dir(src_dir, chip_type_alias[0:7:1])
        pyron_lite_libs_paths = get_pyron_lite_lib_paths(src_dir_chip_path, default_pyron_lite_api_version)
        json_file_path = get_json_file_path(src_dir_chip_path)
        header_files_path = src_dir_chip_path
        dest_models_path = os.path.join(zip_dir, models_dest_basename, operator)
        

        # Export files
        export_lib_files(dest_libs_path, pyron_lite_libs_paths)
        export_model_files(dest_models_path, src_dir, json_file_path)
        export_header_files(dest_headers_path, header_files_path)

    elif(operator == "gva"):
        hotoword_libs_path = find_hotword_libs(src_dir, build_config)
        extract_gva_libs(hotoword_libs_path, dest_libs_path)
        hotword_header_file = find_hotword_header(src_dir)
        extract_gva_header(hotword_header_file, dest_headers_path)
    
    else:
        print_error_and_exit("Unsupported operator")

    # Remove any interim files
    cleanup(unzipped_already, path_prefix + os.path.abspath(src_dir))

if __name__ == '__main__':
    args = parse_args()
    extract(args.audio_bin , args.file_name, args.build_config, args.chip, args.operator)

