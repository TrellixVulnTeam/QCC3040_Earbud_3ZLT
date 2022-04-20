"""
Writing information to database file
"""
from sqlalchemy import (Column, Integer, VARCHAR, Text, Boolean,
                        ForeignKey, text)
from sqlalchemy import create_engine
from sqlalchemy import func
from sqlalchemy.dialects.sqlite import DATETIME
from sqlalchemy.ext.declarative import declarative_base
from sqlalchemy.orm import sessionmaker, relationship

# SQLalchemy expected naming convention
# pylint: disable=invalid-name
Base = declarative_base()
# pylint: enable=invalid-name

# Define datetime format to match previous perl implementation
# pylint: disable=invalid-name
dt = DATETIME(storage_format="%(year)04d-%(month)02d-%(day)02d "
                             "%(hour)02d:%(minute)02d",
              regexp=r"(\d+)-(\d+)-(\d+) (\d+):(\d+)"
              )
# pylint: enable=invalid-name


class ConnectDB:
    """
    Comment to the database object
    """
    def __init__(self, db_filename, memory_only=False, echo=False):
        if memory_only:
            self.engine = create_engine('sqlite:///:memory:', echo=echo)
        else:
            self.engine = create_engine(
                'sqlite:///{}'.format(db_filename),
                echo=echo)
        # pylint: disable=invalid-name
        Session = sessionmaker(bind=self.engine)
        # pylint: enable=invalid-name
        self.session = Session()

    def create_new(self):
        """
        Create empty tables in the specified database
        """
        Base.metadata.drop_all(self.engine)
        Base.metadata.create_all(self.engine)
        self.add_schema_version(3, 1, '')

    def get_subsystems(self):
        """Return the rows from the subsystem_firmware_versions table"""
        return [row for row in self.session.query(
            SubsystemFirmwareVersions).all()]

    def get_subsys_firm_ver(self, subsystem_name, fw_version,
                            fw_layer=None, fw_variant=None):
        """
        For the specified parameters, return the database row
        """
        queries = self.session.query(SubsystemFirmwareVersions, func.max(
            SubsystemFirmwareVersions.subfw_uid))
        if subsystem_name:
            queries = queries.filter_by(subsystem_name=subsystem_name)
        if fw_version:
            queries = queries.filter_by(version=fw_version)
        if fw_layer:
            queries = queries.filter_by(subsystem_layer=fw_layer)
        if fw_variant:
            queries = queries.filter_by(variant=fw_variant)
        return queries.one().SubsystemFirmwareVersions

    def get_subsystem_firmware_versions(self, subfw_uid):
        """
        Return the rows from subsystem_firmware_versions table with the given
        subfw_uid
        """
        # "SELECT version FROM subsystem_firmware_versions
        # WHERE subfw_uid=$subfw_uid";
        return [r.version for r in
                self.session.query(
                    SubsystemFirmwareVersions.version).filter_by(
                        subfw_uid=subfw_uid
                    )]

    def add_chip_data(self, data):
        """
        Add chip specification into database
        """
        # system_uid = self.get_last_system_uid()
        sub_sys_info = data['subsystem_info']
        # metadat_hash = self.calculate_metadata_hash(sub_sys_info)
        metadat_hash = 'no_implemented'
        # INSERT INTO system_versions (system_uid, chip_name, chip_id,
        # rom_name, rom_version, patch_release_level, system_version_label,
        # system_description, customer_name, system_release_date_time,
        # metadata_hash) ";

        sys_ver = SystemVersions(
            chip_name=data['chip_name'],
            chip_id=data['chip_id'],
            rom_name=data['rom_name'],
            rom_version=data['rom_version'],
            patch_release_level=data['patch_release_level'],
            system_version_label=data['system_version_label'],
            system_description=data['system_description'],
            customer_name=data['customer_name'],
            # system_release_date_time=build_dt_obj,
            system_release_date_time=data['system_release_date_time'],
            metadata_hash=metadat_hash)
        # INSERT INTO system_efuse_hashes (system_uid, efuse_hash) ";
        for efuse_hash in data['efuse_hash']:
            sys_efuse_hash = SystemEfuseHashes(system_versions=sys_ver,
                                               efuse_hash=efuse_hash)
        # "INSERT INTO system_subfw_versions (system_uid, subfw_uid)
        # VALUES ($system_uid, $subfw_uid)");
        sysfw_vers = []
        for sub_sys in sub_sys_info:
            ss_firm_ver = self.get_subsys_firm_ver(
                subsystem_name=sub_sys,
                fw_version=sub_sys_info[sub_sys]['fw_version'],
                fw_layer=sub_sys_info[sub_sys]['layer'],
                fw_variant=sub_sys_info[sub_sys]['fw_variant'])
            sysfw_vers.append(SystemSubfwVersions(
                system_versions=sys_ver,
                subsystem_firmware_versions=ss_firm_ver
            ))

        # "INSERT INTO patch_files (subfw_uid, patch_level, patch_hash)
        # VALUES ($subfw_uid, $patch_level, '$patch_hash')"
        # print(system_uid, subfw_uid)
        self.session.add_all([sys_ver,
                              sys_efuse_hash])
        self.session.add_all(sysfw_vers)
        self.session.commit()

    def add_schema_version(self, schema_version, minor_version, comment):
        """
        Add scheme information into the database
        """
        data_dict = {'schema_version': schema_version,
                     'minor_version': minor_version,
                     'comment': comment}
        if not self.get_instance(SchemaVersion, **data_dict):
            self.session.query(SchemaVersion).delete()
            schema_version = SchemaVersion(**data_dict)
            self.session.add(schema_version)
            self.session.commit()

    def get_instance(self, model, **data_dict):
        """Return first instance found."""
        return self.session.query(model).filter_by(**data_dict).first()

    def get_all(self, model):
        """Return all instances in a table"""
        return self.session.query(model).all()

    def get_enum_for_subfw_uid(self, subfw_uid):
        """Get all enum_defs for a given subfw_uid"""
        sub_query = self.session.query(
            SubfwElements.enum_uid).distinct().filter_by(
                subfw_uid=subfw_uid).subquery()
        return [row for row in self.session.query(
            EnumDefs).filter(EnumDefs.enum_uid.in_(sub_query))]

    def get_enum_entries_for_enum_uid(self, enum_uid):
        """Get all the enum_entries for a give enum_uid"""
        return [row for row in self.session.query(
            EnumEntry).filter_by(enum_uid=enum_uid).all()]

    def get_enum_entries_for_enum_name(self, enum_name):
        """Get all enum entries for a given enum_name"""
        sub_query = self.session.query(EnumDefs.enum_uid).filter_by(
            enum_name=enum_name)
        return [row
                for row in self.session.query(EnumEntry).filter(
                    EnumEntry.enum_uid.in_(sub_query))
                ]

    def get_functions_by_elem_uid(self, elem_uid):
        """Get all the functions for a given elem_uid"""
        return [row
                for row in self.session.query(
                    Functions).filter_by(elem_uid=elem_uid)]

    def get_struct_by_subfw_uid(self, subfw_uid):
        """Get all subfw_elements rows for a give subfw_uid"""
        sub_query = self.session.query(
            SubfwElements.struct_uid).distinct().filter_by(
                subfw_uid=subfw_uid).subquery()
        return [row for row in self.session.query(
            StructDefs).filter(StructDefs.struct_uid.in_(sub_query))]

    def get_struct_elem_by_struct_uid(self, struct_uid):
        """Get all structure_elements for a given struct_uid"""
        return [row for row in self.session.query(
            StructureElements).filter_by(struct_uid=struct_uid).all()]

    def get_structure_elements(self, struct_name):
        """
        Get all structure elements for a given struct_name.
        Returned in order (based on little-endian)
        i.e not the order specified by the lsb_bit_position
        """
        msb_unorder = {}
        result = []
        for row in self.session.query(StructureElements).join(
                ConfigElements).join(StructDefs).filter(
                    StructDefs.struct_name == struct_name):

            msb_unorder[self._msb_position(row.lsb_bit_position)] = row
        for msb_key in sorted(msb_unorder.keys()):
            result.append(msb_unorder[msb_key])
        return result

    def get_config_elem_by_subfw_uid(self, subfw_uid):
        """Get all subfw_elements for a given subfw_uid"""
        return [row for row in self.session.query(
            SubfwElements).filter_by(
                subfw_uid=subfw_uid).outerjoin(
                    ConfigTables).all()]

    def get_config_table_by_subfw_uid(self, subfw_uid):
        """Get all comfig_table rows for a give subfw_uid"""
        sub_query = self.session.query(
            SubfwElements.table_uid).filter_by(subfw_uid=subfw_uid)
        return [row for row in self.session.query(ConfigTables).filter(
            ConfigTables.table_uid.in_(sub_query))]

    def get_default_by_elem_uid(self, elem_uid):
        """
        For a given elem_uid, look in the config_element_default_values and
        default_arrays tables and return values associated with the elem_uid
        """
        non_array = self.session.query(ConfigElementDefaultValues).filter_by(
            elem_uid=elem_uid).all()
        return non_array

    def get_default_array_by_def_uid(self, def_uid):
        """Get all default_array rows for a given def_uid"""
        output = self.session.query(DefaultArrays.value).filter_by(
            def_uid=def_uid).all()
        # flatten the list before returning
        return list(sum(output, ()))

    def get_table_index_by_table_uid(self, table_uid):
        """Get all table indices for a given table_uid"""
        return [row
                for row in self.session.query(
                    ConfigTableIndices).filter_by(
                        table_uid=table_uid).join(ConfigElements)
                ]

    def get_type_for_default(self, ce_type, type_list=None):
        """
        Recursively look up what type structure_elements while they are a
        struct_def or enum_def
        """
        if type_list is None:
            type_list = []
        # get type
        enum_lookup = self.get_enum_entries_for_enum_name(ce_type)
        is_enum = len(enum_lookup) > 0
        struct_lookup = self.get_structure_elements(ce_type)
        is_struct = len(struct_lookup) > 0

        if is_enum:
            type_list.append(ce_type)
        elif is_struct:
            type_list.append([])
            for struct in struct_lookup:
                type_list[-1].extend(
                    self.get_type_for_default(
                        struct.config_elements.type)
                )
        else:
            type_list.append(ce_type)
        return type_list

    def row_with_hash(self, hash_value, table_model):
        """
        Return the row from a given table with matching hash value
        """
        return self.session.query(table_model).filter_by(
            hash=hash_value).one_or_none()

    def is_hash_in_table(self, hash_value, table_model):
        """
        Return a logic value for if the hash exists in the given table
        """
        rows = self.row_with_hash(hash_value, table_model)
        return rows is not None

    @staticmethod
    def _msb_position(lsb_bit_position):
        """
        Find the start bit position using little-endian rather than
        the big-endian in the database
        """
        # reverse bit ordering in a 16-bit word to ensure defaults are in the
        # correct order (MSB first within each word)
        word = int(lsb_bit_position) // 16
        msb_bit_position = 15 - (int(lsb_bit_position) % 16)
        msb_bit_position += (16 * word)
        return msb_bit_position

    @staticmethod
    def _row2dict(row):
        """
        Return a database row as a python dictionary
        """
        data = {}
        for column in row.__table__.columns:
            # data[column.name] = str(getattr(row, column.name))
            data[column.name] = getattr(row, column.name)
        return data

    def get_last_system_uid(self):
        """
        Get the last entry from the system_versions table
        """
        # "SELECT max(system_uid) FROM system_versions"
        return self.session.query(
            SystemVersions, func.max(
                SystemVersions.system_uid)).first().SystemVersions.system_uid

    def get_max_subfw_uid(self, subsystem_name, fw_version,
                          fw_variant=None, fw_layer=None):
        """
        For the given configuration, return the subfw_uid
        """
        # "SELECT max(subfw_uid) FROM subsystem_firmware_versions
        # WHERE subsystem_name='$subsystem' and version=$fw_version
        # and variant=$fw_variant";
        subsys_firm_ver = self.get_subsys_firm_ver(
            subsystem_name=subsystem_name,
            fw_version=fw_version,
            fw_variant=fw_variant,
            fw_layer=fw_layer)
        return subsys_firm_ver.subfw_uid


