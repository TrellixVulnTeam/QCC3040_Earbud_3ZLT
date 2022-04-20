'''
Copyright (c) 2017-2021 Qualcomm Technologies International, Ltd.

Script to generate chain files
'''

# pylint: disable=missing-docstring
from __future__ import print_function


import sys
import os
import xml.etree.ElementTree as ET
import errno
from collections import namedtuple
import logging

try:
    from vm_build.codegen import  CommentBlockDoxygen, HeaderGuards, Enumeration, Array
    from vm_build.plant_uml_chain_diagram import PlantUmlChainDiagram
    logging.getLogger(__name__).info('Using: vm_build for imports')
except ImportError:
    ME = os.path.abspath(__file__)
    MEDIR = os.path.dirname(ME)
    PARENT = os.path.abspath(os.path.join(MEDIR, os.pardir))
    GPARENT = os.path.abspath(os.path.join(PARENT, os.pardir))
    sys.path.insert(0, GPARENT)
    sys.path.insert(0, PARENT)

    from codegen.codegen import CommentBlockDoxygen, HeaderGuards, Enumeration, Array
    from plant_uml_chain_diagram import PlantUmlChainDiagram
    __all__ = ["process_file"]


class ChainException(Exception):
    ''' General exception for chains processing '''
    pass


class ChainTerminalException(ChainException):
    ''' Exception class indicating a problem with a named terminal '''
    pass


class ChainParserException(ChainException):
    ''' Exception class indicating a problem with parsing the chainfile '''
    pass


Operator = namedtuple('Operator', ['name', 'id', 'processor', 'priority', 'sinks', 'sources', 'set_sample_rate'])
Sink = namedtuple('Sink', ['name', 'terminal', 'role'])
Source = namedtuple('Source', ['name', 'terminal', 'role'])
Connection = namedtuple('Connection', ['source', 'sink'])
Configuration = namedtuple('Configuration', ['name', 'opmsgs'])
Opmsg = namedtuple('Opmsg', ['op', 'id', 'msg'])


class Chain(object):
    def __init__(self, filename):
        self._log = logging.getLogger(__name__)
        self.filename = filename
        self._log.info('Running module from: {}'.format(__name__))
        with open(filename) as chainfile:
            self.is_xml = "<?xml" in chainfile.readline()

    def parse(self):
        if self.is_xml:
            self._parse_xml(self.filename)
        else:
            ChainParserException('Failed to parse: {}, only xml format supported'.format(self.filename))
        return self

    def _parse_xml(self, filename):
        try:
            tree = ET.parse(filename)
        except IOError as e:
            raise ChainParserException('Failed to parse: {}, IOError = {}'.format(filename, e))

        element_tree_root = tree.getroot()

        self.name = element_tree_root.attrib['name'].lower()
        self.id = element_tree_root.attrib['id']
        self.ucid = element_tree_root.attrib.get('ucid', 0)
        self.default_priority = element_tree_root.attrib.get('default_priority', "DEFAULT").upper()

        self.operators = [Operator(
            name=op.attrib['name'],
            id=op.attrib['id'],
            processor=[p for p in op.attrib.get('processor', "").upper().split(',') if p != ""],
            priority=op.attrib.get('priority', self.default_priority).upper(),
            sinks=[Sink(name=s.attrib['name'], terminal=s.attrib['terminal'], role=None) for s in op.findall("./sink")],
            sources=[Source(name=s.attrib['name'], terminal=s.attrib['terminal'], role=None) for s in op.findall("./source")],
            set_sample_rate=op.get('set_sample_rate', 'true').lower()
        ) for op in element_tree_root.findall("./operator")]

        self.inputs = [Sink(name=i.attrib['sink'], terminal=None, role=i.attrib.get('role')) for i in element_tree_root.findall("./input")]
        self.outputs = [Source(name=o.attrib['source'], terminal=None, role=o.attrib.get('role')) for o in element_tree_root.findall("./output")]

        self.connections = [Connection(
            source=Source(name=c.attrib['source'], terminal=None, role=None),
            sink=Sink(name=c.attrib['sink'], terminal=None, role=None)
        ) for c in element_tree_root.findall("./connection")]

        self.configurations = [Configuration(
            name=config.attrib['name'].lower(),
            opmsgs=[Opmsg(
                op=opmsg.attrib['op'],
                id=opmsg.attrib['id'],
                msg=opmsg.attrib.get('msg')
            ) for opmsg in config]
        ) for config in element_tree_root.findall("./configuration")]

        self.include_headers = [h.attrib['name'] for h in element_tree_root.findall("./include_header")]
        self.generate_operator_roles_enum = element_tree_root.attrib.get('generate_operator_roles_enum', "true").lower() != 'false'
        self.generate_endpoint_roles_enum = element_tree_root.attrib.get('generate_endpoint_roles_enum', "true").lower() != 'false'

        self.exclude_from_configure_sample_rate = [op.name for op in self.operators if op.set_sample_rate == "false"]  # pylint: disable=invalid-name

    def find_op_by_name(self, op_name):
        try:
            return self.op_dict[op_name]
        except (AttributeError, KeyError) as e:
            if isinstance(e, AttributeError):
                self.op_dict = dict()

            for op in self.operators:
                if op.name == op_name:
                    self.op_dict[op_name] = op
                    break
            else:
                raise ChainException("Operator with name: {} not found in chain".format(op_name))

            return self.op_dict[op_name]

    @staticmethod
    def find_sink_by_name(op, sink_name):
        for sink in op.sinks:
            if sink.name == sink_name:
                return sink
        else:
            raise ChainTerminalException("Terminal with name: {} not found in operator {}".format(sink_name, op.name))

    @staticmethod
    def find_source_by_name(op, source_name):
        for source in op.sources:
            if source.name == source_name:
                return source
        else:
            raise ChainTerminalException("Terminal with name: {} not found in operator {}".format(source_name, op.name))


