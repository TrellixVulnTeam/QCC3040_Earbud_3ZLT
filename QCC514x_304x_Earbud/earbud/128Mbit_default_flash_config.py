# Copyright (c) 2017-2020 Qualcomm Technologies International, Ltd.
# The format of this file is documented in CS-00410685-UG / 80-CG297-1

# The block assignment in this layout should satisfy:
#   boot_block_size + image_header + sum(layout['capacity']) <= alt_image_offset
# image_header size is usually just 1 * block_size

{
    "flash_device": {
        "block_size": 4 * 1024,
        # Boot block is common for both banks, but only bank A is used.
        # To ensure DFU works with ADK20.1 and earlier projects, boot_block_size must be set to 16 * 4 *1024.
        # boot_block_size is set to 1 * 4 *1024, for more efficient flash usage
        "boot_block_size": 1 * 4 * 1024,
        "alt_image_offset": 2048 * 4 * 1024
    },
    "encrypt": False,
    "layout": [
        # image_header partition
        ("curator_fs",      {"capacity":  16 * 4 * 1024,  "authenticate": False, "src_file_signed": False}),
        ("apps_p0",         {"capacity": 192 * 4 * 1024,  "authenticate": True, "src_file_signed": True}),
        ("apps_p1",         {"capacity": 300 * 4 * 1024,  "authenticate": False}),
        # Device config filesystem size limited by size of production test buffer,  ( 1024*2)-10.
        ("device_ro_fs",    {"capacity":  16 * 4 * 1024,  "authenticate": False, "inline_auth_hash": True }),
        ("rw_config",       {"capacity":  32 * 4 * 1024}),
        ("debug_partition", {"capacity":   6 * 4 * 1024}),
        # If the GAA Voice Assistant is not needed in the build
        # the VMdl partition can be removed from the flash layout and the capacity
        # can be distributed as needed to the other partitions.
        # The partition can hold: 2 (current model size + headroom) + rafs FAT table = 2 * (65Kb + 4Kb) + 4Kb = 142Kb
        # => 144Kb (rounded to the nearest block) = 36 blocks
        ("ra_partition",    {"capacity":  36 * 4 * 1024,  "id": "VMdl"}),
        ("ro_cfg_fs",       {"capacity":  64 * 4 * 1024,  "authenticate": False}),
        ("ro_fs",           {"capacity": 444 * 4 * 1024,  "authenticate": False})
    ]
}