class SchemaVersion(Base):
    """
    Schema definition for Hydra schema_version table
    """
    __tablename__ = 'schema_version'

    schema_version = Column(Integer, primary_key=True, nullable=False)
    minor_version = Column(Integer, nullable=False)
    comment = Column(VARCHAR(100))


class SystemVersions(Base):
    """
    Schema definition for Hydra system_versions table
    """
    __tablename__ = 'system_versions'

    system_uid = Column(Integer, primary_key=True, nullable=True)
    chip_name = Column(VARCHAR(80))
    chip_id = Column(Integer)
    rom_name = Column(VARCHAR(80))
    rom_version = Column(Integer)
    patch_release_level = Column(Integer)
    system_version_label = Column(VARCHAR(80))
    system_description = Column(Text)
    customer_name = Column(VARCHAR(80))
    system_release_date_time = Column(dt)
    # an MD5 hash of the hashes of all elements relevant to this
    # combination of subfw_ids
    metadata_hash = Column(VARCHAR(33))


class SubsystemFirmwareVersions(Base):
    """
    Schema definition for Hydra subsystem_firmware_versions table
    """
    __tablename__ = 'subsystem_firmware_versions'

    subfw_uid = Column(Integer, primary_key=True, nullable=True)
    subsystem_name = Column(VARCHAR(20))
    subsystem_layer = Column(VARCHAR(20))
    subsystem_alias = Column(VARCHAR(20))
    # enum value from FILENAME_SUBSYSTEM_ID defined in filename.xml or
    # filename_prim.h
    subsystem_id = Column(Integer)
    version = Column(Integer)
    variant = Column(Integer, default=0)
    description = Column(Text)
    build_date_time = Column(dt)


