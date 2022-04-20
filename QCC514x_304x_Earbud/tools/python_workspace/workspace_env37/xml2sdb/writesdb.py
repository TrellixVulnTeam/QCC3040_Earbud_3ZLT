"""
Collection of classes and function to read HydraMeta configuration information
from XML files
"""
from datetime import datetime
from hashlib import md5
from re import sub, findall

from lxml import etree, objectify

from xml2sdb import sdb


class Database:
    """
    Class for reading HydraConfig format XML files and writing them to a
    database
    """

    def __init__(self, sdb_file,
                 create_new_sdb=False,
                 xml_xsd='HydraMeta.xsd'):
        # two letter variable name OK for a database
        # pylint: disable=invalid-name
        self.db = sdb.ConnectDB(sdb_file)
        # pylint: enable=invalid-name

        if create_new_sdb:
            self.db.create_new()
        self.xml_xsd = xml_xsd
        xmlschema_doc = etree.parse(str(self.xml_xsd))
        xmlschema = etree.XMLSchema(xmlschema_doc)
        self.parser = objectify.makeparser(schema=xmlschema,
                                           remove_blank_text=False)
        self.xml_doc = None
        self._data = {'metadata': [],
                      'enum_def': [],
                      'enum_entry': [],
                      'struct_def': [],
                      'structure_element': [],
                      'config_element': [],
                      'config_tables': [],
                      'ct_indices': []}

    def add_metadata(self, element):
        """
        Add the information from the XML metadata element into the database
        """
        build_dt = element.attrib.get('build_date_time')
        if build_dt:
            build_dt = self._process_date_time(build_dt)
            element.attrib.pop('build_date_time')
        this_subsys = sdb.SubsystemFirmwareVersions(**element.attrib)
        if build_dt:
            this_subsys.build_date_time = build_dt
        self._data['metadata'].append(this_subsys)
        self.db.session.add(this_subsys)

    def add_enum_def(self, element):
        """
        Add the information from the XML enum_def element into the database
        """
        this_enum_def = sdb.EnumDefs(**element.attrib)
        self.db.session.add(this_enum_def)
        self._data['enum_def'].append(this_enum_def)

    def add_enum_entry(self, element):
        """
        Add the information from the XML enum_entry element into the database
        """
        this_enum_entry = sdb.EnumEntry(**element.attrib)
        this_enum_entry.enum_def = self._data['enum_def'][-1]
        self.db.session.add(this_enum_entry)
        self._data['enum_entry'].append(this_enum_entry)

    def add_bool_enum(self):
        """
        Check if a 'boolean' enum_def and enum_entry already exists in
        database, if not then add a definition for one.
        """
        existing_names = []
        # Test if boolean enum_def is in the SDB
        bool_in_db = [row for row in self.db.session.query(
            sdb.EnumDefs).filter_by(enum_name='boolean')]
        if not bool_in_db:
            path = objectify.ObjectPath('metadata_list.metadata.enum_def')
            try:
                existing_names = [el.attrib.get('enum_name')
                                  for el in path.find(self.xml_doc)]
            except AttributeError:
                existing_names = []
        if 'boolean' not in existing_names and not bool_in_db:
            path = objectify.ObjectPath('metadata_list.metadata')
            element = path.find(self.xml_doc)
            def_elem = objectify.SubElement(element, 'enum_def')
            def_elem.set('enum_name', 'boolean')

            ent_elem_t = objectify.SubElement(def_elem, 'enum_entry')
            ent_elem_t.set('enum_label', 'true')
            ent_elem_t.set('enum_value', '1')

            ent_elem_f = objectify.SubElement(def_elem, 'enum_entry')
            ent_elem_f.set('enum_label', 'false')
            ent_elem_f.set('enum_value', '0')
            # self.add_enum_def(def_elem)
            # self.add_enum_entry(ent_elem_t)
            # self.add_enum_entry(ent_elem_f)

    def add_struct_def(self, element):
        """
        Add the information from the XML struct_def element into the database
        """
        struct_name = element.get("struct_name")

        # Work out the hash value to see if we need to add this:
        se_values = []
        ce_values = []
        sub_elements = self._get_struct_elements_under(element)
        for sub_elem in sub_elements:
            se_dict = self._get_struct_elements_se_values(sub_elem)
            se_values.extend(self._order_values_for_hash(
                se_dict, sdb.StructureElements))
            ce_dict = self._get_struct_elements_ce_values(sub_elem)
            ce_values.extend(self._order_values_for_hash(
                ce_dict, sdb.ConfigElements))
        hash_value = self._generate_hash_value(
            [struct_name] + se_values + ce_values)
        # if this is new data then add it into the database
        if not self.db.is_hash_in_table(hash_value, sdb.StructDefs):
            struct_def_values = self._get_struct_def_values(element)
            struct_def_values['hash'] = hash_value
            this_struct_def = sdb.StructDefs(**struct_def_values)
            self.db.session.add(this_struct_def)
            self._data['struct_def'].append(this_struct_def)

    def add_structure_element(self, element):
        """
        Add the information from the XML structure_element element into the
         database
        """
        se_values = self._get_struct_elements_se_values(element)
        ce_values = self._get_struct_elements_ce_values(element)

        # Test if config_element entry exists
        ce_for_hash = self._order_values_for_hash(ce_values,
                                                  sdb.ConfigElements)
        hash_value = self._generate_hash_value(ce_for_hash)

        if not self.db.is_hash_in_table(hash_value, sdb.ConfigElements):
            # Does exist already so add config element entry
            ce_values['hash'] = hash_value
            this_config_elem = sdb.ConfigElements(**ce_values)
            self.db.session.add(this_config_elem)
            self._data['config_element'].append(this_config_elem)
            # Add SubfwElement for this config element
            enum_row = self.db.session.query(sdb.EnumDefs).filter_by(
                enum_name=this_config_elem.type
            ).first()
            struct_row = self.db.session.query(sdb.StructDefs).filter_by(
                struct_name=this_config_elem.type
            ).first()
            if enum_row:
                this_subfw_elem = sdb.SubfwElements(
                    subsystem_firmware_versions=self._data['metadata'][-1],
                    config_elements=this_config_elem,
                    enum_defs=enum_row
                )
                self.db.session.add(this_subfw_elem)
            elif struct_row:
                this_subfw_elem = sdb.SubfwElements(
                    subsystem_firmware_versions=self._data['metadata'][-1],
                    config_elements=this_config_elem,
                    struct_defs=struct_row
                )
                self.db.session.add(this_subfw_elem)
            else:
                this_subfw_elem = sdb.SubfwElements(
                    subsystem_firmware_versions=self._data['metadata'][-1],
                    config_elements=this_config_elem,
                )
                self.db.session.add(this_subfw_elem)
        else:
            # It exists so get the config_element row
            this_config_elem = self.db.row_with_hash(hash_value,
                                                     sdb.ConfigElements)

        # Add structure element entry
        this_struct_elem = sdb.StructureElements(**se_values)
        this_struct_elem.struct_defs = self._data['struct_def'][-1]
        this_struct_elem.config_elements = this_config_elem
        self.db.session.add(this_struct_elem)
        self._data['structure_element'].append(this_struct_elem)

    def add_config_element(self, element):
        """
        Add the information from the XML config_element element into the
        database
        """
        # Collect data
        config_elem_dict = self._get_config_elem_values(element)
        default_list = self._get_defaults_under(element)
        table_name = self._get_table_under(element)
        functions = self._get_functions_under(element)

        # Work out the hash value to see if we need to add this entry
        elem_values = self._order_values_for_hash(
            config_elem_dict, sdb.ConfigElements)
        hash_value = self._generate_hash_value(elem_values)
        if not self.db.is_hash_in_table(hash_value, sdb.ConfigElements):
            config_elem_dict['hash'] = hash_value
            this_config_elem = sdb.ConfigElements(**config_elem_dict)
            self.db.session.add(this_config_elem)
            self._data['config_element'].append(this_config_elem)

            if functions:
                self.add_functions(functions, this_config_elem)
            for default in default_list:
                self.add_default(default[0], default[1], this_config_elem)

        else:
            this_config_elem = self.db.row_with_hash(hash_value,
                                                     sdb.ConfigElements)

        enum_row = self.db.session.query(sdb.EnumDefs).filter_by(
            enum_name=this_config_elem.type).first()
        struct_row = self.db.session.query(sdb.StructDefs).filter_by(
            struct_name=this_config_elem.type).first()

        if table_name:
            # make sure the config_table referenced exists
            self.ensure_config_table_exists(table_name, element)
            this_subfw_elem = sdb.SubfwElements(
                subsystem_firmware_versions=self._data['metadata'][-1],
                config_elements=this_config_elem,
                config_tables=self._data['config_tables'][-1]
            )
            self.db.session.add(this_subfw_elem)

        elif enum_row:
            this_subfw_elem = sdb.SubfwElements(
                subsystem_firmware_versions=self._data['metadata'][-1],
                config_elements=this_config_elem,
                enum_defs=enum_row
            )
            self.db.session.add(this_subfw_elem)

        elif struct_row:
            this_subfw_elem = sdb.SubfwElements(
                subsystem_firmware_versions=self._data['metadata'][-1],
                config_elements=this_config_elem,
                struct_defs=struct_row
            )
            self.db.session.add(this_subfw_elem)
        else:
            this_subfw_elem = sdb.SubfwElements(
                subsystem_firmware_versions=self._data['metadata'][-1],
                config_elements=this_config_elem,
            )
            self.db.session.add(this_subfw_elem)

    def add_functions(self, functions, config_elem):
        """
        Add the information from the XML function or function_list elements
        that are under a config_element element into the database
        """
        for func_values in functions:
            local_values = dict(func_values).copy()
            for tag, data in local_values.items():
                if tag == 'is_for_vif':
                    local_values[tag] = data == 'true'
            local_values['config_element'] = config_elem

            this_func = sdb.Functions(**local_values)
            self.db.session.add(this_func)

    def add_default(self, default_text, default_attrib, config_elem):
        """
        Add the information from the XML default or default_list elements
        that are under a config_element element into the database

        """
        # Collect parameters to work out the heuristics of the default string
        ce_type = config_elem.type
        ce_format = config_elem.format
        is_array = config_elem.is_array
        clean_attrib = self._clean_ce_def_cols(default_attrib)
        enum_lookup = self.db.get_enum_entries_for_enum_name(ce_type)
        is_enum = len(enum_lookup) > 0
        struct_lookup = self.db.get_structure_elements(ce_type)
        is_struct = len(struct_lookup) > 0
        is_octet_str = ce_type == 'octet_string'

        if all([not is_enum, not is_struct, not is_octet_str]):
            # Assume this value is and int of some kind
            int_values = self._parse_int_value(default_text, ce_type,
                                               ce_format, is_array)
        elif all([not is_enum, not is_struct, is_octet_str,
                  ce_format != 'unicode_string']):
            int_values = self._parse_octet_string(default_text, ce_type,
                                                  ce_format, is_array)
        elif all([not is_enum, not is_struct,
                  is_octet_str, ce_format == 'unicode_string']):
            int_values = self._parse_unicode_string(default_text, ce_type,
                                                    ce_format, is_array)
        elif all([is_enum, not is_struct, not is_octet_str]):
            int_values = self._parse_enum_default(default_text,
                                                  enum_lookup,
                                                  is_array)
        elif all([not is_enum, is_struct, not is_octet_str]):
            int_values = self._parse_struct_default(default_text,
                                                    ce_type,
                                                    is_array)

        # Now we have the data, add it to the database
        if isinstance(int_values, str):
            inst_values = {'config_elements': config_elem,
                           'value_string': int_values}
            all_values = {**inst_values, **clean_attrib}
            this_default = sdb.ConfigElementDefaultValues(**all_values)
            self.db.session.add(this_default)
            this_subfw_def = sdb.SubfwDefaults(
                subsystem_firmware_versions=self._data['metadata'][-1],
                config_element_default_values=this_default
            )
            self.db.session.add(this_subfw_def)
        elif len(int_values) == 1:
            inst_values = {'config_elements': config_elem,
                           'value_int': int(int_values[0])}
            all_values = {**inst_values, **clean_attrib}
            this_default = sdb.ConfigElementDefaultValues(**all_values)
            self.db.session.add(this_default)
            this_subfw_def = sdb.SubfwDefaults(
                subsystem_firmware_versions=self._data['metadata'][-1],
                config_element_default_values=this_default
            )
            self.db.session.add(this_subfw_def)

        else:
            this_default = sdb.ConfigElementDefaultValues(
                config_elements=config_elem)
            self.db.session.add(this_default)
            this_subfw_def = sdb.SubfwDefaults(
                subsystem_firmware_versions=self._data['metadata'][-1],
                config_element_default_values=this_default
            )
            self.db.session.add(this_subfw_def)

            for array_item in int_values:
                this_def_array = sdb.DefaultArrays(
                    config_element_default_values=this_default,
                    value=array_item
                )
                self.db.session.add(this_def_array)

    def add_config_table(self, element):
        """
        Add the information from the XML config_table element into the
        database
        """
        local_values = dict(element.attrib).copy()
        for sub_elems in element.getchildren():
            local_values[sub_elems.tag] = sub_elems.text
        ct_values = self._remove_keys(local_values, sdb.ConfigTables)
        for tag, data in ct_values.items():
            if tag.startswith('description'):
                ct_values[tag] = self._clean_string(data)
        this_config_table = sdb.ConfigTables(**ct_values)
        self.db.session.add(this_config_table)
        self._data['config_tables'].append(this_config_table)
        for position in ['index1', 'index2', 'index3']:
            try:
                idx_val = element.__getattribute__(position).attrib
            except AttributeError:
                break
            if idx_val:
                ce_obj = self.db.session.query(
                    sdb.ConfigElements).filter_by(
                        name=idx_val['name']).first()

                this_ct_indices = sdb.ConfigTableIndices(
                    config_table=this_config_table,
                    config_elements=ce_obj,
                    position=position[-1]
                )
                self.db.session.add(this_ct_indices)
                self._data['ct_indices'].append(this_ct_indices)

    def _get_struct_def_values(self, struct_def):
        """
        Return dictionary of struct_def element that that only contain the
        values for columns used in the struct_defs database table
        """
        local_values = dict(struct_def.attrib).copy()
        struct_values = self._remove_keys(local_values, sdb.StructDefs)
        return struct_values

    def _get_struct_elements_under(self, struct_def):
        """
        Return all the structure_elements under the given struct_def
        """
        return struct_def.getchildren()

    def _get_struct_elements_ce_values(self, struct_elem):
        """
        Return dictionary of values to add to the config_element table
        for the given structure_element entry
        """
        local_values = self._get_struct_elem_values(struct_elem)
        elem_values = self._remove_keys(local_values, sdb.ConfigElements)
        return elem_values

    def _get_struct_elements_se_values(self, struct_elem):
        """
        Return dictionary of values to add to the structure_element table for
        for the give structure_element element
        """
        local_values = self._get_struct_elem_values(struct_elem)
        struct_values = self._remove_keys(local_values, sdb.StructureElements)
        return struct_values

    def _get_struct_elem_values(self, struct_elem):
        needs_clean = ['description_user', 'description_internal', 'label']
        local_values = dict(struct_elem.attrib).copy()
        for sub_elems in struct_elem.getchildren():
            local_values[sub_elems.tag] = sub_elems.text
        for tag, data in local_values.items():
            if 'range' in tag:
                local_values[tag] = int(local_values[tag])
            elif tag == 'is_array':
                if data.isdigit():
                    local_values[tag] = bool(int(data))
                else:
                    local_values[tag] = data == 'true'
            elif tag in needs_clean:
                local_values[tag] = self._clean_string(data)
        return local_values

    def _get_config_elem_values(self, config_elem):
        """
        For a given config_element, find all the values in and under it
        for adding into the database
        """
        needs_clean = ['description_user', 'description_internal', 'label']
        default_list = []
        local_values = dict(config_elem.attrib).copy()

        for sub_elems in config_elem.getchildren():
            if sub_elems.tag == 'default_list':
                for i in range(config_elem.default_list.countchildren()):
                    default_list.append(
                        [config_elem.default_list.default[i],
                         config_elem.default_list.default[i].attrib])

            if sub_elems.text:
                local_values[sub_elems.tag] = sub_elems.text
        for tag, data in local_values.items():
            if tag == 'is_internal':
                local_values[tag] = data == 'true'
            elif tag == 'is_array':
                if data.isdigit():
                    local_values[tag] = bool(int(data))
                else:
                    local_values[tag] = data == 'true'
            elif tag in needs_clean:
                local_values[tag] = self._clean_string(data)
        return self._remove_keys(local_values, sdb.ConfigElements)

    def _get_defaults_under(self, config_elem):
        """
        Find the defaults that are set within a config_element element
        """
        default_list = []
        try:
            local_values = dict(config_elem.default.attrib).copy()
            default_list.append([config_elem.default.text, local_values])
        except AttributeError:
            for sub_elems in config_elem.getchildren():
                if sub_elems.tag == 'default_list':
                    for i in range(config_elem.default_list.countchildren()):
                        default_list.append(
                            [config_elem.default_list.default[i].text,
                             config_elem.default_list.default[i].attrib])
        return default_list

    def _get_table_under(self, config_elem):
        """
        Return the table_name element text for given config_element
        """
        try:
            return config_elem.table_name.text
        except AttributeError:
            return None

    def _get_functions_under(self, config_elem):
        """
        Return the information for function elements defined under a
        config_element
        """
        functions = []
        for sub_elems in config_elem.getchildren():
            if sub_elems.tag == 'function':
                functions.append(config_elem.function.attrib)
            if sub_elems.tag == 'function_list':
                for i in range(config_elem.function_list.countchildren()):
                    functions.append(
                        config_elem.function_list.function[i].attrib)
        return functions

    @staticmethod
    def _process_date_time(date_str):
        """
        Take date and time str and return a Python datetime object
        """
        # Remove seconds from time
        return datetime.strptime(date_str[:16], '%Y-%m-%d %H:%M')

    @staticmethod
    def _remove_keys(input_dict, db_model):
        """
        Return a new dictionary with only keys required for the
        given table model.
        """
        wanted_keys = db_model.__table__.columns.keys()
        result = {}
        for key in input_dict:
            if key in wanted_keys:
                result[key] = input_dict[key]
        return result

    @staticmethod
    def _clean_string(data):
        """
        For backwards compatibility with the previous implementation in Perl,
        there is a need to do special processing on some strings
        """
        data = sub(r'^\s*\n+', '', data)
        data = sub(r'\n\s+$', '', data)
        data = sub(r'^\s+$', '', data)
        data = sub(r'^\\n', '', data)
        data = data.rstrip()
        if data.isspace():
            data = None
        return data

    @staticmethod
    def _clean_ce_def_cols(attrib_dict):
        """
        Handle the special case on the config_element data where the element
        keys in the XML schema do not match the keys in the SDB schema.
        Returns a new dictionary with mapped keys names
        """
        result = {}
        for k in attrib_dict:
            if k.startswith('index'):
                result[f'{k}_value'] = attrib_dict[k]
            else:
                result[k] = attrib_dict[k]
        return result

    @staticmethod
    def _generate_hash_value(data):
        """
        Utility method to generate and md5 hash for a given set of values.
        Replaces "None" values with empty strings before creating the hash
        value
        """
        hash_data = ''.join([str(x) if x is not None else '' for x in data])
        hash_obj = md5()
        hash_obj.update(hash_data.encode('utf-8'))
        return hash_obj.hexdigest()

    @staticmethod
    def _order_values_for_hash(unsorted_dict, table):
        """
        Use the order of the columns in the database schema to order values
        from a dictionary in readiness for being hashed.
        This is designed to protect against Python dictionaries are designed
        not to be guarantee order
        """
        result = []
        column_order = list(table.__table__.columns.keys())
        for column in column_order:
            result.append(unsorted_dict.get(column))
        return result

    @staticmethod
    def _parse_int_value(value, ce_type, ce_format=None, is_array=False):
        """
        The HydraConfig.xsd defines all values for the default element as
        string. If they should be of type integer, then process the
        string so an integer (or list of integers if an array) are returned
        """
        result = []
        if is_array:
            values_list = Database._process_array(value)
            for array_item in values_list:
                if array_item.startswith('0x'):
                    result.append(int(array_item, 16))
                else:
                    result.append(int(array_item))
        else:
            if value.startswith('0x'):
                result.append(int(value, 16))
            else:
                result.append(int(value))
        return result

    @staticmethod
    def _parse_octet_string(value, ce_type, ce_format=None, is_array=False):
        """
        The HydraConfig.xsd defines all values for the default element as
        string. If they should be of type `octet_string`, then process
        the string so a list of integers are returned
        """
        result = []
        values_list = findall(r'[0-9A-Fa-f]{2}', value)
        for array_item in values_list:
            result.append(int(array_item, 16))
        return result

    @staticmethod
    def _parse_unicode_string(value, ce_type, ce_format=None, is_array=False):
        """
        The HydraConfig.xsd defines all values for the default element as
        string. If they should be of type of `octet_string` and format
        'unicode_string', then it just needs a bit of string clean-up
        and returned
        """
        return Database._clean_string(str(value))

    @staticmethod
    def _parse_enum_default(value, enum_entries, is_array=False):
        """
        If a config_element has default type of enum, then we need to treat
        the default values as the enum_labal and return the enum_value which
        is an integer.
        """
        result = []
        if is_array:
            raise NotImplementedError('The default is an array of enum_defs'
                                      'and I do not know what to do with that')
        else:
            for look_up in enum_entries:
                if look_up.enum_label == value:
                    result.append(int(look_up.enum_value))
        return result

    def _parse_struct_default(self, value, structure_elements, is_array=False):
        """
        If a config_element has a default type which is a struct, then this
        method looks up each struct_element and finds its type. This might
        require a recursive look-up until a primary type is found.
        The struct_defs appear in the order of the msb_bit_position.
        (i.e. the other end of the word from the lsb_bit_position specified in
        the database).
        """
        result = []
        # Make flat list
        flat_array_values = Database._process_array(value)
        flat_struct_types = self._get_type_for_default(structure_elements, [])

        if is_array:
            full_len_types = flat_struct_types * (len(flat_array_values)
                                                  // len(flat_struct_types))
        else:
            full_len_types = flat_struct_types
        matched_type_values = zip(flat_array_values, full_len_types)
        for s_value, s_type in matched_type_values:
            enum_lookup = self.db.get_enum_entries_for_enum_name(s_type)
            if enum_lookup:
                result.append(self._parse_enum_default(s_value, enum_lookup)[0])
            elif s_type == 'integer':
                if s_value.startswith('0x'):
                    result.append(int(s_value, 16))
                else:
                    result.append(int(s_value))
            else:
                raise TypeError(f'Unsupported type {s_type} being'
                                f' added to default_arrays table')

        return result

    def _get_type_for_default(self, ce_type, type_list=None):
        """
        Recursively look up what type structure_elements while they are a
        struct_def or enum_def
        """
        # flatten the returned list.
        def flatten_lists(hlist):
            for el in hlist:
                if isinstance(el, list) and not isinstance(el, (str, bytes)):
                    yield from flatten_lists(el)
                else:
                    yield el

        return [this_item for this_item in flatten_lists(
            self.db.get_type_for_default(ce_type, type_list))]


    @staticmethod
    def _process_array(value_string):
        """
        One of the processing steps to extract the value(s) from the
        "default" XML value in the HydraMeta format. Removes linebreaks,
        comments, curly braces in the string and splits the values by ",".
        Then returns those strings as a python list
        e.g. Takes this:
        "{
                        256, /* max_tx_payload_size. */
                        TRUE, /* reliable_stream. */
                        3, /* priority. */
                        0 /* unused - must be zero. */
                      }"
        and returns:
        ['256', 'TRUE', '3', '0']
        """
        value_string = value_string.strip()
        # value_string = value_string.strip('{').strip('}')
        value_string = sub(r'\{', r'', value_string)
        value_string = sub(r'\}', r'', value_string)
        value_string = sub(r'/\*.*\*/', r'', value_string)
        value_string = sub('\n', r'', value_string)
        value_string = sub(r'\\', r'', value_string)
        # value_string = value_string.strip('{')
        # value_string = value_string.strip('}')
        value_string = value_string.replace(' ', '')
        # # array_list = array_list.strip('\\\n')
        value_string = value_string.split(',')
        # array_list = [values.strip('\\\n') for values in array_list]
        return value_string

    def commit(self):
        """
        Commit pending database changes to disk
        """
        self.db.session.commit()

    def ensure_config_table_exists(self, table_name, ce_element):
        """
        Look ahead to see if the config_table is defined in the database.
        If it does not, then get the information from the XML definition and
        create it in the database
        """
        row = self.db.session.query(sdb.ConfigTables).filter_by(
            table_name=table_name).first()
        if not row:
            md_element = ce_element.getparent()
            ct_element = md_element.xpath('config_table')
            for config_table in ct_element:
                self.add_config_table(config_table)

    def parse(self, xml_file, verify_xml=True):
        """
        Read, verify and store XML data in an internal Python model
        """
        xml_string = self.read_xml_file(xml_file)
        self.make_xml_object(xml_string, verify_xml)

        # Add Boolean enum if it does not exist
        self.add_bool_enum()

    def write_to_sdb(self):
        """
        Iterates over teh internal Python model of the XML files and writes
        the information to a Database
        """
        for mdata in self.xml_doc.iter('metadata_list'):
            for metadata in mdata.iterchildren('metadata'):
                # print(f'metadata {metadata.attrib}')
                self.add_metadata(metadata)
                for enum_def in metadata.iterchildren('enum_def'):
                    # print(f'enum_def {enum_def.attrib}')
                    self.add_enum_def(enum_def)
                    for enum_entry in enum_def.iterchildren('enum_entry'):
                        # print(f'enum_entry {enum_entry.attrib}')
                        self.add_enum_entry(enum_entry)
                for struct_def in metadata.iterchildren('struct_def'):
                    # print(f'struct_def {struct_def.attrib}')
                    self.add_struct_def(struct_def)
                    for structure_element in struct_def.iterchildren('structure_element'):
                        # print(f'structure_element {structure_element.attrib}')
                        self.add_structure_element(structure_element)
                for config_element in metadata.iterchildren('config_element'):
                    # print(f'config_element {config_element.attrib}')

                    self.add_config_element(config_element)
        self.commit()

    def make_xml_object(self, xml_string, verify_xml=True):
        """
        Parse XML string. Can be parsed with or without schema checking against
        XSD file. Default is to check against schema.
        Stores the result in an internal Python model ready for writing to
        database
        """
        if verify_xml:
            self.xml_doc = objectify.fromstring(xml_string, self.parser)
        else:
            self.xml_doc = objectify.fromstring(xml_string)

    def read_xml_file(self, xml_file):
        """
        Read XML file and returns string of document ready to pass to parser
        """
        print(f'Reading {xml_file}')
        with open(str(xml_file)) as xml_fh:
            xml = xml_fh.readlines()
            xml_string = ''.join(xml).encode('utf-8')
        return xml_string