class ChainGenerator(object):  # pylint: disable=too-many-instance-attributes
    ''' Base chain generator class '''
    def __init__(self, chain, outfile=None):
        self.outfile = outfile
        self.chain = chain

    def terminal_num(self, operator, terminal, for_sink=True):
        ''' Given operator and terminal names:
            if for_sink is True, returns the sink terminal number, else the source terminal number. '''
        op = self.chain.find_op_by_name(operator)
        term = self.chain.find_sink_by_name(op, terminal) if for_sink else self.chain.find_source_by_name(op, terminal)
        return term.terminal

    def metadata(self, endpoint_item):
        ''' Given an endpoint and whether it is a sink/source, extract the
            operator and terminal information and include the role information
            (possibly pre-defined in file).
            '''
        op_term = endpoint_item.name.split('.')
        assert len(op_term) == 2
        metadata = {}
        metadata['operator'] = op_term[0]
        metadata['terminal'] = op_term[1]
        metadata['terminal_num'] = self.terminal_num(op_term[0], op_term[1], isinstance(endpoint_item, Sink))
        if endpoint_item.role is not None:
            metadata['role'] = endpoint_item.role
        else:
            metadata['role'] = "{operator}_{terminal}".format(**metadata)
        return metadata

    def filename(self, file_extension):
        ''' The filename associated with this chain '''
        return "{}.{}".format(self.chain.name, file_extension)

    @staticmethod
    def opmsg_name(msgid, operator, config):
        ''' The opmsg name '''
        return "{}_{}_{}".format(msgid, operator, config)

    def opmsgs_config_name(self, config):
        ''' The name of the opmsgs config '''
        return "{}_opmsgs_config_{}".format(self.chain.name, config)

    def exclude_from_configure_sample_rate_array_name(self):  # pylint: disable=invalid-name
        ''' The name of the array '''
        return "{}_exclude_from_configure_sample_rate".format(self.chain.name)