class SystemSubfwVersions(Base):
    """
     Schema definition for Hydra systtem _subfw_versions table
    """
    __tablename__ = 'system_subfw_versions'

    system_uid = Column(Integer,
                        ForeignKey('system_versions.system_uid'),
                        primary_key=True, nullable=True,
                        )
    subfw_uid = Column(Integer,
                       ForeignKey('subsystem_firmware_versions.subfw_uid'),
                       primary_key=True, nullable=True)
    system_versions = relationship("SystemVersions")
    subsystem_firmware_versions = relationship("SubsystemFirmwareVersions")


class SystemEfuseHashes(Base):
    """
     Schema definition for Hydra system_efuse_Hashes table
    """
    __tablename__ = 'system_efuse_hashes'

    # rowid is a fake primary key to keep SQLalchemy
    rowid = Column(Integer, primary_key=True)
    system_uid = Column(Integer, ForeignKey('system_versions.system_uid'))
    system_versions = relationship("SystemVersions")
    efuse_hash = Column(Integer)


class PatchFiles(Base):
    """
    Schema dfinition for Hy
    """
    __tablename__ = 'patch_files'

    # rowid is a fake primary key to keep SQLalchemy
    rowid = Column(Integer, primary_key=True)
    subfw_uid = Column(Integer,
                       ForeignKey('subsystem_firmware_versions.subfw_uid'))
    subsystem_firmware_versions = relationship("SubsystemFirmwareVersions")
    patch_level = Column(Integer)
    # an MD5 hash of the patch file
    patch_hash = Column(VARCHAR(33))


