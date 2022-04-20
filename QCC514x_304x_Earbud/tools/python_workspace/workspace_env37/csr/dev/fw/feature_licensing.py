############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2021 Qualcomm Technologies International, Ltd.
#
############################################################################

from csr.dev.fw.firmware_component import FirmwareComponent
from csr.wheels import gstrm
from csr.wheels.global_streams import iprint
from csr.dev.model import interface
from csr.dev.adaptor.text_adaptor import TextAdaptor
from csr.dev.fw.meta.i_firmware_build_info import BuildDirNotFoundError

class FeatureLicensing(FirmwareComponent):
    """
    This class queries License Manager requires a live working device.
    """

    def __init__(self, fw_env, core):
        FirmwareComponent.__init__(self, fw_env, core)
        self._apps1 = self._core.subsystem.cores[1]

    @property
       
   
    def _on_reset(self):
        return None
   
    def dump(self):
        '''
        Dump the list of features into a table 
        Also check if the firmware considers them as internal.
        '''
        output = interface.Group('License Features SMB')
        
        #Checking for ELF is slow so use a flag so this is only tried once. 
        have_p0 = True

        if self._apps1.is_running:
            license_check = self.list_licenses()

            table = interface.Table(["ID", "Name", "Enabled"])
            
            for i in range(self._apps1.fw.env.enums["feature_id"]["MAX_FEATURES_NUMBER"]+1):
                licensed = license_check[i][1]
                feature = license_check[i][0]
                
                #Firmware ELF won't be available to the customer. 
                try: 
                    if have_p0 and self._apps1.subsystem.p0.fw.call.license_manager_is_feature_internal(i): 

                        #Only preserved in recent firmware.
                        if "internal_feature_id" in self._apps1.subsystem.p0.fw.env.enums.keys():
                            feature = self._apps1.subsystem.p0.fw.env.enums["internal_feature_id"][i]
                        feature = feature + "(Internal)"
                except BuildDirNotFoundError:
                    have_p0 = False
                
                table.add_row([i,feature,licensed])
            
            output.append(table)
            
        else: 
            # Pylib would need an implementation of the License Algorithm for this to work.
            output.append("Report only works on a live running device")

        TextAdaptor(output, gstrm.iout)
        return None
        
    def list_licenses(self):
        '''
        Loop through all the features calling FeatureVerifyLicense on each one.
       
        '''
        license_listing = []
        
        if self._apps1.is_running:

            for i in range(self._apps1.fw.env.enums["feature_id"]["MAX_FEATURES_NUMBER"]+1):
                licensed = self._apps1.fw.call.FeatureVerifyLicense(i)
                try:
                    feature = self._apps1.fw.env.enums["feature_id"][i]
                except KeyError:
                    feature = "-"
                license_listing.extend([(feature,licensed)])

        return license_listing