"""
Collection of classes and function to read HydraMeta configuration information
from SDB database
"""
from pathlib import Path
from lxml import etree, objectify

from xml2sdb import sdb


class Schema:
    """
    Class for an XML schema
    """

    def __init__(self, schema_xsd):
        self.xsd_file = schema_xsd

    def validate_xml(self, xml_tree):
        """Check an XML document against the schema"""
        # remove lxml annotation
        objectify.deannotate(xml_tree)
        etree.cleanup_namespaces(xml_tree)
        try:
            xmlschema_doc = etree.parse(str(self.xsd_file))
        except OSError as err_msg:
            # Not raising an error because legacy tool allowed this.
            # This should probably be changed at some point
            print(f'\t\t{err_msg}')
            return None

        xmlschema = etree.XMLSchema(xmlschema_doc)

        try:
            xmlschema.assertValid(xml_tree)
        except etree.DocumentInvalid as err_msg:
            # Not raising an error because legacy tool allowed this.
            # This should probably be changed at some point
            print(f'\t\t{err_msg}')
        else:
            print(f'Valid XML created')


class HydraConfig:
    """
    Class for reading an SDB Database and writing to an HydraConfig format
    XML file
    """

    def __init__(self, sdb_file, xml_file, xsd_file='HydraConfig.xsd'):
        self.sdb_file = sdb_file
        self.xml_file = xml_file
        self.xsd_file = xsd_file
        # two letter variable name OK for a database
        # pylint: disable=invalid-name
        self.db = sdb.ConnectDB(sdb_file)
        # pylint: enable=invalid-name

    def parse(self, subsys_info, unfiltered=True, verify_xml=True):
        """Iterate over database and output to XML file"""
        doc = self.create_metadata_list()
        # If no subsys_info then get everything
        if subsys_info is None:
            subsystems = self.db.get_subsystems()
        else:
            for subsys_params in subsys_info:
                subsystems = self.db.get_subsys_firm_ver(**subsys_params)
        for subsystem in subsystems:
            metadata_lvl = self.create_metadata(doc, subsystem)
            for enums in self.db.get_enum_for_subfw_uid(subsystem.subfw_uid):
                enum_def_lvl = self.create_enums(metadata_lvl, enums)
                for enum_entry in self.db.get_enum_entries_for_enum_uid(
                        enums.enum_uid):
                    self.create_enum_entry(enum_def_lvl, enum_entry)
            for struct in self.db.get_struct_by_subfw_uid(subsystem.subfw_uid):
                struct_def_lvl = self.create_struct(metadata_lvl, struct)
                for struct_elem in self.db.get_struct_elem_by_struct_uid(
                        struct.struct_uid):
                    self.create_struct_elem(struct_def_lvl, struct_elem)
            for conf_elem in self.db.get_config_elem_by_subfw_uid(
                    subsystem.subfw_uid):
                if unfiltered:
                    self.create_conf_elem(metadata_lvl, conf_elem)
                elif not conf_elem.config_elements.is_internal:
                    self.create_conf_elem(metadata_lvl, conf_elem, unfiltered)
            for conf_table in self.db.get_config_table_by_subfw_uid(
                    subsystem.subfw_uid):
                if unfiltered:
                    self.create_conf_table(metadata_lvl, conf_table)
                elif not conf_table.is_internal:
                    self.create_conf_table(metadata_lvl, conf_table,
                                           unfiltered)

        # Test for XML correctness
        if verify_xml:
            schema = Schema(self.xsd_file)
            schema.validate_xml(doc)

        # Show progress
        self.write_xml(doc, file_path=self.xml_file)

    def create_metadata_list(self):
        """Add metadata_list root to XML object"""
        xsd_name = str(Path(self.xsd_file).name)
        namespace = 'http://www.w3.org/2001/XMLSchema-instance'
        location_attrib = '{{{0}}}noNamespaceSchemaLocation'.format(namespace)
        return objectify.Element('metadata_list',
                                 attrib={location_attrib: xsd_name})

    def create_metadata(self, root, subsystem_info):
        """Add metadata element to XML object"""
        attributes = ['subsystem_name', 'subsystem_layer', 'subsystem_alias',
                      'subsystem_id', 'version', 'variant', 'description',
                      'build_date_time']

        sub_elem = objectify.SubElement(root, 'metadata')
        return self._build_xml_attributes(attributes, sub_elem, subsystem_info)

    def create_enums(self, parent, db_row):
        """Add enum_def to XML object"""
        attributes = ['enum_name']
        ignore_enums = ['boolean']
        if db_row.enum_name in ignore_enums:
            return None
        else:
            sub_elem = objectify.SubElement(parent, 'enum_def')
            return self._build_xml_attributes(attributes, sub_elem, db_row)

    def create_enum_entry(self, parent, db_row):
        """Add enum_entry to XML object"""
        if parent is None:
            return None
        else:
            attributes = ['enum_label', 'enum_value', 'enum_description']
            sub_elem = objectify.SubElement(parent, 'enum_entry')
            return self._build_xml_attributes(attributes, sub_elem, db_row)

    def create_struct(self, parent, db_row):
        """Add struct_def to XML object"""
        sub_elem = objectify.SubElement(parent, 'struct_def')
        sub_elem.set('struct_name',
                     str(db_row.__getattribute__('struct_name')))
        return sub_elem

    def create_struct_elem(self, parent, db_row):
        """Add structure_elements to XML object"""
        attributes = ['lsb_bit_position', 'bit_width', 'name']
        elements = ['label', 'description_user', 'description_internal',
                    'type',
                    'format', 'range_min', 'range_max', 'is_array',
                    'array_length_min', 'array_length_max']
        sub_elem = objectify.SubElement(parent, 'structure_element')
        elem_path = self._build_xml_attributes(attributes, sub_elem, db_row)
        elem_path.set('name', db_row.config_elements.name)
        return self._build_xml_elements(elements, elem_path,
                                        db_row.config_elements)

    def create_conf_elem(self, parent, db_row, unfiltered=True):
        """Add config_elements to XML object"""
        attributes = ['name', 'psid']
        elements = ['source_default', 'description_user',
                    'description_internal', 'access_rights', 'range_min',
                    'type', 'additional_information', 'format', 'is_array',
                    'function_list', 'category', 'oid', 'default_list',
                    'function', 'range_max', 'array_length_max', 'units',
                    'default', 'label', 'is_internal', 'array_length_min',
                    'table_name', 'source_reference']

        if not unfiltered:
            elements.remove('description_internal')

        if db_row.config_elements.psid is not None:
            sub_elem = objectify.SubElement(parent, 'config_element')
            elem_path = self._build_xml_attributes(attributes, sub_elem,
                                                   db_row.config_elements)
            elem_path = self._build_xml_elements(elements, sub_elem,
                                                 db_row.config_elements)
            if db_row.config_tables is not None:
                elem_path.table_name = db_row.config_tables.table_name
            default_attr, default_value = self._build_xml_default_string(
                db_row.config_elements)
            if default_value is not None and default_attr is None:
                elem_path.default = default_value
            elif default_value is not None and default_attr is not None:
                def_list = objectify.SubElement(sub_elem, 'default_list')
                for def_index, def_item in enumerate(default_value):
                    def_elem = objectify.SubElement(def_list, 'default')
                    def_list.default[def_index] = default_value[def_index]['default']
                    for attr_index in default_attr[def_index]:
                        if default_attr[def_index][attr_index]:
                            def_list.default[def_index].set(
                                attr_index,
                                str(default_attr[def_index][attr_index]))
            self._build_functions(sub_elem, db_row)

            return sub_elem
        return parent

    def create_conf_table(self, parent, db_row, unfiltered=True):
        """Add config_tables to XML object"""
        attributes = ['table_name', 'num_indices']
        elements = ['description_user', 'description_internal', 'index1',
                    'index2', 'index3', 'oid', 'source_reference',
                    'is_internal']
        if not unfiltered:
            elements.remove('description_internal')
        sub_elem = objectify.SubElement(parent, 'config_table')
        elem_path = self._build_xml_attributes(attributes, sub_elem, db_row)
        elem_path = self._build_xml_elements(elements, sub_elem, db_row)
        # get index values
        indexes = self.db.get_table_index_by_table_uid(db_row.table_uid)
        for index in indexes:
            data = index.config_elements.name
            sub_elem.__setattr__(f'index{index.position}', data)
        return sub_elem

    def write_xml(self, root, file_path='hydracore_config.sdb'):
        """Write the lxml objectify object to an XML file"""
        # remove lxml annotation
        objectify.deannotate(root)
        etree.cleanup_namespaces(root)

        obj_xml = etree.tostring(root,
                                 pretty_print=True,
                                 xml_declaration=True)
        with open(file_path, 'wb') as xml_writer:
            xml_writer.write(obj_xml)
        print(f'XML file written to: {file_path}')

    @staticmethod
    def _build_xml_attributes(xml_attrs, xml_element, db_info):
        """Construct the attributes for a given XML element"""
        for column in db_info.__table__.columns.keys():
            if column in xml_attrs:
                data = db_info.__getattribute__(column)
                if data or data == 0:
                    xml_element.set(column,
                                    str(db_info.__getattribute__(column)))
        return xml_element

    @staticmethod
    def _build_xml_elements(xml_elements, xml_element, db_info):
        """Construct the element to a given XML parent"""
        for column in db_info.__table__.columns.keys():
            if column in xml_elements:
                data = db_info.__getattribute__(column)
                if data or data == 0:
                    xml_element.__setattr__(column, data)
        return xml_element

    def _build_xml_default_string(self, config_element):
        """Construct the default string to write into XML default field"""
        attributes = ['index1', 'index2', 'index3']
        elements = ['default']
        elem_uid = config_element.elem_uid
        default_rows = self.db.get_default_by_elem_uid(elem_uid=elem_uid)
        if len(default_rows) > 1:
            attrs = []
            values = []
            for this_row in default_rows:
                attr1 = this_row.__getattribute__("index1_value")
                attr2 = this_row.__getattribute__("index2_value")
                attr3 = this_row.__getattribute__("index3_value")
                attrs.append({'index1': attr1,
                              'index2': attr2,
                              'index3': attr3})
                values.append({'default': self._create_default_string(
                    this_row, config_element)})
            return attrs, values
        elif len(default_rows) > 0:
            attr = None
            value = self._create_default_string(default_rows[0], config_element)
            return attr, value
        else:
            return None, None

    def _create_default_string(self, row, config_element):
        struct_lookup = self.db.get_structure_elements(config_element.type)
        is_struct = len(struct_lookup) > 0

        is_octet_str = 'octet_string' in config_element.type
        data = self.db._row2dict(row)
        if data['value_int'] is not None:
            if 'int' in config_element.type:
                return data['value_int']
            # elif 'int32' in config_element.type:
            #     return f'0x{data["value_int"]:08x}'
            elif is_octet_str:
                return f'[ {data["value_int"]:02X} ]'
            else:
                return self._enum_label_from_value(data['value_int'],
                                                   config_element.type)

        elif data['value_string']:
            return data['value_string']
        else:
            array_values = self.db.get_default_array_by_def_uid(
                row.def_uid)
            if config_element.is_array and config_element.type == 'uint16':
                return self._uint16_array(array_values)
            elif is_octet_str and config_element.format is None:
                return self._octet_string(array_values)
            elif is_octet_str and config_element.format == 'mac_addr':
                return self._mac_addr_str(array_values)
            elif is_struct:
                return self._struct_array(array_values,
                                          config_element.type,
                                          config_element.is_array)
            return self.db.get_default_array_by_def_uid(row.def_uid)

    def _enum_label_from_value(self, value, enum_name):
        sub_query = self.db.session.query(
            sdb.EnumDefs.enum_uid).filter_by(enum_name=enum_name)
        result = self.db.session.query(
            sdb.EnumEntry.enum_label).filter(
                sdb.EnumEntry.enum_uid.in_(sub_query)).filter_by(
                    enum_value=value).one()
        return result[0]

    def _octet_string(self, values):
        result = '['
        for value in values:
            result = result + f'{value:02X} '
        result = result[:-1] + ']'
        return result

    def _struct_array(self, array_values, ce_type, is_array):
        struct_types = self._get_type_for_default(ce_type)
        return self._build_struct(struct_types, array_values, is_array)

    def _build_struct(self, structure, raw_data, is_array):
        def_str = '{' if is_array else ''
        while len(raw_data) > 0:
            def_str, raw_data = self._build_list(structure,
                                                 def_str,
                                                 raw_data)
        struct_string = def_str.replace(', }', '}').rstrip(', ')
        if is_array:
            struct_string += '}'
        return struct_string

    def _build_list(self, var_list, string, raw_data):
        string += '{'

        for var in var_list:
            if isinstance(var, list):
                ret_string, raw_data = self._build_list(var, '', raw_data)
                string += ret_string
            elif var == 'integer':
                string += f'{raw_data.pop(0)}, '
            elif self.db.get_enum_entries_for_enum_name(var):
                string += f'{self._enum_label_from_value(raw_data.pop(0), var)}, '

            else:
                raise TypeError(f'Unsupported type {var} being'
                                f' added to default_arrays table')
        string += '}, '
        return string, raw_data

    def _build_functions(self, parent, db_row):
        func_attrs = ['function_name', 'type', 'is_for_vif']
        elem_uid = db_row.config_elements.elem_uid
        functions = self.db.get_functions_by_elem_uid(elem_uid=elem_uid)
        if len(functions) > 1:
            parent = objectify.SubElement(parent, 'function_list')
        for func in functions:
            sub_elem = objectify.SubElement(parent, 'function')
            self._build_xml_attributes(func_attrs, parent, func)

    def _int_array(self, array_values):
        result = '{'
        for value in array_values:
            result = result + f'{value}, '
        result = result[:-2]
        result = result + '}'
        return result

    def _uint16_array(self, array_values):
        result = '{'
        for value in array_values:
            result = result + f'{value:#06x}, '
        result = result[:-2]
        result = result + '}'
        return result

    def _get_type_for_default(self, ce_type, type_list=None):
        """
        Recursively look up what type structure_elements while they are a
        struct_def or enum_def
        """
        return self.db.get_type_for_default(ce_type, type_list)[0]

    def _mac_addr_str(self, values):
        mac_str = (f'{values[0]:02x}:'
                   f'{values[1]:02x}:'
                   f'{values[2]:02x}:'
                   f'{values[3]:02x}:'
                   f'{values[4]:02x}:'
                   f'{values[5]:02x}')
        return mac_str
