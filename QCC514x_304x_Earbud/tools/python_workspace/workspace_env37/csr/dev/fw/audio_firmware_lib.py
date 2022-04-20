############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2014 - 2020 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
from io import StringIO
import sys;
import telnetlib
import re;
import os;
import inspect
from csr.wheels import gstrm
from csr.wheels.global_streams import iprint
import csr # for csr.dev.trb_raw.debug_write

#CS-205120 for block IDs when using TBus debug/SPI
_BLOCK_ID = {
            "BUILD_ID_CPU0_DM":0b1000,
            "BUILD_ID_CPU0_PM":0b1001,
            "BUILD_ID_CPU1_DM":0b1010,
            "BUILD_ID_CPU1_PM":0b1011
            }

def get_byte(line):
    line_res = re.search("^@([0-9A-Fa-f]*) ([0-9A-Fa-f]*)", line);
    address = int(line_res.group(1), 16)
    data = int(line_res.group(2), 16)
    return (address, data)

def load_ram_contents(path=None, proc=0, verbose=0):
    """
    @brief Loads a build into ram.
    @param path Path to pm and dm files
    @param path proc processor to load the pm and dm files. By defautl p0
    @param path verbose debug level. Enabling this will print the copies to
           RAM
    """

    if path is None:
        #keep it generic for different chips
        return False
    dm = path + ".dm"
    pm = path + ".pm"
        
    if not os.path.isfile(dm):
        iprint("Error, [", dm, "] is not a file")
        return False
    if not os.path.isfile(pm):
        iprint("Error, [", pm, "] is not a file")
        return False
    
    if proc > 1 or proc < 0:
        iprint("Error, proc", proc, "does not exist.")
        return False
    pm_block_id = _BLOCK_ID["BUILD_ID_CPU" + str(proc) + "_PM"]
    dm_block_id = _BLOCK_ID["BUILD_ID_CPU" + str(proc) + "_DM"]
        
    iprint("Loading proc", proc, " pm...")
    ret = load_ram_section(pm, pm_block_id, verbose)
    iprint("Loading proc", proc, " dm...")
    ret = load_ram_section(dm, dm_block_id, verbose)
    return ret

def load_ram_section(fname, block_id, verbose):
    ret = True
    with open(fname, "r") as f:
        content = f.readlines()
        
    for i in range(0, len(content), 4):
        to_write = 0
        (address, data) = get_byte(content[i])
        to_write |= data
        (dummy_addr, data) = get_byte(content[i+1])
        to_write |= data << 8
        (dummy_addr, data) = get_byte(content[i+2])
        to_write |= data << 16
        (dummy_addr, data) = get_byte(content[i+3])
        to_write |= data << 24
        if verbose:
            iprint("writting [", hex(address), ",", hex(to_write), "]")
        try:
            csr.dev.trb_raw.debug_write(3, address, 4, to_write, blockId=block_id)
        except: # TrbStatusException
            iprint("Error, exception while trying to load line", content[i])
            raise
            ret = False
    return ret

def dump_args(func):
    enabled = False  # enabled this for more logging
    if not enabled:
        return func
    
    from csr.wheels.open_source import arg_display_wrapper

    def decorator(*func_args, **func_kwargs):
        return arg_display_wrapper(func, *func_args, **func_kwargs)

    return decorator


class Capturing(list):
    def __enter__(self):
        self._stdout = gstrm.iout
        gstrm.iout = self._stringio = StringIO()
        return self
    def __exit__(self, *args):
        self.extend(self._stringio.getvalue().splitlines())
        gstrm.iout = self._stdout

def decode_firmware_id_string(string):
    string.replace(os.linesep, "");
    string = re.search(r"id_string:\[([[x0-9A-Fa-f\s]*)\]", string).group(1)
    string = string.replace("0x", "").replace(" ", "").decode('hex');
    string = re.sub("(.)(.)", "\\2\\1", string)
    return string;

def decode_id_string(string):
    iprint(decode_firmware_id_string(string))


