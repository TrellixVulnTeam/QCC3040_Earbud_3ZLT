"""
This module contains the business logic for reading an Hydra
Config SDB database and writing it to an XML file
"""
import argparse
import sys

from xml2sdb import writexml


def build_xml_from_db(sdb_file, xml_file, unfiltered,
                      xsd_file='HydraConfig.xsd', verify_xml=True):
    """Create an XML file from a given database"""
    sdb = writexml.HydraConfig(sdb_file, xml_file, xsd_file)
    sdb.parse(subsys_info=None, unfiltered=unfiltered, verify_xml=verify_xml)


def _parse_args(args):
    """
    Define what commandline options are available
    """
    parser = argparse.ArgumentParser(
        description="""Export to an XML file """)
    parser.add_argument('-e', '--export', default=False,
                        action='store_true',
                        help='Specifies to generate an XML from the database')
    parser.add_argument('-u', '--unfiltered', default=False,
                        action='store_true',
                        help='Include items marked at internal')
    parser.add_argument('-n', '--no_schema', default=False,
                        action='store_true',
                        help='Turn off verification of XML against Schema '
                             '(XSD)')
    supported_formats = ['xml']
    parser.add_argument('-f', '--format',
                        default='xml', choices=supported_formats,
                        help='Do not include items marked at internal')
    parser.add_argument('-i', '--sdb_file',
                        help='location of SDB file to be read')
    parser.add_argument('-o', '--xml_file',
                        help='location of input XML file')

    parser.add_argument('-x', '--xsd_file', default='HydraMeta.xsd',
                        help='location of XML Schema file to use')
    return parser.parse_args(args)


def run(args=None):
    """
    Main entry point when running on the command line
    """
    # print(args)
    args = _parse_args(args)
    verify_xml = not args.no_schema
    # print(type(args))
    if args.format == 'xml':
        build_xml_from_db(sdb_file=args.sdb_file,
                          xml_file=args.xml_file,
                          unfiltered=args.unfiltered,
                          xsd_file=args.xsd_file,
                          verify_xml=verify_xml)


if __name__ == '__main__':
    run(sys.argv[1:])
