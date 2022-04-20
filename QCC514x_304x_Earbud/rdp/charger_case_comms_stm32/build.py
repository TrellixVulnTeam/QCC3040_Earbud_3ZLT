#
# \copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
#            All Rights Reserved.\n
#            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#\file
#\brief      Charger Case build script
#

import os
import zlib
import argparse
from shutil import copyfile
from shutil import rmtree
from datetime import datetime

global cwd

# All the build variants.
# field 0: name
# field 1: bootloader name
# field 2: long name(s)
all_variants = [
    ['CB',  'CB', ['qcc5151_rdp_1.0.1_case'] ],
    ['ST2', 'CB', ['qcc5171_rdp_1.0.1_case'] ]
]


def fail():

    print('')
    print(' ########    ###    #### ##       ##     ## ########  ######## ')
    print(' ##         ## ##    ##  ##       ##     ## ##     ## ##       ')
    print(' ##        ##   ##   ##  ##       ##     ## ##     ## ##       ')
    print(' ######   ##     ##  ##  ##       ##     ## ########  ######   ')
    print(' ##       #########  ##  ##       ##     ## ##   ##   ##       ')
    print(' ##       ##     ##  ##  ##       ##     ## ##    ##  ##       ')
    print(' ##       ##     ## #### ########  #######  ##     ## ######## ')
    exit(1)


def build_variant(variant):
    global cwd

    os.chdir(os.path.join(cwd, variant))

    if os.system("make clean"):
        fail()

    if os.system("make all"):
        fail()

    os.chdir(cwd)


def byte_swap_32(x):
    return ((x & 0x000000FF) << 24) + \
        ((x & 0x0000FF00) << 8) + \
        ((x & 0x00FF0000) >> 8) + \
        ((x & 0xFF000000) >> 24)


def s0(csum_a, csum_b, variant):

    # Address.
    r = '0000'

    # Payload.
    r += '{0:0{1}X}'.format(byte_swap_32(csum_a), 8)
    r += '{0:0{1}X}'.format(byte_swap_32(csum_b), 8)

    vlen = len(variant)
    for n in range(7):
        if n < vlen:
            r += variant[n].encode('utf-8').hex()
        else:
            r += '00'
    r += '00'

    # Length.
    rlen = len(r)
    r = '{0:0{1}X}'.format(int(rlen / 2) + 1, 2) +  r

    # Checksum.
    csum = 0
    for n in range(0, rlen + 2, 2):
        csum += int(r[n:n+2], 16)
    csum = ((csum ^ 0xFF) & 0xFF)
    r += '{0:0{1}X}'.format(csum, 2)

    return 'S0' + r


def bit_swap(data):
    result = 0;

    for n in range(32):
        result = result << 1

        if (data & 0x1):
            result = result | 0x1

        data = data >> 1;

    return result & 0xFFFFFFFF


def calculate_crc32(all_records):
    crc32 = 0

    for record in all_records:
        if record.startswith("S3"):
            payload = record.rstrip()[12:-2]
            plen = len(payload)

            # Pad out payload if it is not divisible by 4 bytes.
            while plen % 8:
                payload += 'FF'
                plen += 2

            for n in range(0, plen, 8):
                crc32 = zlib.crc32(bit_swap(int(payload[n:n+8], 16)).to_bytes(4, 'big'), crc32)

    crc32 = bit_swap(crc32) ^ 0xFFFFFFFF

    return crc32


def build_dfu(variant):
    global cwd

    fout = open(os.path.join(cwd, variant) + "_dfu.srec", "w")

    fa = open(os.path.join(cwd, variant + "_A", "charger-comms-stm32f0.srec"))
    alla = fa.readlines()

    fb = open(os.path.join(cwd, variant + "_B", "charger-comms-stm32f0.srec"))
    allb = fb.readlines()

    # Work out CRC32 for Image A.
    crc32a = calculate_crc32(alla)

    # Work out CRC32 for Image B.
    crc32b = calculate_crc32(allb)

    # Write the S0 record.
    fout.write(s0(crc32a, crc32b, variant) + "\n")

    # Write the S3 records for Image A.
    for record in alla:
        if record.startswith("S3"):
            fout.write(record)

    # Write the S3 records for Image B.
    for record in allb:
        if record.startswith("S3"):
            fout.write(record)

    # Write the S7 record.
    fout.write("S70500000000FA\n")

    fa.close()
    fb.close()
    fout.close()


