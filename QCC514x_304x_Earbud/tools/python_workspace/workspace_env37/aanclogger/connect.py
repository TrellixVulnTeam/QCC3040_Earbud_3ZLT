############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2021 Qualcomm Technologies, Inc. and/or its subsidiaries.
# All rights reserved.
#
############################################################################
"""Module to parse a JSON connection file."""

import json
import os

from aanclogger.schema import VALIDATOR

IMAGE_KEY = 'images'
IMAGE_AUDIO_PATH = 'audio_path'
IMAGE_CHIP = 'chip'
IMAGE_KYMERA_AUDIO = 'kymera_audio'
IMAGE_DOWNLOADABLES = 'downloadables'

TRANSPORTS_KEY = 'transports'
CONN_LEFT = 'left'
CONN_RIGHT = 'right'

IMAGE_CONFIG = {
    "type": "object",
    "properties": {
        IMAGE_AUDIO_PATH: {"type": "string"},
        IMAGE_CHIP: {"type": "string"},
        IMAGE_KYMERA_AUDIO: {"type": "string"},
        IMAGE_DOWNLOADABLES: {
            "type": "array",
            "default": [],
            "items": {
                "type": "string"
            }
        }
    }
}

TRANSPORT_CONFIG = {
    "type": "object",
    "properties": {
        CONN_LEFT: {"type": "string"},
        CONN_RIGHT: {"type": "string", "default": ""}
    },
    "required": [
        CONN_LEFT
    ]
}

CONN_SCHEMA = {
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "properties": {
        IMAGE_KEY: IMAGE_CONFIG,
        TRANSPORTS_KEY: TRANSPORT_CONFIG,

    },
    "required": [
        IMAGE_KEY,
        TRANSPORTS_KEY]
}

class Connection(): # pylint: disable=too-many-instance-attributes
    """Represent a connection to one or more devices for aanclogger.

    Args:
        fname (str): Filename to parse for connection configuration.
    """
    def __init__(self, fname):
        if not os.path.isfile(fname):
            raise ValueError("Couldn't find connection file: %s" % fname)

        with open(fname, 'r') as fid:
            content = json.load(fid)

        VALIDATOR(CONN_SCHEMA).validate(content)

        self._images = content.pop(IMAGE_KEY)
        self._transport = content.pop(TRANSPORTS_KEY)

        self.left = self._transport[CONN_LEFT]
        self.right = self._transport[CONN_RIGHT]

        self.audio_path = self._images[IMAGE_AUDIO_PATH]
        self.chip = self._images[IMAGE_CHIP]

        # Resolve kymera audio path
        self.kymera_audio = self._images[IMAGE_KYMERA_AUDIO]
        if not os.path.isfile(self.kymera_audio):
            auto_kymera_audio = os.path.join(
                self.output_dir,
                self.build_name,
                'build',
                'debugbin',
                self.kymera_audio
            )
            if not os.path.isfile(auto_kymera_audio):
                raise ValueError("Couldn't find kymera audio ELF: (%s or %s)" %
                                 (self.kymera_audio, auto_kymera_audio))
            self.kymera_audio = auto_kymera_audio

        # Resolve downloadables path
        self.downloadables = self._images[IMAGE_DOWNLOADABLES]
        for idx, dnld in enumerate(self.downloadables):
            if not os.path.isfile(dnld):
                auto_dnld = os.path.join(self.download_dir, dnld)
                if not os.path.isfile(auto_dnld):
                    raise ValueError("Couldn't find downloadable: (%s or %s)" %
                                     (dnld, auto_dnld))
                self.downloadables[idx] = auto_dnld

    def generate_acat_args(self):
        """Generate common ACAT arguments.

        Returns:
            list (str): List of ACAT arguments.
        """
        args = [
            "-b", self.kymera_audio,
        ]
        for dnld in self.downloadables:
            args += ["-j", dnld]

        return args

    @property
    def left_acat_args(self):
        """list(str): ACAT args for left connection."""
        args = self.generate_acat_args()
        args += ["-s", self.left]
        return args

    @property
    def right_acat_args(self):
        """list(str): ACAT args for right connection."""
        args = []
        if self.right:
            args = self.generate_acat_args()
            args += ["-s", self.right]
        return args

    @property
    def kalimba_rom_id(self):
        """str: ROM ID in the chip directory."""
        chip_dir = os.path.join(self.audio_path, self.chip)
        if not os.path.isdir(chip_dir):
            raise ValueError("Couldn't find chip directory: %s" % chip_dir)

        dir_list = os.listdir(chip_dir)
        kalimba_dir = [name for name in dir_list if 'kalimba_ROM' in name]
        if not kalimba_dir:
            raise ValueError("Couldn't find kalimba_ROM_xxx in %s" % chip_dir)
        return kalimba_dir[0]

    @property
    def download_dir(self):
        """str: Downloadable output directory."""
        download_dir = os.path.join(
            self._images[IMAGE_AUDIO_PATH],
            self._images[IMAGE_CHIP],
            self.kalimba_rom_id,
            'kymera',
            'prebuilt_dkcs',
            self.build_name
        )
        if not os.path.isdir(download_dir):
            raise ValueError("Couldn't find downloadable directory: %s" %
                             download_dir)
        return download_dir

    @property
    def output_dir(self):
        """str: Build output directory."""
        output_dir = os.path.join(
            self._images[IMAGE_AUDIO_PATH],
            self._images[IMAGE_CHIP],
            self.kalimba_rom_id,
            'kymera',
            'output'
        )
        if not os.path.isdir(output_dir):
            raise ValueError("Couldn't find output directory: %s" % output_dir)
        return output_dir

    @property
    def build_name(self):
        """str: build name."""
        output_name = os.listdir(self.output_dir)
        if not output_name:
            raise ValueError("Empty output directory: %s" % self.output_dir)
        return output_name[0]