# An element might appear in a table as a Value, and under these
# circumstances there'll be a table name specified in the config_element.
# the Indexes surrounding the tabled value are specified in the
# config_table_indices above. a particular elem_uid can appear as an index in
#  several tables, but a Value can appear only in one table,
# i.e. the config_element itself will name the table it's in.
# contains just unique versions of these items
class ConfigElements(Base):
    """
    Schema definition for Hydra config_elements table
    """
    __tablename__ = 'config_elements'
    # this is the unique ID for this version of the config element
    elem_uid = Column(Integer, primary_key=True, nullable=True)
    # this is the PSID-like ID.  There might be several versions of elems all
    # with this ID.  These mignt also be NULL IDs, e.g. for array elements
    # which are actually structures, and for table indices
    psid = Column(Integer)
    name = Column(VARCHAR(80))
    label = Column(Text)
    category = Column(VARCHAR(40))
    is_internal = Column(Boolean, default=False, server_default=text('0'))
    format = Column(VARCHAR(20))
    range_min = Column(Integer)
    range_max = Column(Integer)
    # BER encoded dotted decimal number for this, when referring to wi-fi data
    oid = Column(VARCHAR(20))
    # either integer, octet_string, uint8, int8, uint16, int16, uint32, int32,
    # uint64, int64, a struct_name or an enum_name
    type = Column(VARCHAR(80), nullable=False)
    units = Column(VARCHAR(80))
    # -- indicates that this element can contain a number of elements of the
    # defined type, where the number may be constrained by array_length_min
    # and/or array_length_max.
    is_array = Column(Boolean, default=False, server_default=text('0'))
    array_length_min = Column(Integer)
    array_length_max = Column(Integer)
    description_user = Column(Text)
    description_internal = Column(Text)
    additional_information = Column(Text)
    # -- description of the origin of this config element.
    source_reference = Column(Text)
    source_default = Column(VARCHAR(20))
    # -- One of read_write, read_only, write_only, not_accessible.
    access_rights = Column(VARCHAR(14))
    # -- an MD5 hash of the contents of all the fields in the config_elements
    # table (except for the elem_uid)
    hash = Column(VARCHAR(33))

    def __repr__(self):
        return f"<ConfigElement(elem_uid='{self.elem_uid}', " + \
               f"psid='{self.psid}', " + \
               f"name='{self.name}', " + \
               f"label='{self.label}', " + \
               f"category='{self.category}', " + \
               f"is_internal='{self.is_internal}', " + \
               f"format='{self.format}', " + \
               f"range_min='{self.range_min}', " + \
               f"range_max='{self.range_max}', " + \
               f"oid='{self.oid}', " + \
               f"type='{self.type}', " + \
               f"units='{self.units}', " + \
               f"is_array='{self.is_array}', " + \
               f"array_length_min='{self.array_length_min}', " + \
               f"array_length_max='{self.array_length_max}', " + \
               f"description_user='{self.description_user}', " + \
               f"description_internal='{self.description_internal}', " + \
               f"additional_information='{self.additional_information}', " + \
               f"source_reference='{self.source_reference}', " + \
               f"source_default='{self.source_default}', " + \
               f"access_rights='{self.access_rights}', " + \
               ">"