class Daemoniser():
    def __init__(self, name, port, verbose=0, timeout=5):
        self.name = name;
        self.disable = 0;
        try:
            self.tn = telnetlib.Telnet("localhost", port);
        except:
            iprint("Warning, could not setup ", self.name, " connection");
            self.disable = 1;
        self.verbose = verbose;
        self.timeout = timeout;

    def debug(self, verbose):
        self.verbose = verbose

    def expect(self, success_regex, verbose=0):
        if self.disable:
            return (True, None);
        (a, b, c) = self.tn.expect([success_regex], self.timeout);
        if verbose > 0:
            iprint("a is ", a, " c is [", c, "]");
        if a == 0:
            ret = True;
        else:
            ret = False;
        return (ret, c);
        

    def write(self, cmd, success_regex="", verbose=0):
        if self.disable:
            return (True, None);
        consume = self.tn.read_very_eager(); ## flush all output;
        c = "";
        ret = True;
        if self.verbose and cmd != "":
            sys.stderr.write("* " + self.name + " [" + cmd + "]\n");
        self.tn.write(cmd + "\n");
        if success_regex != "":
            (ret, c) = self.expect(success_regex, verbose - 1);
        return (ret, c);

    def read(self):
        return self.tn.read_very_eager();

class Btcli():
    def restart(self):
        (ret, resp) = self.daemon.write("restart", "daemoniser: 'restart' success");
        if not ret:
            raise Exception("BtcliError-restart");
        return (ret, resp);

    def get_nop(self):
        (ret, resp) = self.daemon.write("", "command_status pending nhcp:0x01 nop");
        if not ret:
            raise Exception("BtcliError-nop");
        return (ret, resp);

    def rba(self):
        (ret, resp) = self.daemon.write("rba", "read_bd_addr success ba:0x[0-9A-Fa-f]*");
        if not ret:
            raise Exception("BtcliError-rba");
        return (ret, resp);

    def __init__(self):
        self.daemon = Daemoniser("btcli", 6666, 1);

    def bcset(self, string, val):
        (ret, resp) = self.daemon.write(
                "bcset {cmd} {val}".format(cmd = string, val = str(val)),
                string + " ok");
        if not ret:
            raise Exception("BtcliError-" + string);
        return (ret, resp);

    def set_timeout(self, timeout):
        self.daemon.timeout = timeout;

class Omnicli():
    def restart(self):
        self.daemon.write("restart");

    def sapconnect(self):
        self.daemon.write(r"load G:\depot\dspsw\csra68100_dev\kalimba\kymera\common\hydra\interface\accmd_saps.xml")
        (ret, resp) = self.daemon.write("sapconnect audio_accmd tcpclient localhost:9901", "I connected");
        if not ret:
            raise Exception("OmnicliError-sapconnect");
        return (ret, resp);

    def get_firmware_version(self):
        (ret, resp) = self.daemon.write("get_firmware_version_req " + str(self.uid),
                "get_firmware_version_resp seq_no:0x[0-9A-Fa-f]* ok version:0x[0-9A-Fa-f]{4}");
        if not ret:
            raise Exception("OmnicliError-get_firmware_version");
        self.uid += 1
        return (ret, resp);

    def get_firmware_id_string(self, decode=1):
        (ret, resp) = self.daemon.write("get_firmware_id_string_req " + str(self.uid),
                "]\r\n$");
        if not ret:
            raise Exception("OmnicliError-get_firmware_id_string");
        if decode:
            resp = decode_firmware_id_string(resp);
        self.uid += 1
        return (ret, resp);
        
    def set_verbose(self, verbose):
        self.daemon.verbose = verbose;

    def set_timeout(self, timeout):
        self.daemon.timeout = timeout;

    def __init__(self):
        self.daemon = Daemoniser("omnicli", 6667, verbose=1);
        self.uid = 0

def disable_patchpoints(audio, pid):
    for cnt in pid:
        audio.fields["ROM_PATCH{cnt}_EN".format(cnt=cnt)] = 0

def extract_symbol_address(audio, sym_name):
    fname = os.path.join(audio.fw.env.build_info.build_dir, "kymera_csra68100_audio.sym")
    if not os.path.isfile(fname):
        iprint("Error, [", fname, "] does not exist")
        return False
    with open(fname, "r") as f:
        for line in f.readlines():
            m = re.match(r"([0-9A-Fa-f]*).*" + sym_name, line)
            if m:
                return int(m.group(1), 16)
    return False
