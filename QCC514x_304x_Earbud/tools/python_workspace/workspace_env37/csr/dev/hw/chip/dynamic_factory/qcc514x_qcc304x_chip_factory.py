CHIP_VERSION_MAJOR = (0x4b,0x4c)

def factory(chip_version, access_cache_type):
    
    if chip_version.minor in (0, 1):
        from ..qcc514x_qcc304x_d00_chip import QCC514X_QCC304XD00Chip
        chip = QCC514X_QCC304XD00Chip(access_cache_type)
    else:
        raise NotImplementedError("QCC514X_QCC304X r%02d not yet "
                                  "supported" % chip_version.minor)

    return chip