class ChainConfigurationGenerator(ChainGenerator):
    ''' Given a xml definition of a chain,
        generates the C code to define the configuration of a chain '''

    def endpoint_enum_line(self, endpoint_item):
        ''' Generate the string for a single element of the endpoint enumeration '''
        metadata = self.metadata(endpoint_item)
        return metadata['role']

    def operators_array_line(self, op_item, default_processor="P0"):
        ''' Generate the string for a single element of the operators array

            Supports requests for priority and processor, forming the macro
            MAKE_OPERATOR_CONFIG[_<processor>][_PRIORITY_<priority>]
        '''
        requested_priority = op_item.priority
        priority_string = "_PRIORITY_{}".format(requested_priority) if requested_priority != "DEFAULT" else ""

        # The possible combinations of default_processor and the requested_processor attribute,
        # and the resulting run_processor on which to run this operator.
        '''
        default_processor   requested_processor   run_processor
        P0                  P0                    On P0
                            P1                    On P1
                            P0,P1                 On P0
        P1                  P0                    On P0
                            P1                    On P1
                            P0,P1                 On P1
        '''
        requested_processor = op_item.processor
        if default_processor == "" or default_processor == "P0":
            processor_string = ""
        else:
            if default_processor in requested_processor:
                processor_string = "_{processor}".format(processor=default_processor)
            else:
                processor_string = ""

        return "MAKE_OPERATOR_CONFIG{}{}({}, {})".format(processor_string,
                                                         priority_string,
                                                         op_item.id,
                                                         op_item.name)

    def inputs_array_line(self, input_item):
        ''' Generate the string for a single element of the inputs array '''
        metadata = self.metadata(input_item)
        return "{{{operator}, {role}, {terminal_num}}}".format(**metadata)

    def outputs_array_line(self, output_item):
        ''' Generate the string for a single element of the outputs array '''
        metadata = self.metadata(output_item)
        return "{{{operator}, {role}, {terminal_num}}}".format(**metadata)

    def connections_array_line(self, connection_item):
        ''' Generate the string for a single element of the connections array '''
        source_metadata = self.metadata(connection_item.source)
        sink_metadata = self.metadata(connection_item.sink)
        str_a = '{{{operator}, {terminal_num}, '.format(**source_metadata)
        str_b = "{operator}, {terminal_num}, 1}}".format(**sink_metadata)
        return str_a + str_b

    @staticmethod
    def opmsgs_array_line(op_id_dict):
        ''' Generate the string for a single element of a opmsgs array '''
        return "{{{op_name}, {msg_name}, ARRAY_DIM({msg_name})}}".format(**op_id_dict)

    def get_used_operator_processors(self):
        processors_used = set()
        for operator in self.chain.operators:
            for p in operator.processor:
                processors_used.add(p)
        return sorted(list(processors_used))

    def output_config_definition(self, proc_suffix=''):
        config_str = "const chain_config_t {ch_name} = {{{ch_id}, {ucid}, {op_name}, {op_len}, " +\
                     "{in_name}, {in_len}, {out_name}, {out_len}, {con_name}, {con_len}}};\n"
        ch_name = self.chain.name + '_config' + proc_suffix
        if self.chain.operators:
            op_name = 'operators' + proc_suffix
        else:
            op_name = 'NULL'
        print(config_str.format(ch_name=ch_name,
                                ch_id=self.chain.id,
                                ucid=self.chain.ucid,
                                op_name=op_name,
                                op_len=len(self.chain.operators) if self.chain.operators else 0,
                                in_name='inputs' if self.chain.inputs else 'NULL',
                                in_len=len(self.chain.inputs) if self.chain.inputs else 0,
                                out_name='outputs' if self.chain.outputs else 'NULL',
                                out_len=len(self.chain.outputs) if self.chain.outputs else 0,
                                con_name='connections' if self.chain.connections else 'NULL',
                                con_len=len(self.chain.connections) if self.chain.connections else 0
                                ), file=self.outfile)

    def generate_source(self):  # pylint: disable=too-many-branches,too-many-locals
        ''' Generate chain definitions '''
        # Generate the C chain definition for the .c file
        headers = [self.filename('h'), "cap_id_prim.h", "opmsg_prim.h", "hydra_macros.h"]
        headers.extend(self.chain.include_headers)

        for header in headers:
            print("#include <{}>".format(header), file=self.outfile)

        processors_used = None
        if self.chain.operators:
            processors_used = self.get_used_operator_processors()
            if len(processors_used) == 0:
                with Array("static const operator_config_t", "operators", outfile=self.outfile) as array:
                    array.extend([self.operators_array_line(operator) for operator in self.chain.operators])  # pylint: disable=no-member
            else:
                for p in processors_used:
                    var_name = "operators_{}".format(p.lower())
                    with Array("static const operator_config_t", var_name, outfile=self.outfile) as array:
                        array.extend([self.operators_array_line(operator, p) for operator in self.chain.operators])  # pylint: disable=no-member

        if self.chain.inputs:
            with Array("static const operator_endpoint_t", "inputs", outfile=self.outfile) as array:
                array.extend([self.inputs_array_line(inp) for inp in self.chain.inputs])

        if self.chain.outputs:
            with Array("static const operator_endpoint_t", "outputs", outfile=self.outfile) as array:
                array.extend([self.outputs_array_line(output) for output in self.chain.outputs])

        if self.chain.connections:
            with Array("static const operator_connection_t", "connections", outfile=self.outfile) as array:
                array.extend([self.connections_array_line(connection) for connection in self.chain.connections])

        if self.chain.exclude_from_configure_sample_rate:
            with Array("const unsigned", self.exclude_from_configure_sample_rate_array_name(), outfile=self.outfile) as array:
                array.extend(self.chain.exclude_from_configure_sample_rate)

        if self.chain.configurations:
            for configuration in self.chain.configurations:
                config_name = configuration.name
                opmsgs = []
                for opmsg in list(configuration):
                    assert opmsg.tag == "opmsg"
                    op_name = opmsg.op
                    msgid = opmsg.id
                    message_name = self.opmsg_name(msgid.lower(), op_name.lower(), config_name)
                    opmsgs.append({'op_name': op_name, 'msg_name': message_name})
                    with Array("static const uint16", message_name, outfile=self.outfile) as array:
                        array.extend([opmsg.id])
                        msg_data = opmsg.get('msg')
                        if msg_data is not None:
                            array.extend(msg_data.split(','))
                with Array("const chain_operator_message_t", self.opmsgs_config_name(config_name), outfile=self.outfile) as array:
                    array.extend([self.opmsgs_array_line(opmsg) for opmsg in opmsgs])

        if not processors_used:
            self.output_config_definition()
        else:
            for p in processors_used:
                self.output_config_definition("_" + p.lower())

    def generate_header(self):
        # Generate the C chain declarations for the .h file
        print("#include <chain.h>\n", file=self.outfile)

        if self.chain.generate_operator_roles_enum:
            with Enumeration("{}_operators".format(self.chain.name), outfile=self.outfile) as enum:
                enum.extend([op.name for op in self.chain.operators])  # pylint: disable=no-member

        if self.chain.generate_endpoint_roles_enum:
            with Enumeration("{}_endpoints".format(self.chain.name), outfile=self.outfile) as enum:
                enum.extend([self.endpoint_enum_line(endpoint) for endpoint in self.chain.inputs + self.chain.outputs])

        for configuration in self.chain.configurations:
            config_name = configuration.name
            print("extern const chain_operator_message_t {}[{}];".format(self.opmsgs_config_name(config_name),
                                                                         len(list(configuration))), file=self.outfile)

        if self.chain.exclude_from_configure_sample_rate:
            print("extern const unsigned {}[{}];".format(self.exclude_from_configure_sample_rate_array_name(),
                                                         len(self.chain.exclude_from_configure_sample_rate)), file=self.outfile)

        if self.chain.operators:
            processors_used = self.get_used_operator_processors()
            if len(processors_used) == 0:
                print("extern const chain_config_t {}_config;\n".format(self.chain.name), file=self.outfile)
            else:
                for p in processors_used:
                    print("extern const chain_config_t {}_config_{};\n".format(self.chain.name, p.lower()), file=self.outfile)