class SubfwElements(Base):
    """
    Schema definition for Hydra subfw_elements table
    """
    __tablename__ = 'subfw_elements'

    subfw_uid = Column(Integer,
                       ForeignKey('subsystem_firmware_versions.subfw_uid'),
                       primary_key=True, nullable=True)
    elem_uid = Column(Integer,
                      ForeignKey('config_elements.elem_uid'),
                      primary_key=True, nullable=True)
    enum_uid = Column(Integer, ForeignKey('enum_defs.enum_uid'))
    struct_uid = Column(Integer, ForeignKey('struct_defs.struct_uid'))
    table_uid = Column(Integer, ForeignKey('config_tables.table_uid'))
    subsystem_firmware_versions = relationship("SubsystemFirmwareVersions")
    config_elements = relationship("ConfigElements")
    enum_defs = relationship("EnumDefs")
    struct_defs = relationship("StructDefs")
    config_tables = relationship("ConfigTables")


# contains just unique versions of these items
class EnumDefs(Base):
    """
    Schema definition for Hydra  enum_defs table
    """
    __tablename__ = 'enum_defs'

    enum_uid = Column(Integer, primary_key=True, nullable=True)
    enum_name = Column(VARCHAR(80))
    # an MD5 hash of the enum_name, the enum_include and all of the enum
    # entries' names and values.
    hash = Column(VARCHAR(33))


class EnumEntry(Base):
    """
    Schema definition for Hydra enum_entry table
    """
    __tablename__ = 'enum_entry'
    enum_entry_uid = Column(Integer, primary_key=True, nullable=True)
    enum_uid = Column(Integer, ForeignKey('enum_defs.enum_uid'))
    enum_def = relationship("EnumDefs")
    enum_label = Column(VARCHAR(80))
    enum_value = Column(Integer)
    enum_description = Column(Text)


class StructDefs(Base):
    """
    Schema definition for Hydra struct_defs table
    """
    __tablename__ = 'struct_defs'

    struct_uid = Column(Integer, primary_key=True, nullable=True)
    struct_name = Column(VARCHAR(80))
    #  an MD5 hash of struct_name and all the config_element contents pointed
    #  to by structure elements referencing this structure definition.
    hash = Column(VARCHAR(33))


