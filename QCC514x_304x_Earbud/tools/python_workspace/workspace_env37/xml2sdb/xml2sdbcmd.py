"""
This module contains the business logic for reading an Hydra Config XML file
and writing it to an SDB database
"""
import argparse
import sys

from xml2sdb import read_system_info
from xml2sdb import sdb
from xml2sdb import writesdb


def create_new(db_filename):
    """
    Create a new database with empty tables on disk at given location
    """
    engine = sdb.ConnectDB(db_filename)
    engine.create_new()


def build_db_from_xml(xml_file, db_filename, xsd_file, verify_xml=True):
    """
    Read XML file and write to external sdb database file
    """
    # two letter variable name OK for a database
    # pylint: disable=invalid-name
    xml_data = writesdb.Database(db_filename, xml_xsd=xsd_file)
    # pylint: enable=invalid-name
    xml_data.parse(xml_file, verify_xml=verify_xml)
    xml_data.write_to_sdb()


def build_system_info(sys_info_file, db_file):
    """
    Read system info from file and add to database
    """
    chip_info = read_system_info.SystemVersion(sys_info_file)
    chip_data = chip_info.parse()
    db_session = sdb.ConnectDB(db_file)
    db_session.add_chip_data(chip_data)


def _parse_args(args):
    """
    Define what commandline options are available
    """
    parser = argparse.ArgumentParser(
        description="""Process chip config XML files and create sqlite3
        database""")
    parser.add_argument('-n', '--new_db', default=False, action='store_true',
                        help='create new database')
    parser.add_argument('-a', '--append', default=False, action='store_true',
                        help='add XML metadata to database')
    parser.add_argument('-u', '--unchecked', default=False,
                        action='store_true',
                        help='Turn off verification of XML against Schema '
                             '(XSD')

    parser.add_argument('-s', '--sys_info', default=False,
                        action='store_true',
                        help="""add system version defined in text file
                        (see CS-218470-SP)"""
                        )
    parser.add_argument('-i', '--xml_file', nargs='+',
                        help='location of input XML file')
    parser.add_argument('-o', '--sdb_file',
                        help='location of SDB file')
    parser.add_argument('-x', '--xsd_file', default='HydraMeta.xsd',
                        help='location of XML Schema file to use')
    return parser.parse_args(args)


def run(args=None):
    """
    Main entry point when running on the command line
    """
    args = _parse_args(args)
    verify_xml = not args.unchecked
    if args.new_db:
        create_new(args.sdb_file)
    elif args.append:
        for xml_file in args.xml_file:
            build_db_from_xml(xml_file, args.sdb_file,
                              xsd_file=args.xsd_file, verify_xml=verify_xml)
    elif args.sys_info:
        build_system_info(args.xml_file[0], args.sdb_file)


if __name__ == '__main__':
    run(sys.argv[1:])