class ChainPlantUmlDiagramGenerator(ChainGenerator):
    ''' Generate plant UML diagram '''

    def generate(self, outfile=None):
        ''' Generate the plant UML chain diagram '''
        with CommentBlockDoxygen(outfile=self.outfile) as cbd:
            cbd.doxy_page(self.chain.name)  # pylint: disable=no-member
            with PlantUmlChainDiagram(outfile=self.outfile) as puml:
                for operator in self.chain.operators:
                    puml.object(operator.name, operator.id)
                for connection in self.chain.connections:
                    source_metadata = self.metadata(connection.source)
                    sink_metadata = self.metadata(connection.sink)
                    puml.wire(source_metadata, sink_metadata)
                for inpt in self.chain.inputs:
                    sink_metadata = self.metadata(inpt)
                    puml.input(sink_metadata)
                for outpt in self.chain.outputs:
                    source_metadata = self.metadata(outpt)
                    puml.output(source_metadata)


class DoxygenGenerator(ChainGenerator):
    ''' Generate doxygen file header comment '''
    def generate(self, file_extension):
        with CommentBlockDoxygen(outfile=self.outfile) as cbd:
            # pylint: disable=no-member
            cbd.doxy_copyright()
            cbd.doxy_filename(self.filename(file_extension))
            cbd.doxy_brief("The {} chain.\n\n    This file is generated by {}.".format(self.chain.name, __file__))


