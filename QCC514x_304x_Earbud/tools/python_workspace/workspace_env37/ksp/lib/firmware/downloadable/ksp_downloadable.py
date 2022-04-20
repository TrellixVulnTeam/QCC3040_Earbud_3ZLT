#
# Copyright (c) 2020 Qualcomm Technologies, Inc. and/or its
# subsidiaries.  All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""Interact with the KSP downloadable in the firmware."""
import logging

from ksp.lib.exceptions import DownloadableError, FirmwareError
from ksp.lib.logger import method_logger

logger = logging.getLogger(__name__)

EDKCS_FILENAME = 'download_ksp.edkcs'
DKCS_FILENAME = 'download_ksp.dkcs'


class KSPDownloadable(object):
    """Handle KSP Downloadable operations.

    Args:
        firmware (Firmware obj): An instance of the Firmware object.
        edkcs (bool): When it's True, it loads a signed downloadable.
        processor (int): The processor number.
    """
    def __init__(self, firmware, edkcs=False, processor=0):
        self._handle = 0
        self._index = 0
        self._processor = processor
        self._firmware = firmware
        self._is_loaded = False

        if edkcs:
            self._filename = EDKCS_FILENAME
        else:
            self._filename = DKCS_FILENAME

        self._set_downloadable_index()

    @method_logger(logger)
    def load(self):
        """Loads the KSP downloadable.

        Raises:
            DownloadableError: If something goes wrong with loading the
                downloadable to the chip.
        """
        if self._is_loaded:
            logger.warning("The operator is already loaded.")
            return

        try:
            bdl = self._firmware.load_downloadable(self._index, 1)

        except FirmwareError as error:
            raise DownloadableError(
                "Downloadable fails to load with '%s'" % error
            )

        if bdl == 0:
            raise DownloadableError(
                "Downloadable fails to load. Possibly memory "
                "shortage or using wrong downloadable, check audio "
                "log for further clues."
            )

        logger.info("KSP downloadable is loaded.")
        self._handle = bdl
        self._is_loaded = True

    @method_logger(logger)
    def unload(self):
        """Unloads the KSP downloadable."""
        if self._is_loaded:

            ret = self._firmware.unload_downloadable(self._handle)
            if ret == 0:
                raise DownloadableError("Failed unloading the downloadable.")

            logger.info("KSP downloadable is unloaded.")
            self._is_loaded = False

    def is_loaded(self):
        """Check whether the downloadable is loaded or not.

        Returns:
            bool: True if it's loaded, False otherwise.
        """
        return self._is_loaded

    @method_logger(logger)
    def _set_downloadable_index(self):
        """Sets the index of the downloadable file.

        The file should be in the filesystem. This method also sets the
        index of the downloadable file.
        """
        self._index = self._firmware.find_file(1, self._filename)
        if self._index == 0:
            raise DownloadableError(
                "Downloadable file %s is not found." % self._index
            )

        logger.info("Downloadable file is found, index=%s.", self._index)
