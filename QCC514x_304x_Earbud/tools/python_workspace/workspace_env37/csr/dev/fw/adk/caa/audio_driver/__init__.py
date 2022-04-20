############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2021 Qualcomm Technologies International, Ltd.
#
############################################################################

from .audio_driver import AudioDriver

PYDBG_PLUGIN_CONTAINER_NAME = "audio_driver"
PYDBG_PLUGIN_CONTAINER_CLASS = AudioDriver

__all__ = [PYDBG_PLUGIN_CONTAINER_NAME, PYDBG_PLUGIN_CONTAINER_CLASS]
