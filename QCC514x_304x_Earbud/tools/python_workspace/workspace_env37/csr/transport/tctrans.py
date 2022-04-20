# ***********************************************************************
# * Copyright 2017-2021 Qualcomm Technologies International, Ltd.
# ***********************************************************************

from contextlib import contextmanager
from ctypes import c_int, c_uint, c_void_p, c_char_p, c_bool, c_uint16, byref, POINTER, Structure, cdll, c_uint8, \
    c_uint32
import os
import sys


class CTypesStrIn(object):
    """
    Python 2/3 compatible wrapper for passing strings into ctypes wrapped functions.
    """
    @classmethod
    def from_param(cls, value):
        """
        Called by ctypes when the given value is going to be passed as a parameter.
        """

        # Already bytes: nothing to do
        if isinstance(value, bytes):
            return value

        # Python 3 (or in principle Python 2 unicode type) => encode to ASCII
        return value.encode('ascii')


def from_cstr(s):
    """
    Python 2 and 3 compatible ASCII-encoded C string to Python-string conversion.
    Used when bringing values back from the C interface.
    """
    if s is None:
        return None

    # Python 2: no-op
    if isinstance(s, str):
        return s

    # Python 3: from bytes
    return s.decode('ascii')


class TcError(RuntimeError):
    def __init__(self, message):
        RuntimeError.__init__(self, message)


class TcErrorBadParam(TcError):
    pass


class TcErrorNeedsConnection(TcError):
    pass


class TcEnumerationFailed(TcError):
    pass


class TcErrorConnectionFailed(TcError):
    pass


class TcErrorIoFailed(TcError):
    pass


class TcErrorBadAccessWidth(TcError):
    pass

    
class TcErrorDeviceLostAfterReset(TcError):
    pass

class TcErrorBufferTooSmall(TcError):
    pass


TC_ERROR_LOOKUP = [
    None,                                    # TC_ERR_NO_ERROR
    TcErrorBadParam,                         # TC_ERR_BAD_PARAM
    TcErrorNeedsConnection,                  # TC_ERR_NEEDS_CONNECTION
    TcEnumerationFailed,                     # TC_ERR_ENUMERATION_FAILED
    TcErrorConnectionFailed,                 # TC_ERR_CONNECTION_FAILED
    TcErrorIoFailed,                         # TC_ERR_IO_FAILED
    TcErrorBadAccessWidth,                   # TC_ERR_BAD_ACCESS_WIDTH
    TcErrorDeviceLostAfterReset,             # TC_ERR_DEVICE_LOST_AFTER_RESET
    TcErrorBufferTooSmall                    # TC_ERR_BUFFER_TOO_SMALL 
]

# constants for TestTunnel
_TC_TUNNEL_CLIENT_RFCLI=0
_TC_TUNNEL_CLIENT_BTCLI=1
_TC_TUNNEL_CLIENT_ACCMD=2
_TC_TUNNEL_CLIENT_PTCMD=3
_TC_TUNNEL_CLIENT_HOSTCOMMS=4
_TC_TUNNEL_CLIENT_QACT=5

_TC_TUNNEL_ISP_USBDBG=0xFF
_TC_TUNNEL_ISP_TRB=0xFE
_TC_TUNNEL_ISP_USBCC=0xFD
_TC_TUNNEL_ISP_WIRELESS=0xFC

class TcErrorRebootFailed(TcError):
    """
    Doesn't map to a C library error code.
    Wrapper exception raised when a reboot Curator command fails, to convey diagnostic information.
    """
    def __init__(self, inner_error, wait_info):
        self.inner_error = inner_error
        self.wait_info = wait_info

    def __str__(self):
        return "\n".join([
            "\nInner error: {0}".format(type(self.inner_error).__name__),
            "{0}".format(self.wait_info),
        ])


class TcTransLibraryFunctionMissingOrMismatch(RuntimeError):
    def __init__(self, lib_name, func_name):
        RuntimeError.__init__(
            self,
            "tctrans library function '{0}' not found in shared library '{1}', or the prototype didn't match.\n"
            "Check that the shared library is the same version as these Python bindings.".format(func_name,
                                                                                                 lib_name))


class TcDeviceListRaw(Structure):
    """
    Corresponds to the C struct 'tc_device_list'.
    """
    _fields_ = [("num_devices", c_uint),
                ("device_ids", POINTER(c_char_p)),
                ("lock_status", POINTER(c_bool))]


class TcDevice(object):
    """
    Represents an enumerated device. Currently just an ID.
    """
    def __init__(self, device_id, transport, locked):
        self.id = device_id
        self.transport = transport
        if locked:
            self.lock_status = 'locked'
        else:
            self.lock_status = 'unlocked'
            


class TcDeviceList(list):
    def __init__(self, devices):
        super(TcDeviceList, self).__init__(devices)

    def __repr__(self):
        result = []
        if len(self) == 0:
            result = ["No devices found"]
        else:
            for i, dev in enumerate(self):
                result.append("{0}. {2} device with ID '{1}' - {3}".format(i + 1, dev.id, dev.transport, dev.lock_status))
        return "\n".join(result)


