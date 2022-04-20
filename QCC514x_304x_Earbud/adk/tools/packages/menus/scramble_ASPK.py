#!/usr/bin/env python
# Copyright (c) 2019-20 Qualcomm Technologies International, Ltd.
#   %%version
"""
Generates a scrambled ASPK from using a  modulus, seed and ASPK.
After device model registration,the OEM provides distribute a
256-bit Anti-Spoofing Private Key (ASPK).This is an integer on the
secp256r1 elliptic curve).

SecurityCmd.exe -product mod seed aspk output
"""

#!/usr/bin/env python
# Copyright (c) 2019-20 Qualcomm Technologies International, Ltd.
#   %%version
"""
Generates a scrambled ASPK from using a  modulus, seed and ASPK.
After device model registration,the OEM provides distribute a
256-bit Anti-Spoofing Private Key (ASPK).This is an integer on the
secp256r1 elliptic curve).

SecurityCmd.exe -product mod seed aspk output
"""
import os
import json
import sys
from textwrap import wrap

class ScrambleASPK(object):
    def __init__(self):
        _TOOL = '..\\bin\\SecurityCmd.exe'
        _TOOLCMD = 'scrambleaspk '
        _PRODUCT = '-product CDA'

        self._params ={}
        self._tool = _TOOL
        self._tool_cmd = _TOOLCMD
        self._product = _PRODUCT
        self._saspk = ''

    def _readJsonValue(self, data, value):
        return data.get(value, None)

    def _deleteFile(self, input_file):
        try:
            os.remove(input_file)
        except(IOError) as e:
            print("Error while deleting file ", input_file)
            print("Error-no",e.errno)

    def _readAspk(self, input_file):
        """ Read Scramabled ASPK from file and ensure it is 64 Octets """
        try:
            with open(input_file) as f:
                saspk_str = f.read()
        except (IOError) as e:
            FILE_NOT_FOUND = 2
            if e.errno == FILE_NOT_FOUND:
                print("File not found")
            sys.exit(1)

        return saspk_str.zfill(64)

    def _readInput(self, json_file):
        """ parse json file to read Mod, Seed and ASPK """
        params={}

        try:
            with open(json_file,'r') as json_data:
                data = json.load(json_data)
                # Python 3: except FileNotFoundError:
        except (IOError) as e:
            FILE_NOT_FOUND = 2
            if e.errno == FILE_NOT_FOUND:
                print("File not found")
            sys.exit(1)

        params['mod'] = self._readJsonValue(data, 'MOD')
        params['seed'] = self._readJsonValue(data, 'SEED')
        params['aspk'] = self._readJsonValue(data, 'ASPK')

        return params

    def __call__(self,input_file):
        """ Call Security Command to scramble inputs """
        saspk_file = os.path.join(os.path.dirname(__file__), 'saspk.txt')

        self._params = self._readInput(input_file)

        mod = self._params['mod']
        seed = self._params['seed']
        aspk = self._params['aspk']

        cmd = ' '.join([self._tool,'-quiet', self._tool_cmd, mod, seed, aspk, \
                      saspk_file, self._product])
        print('\nExecuting : {}\n'.format(cmd))
        os.system(cmd)

        self._saspk = self._readAspk(saspk_file)
        self._deleteFile(saspk_file)

        return self._saspk

    def __str__(self):
        return "%s" % (self._saspk)

    def getCstring(self):
        inc = 4
        comma=', '
        words = list( map(lambda x:'0x'+x, wrap(self._saspk,inc)) )
        words_str = comma.join(words)

        return '#define SCRAMBLED_ASPK {' + words_str + '}'

    def getScrambledAspk(self):
        """ Read Scramabled ASPK in little endian format """
        scrambled_aspk = " ".join( list(map(lambda x:x[2:]+' '+x[:2], wrap(self._saspk,4))) )

        return 'USR6 = [ ' + scrambled_aspk + ' ]'

def main():
    PARAM_FILE = os.path.join(os.path.dirname(__file__), 'user_defined_mod_seed_aspk.json')

    scramble = ScrambleASPK()
    scramble(PARAM_FILE)

    print ('ASPK')
    print('Scrambled {}'.format(scramble))
    print('*C syntax*: ASPK')
    print(scramble.getCstring())
    print('Scrambled ASPK to be replaced in USR6 PS Key in subsys7_psflash.htf file')
    print(scramble.getScrambledAspk())

if __name__ == '__main__':
    main()

