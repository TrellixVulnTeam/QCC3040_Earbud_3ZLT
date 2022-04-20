############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2017 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
from .qcc514x_qcc304x_chip import QCC514X_QCC304XChip

class QCC514X_QCC304XD00Chip (QCC514X_QCC304XChip):
    """
    QCC514X_QCC304X D00 Chip Proxy
    """
    # QCC512X_QCC302XChip compliance
    
    def _create_curator_subsystem(self):
         
        from csr.dev.hw.subsystem.qcc514x_qcc304x_curator_subsystem \
                                           import QCC514X_QCC304XCuratorD00Subsystem
        return QCC514X_QCC304XCuratorD00Subsystem(self, self.SSID.CURATOR,
                                            self._access_cache_type)
        
    def _create_host_subsystem(self):
        
        from csr.dev.hw.subsystem.qcc514x_qcc304x_host_subsystem \
                                                  import QCC514X_QCC304XHostSubsystem
        return QCC514X_QCC304XHostSubsystem(self, self.SSID.HOST,
                                      self._access_cache_type)
    
    def _create_apps_subsystem(self):
        from csr.dev.hw.subsystem.qcc514x_qcc304x_apps_subsystem \
                                                  import QCC514X_QCC304XAppsD00Subsystem
        return QCC514X_QCC304XAppsD00Subsystem(self, self.SSID.APPS,
                                         self._access_cache_type)
    
    def _create_bt_subsystem(self):
        
        from csr.dev.hw.subsystem.bt_subsystem import SimpleBTSubsystem
        return SimpleBTSubsystem(self, self.SSID.BT, self._access_cache_type)

    def _create_audio_subsystem(self):
        from csr.dev.hw.subsystem.qcc514x_qcc304x_audio_subsystem \
                                                  import QCC514X_QCC304XAudioSubsystem
        return QCC514X_QCC304XAudioSubsystem(self, self.SSID.AUDIO,
                                      self._access_cache_type, "d00")