class TcSiflashProperties(object):
        def __init__(self, subsys_id, bank, manufacturer_id, device_id, num_sectors, sector_size_kb):
            self.subsys_id = subsys_id
            self.bank = bank
            self.manufacturer_id = manufacturer_id
            self.device_id = device_id
            self.num_sectors = num_sectors
            self.sector_size_kb = sector_size_kb

        def __repr__(self):
            num_sectors = "<Unknown>" if self.num_sectors is None else str(self.num_sectors)
            sector_size_kb = "<Unknown>" if self.sector_size_kb is None else str(self.sector_size_kb)

            return "\n".join([
                "Siflash properties for subsys {0} bank {1}:".format(self.subsys_id, self.bank),
                "Manufacturer ID:        {0}".format(self.manufacturer_id),
                "Manufacturer device ID: {0}".format(self.device_id),
                "Num sectors:            {0}".format(num_sectors),
                "Sector size (kBytes):   {0}".format(sector_size_kb)
            ])


class SharedLibLoader(object):
    """
    Helper class with common shared-library loading logic.
    """

    def __init__(self, lib_name, default_dll_filename, default_so_filename, has_dll_dependencies):
        """
        :param has_dll_dependencies: does the library have runtime DLL dependencies on Windows? If so, we supplement
        the PATH environment variable to include the location that the DLL is being loaded from, otherwise the Windows
        DLL loader doesn't seem to find the dependencies (even if they're next to the library we want to load!).
        """
        self._lib_name = lib_name
        self._default_dll_filename = default_dll_filename
        self._default_so_filename = default_so_filename
        self._has_dll_dependencies = has_dll_dependencies

    @staticmethod
    def _get_path_element_separator():
        # On Cygwin, path elements are separated by colons, but on win32 it's a semi-colon.
        return ";" if sys.platform.startswith('win32') else ":"

    def prepend_path_environment_element(self, new_element):
        """
        This is useful to allow dependencies of a DLL to be found by the default Windows loader, when the primary
        DLL was loaded from a location that isn't on the DLL search path (cwd, PATH, etc).
        A potential problem with adding PATH elements is that the PATH search is quite low down on the DLL search
        order, so another DLL might be found in one of the higher priority search locations -- this might not be
        the one wanted.
        
        We make sure that our new element is first in the PATH, but try to avoid adding it multiple
        times by comparing (after normalisation) to the current first element.
        """
        cur_path = os.environ.get('PATH', '')
        new_element = os.path.normpath(new_element).lower()
        split_path = cur_path.split(self._get_path_element_separator())
        if split_path:
            first_elem = split_path[0]
            if os.path.normpath(first_elem).lower() == new_element:
                return
        
        os.environ['PATH'] = new_element + self._get_path_element_separator() + cur_path

    @staticmethod
    def _get_this_file_dir():
        """
        This mechanism assumes that nothing has changed the current directory since this module was loaded (e.g.
        os.chdir()), because __file__ is a constant. It seems fairly reasonable to make this assumption.
        """
        return os.path.abspath(os.path.dirname(os.path.realpath(__file__)))

    def prepare_shared_library_load_common(self, override_lib_path, default_lib_filename):
        if override_lib_path:
            if not os.path.isabs(override_lib_path):
                # Something was supplied, but it's not absolute. Form a full path relative to this file's location.
                # This step allows the load to work whether or not the current directory is the same as this file's
                # directory (it probably isn't).
                load_lib_path = os.path.join(self._get_this_file_dir(), override_lib_path)
            else:
                load_lib_path = override_lib_path
        else:
            here_lib_path = os.path.join(self._get_this_file_dir(), default_lib_filename)
            # No override supplied, so look in this file's directory for the shared library, with the default name.
            # If it's there, use the path we formed (this way, we guarantee that we load *this* library,
            # not anything else that the dynamic loader might find via environment/search rules).
            # Otherwise throw ourselves to the mercy of the dynamic loader and
            # the environment, and just use the bare default filename.
            if os.path.isfile(here_lib_path):
                load_lib_path = here_lib_path
            else:
                load_lib_path = default_lib_filename

        return load_lib_path

    def prepare_shared_library_load_linux(self, override_lib_path):
        """
        Return a path to pass to ctypes' LoadLibrary.
        May also do environmental preparation.
        :param override_lib_path: any override path that was passed to __init__.
        """
        return self.prepare_shared_library_load_common(override_lib_path, self._default_so_filename)

    def prepare_shared_library_load_windows(self, override_lib_path):
        """
        Return a path to pass to ctypes' LoadLibrary.
        May also do environmental preparation.
        :param override_lib_path: any override path that was passed to __init__.
        """
        load_path = self.prepare_shared_library_load_common(override_lib_path, self._default_dll_filename)

        if self._has_dll_dependencies:
            # Help the loader find the DLL dependencies as these won't load if the library is being loaded from a
            # location that isn't on the DLL search path (for example, not the current working dir).
            self.prepend_path_environment_element(
                os.path.abspath(
                    os.path.dirname(load_path)
                )
            )
        return load_path

    def load_shared_library_linux(self, override_lib_path):
        load_library_path = self.prepare_shared_library_load_linux(override_lib_path)
        try:
            lib_handle = cdll.LoadLibrary(load_library_path)
            return lib_handle, load_library_path
        except Exception as ex:
            message = """Could not find or load '{0}' (or one of its dependencies).

Inner Python exception: {1}

Check that:
- the architecture of your Python interpreter (32/64-bit) matches the architecture of the {2} shared library 
  being loaded (e.g. via the 'file' command).
- the location of the library is in the shared library search path.

The LD_LIBRARY_PATH used for the search was:
    {3}""".format(load_library_path,
                  repr(ex),
                  self._lib_name,
                  "\n    ".join(os.environ.get('LD_LIBRARY_PATH', '').split(":")))

            raise OSError(message)

    def _load_shared_library_windows(self, override_lib_path):
        load_library_path = self.prepare_shared_library_load_windows(override_lib_path)
        try:
            lib_handle = cdll.LoadLibrary(load_library_path)
            return lib_handle, load_library_path
        except Exception as ex:
            message = """Could not find or load '{0}' (or one of its dependencies).

Inner Python exception: {1}

Check that:
- the architecture of your Python interpreter (32/64-bit) matches the architecture of the {2} DLL being loaded.
- the location of the library is in the DLL search path.
- the Microsoft Visual C++ 2015 Redistributable package is installed.

The system PATH used for the search was:
    {3}""".format(load_library_path,
                  repr(ex),
                  self._lib_name,
                  "\n    ".join(os.environ.get('PATH', '').split(self._get_path_element_separator())))

            raise OSError(message)

    def load_shared_library(self, override_lib_path=None):
        """
        Load the native shared library. Give a bit of support to try to ease troubleshooting of common problems.
        :param override_lib_path: any override path that was passed to __init__.
        :return the path that was passed to ctypes LoadLibrary, if successful.
        """
        if sys.platform.startswith('linux'):
            return self.load_shared_library_linux(override_lib_path)
        elif sys.platform.startswith('cygwin') or sys.platform.startswith('win32'):
            return self._load_shared_library_windows(override_lib_path)
        else:
            raise OSError("Cannot load the {0} library. The system '{1}' you are using is not supported.".format(
                self._lib_name,
                sys.platform)
            )


