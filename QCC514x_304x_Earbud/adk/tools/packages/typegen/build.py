'''
Copyright (c) 2019 Qualcomm Technologies International, Ltd.

Build all the typedef files found under a specified path

'''

from __future__ import print_function
import argparse
import os
import sys
import glob
import xml.etree.ElementTree as ET
from contextlib import contextmanager

from typegen import TypesGenerator

TYPEDEF_PATTERN = '.typedef'

@contextmanager
def redirect_stdout(new_target):
    old_target, sys.stdout = sys.stdout, new_target # replace sys.stdout
    try:
        yield new_target # run some code with the replaced stdout
    finally:
        sys.stdout = old_target # restore to the previous value


class Builder(object):
    def __init__(self, root):
        self._root = root


    def _get_files(self, path):
        files = []
        for f in glob.glob(path):
            f = f.replace('\\', '/')
            files.append(f)
        return files


    def find_files(self, folders, pattern):
        matches = []
        for folder in folders:
            for dirName, subdirList, fileList in os.walk(folder):
                dir_name = os.path.basename(dirName)
                for fname in fileList:
                    file, ext = os.path.splitext(fname)
                    if ext == pattern:
                        matches.append(os.path.join(dirName, fname))
        return matches

    def get_xml(self, xml_file):
        try:
            tree = ET.parse(xml_file)
        except IOError:
            print >> stderr, '*** ERROR -- Failed to parse [{0}]'.format(args.filename)
            return False
        return tree.getroot() 

    def generate_file(self, file_name, func):
        print('Processing: {}'.format(file_name))
        with open(file_name, 'w') as f:
            with redirect_stdout(f):
                func()

    def generate_typedefs(self, files):
        for file in files:
            file_name, _ = os.path.splitext(file)
            typegen = TypesGenerator(self.get_xml(file))
            self.generate_file(file_name + '_typedef.h', typegen.generate_typedef_header)
            self.generate_file(file_name + '_marshal_typedef.h', typegen.generate_marshal_header)
            self.generate_file(file_name + '_marshal_typedef.c', typegen.generate_marshal_source)

    def run(self, src, is_directory):
        if is_directory:
            files = self.find_files(src, TYPEDEF_PATTERN)
            self.generate_typedefs(files)
        else:
            self.generate_typedefs(src)

def get_typedefs(x2pfile):
    tree = ET.parse(x2pfile)
    root = tree.getroot()
    filelist = []

    for sourcefile in root.iter("file"):
        path = sourcefile.get("path")
        if path.find(".typedef") >= 0:
            filelist.append(path)

    return filelist

def runbuilder(source, is_directory):
    root = os.getcwd()
    builder = Builder(root)
    builder.run(source, is_directory)

def process_commandline_arguments():
    """
    Read in the command line arguments
    """
    parser = argparse.ArgumentParser(
        description=('Build all typegen files found under the supplied folder')
    )

    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument('--file', dest='x2pfile', help='X2P File for reading the TYPEDEF list')    
    group.add_argument('--source', dest='dir', help='The root directory from which to search for*.typedef files')

    return parser.parse_args()

def main():
    args = process_commandline_arguments()
    if args.x2pfile:
        runbuilder(get_typedefs(args.x2pfile), False)

    if args.dir:
        runbuilder(args.dir, True)

if __name__ == "__main__":
    main()