def build_image(variant, boot_variant):
    global cwd

    fout = open(os.path.join(cwd, variant) + ".srec", "w")

    # Write a minimal S0 record.
    fout.write("S0050000434275\n")

    # Write all the S3 records from the bootloader.
    fboot = open(os.path.join(cwd, boot_variant + "_BOOT", "charger-comms-stm32f0.srec"))
    allboot = fboot.readlines()
    for record in allboot:
        if record.startswith("S3"):
            fout.write(record)

    # Write all the S3 records from Image A.
    fa = open(os.path.join(cwd, variant + "_A", "charger-comms-stm32f0.srec"))
    alla = fa.readlines()
    for record in alla:
        if record.startswith("S3"):
            fout.write(record)

    # Write the S7 record.
    fout.write("S70500000000FA\n")

    fa.close()
    fboot.close()
    fout.close()


def main(argv = []):
    global cwd

    cwd = os.getcwd()

    parser = argparse.ArgumentParser(description='Charger Case Build')
    parser.add_argument("-a", default=False, action="store_true", help = "Build image A only")
    parser.add_argument("-b", default=False, action="store_true", help = "Build image B only")
    parser.add_argument("-o", default=False, action="store_true", help = "Build boot image only")
    parser.add_argument("-s", default=0, type=int, help = "Software version number")
    parser.add_argument("-t", default='UNRELEASED', type=str, help = "Software version string")
    parser.add_argument("-u", default=False, action="store_true", help = "Run unit tests")
    parser.add_argument("-v", default='all', type=str, help = "Variant")
    args = parser.parse_args()

    if not os.path.exists(os.path.join(cwd, "include", "override")):
        os.makedirs(os.path.join(cwd, "include", "override"))
    fts = open(os.path.join(cwd, "include", "override", "timestamp.h"), "w")
    now = datetime.now()
    fts.write("#define DATE_STAMP \"" + now.strftime("%b %d %Y") + "\"\n")
    fts.write("#define TIME_STAMP \"" + now.strftime("%H:%M:%S") + "\"\n")
    fts.close()

    fver = open(os.path.join(cwd, "include", "override", "version.h"), "w")
    fver.write("#define SW_VERSION_NUMBER " + str(args.s) + "\n")
    fver.write("#define SW_VERSION_STRING \"" + args.t + "\"\n")
    fver.close()

    for v in all_variants:
        if args.v == 'all' or args.v == v[0]:
            if v[1] == '':
                build_variant(v[0])
                copyfile(os.path.join(cwd, v[0], "charger-comms-stm32f0.hex"), os.path.join(cwd, v[0] + '.hex'))
            else:
                if args.a:
                    build_variant(v[0] + '_A')
                elif args.b:
                    build_variant(v[0] + '_B')
                elif args.o:
                    build_variant(v[1] + '_BOOT')
                else:
                    build_variant(v[1] + '_BOOT')
                    build_variant(v[0] + '_A')
                    build_variant(v[0] + '_B')
                    build_dfu(v[0])
                    build_image(v[0], v[1])

                    # Create long-named output files.
                    for ln in v[2]:
                        copyfile(os.path.join(cwd, v[0] + '.srec'), os.path.join(cwd, ln + '.srec'))
                        copyfile(os.path.join(cwd, v[0] + '_dfu.srec'), os.path.join(cwd, ln + '_dfu.srec'))

            if args.u:
                if os.system("ceedling project:" + v[0] + " clobber gcov:all utils:gcov"):
                    fail()

                try:
                    rmtree(os.path.join(cwd, "build", v[0], "gcov"))
                except:
                    fail()
                    
                try:
                    rmtree(os.path.join(cwd, "build", v[0], "test"))
                except:
                    fail()
                
    print('')
    print('  ######  ##     ##  ######   ######  ########  ######   ######  ')
    print(' ##    ## ##     ## ##    ## ##    ## ##       ##    ## ##    ## ')
    print(' ##       ##     ## ##       ##       ##       ##       ##       ')
    print(' ######   ##     ## ##       ##       ######    ######   ######  ')
    print('       ## ##     ## ##       ##       ##             ##       ## ')
    print(' ##    ## ##     ## ##    ## ##    ## ##       ##    ## ##    ## ')
    print('  ######   #######   ######   ######  ########  ######   ######  ')

    exit(0)

if __name__ == "__main__":
    main()
