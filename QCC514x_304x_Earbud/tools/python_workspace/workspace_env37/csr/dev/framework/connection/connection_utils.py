############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2021 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
import platform
import subprocess
import time

from csr.wheels import iprint
from csr.dev.hw.address_space import ReadRequest, WriteRequest

class TransportInfo:
    """
    Simple object that allows some control of whether data buses within the chip should
    aim to prefetch memory data where possible.
    """
    def __init__(self, high_latency=False, prefetch_override=None, is_coresight=False):
        self._high_latency = high_latency
        self._prefetch_override = prefetch_override
        self.is_coresight = is_coresight

    @property
    def has_high_latency(self):
        return self._high_latency

    @property
    def prefetch_recommended(self):
        if self._prefetch_override is None:
            return self._high_latency
        return self._prefetch_override

    def set_prefetch_override(self, override):
        self._prefetch_override = override

    def clear_prefetch_override(self):
        self.set_prefetch_override(None)


class GAIAConnectionError(RuntimeError):
    pass
NotFoundExceptionType = WindowsError if platform.system() == "Windows" else FileNotFoundError

class SupportsGAIAApp:
    """
    Mixin to provide support for connecting to the GAIA app running on a phone via
    ADB port forwarding
    """
    def _start_adb_forwarding(self):
        '''
        First check that adb is present and that there is a phone connected.
        Then do the adb port forwarding command with the appropriate port number.
        '''
        try: 
            result = subprocess.check_output(["adb", "devices"])
            result = result.splitlines()
            # Expecting result to be something like:
            # ['List of devices attached', 'CB5A2B44U6\tdevice', '', '']
            # If no devices are connected the result is:
            # ['List of devices attached', '', '']
            if not result[1]:
                raise GAIAConnectionError(
                    "Phone is not connected on USB or USB debugging is not enabled")
            if result[2]:
                raise GAIAConnectionError(
                    "Multiple phones are connected."
                    " Adb port forwarding should be setup manually and"
                    " pydbg started without the 'adb' in the 'skt' transport string")
            else:
                device_state = result[1].split()[1].decode()
                if device_state != "device":
                    raise GAIAConnectionError(
                        "USB debugging is not enabled on the phone (state={})".format(device_state))
        except NotFoundExceptionType:
            raise GAIAConnectionError(
                "Cannot find adb. Is it installed and on the system path?")
        
        try: 
            result = subprocess.check_output(["adb", "forward", 
                                              "tcp:{}".format(self._ip_port), 
                                              "tcp:{}".format(self._ip_port)])
            result = result.splitlines()
            if len(result) and result[0].decode() == "{}".format(self._ip_port):
                iprint("adb port forwarding started")
            else:
                iprint("adb port forwarding already active")
        except NotFoundExceptionType:
            raise GAIAConnectionError(
                "Cannot find adb. Is it installed and on the system path?")
        except subprocess.CalledProcessError as e:
            raise GAIAConnectionError("adb forwarding failed: {}".format(e))
            
    def _start_gaia_debug_client_app(self):
        '''
        Use the adb commands to see if the phone has the GAIA debug client app
        currently running. If not it checks whether it is installed and starts it
        if it is.
        Hopefully this gives enough debugging of things that could go wrong.
        '''
        self.phone_app_display_name = "Qualcomm remote debug app"
        phone_app_android_name = "com.qualcomm.qti.remotedebug"
        try: 
            subprocess.check_output(["adb", "shell", "pidof", phone_app_android_name]) 
        except NotFoundExceptionType:
            raise GAIAConnectionError(
                "Cannot find adb. Is it installed and on the system path?")
        except subprocess.CalledProcessError:
            iprint("{} isn't running - starting it now".format(self.phone_app_display_name))
        
            try:
                result = subprocess.check_output(["adb", "shell", "am", 
                            "start", 
                            "-n {}/.ui.MainActivity".format(phone_app_android_name)], 
                            stderr=subprocess.STDOUT).decode()
                # Allow time for app to start and open the socket. One second
                # seemed to work OK so I've set it to 2 to be safer
                time.sleep(2)
            except NotFoundExceptionType:
                raise GAIAConnectionError(
                    "Cannot find adb. Is it installed and on the system path?")
            except subprocess.CalledProcessError:
                raise GAIAConnectionError(
                    "{} isn't installed on the phone".format(self.phone_app_display_name))
            if "Error" in result:
                if "does not exist" in result:
                    raise GAIAConnectionError(
                        "{} isn't installed on the phone".format(self.phone_app_display_name))
                else:
                    raise GAIAConnectionError(
                        "Error starting {} on the phone".format(self.phone_app_display_name))