# contains just unique versions of these items
class StructureElements(Base):
    """
    Schema definition for Hydra structure_elements table
    """
    __tablename__ = 'structure_elements'

    struct_uid = Column(Integer,
                        ForeignKey('struct_defs.struct_uid'),
                        primary_key=True, nullable=True)
    elem_uid = Column(Integer,
                      ForeignKey('config_elements.elem_uid'),
                      primary_key=True, nullable=True)
    struct_defs = relationship("StructDefs")
    config_elements = relationship("ConfigElements")
    # the position in bits of this structure element, counting from the
    # msb in the parent object, little-endian
    lsb_bit_position = Column(Integer)
    # -- the length in bits represented by this structure element in
    # the'master' element, at offset above.
    bit_width = Column(Integer)


# Tables are first-class objects, at the level of config_elements.
# They contain rows and columns.  The columns are config elements themselves.
class ConfigTables(Base):
    """
    Schema definition for Hydra config_tables table
    """
    __tablename__ = 'config_tables'

    table_uid = Column(Integer, primary_key=True, nullable=True)
    table_name = Column(VARCHAR(80))
    description_user = Column(Text)
    description_internal = Column(Text)
    # the BER encoded (dotted decimal) representation of the table.
    oid = Column(VARCHAR(20))
    # description of the origin of this table.
    source_reference = Column(Text)
    is_internal = Column(Boolean, default=False, server_default=text('0'))
    num_indices = Column(Integer)
    # -- an MD5 hash of the contents of the name and the index info
    hash = Column(VARCHAR(33))


class ConfigTableIndices(Base):
    """
    Schema definition for Hydra config_table_indices table
    """
    __tablename__ = 'config_table_indices'

    table_uid = Column(Integer,
                       ForeignKey('config_tables.table_uid'),
                       primary_key=True, nullable=True)
    elem_uid = Column(Integer,
                      ForeignKey('config_elements.elem_uid'),
                      nullable=False, primary_key=True)
    config_table = relationship("ConfigTables")
    config_elements = relationship("ConfigElements")
    position = Column(Integer)


class ConfigElementDefaultValues(Base):
    """
    Schema definition for Hydra config_element_default_values table
    """
    __tablename__ = 'config_element_default_values'

    def_uid = Column(Integer, primary_key=True, nullable=False)
    # -- the 'leaf' element for which this default value applies
    elem_uid = Column(Integer,
                      ForeignKey('config_elements.elem_uid'),
                      nullable=False)
    config_elements = relationship("ConfigElements")
    value_string = Column(Text)
    value_int = Column(Integer)
    index1_value = Column(Integer)
    index2_value = Column(Integer)
    index3_value = Column(Integer)
    # -- an MD5 hash of the contents of the name and the index info
    hash = Column(VARCHAR(33))


class SubfwDefaults(Base):
    """
    Schema definition for Hydra subfw_defaults table
    """
    __tablename__ = 'subfw_defaults'

    subfw_uid = Column(Integer,
                       ForeignKey('subsystem_firmware_versions.subfw_uid'),
                       primary_key=True, nullable=True)
    def_uid = Column(Integer,
                     ForeignKey('config_element_default_values.def_uid'),
                     primary_key=True, nullable=True)
    subsystem_firmware_versions = relationship("SubsystemFirmwareVersions")
    config_element_default_values = relationship("ConfigElementDefaultValues")


class DefaultArrays(Base):
    """
    Schema definition for Hydra default_arrays table
    """
    __tablename__ = 'default_arrays'

    # rowid is a fake primary key to keep SQLalchemy
    rowid = Column(Integer, primary_key=True)
    def_uid = Column(Integer,
                     ForeignKey('config_element_default_values.def_uid'))
    config_element_default_values = relationship("ConfigElementDefaultValues")
    value = Column(Integer)


class Functions(Base):
    """
    Schema definition for Hydra functions table
    """
    __tablename__ = 'functions'

    func_uid = Column(Integer, primary_key=True, nullable=True)
    elem_uid = Column(Integer, ForeignKey('config_elements.elem_uid'))
    config_element = relationship('ConfigElements')
    function_name = Column(VARCHAR(80))
    type = Column(VARCHAR(20))
    is_for_vif = Column(Boolean, default=False)