class TcTrans(object):
    """
    Python wrapper for the tctrans library.
    """

    def _handle_error(self, err):
        """
        Internal routine to convert tctrans errors into Python exceptions.
        """
        if bool(err):
            err_str = from_cstr(self._cfuncs['tc_get_error_str'](err))
            err_code = self._cfuncs['tc_get_error_code'](err)
            self._cfuncs['tc_free_error'](err)
            
            try:
                ex = TC_ERROR_LOOKUP[err_code](err_str)
            except IndexError:
                raise TcError(
                    'Unknown error from tctrans: error code {0} has not been mapped to Python. Please report this as a'
                    'bug.'.format(err_code)
                )

            raise ex

    def __init__(self, override_lib_path = None):
        """
        :param override_lib_path: an optional path (including file name) from which to load the shared library. May be
        absolute, or relative to the directory of this file.
        """
        self._stream = c_void_p()
        self.current_dongle_id = None
        self._tc_dll = None
        self._cfuncs = {}
        loader = SharedLibLoader(
            lib_name='tctrans',
            default_dll_filename='tctrans.dll',
            default_so_filename='libtctrans_shared.so',
            has_dll_dependencies=True
        )
        self._tc_dll, load_library_path = loader.load_shared_library(override_lib_path)

        def gen_prototype(name, restype, argtypes):
            try:
                func = getattr(self._tc_dll, name)
            except AttributeError:
                raise TcTransLibraryFunctionMissingOrMismatch(load_library_path, name)

            func.argtypes = argtypes
            func.restype = restype
            self._cfuncs[name] = func

        gen_prototype('tc_get_version', c_char_p, None)

        gen_prototype('tc_get_error_str' , c_char_p, [c_void_p])
        gen_prototype('tc_get_error_code', c_int,    [c_void_p])
        gen_prototype('tc_free_error',     None,     [c_void_p])

        gen_prototype('tc_enumerate',                c_void_p, [POINTER(TcDeviceListRaw)])
        gen_prototype('tc_enumerate_usbcc',          c_void_p, [POINTER(TcDeviceListRaw)])
        gen_prototype('tc_free_enumeration_results', None,     [POINTER(TcDeviceListRaw)])

        gen_prototype('tc_stream_open', c_void_p, [CTypesStrIn, POINTER(c_void_p)])
        gen_prototype('tc_stream_close', None, [c_void_p])

        gen_prototype('tc_get_device_id', c_void_p, [c_void_p, POINTER(c_char_p)])

        gen_prototype('tc_reboot_curator_cmd', c_void_p,
                      [c_void_p, c_uint, c_uint, POINTER(c_uint), POINTER(c_uint)])

        gen_prototype('tc_read_windowed',  c_void_p, [c_void_p, c_uint8, c_uint32, POINTER(c_uint16), c_uint16])
        gen_prototype('tc_write_windowed', c_void_p, [c_void_p, c_uint8, c_uint32, POINTER(c_uint16), c_uint16])

        gen_prototype('tc_read_regbased_32', c_void_p,
                      [c_void_p, c_uint8, c_uint8, c_bool, c_uint32, POINTER(c_uint32), c_uint32])
        gen_prototype('tc_read_regbased_16', c_void_p,
                      [c_void_p, c_uint8, c_uint8, c_bool, c_uint32, POINTER(c_uint16), c_uint32])
        gen_prototype('tc_read_regbased_8', c_void_p,
                      [c_void_p, c_uint8, c_uint8, c_bool, c_uint32, POINTER(c_uint8), c_uint32])
        gen_prototype('tc_read_regbased', c_void_p,
                      [c_void_p, c_uint8, c_uint8, c_uint8, c_bool, c_uint32, POINTER(c_uint8), c_uint32])

        gen_prototype('tc_write_regbased_32', c_void_p,
                      [c_void_p, c_uint8, c_uint8, c_bool, c_uint32, POINTER(c_uint32), c_uint32])
        gen_prototype('tc_write_regbased_16', c_void_p,
                      [c_void_p, c_uint8, c_uint8, c_bool, c_uint32, POINTER(c_uint16), c_uint32])
        gen_prototype('tc_write_regbased_8', c_void_p,
                      [c_void_p, c_uint8, c_uint8, c_bool, c_uint32, POINTER(c_uint8), c_uint32])
        gen_prototype('tc_write_regbased', c_void_p,
                      [c_void_p, c_uint8, c_uint8, c_uint8, c_bool, c_uint32, POINTER(c_uint8), c_uint32])

        stream_subsys_bank = [c_void_p, c_uint8, c_uint8]
        gen_prototype('tc_siflash_identify', c_void_p,
                      stream_subsys_bank +
                      [POINTER(c_uint8), POINTER(c_uint16), POINTER(c_uint32), POINTER(c_uint16)])
        gen_prototype('tc_siflash_identify_alt', c_void_p,
                      stream_subsys_bank +
                      [POINTER(c_uint8), POINTER(c_uint16)])
        gen_prototype('tc_siflash_read', c_void_p,
                      stream_subsys_bank + [c_uint32, POINTER(c_uint16), c_uint])
        gen_prototype('tc_siflash_write', c_void_p,
                      stream_subsys_bank + [c_uint32, POINTER(c_uint16), c_uint])
        gen_prototype('tc_siflash_sector_erase', c_void_p,
                      stream_subsys_bank + [c_uint32, c_uint16, POINTER(c_uint32)])
        gen_prototype('tc_siflash_chip_erase', c_void_p,
                      stream_subsys_bank + [POINTER(c_uint32)])
        gen_prototype('tc_siflash_vector_inject', c_void_p,
                      stream_subsys_bank + [POINTER(c_uint16), c_uint, POINTER(c_uint8), c_uint8])

        gen_prototype('tc_tunnel_open', c_void_p,  
            [CTypesStrIn, c_uint16, c_uint8, POINTER(c_void_p)])
        gen_prototype('tc_tunnel_close', None, 
            [c_void_p])
        gen_prototype('tc_tunnel_request',  c_void_p,  
            [c_void_p,  POINTER(c_uint8), c_uint16])
        gen_prototype('tc_tunnel_response', c_void_p, 
            [c_void_p, POINTER(c_uint8), c_uint16, POINTER(c_uint32), c_uint32])



    def enumerate_devices(self):
        """
        Enumerate usb2tc devices and usbcc devices
        :return: a TcDeviceList
        """
        raw_devices = TcDeviceListRaw()
        self._handle_error(
            self._cfuncs['tc_enumerate'](byref(raw_devices))
        )

        enumerated_usbdbg = []
        try:
            enumerated_usbdbg = TcDeviceList(
                [TcDevice(
                    from_cstr(raw_devices.device_ids[i]), 
                    'usb2tc',
                    raw_devices.lock_status[i]
                ) for i in range(raw_devices.num_devices)]
            )
        finally:
            self._cfuncs['tc_free_enumeration_results'](byref(raw_devices))
            


        raw_usbcc_devices = TcDeviceListRaw()
        self._handle_error(
            self._cfuncs['tc_enumerate_usbcc'](byref(raw_usbcc_devices))
        )

        enumerated_usbcc = []
        try:
            enumerated_usbcc = TcDeviceList(
                [TcDevice(
                    from_cstr(raw_usbcc_devices.device_ids[i]), 
                    'usbcc',
                    raw_usbcc_devices.lock_status[i]
                ) for i in range(raw_usbcc_devices.num_devices)]
            )
        finally:
            self._cfuncs['tc_free_enumeration_results'](byref(raw_usbcc_devices))

        enumerated = []
        if enumerated_usbdbg:
            enumerated.extend(enumerated_usbdbg)
            
        if enumerated_usbcc:
            enumerated.extend(enumerated_usbcc)
            
        return TcDeviceList(enumerated)   

    def open(self, dongle_id = None):
        """
        Opens a stream associated with the specified usb2tc device.
        :param dongle_id: the id of the usb2tc device to connect to. If None,
        then an enumeration call is made. If this results in a single device,
        a connection to it is opened. If there are multiple devices, an
        exception is thrown.
        """
        if dongle_id is None:
            devices = self.enumerate_devices()
            if len(devices) == 1:
                dongle_id = devices[0].id
            elif len(devices) == 0:
                raise RuntimeError("No devices found (dongle_id was None so enumeration was done)")
            elif len(devices) > 1:
                raise RuntimeError("Detected multiple devices: specify a dongle_id to TcTrans.open()")

        if not isinstance(dongle_id, str) or not dongle_id:
            raise ValueError("'dongle_id' must be a non-empty string")

        # If we have a stream open, close it first.
        self.close()

        self._handle_error(
            self._cfuncs['tc_stream_open'](dongle_id, byref(self._stream))
        )
        self.current_dongle_id = dongle_id

    def is_open(self):
        return bool(self._stream)

    def close(self):
        """
        Closes any open stream associated with this TcTrans instance.
        If there is no open stream, this method does nothing.
        """
        if self._stream.value:
            self._cfuncs['tc_stream_close'](self._stream)
        self._stream = c_void_p()
        self.current_dongle_id = None

    def __del__(self):
        # If we're called during program teardown, as opposed to just being GC'ed during execution,
        # then globals may not be available. See
        # https://docs.python.org/2/reference/datamodel.html#object.__del__
        # Specifically, we've seen the type c_void_p be None inside close(), giving a TypeError.
        # The best course of action seems to be to just swallow TypeError and NameError exceptions, which are the likely
        # exception types in this sort of situation.
        # We don't swallow *all* exceptions, because we wouldn't want to hide an OSError generated by ctypes
        # intercepting an access violation inside the tctrans library (via Win32 SEH), for example.
        try:
            self.close()
        except TypeError:
            pass
        except NameError:
            pass
        
    def get_version(self):
        """
        Returns the version of the tctrans library.
        """
        return from_cstr(self._cfuncs['tc_get_version']())

    def get_device_id(self):
        result = c_char_p()
        self._handle_error(
            self._cfuncs['tc_get_device_id'](self._stream, byref(result))
        )
        return from_cstr(result.value)

    class TcWaitTimeResults(object):
        def __init__(self, actual_total_wait_time_millis, actual_reconnection_attempts):
            self.actual_total_wait_time_millis = actual_total_wait_time_millis
            self.actual_reconnection_attempts = actual_reconnection_attempts

        def __repr__(self):
            return "Waited {0} ms".format(self.actual_total_wait_time_millis)

    def reboot_curator_cmd(self,
                           wait_between_reopen_attempts_millis = 0,
                           max_num_reopen_attempts = 0):
        """
        Send a reboot-Curator ToolCmd and wait for the device to reappear.
        
        If the reboot is reported as successful, this method closes and re-opens the stream,
        because the old stream pointer is no longer usable for I/O.
        See C header for full docs.
        
        :param wait_between_reopen_attempts_millis how long to wait between attempts to re-open
        the connection with the device. If this is zero, a default is used.
        
        :param max_num_reopen_attempts how many times to attempt to re-open the connection with the
        device. If this is zero, a default is used. 
        
        :return a tuple (actual total wait time millis, actual number of connection attempts)
        raises TcErrorRebootFailed if the reboot command fails, or if a connection could not be re-established after
        reboot.
        """
        actual_total_wait_time_millis = c_uint() 
        actual_attempts = c_uint()

        try:
            self._handle_error(
                self._cfuncs['tc_reboot_curator_cmd'](
                    self._stream,
                    wait_between_reopen_attempts_millis,
                    max_num_reopen_attempts,
                    byref(actual_total_wait_time_millis),
                    byref(actual_attempts)
                )
            )
        except TcError as ex:
            raise TcErrorRebootFailed(
                ex,
                TcTrans.TcWaitTimeResults(actual_total_wait_time_millis.value, actual_attempts.value)
            )

        return TcTrans.TcWaitTimeResults(actual_total_wait_time_millis.value, actual_attempts.value)

    def read_windowed(self, subsys_id, addr, num_items):
        """
        Read 16-bit data using TOOL_MEM_READ_WIN_REQ. See C header for further notes.
        :return a list of integers, one per 16-bit element read.
        """
        if num_items == 0:
            return []

        buf = (c_uint16 * num_items)()
        self._handle_error(
            self._cfuncs['tc_read_windowed'](self._stream, subsys_id, addr, buf, num_items)
        )
        # Convert to native Python list on return.
        return buf[0:num_items]

    @staticmethod
    def _make_buf_for_write(data, c_type):
        try:
            length = len(data)
        except TypeError:
            data = [data]
            length = 1

        if length == 0:
            return None, 0

        # noinspection PyCallingNonCallable
        buf = (c_type * length)()
        buf[:] = data
        return buf, length

    def write_windowed(self, subsys_id, addr, data):
        """
        Write 16-bit data using TOOL_MEM_WRITE_WIN_REQ.
        :param data a sequence of 16-bit elements to write.
        """
        buf, length = self._make_buf_for_write(data, c_uint16)
        if length == 0:
            return

        self._handle_error(
            self._cfuncs['tc_write_windowed'](self._stream, subsys_id, addr, buf, length)
        )

    def _do_regbased_read_xy(self, subsys_id, block_id, debug_txns, addr, num_items, c_type, func_name):
        if num_items == 0:
            return []

        buf = (c_type * num_items)()
        self._handle_error(
            self._cfuncs[func_name](
                self._stream, subsys_id, block_id, debug_txns, addr, buf, num_items
            )
        )
        # Convert to native Python list on return.
        return buf[0:num_items]

    def read_regbased_32(self, subsys_id, block_id, debug_txns, addr, num_items):
        """
        Read 32-bit data using TOOL_MEM_READ_REG_EXT_REQ. See C header for further notes.
        :return a list of integers, one per 32-bit element read.
        """
        return self._do_regbased_read_xy(
            subsys_id, block_id, debug_txns, addr, num_items, c_uint32, 'tc_read_regbased_32'
        )

    def read_regbased_16(self, subsys_id, block_id, debug_txns, addr, num_items):
        """
        Read 16-bit data using TOOL_MEM_READ_REG_EXT_REQ. See C header for further notes.
        :return a list of integers, one per 16-bit element read.
        """
        return self._do_regbased_read_xy(
            subsys_id, block_id, debug_txns, addr, num_items, c_uint16, 'tc_read_regbased_16'
        )

    def read_regbased_8(self, subsys_id, block_id, debug_txns, addr, num_items):
        """
        Read 8-bit data using TOOL_MEM_READ_REG_EXT_REQ. See C header for further notes.
        :return a list of integers, one per 8-bit element read.
        """
        # noinspection PyTypeChecker
        # (spurious complaint about c_uint8)
        return self._do_regbased_read_xy(
            subsys_id, block_id, debug_txns, addr, num_items, c_uint8, 'tc_read_regbased_8'
        )

    def read_regbased(self, subsys_id, block_id, octets_per_txn, debug_txns, addr, num_octets):
        """
        Slightly lower-level function that works in terms of octets, and allows explicit specification
        of the octets per transaction.
        See C header for further notes.
        :return a list of integers, one per element read.
        """
        if num_octets == 0:
            return []

        buf = (c_uint8 * num_octets)()
        self._handle_error(
            self._cfuncs['tc_read_regbased'](
                self._stream, subsys_id, block_id, octets_per_txn, debug_txns, addr, buf, num_octets
            )
        )
        # Convert to native Python list on return.
        return buf[0:num_octets]

    def _do_regbased_write_xy(self, subsys_id, block_id, debug_txns, addr, data, c_type, func_name):
        buf, length = self._make_buf_for_write(data, c_type)
        if length == 0:
            return

        self._handle_error(
            self._cfuncs[func_name](
                self._stream, subsys_id, block_id, debug_txns, addr, buf, length
            )
        )

    def write_regbased_32(self, subsys_id, block_id, debug_txns, addr, data):
        """
        Write 32-bit data using TOOL_MEM_WRITE_REG_EXT_REQ. See C header for further notes.
        """
        self._do_regbased_write_xy(subsys_id, block_id, debug_txns, addr, data, c_uint32, 'tc_write_regbased_32')

    def write_regbased_16(self, subsys_id, block_id, debug_txns, addr, data):
        """
        Write 16-bit data using TOOL_MEM_WRITE_REG_EXT_REQ. See C header for further notes.
        """
        self._do_regbased_write_xy(subsys_id, block_id, debug_txns, addr, data, c_uint16, 'tc_write_regbased_16')

    def write_regbased_8(self, subsys_id, block_id, debug_txns, addr, data):
        """
        Write 8-bit data using TOOL_MEM_WRITE_REG_EXT_REQ. See C header for further notes.
        """
        # noinspection PyTypeChecker
        # (spurious complaint about c_uint8)
        self._do_regbased_write_xy(subsys_id, block_id, debug_txns, addr, data, c_uint8, 'tc_write_regbased_8')

    def write_regbased(self, subsys_id, block_id, octets_per_txn, debug_txns, addr, data):
        """
        Slightly lower-level function that works in terms of octets, and allows explicit specification
        of the octets per transaction.
        See C header for further notes.
        """
        # noinspection PyTypeChecker
        # (spurious complaint about c_uint8)
        buf, length = self._make_buf_for_write(data, c_uint8)
        if length == 0:
            return

        self._handle_error(
            self._cfuncs['tc_write_regbased'](
                self._stream, subsys_id, block_id, octets_per_txn, debug_txns, addr, buf, length
            )
        )

    def siflash_identify(self, subsys_id, bank):
        """
        Read information about a Siflash chip on the target.
        :return TcSiflashProperties
        """
        manu_id = c_uint8()
        device_id = c_uint16()
        num_sectors = c_uint16()
        sector_size_kb = c_uint16()

        self._handle_error(
            self._cfuncs['tc_siflash_identify'](
                self._stream, subsys_id, bank,
                byref(manu_id), byref(device_id), byref(num_sectors), byref(sector_size_kb)
            )
        )

        return TcSiflashProperties(
            subsys_id,
            bank,
            manu_id.value,
            device_id.value,
            num_sectors.value,
            sector_size_kb.value
        )
    
    def siflash_identify_alt(self, subsys_id, bank):
        """
        Read information about a Siflash chip on the target.
        :return TcSiflashProperties
        """
        manu_id = c_uint8()
        device_id = c_uint16()

        self._handle_error(
            self._cfuncs['tc_siflash_identify_alt'](
                self._stream, subsys_id, bank,
                byref(manu_id), byref(device_id)
            )
        )

        return TcSiflashProperties(
            subsys_id,
            bank,
            manu_id.value,
            device_id.value,
            None,
            None
        )
    
    def siflash_read(self, subsys_id, bank, octet_addr, num_items):
        """
        Perform a Siflash read.
        """
        buf = (c_uint16 * num_items)()
        self._handle_error(
            self._cfuncs['tc_siflash_read'](
                self._stream, subsys_id, bank, octet_addr, buf, num_items
            )
        )
        # Convert to native Python list on return.
        return buf[0:num_items]

    def siflash_write(self, subsys_id, bank, octet_addr, values):
        """
        Perform a Siflash write.
        """
        buf, length = self._make_buf_for_write(values, c_uint16)
        self._handle_error(
            self._cfuncs['tc_siflash_write'](
                self._stream, subsys_id, bank, octet_addr, buf, length
            )
        )

    def siflash_sector_erase(self, subsys_id, bank, octet_addr, num_sectors):
        """
        Perform a Siflash sector erase.
        If the ToolCmd fails, a TcErrorIoFailed exception will be raised, with an extra field
        'first_siflash_erase_fail_addr', containing the first address not erased.
        """
        first_fail_addr = c_uint32()
        try:
            self._handle_error(
                self._cfuncs['tc_siflash_sector_erase'](
                    self._stream, subsys_id, bank, octet_addr, num_sectors, byref(first_fail_addr)
                )
            )
        except TcErrorIoFailed as ex:
            ex.first_siflash_erase_fail_addr = first_fail_addr.value
            raise ex

    def siflash_chip_erase(self, subsys_id, bank):
        """
        Perform a Siflash full-chip erase.
        If the ToolCmd fails, a TcErrorIoFailed exception will be raised, with an extra field
        'first_siflash_erase_fail_addr', containing the first address not erased.
        """
        first_fail_addr = c_uint32()
        try:
            self._handle_error(
                self._cfuncs['tc_siflash_chip_erase'](
                    self._stream, subsys_id, bank, byref(first_fail_addr)
                )
            )
        except TcErrorIoFailed as ex:
            ex.first_siflash_erase_fail_addr = first_fail_addr.value
            raise ex
    
    def siflash_vector_inject(self, subsys_id, bank, data, reply_count = 0):
        """
        Inject a vector of commands into the Siflash.
        :param reply_count how many octets to request in reply.
        :return if reply_count is > 0, a list of length reply_count with the reply.
        Otherwise, None.
        """
        buf, length = self._make_buf_for_write(data, c_uint16)
        if reply_count:
            # noinspection PyCallingNonCallable,PyTypeChecker
            reply_buf = (c_uint8 * reply_count)()
        else:
            reply_buf = None
            
        self._handle_error(
            self._cfuncs['tc_siflash_vector_inject'](
                self._stream, subsys_id, bank, buf, length, reply_buf, reply_count
            )
        )
        if reply_count:
            # Convert to native Python list on return.
            return reply_buf[0:reply_count]
        else:
            return None

    def createTestTunnel(self, transport='usbdbg'):
        """factory method to create new TestTunnel objects for this TcTrans
        """
        return TcTrans.TestTunnel(self, transport)

    class TestTunnel(object):
        
        def __init__(self, outer_instance, transport):
            """
            manage the outer class instance and our testtunnel.
            convert transport string into isp_addr magic value.
            """
            self.outer_instance = outer_instance
            self._testtunnel = c_void_p()
            self._available_transports={'usbdbg' : _TC_TUNNEL_ISP_USBDBG,
                                        'trb' : _TC_TUNNEL_ISP_TRB,
                                        'usbcc' : _TC_TUNNEL_ISP_USBCC,
                                        'btoip' : _TC_TUNNEL_ISP_WIRELESS
                                        }
            if isinstance(transport, str) and transport in self._available_transports.keys():
                self.transport = self._available_transports[transport]
            else:
                raise ValueError("'transport' must be a none empty string in %r" %
                                 self._available_transports.keys())


        def __del__(self):
            try:
                self.tunnel_close()
            except TypeError:
                pass
            except NameError:
                pass

        def tunnel_open_rfcli(self, dongle_id):
            """open a rfcli testtunnel"""
            self.tunnel_open(dongle_id, _TC_TUNNEL_CLIENT_RFCLI, self.transport)

        def tunnel_open_btcli(self, dongle_id):
            """open a btcli testtunnel"""
            self.tunnel_open(dongle_id, _TC_TUNNEL_CLIENT_BTCLI, self.transport)

        def tunnel_open_accmd(self, dongle_id):
            """open an accmd testtunnel"""
            self.tunnel_open(dongle_id, _TC_TUNNEL_CLIENT_ACCMD, self.transport)

        def tunnel_open_ptcmd(self, dongle_id):
            """open a ptcmd testtunnel"""
            self.tunnel_open(dongle_id, _TC_TUNNEL_CLIENT_PTCMD, self.transport)

        def tunnel_open_hostcomms(self, dongle_id):
            """open a host comms testtunnel"""
            self.tunnel_open(dongle_id, _TC_TUNNEL_CLIENT_HOSTCOMMS, self.transport)
            
        def tunnel_open_qact(self, dongle_id):
            """open a qact esttunnel"""
            self.tunnel_open(dongle_id, _TC_TUNNEL_CLIENT_QACT, self.transport)

        def tunnel_open(self, dongle_id, client_id, isp_addr):
            """
            Opens a testtunnel for the dongle_id, client and addr specified.
            If dongle_id is None will attempt to reuse the current active TcTrans dongle's id.
            Will close an open testtunnel first.
            """
            # reuse current dongle?
            if dongle_id is None:
                dongle_id = self.outer_instance.current_dongle_id

            if not isinstance(dongle_id, str) or not dongle_id:
                raise ValueError("'dongle_id' must be a non-empty string or "
                         "TcTrans.open() must have previously completed.")

            self.tunnel_close()

            if isp_addr == _TC_TUNNEL_ISP_WIRELESS and not (dongle_id == '501' or dongle_id == '502'):
                raise ValueError("'dongle_id' must be '501' or '502' for 'btoip' for Left or Right "
                "connected via ADB at localhost:13572")    

            self.outer_instance._handle_error(
                self.outer_instance._cfuncs['tc_tunnel_open'](dongle_id, client_id, isp_addr,
                               byref(self._testtunnel))
            )

        def tunnel_close(self):
            """
            Closes any open testtunnel associated with this TcTrans instance.
            If there is no open tunnel, this method does nothing.
            """
            if self._testtunnel.value:
                self.outer_instance._cfuncs['tc_tunnel_close'](self._testtunnel)
                self._testtunnel = c_void_p()

        def tunnel_request(self, pdu_data):
            """
            Sends pdu_data to the current testtunnel
            """
            # noinspection PyTypeChecker
            # (spurious complaint about c_uint8)
            sendbuf, length = self.outer_instance._make_buf_for_write(pdu_data, c_uint8)
            if length == 0:
                return

            self.outer_instance._handle_error(
                self.outer_instance._cfuncs['tc_tunnel_request'](self._testtunnel, sendbuf, length)
            )                               

        def tunnel_response(self, num_octets=2048, timeout_milliseconds=1000):
            """
            Get pdu_data response of size not exceeding num_octets from the current
            testtunnel.
            Returns a tuple of (total_octets, [received]).

            If total_octets > len(received) the send/receive actions will need
            repeating with a larger num_octets size.
            """
            if num_octets == 0:
                return (0, [])

            recvbuf = (c_uint8 * num_octets)()

            outlen = c_uint32()

            self.outer_instance._handle_error(
                self.outer_instance._cfuncs['tc_tunnel_response'](self._testtunnel, recvbuf, num_octets,
                                  byref(outlen), timeout_milliseconds)
            )                                                               

            if outlen.value and (outlen.value <= num_octets):
                # Convert to native Python list on return
                return (outlen.value, recvbuf[0:outlen.value])

            return (outlen.value, None)


@contextmanager
def scoped_tc_stream(dongle_id):
    """auto-closing tc_stream"""
    stream = TcTrans()
    try:
        stream.open(dongle_id)
        yield stream
    finally:
        stream.close()
