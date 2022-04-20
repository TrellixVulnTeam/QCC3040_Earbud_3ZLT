CHIP_VERSION_MAJOR = (0x4b,0x4c)

def factory(chip_version, chip, transport, access_cache_type):
    
    from ..qcc514x_qcc304x_lab_device import QCC514X_QCC304XLabDevice
    return QCC514X_QCC304XLabDevice(chip, transport, access_cache_type)
