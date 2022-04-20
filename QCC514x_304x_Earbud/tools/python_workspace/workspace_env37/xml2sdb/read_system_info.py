"""
Classes and methods required to read a Hydra Metadata chip info file
"""
import datetime


class SystemVersion:
    """
    Class for reading the System Version specification file
    """
    def __init__(self, spec_file):
        self.spec_file = spec_file
        self.chip_spec = {'subsystem_info': {}}

    def parse(self):
        """
        Parses System Information file into a Python dictionary
        .. code-block:: python

        """
        with open(self.spec_file, 'r') as spec_h:
            for i, line in enumerate(spec_h):
                # print(i, line)
                if i == 0:
                    self._process_chip_name(line.strip())
                elif i == 1:
                    self._process_rom_name(line.strip())
                elif i == 2:
                    self._process_line3(line.strip())
                elif i == 3:
                    self._process_patch_level(line.strip())
                elif i == 4:
                    self._process_version_label(line.strip())
                elif i == 5:
                    self._process_description(line.strip())
                elif i == 6:
                    self._process_customer_name(line.strip())
                elif i == 7:
                    self._process_release_date(line.strip())
                else:
                    self._process_subsystem_info(line.strip())
        return self.chip_spec

    def _process_chip_name(self, data):
        self.chip_spec['chip_name'] = data

    def _process_rom_name(self, data):
        self.chip_spec['rom_name'] = data

    def _process_line3(self, data):
        # chip_id, rom_version, efuse_hash = data.split(',')
        data = data.split(',')
        self.chip_spec['chip_id'] = int(data[0], 16)
        self.chip_spec['rom_version'] = int(data[1], 16)
        efuse_hashes = []
        for efuse_hash in data[2:]:
            efuse_hashes.append(int(efuse_hash, 16))
        self.chip_spec['efuse_hash'] = efuse_hashes

    def _process_patch_level(self, data):
        self.chip_spec['patch_release_level'] = int(data)

    def _process_version_label(self, data):
        self.chip_spec['system_version_label'] = data

    def _process_description(self, data):
        self.chip_spec['system_description'] = data

    def _process_customer_name(self, data):
        self.chip_spec['customer_name'] = data

    def _process_release_date(self, data):
        if data == 'now':
            rel_date = datetime.datetime.now()
        else:
            rel_date = datetime.datetime.strptime(data, '%Y-%m-%d %H:%M:%S')
        self.chip_spec['system_release_date_time'] = rel_date

    def _process_subsystem_info(self, data):
        if '=' in data and len(data.split('=')) == 2:
            subsystem, fw_version = data.split('=')
        if '_' in subsystem and len(data.split('_')) == 2:
            subsystem, layer = subsystem.split('_')
        else:
            layer = None
        if ',' in fw_version and len(fw_version.split(',')) == 3:
            fw_version, fw_variant, patches = fw_version.split(',')
            patches.replace('[', '').replace(']', '')
        else:
            fw_variant = 0
            patches = None
        if patches and ',' in patches and len(patches.split(',')) == 3:
            patch_level, patch_hash, patches = patches.split(',')
            # Raw SQL example
            # INSERT INTO patch_files (subfw_uid, patch_level, patch_hash)
            #        VALUES ($subfw_uid, $patch_level, '$patch_hash')")
            self.chip_spec['subsystem_info'][subsystem] = {
                'layer': layer,
                'fw_version': int(fw_version),
                'fw_variant': int(fw_variant),
                'patch_info': {'patch_level': patch_level,
                               'patch_hash': patch_hash,
                               'patches': patches}}
        else:
            self.chip_spec['subsystem_info'][subsystem] = {
                'layer': layer,
                'fw_version': int(fw_version),
                'fw_variant': int(fw_variant)}

    def _build_metadata_hash(self):
        pass