def create_header(chain, file_handle):
    doxygen = DoxygenGenerator(chain, file_handle)
    doxygen.generate('h')
    with HeaderGuards(doxygen.chain.name, file_handle):
        try:
            ChainPlantUmlDiagramGenerator(chain).generate(outfile=file_handle)
            ChainConfigurationGenerator(chain, file_handle).generate_header()
        except ChainTerminalException as err:
            print(err, file=sys.stderr)
            exit(2)


def generate_header(chain, write_to_file, filename, output_folder):
    output_file = _get_out_file(write_to_file, filename, 'h', output_folder)
    file_handle = None
    if output_file:
        with open(output_file, 'w') as file_handle:
            print('Generating: {}'.format(output_file))
            create_header(chain, file_handle)
    else:
        create_header(chain, file_handle)


def create_source(element_tree_root, file_handle):
    DoxygenGenerator(element_tree_root, file_handle).generate('c')
    try:
        ChainConfigurationGenerator(element_tree_root, file_handle).generate_source()
    except ChainTerminalException as err:
        print(err, file=sys.stderr)
        exit(2)


def generate_source(element_tree_root, write_to_file, filename, output_folder):
    output_file = _get_out_file(write_to_file, filename, 'c', output_folder)
    file_handle = None
    if output_file:
        with open(output_file, 'w') as file_handle:
            print('Generating: {}'.format(output_file))
            create_source(element_tree_root, file_handle)
    else:
        create_source(element_tree_root, file_handle)


def _create_folders(folder):
    try:
        os.makedirs(folder)
    except OSError as err:
        if err.errno != errno.EEXIST:
            raise


def create_uml(element_tree_root, file_handle):
    try:
        ChainPlantUmlDiagramGenerator(element_tree_root, file_handle).generate()
    except ChainTerminalException as err:
        print(err, file=sys.stderr)
        exit(2)


def generate_uml(element_tree_root, write_to_file, filename, output_folder):
    output_file = _get_out_file(write_to_file, filename, 'uml', output_folder)
    file_handle = None
    if output_file:
        with open(output_file, 'w') as file_handle:
            print('Generating: {}'.format(output_file))
            create_uml(element_tree_root, file_handle)
    else:
        create_uml(element_tree_root, None)


def _get_out_file(write_to_file, filename, ext, output_folder=None):
    '''
    If writing to file, return the output file name
    Otherwise return None
    '''
    if write_to_file:
        if output_folder is None:
            out_path = os.path.dirname(os.path.abspath(filename))
        else:
            if os.path.isabs(output_folder):
                out_path = output_folder
            else:
                out_path = os.path.join(os.path.dirname(os.path.abspath(filename)), output_folder)
            _create_folders(out_path)
        filename, _ = os.path.splitext(os.path.basename(filename))
        filename = '.'.join([filename, ext])
        return os.path.join(out_path, filename)
    else:
        return None


def process_file(header, source, uml, write_to_file, filename, output_folder):  # pylint: disable=too-many-arguments
    try:
        chain = Chain(filename).parse()
    except ChainParserException as e:
        print(e, file=sys.stderr)
        sys.exit(1)

    # Generate the header file
    if header:
        generate_header(chain, write_to_file, filename, output_folder)

    # Generate the source file
    if source:
        generate_source(chain, write_to_file, filename, output_folder)

    # Generate the uml diagram
    if uml:
        generate_uml(chain, write_to_file, filename, output_folder)
