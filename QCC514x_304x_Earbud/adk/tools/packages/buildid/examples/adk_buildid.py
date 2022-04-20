# Copyright (c) 2017 - 2021 Qualcomm Technologies International, Ltd.
import sys
import time
import os
import zlib

try:
    from vm_build.buildid.writer import BuildIdWriter
    
except ImportError:
    from buildid.writer import BuildIdWriter

# Usage
# script reads environment variable to set the fw_ver

class BuildIdGen(object):
    """
    This BuildIdGen class uses an environment variable to control the fw_ver string in an ADK build.
    This script is not intended to generate unique IDs, it incorporates the environment variable in the AppsP1
    firmware image.
    Set ADK_BUILDID to a string which meets the some system requirement. This allows flexibility with the script
    to be incorporated into some build system which generates unique build IDs for software release management.
    If ADK_BUILDID is not set then the build is assumed to be a local untracked build.
    The id_string is generated from the local time, formatted as YY-MM-DD HH:MM:SS, then if ADK_BUILDID is set
    the variable is used as-is. If ADK_BUILDID is not set, then the id_string will report the name of the local 
    user, and assign a semi unique number which is simply the local system time in number of seconds since 1st 
    Jan 1970.
    For example, if ADK_BUILDID=Build@123456789abcdef0 then the output will resemble:
        QTIL ADK 2021-01-15 08:41:13 Build@123456789abcdef0
    If ADK_BUILDID is not set, then id_string will look lile:
        QTIL ADK 2021-01-15 08:41:10 local/localusr @1610700070
    
    An added complication is that the function get_id_number expects to return a 32 bit number for use by the
    appsP1 image. Ideally we should derive this number from the ADK_BUILDID, which is a string. The example here
    attempts to interpret the string as a hex number, from the final : or @, to cope with the above example text.
    If this is not the case, then the algorithm returns a 32-bit hash value calculated from the ADK_BUILDID string.
    """
    
    def __init__(self):
        self.__now = time.time()
        self.adk_buildid = os.getenv("ADK_BUILDID")
        UINT32_MAX = 4294967295
                
        if (self.adk_buildid is None): 
          # Prepare a default build ID string based on the local user and the current system time
          self.adk_buildid = "local/" + os.getenv('username', 'unknown')
          self.id_number = int(self.__now) % UINT32_MAX
          local_time = time.localtime(self.__now)
          self.id_str = "{:s} @{:d}".format(self.adk_buildid, self.id_number)
        else:
          self.id_str = self.adk_buildid
          # id_number interpreted at a hex number, reduced to 32 bits
          # Look for last @ or : in string in case it is formatted like
          # "Release@<id>" or "Build:<id>"
          r_col = self.id_str.rfind(':')
          r_at = self.id_str.rfind('@')
          if (r_col > r_at):
            r = r_col
          else:
            r = r_at
          r = r + 1
          
          try:
            self.id_number = int(self.id_str[r:],16) & UINT32_MAX
          except ValueError:
            # Generate a 32 bit number based on the string provided.
            self.id_number = zlib.crc32(self.id_str.encode())

        local_time = time.localtime(self.__now)
        self.id_string = "QTIL ADK {} {}".format(time.strftime('%Y-%m-%d %H:%M:%S', local_time), self.id_str)

    def get_id_string(self):
        return self.id_string

    def get_id_number(self):
        return self.id_number

if __name__ == "__main__":
    build_id_string_file = sys.argv[1]
    gen = BuildIdGen()
    BuildIdWriter.write(build_id_string_file, gen.get_id_number(), gen.get_id_string())
   