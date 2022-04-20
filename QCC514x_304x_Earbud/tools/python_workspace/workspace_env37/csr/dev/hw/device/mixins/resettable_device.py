from csr.wheels.bitsandbobs import timeout_clock

class ResettableDevice(object):
    """
    Abstract mixin providing high-level basic reset functionality.
    
    A class mixing-in a ResettableDevice must provide a method "reset_dut" taking
    arbitrary keyword arguments, and must be a BasecComponent so that it provides
    an apply_reset() method.
    
    However, note that instances of ResettableDevice itself won't work 
    properly: only a derived class providing a class attribute 
    "DEFAULT_RESET_DELAY" should be mixed into devices (i.e. BaseDevice-derived
    classes).
    """

    def reset_dut(self, reset_type=None, **kwargs):
        trans = self.transport
        try:
            trans.reset_device
        except AttributeError:
            raise NotImplementedError("%s doesn't support device reset!" %
                                      trans.__class__.__name__)
        else:
            trans.reset_device(self.chip.curator_subsystem, reset_type=reset_type)
            trans.reset(**kwargs)

    def reset(self, reset_type=None, **kwargs):
        """
        Perform a SimpleResettableDevice.reset with a configurable delay.  The
        delay is measured from when the simple reset is invoked, so it needs to
        include an extra window for the time between the Python being invoked
        and the reset actually taking effect on chip.  Hopefully this is a small
        number of ms, if that.
        """
        valid_reset_types = ["require_por", "prefer_por",
                              "require_pbr", "prefer_pbr",
                              None]
        if reset_type not in valid_reset_types:
            raise ValueError("Valid values for reset_type are: {}".format(", ".join(
                valid_reset_types[:-1])))
        
        if "delay" in kwargs:
            delay = kwargs["delay"]
            del kwargs["delay"]
        else:
            delay = self.DEFAULT_RESET_DELAY
        
        # Start the timer
        start_time = timeout_clock()
        
        # Physically reset the DUT
        # Pass reset_type as kwarg to satisfy emulator reset_dut signature
        self.reset_dut(reset_type=reset_type, **kwargs)
        # Perform a cascading reset through the device model
        self.apply_reset()
                
        end_time = start_time + delay
        while timeout_clock() < end_time:
            pass


class SimpleResettableDevice(ResettableDevice):
    """
    No wait after reset by default
    """
    DEFAULT_RESET_DELAY = 0 # Don't wait at all
        
        
class BootTimeoutResettableDevice(ResettableDevice):
    """
    3-second wait after reset by default
    """
    DEFAULT_RESET_DELAY = 3 # This is the default because on chips like CSRA68100
    # the Curator waits 2.5 seconds after starting boot for a panic to occur
    # before it decides that boot has worked OK.  So we don't want any Python to
    # interfere before this point, in particular by triggering another reset,
    # which might lead to a three-strikes lock-out.
    
class Haps7BootTimeoutResettableDevice(BootTimeoutResettableDevice):
    """
    Special type of boot-delay resettable which adds in the extra time required
    to cover the HAPS7 FGPA's hardware reset
    """
    # A Haps7 emulator takes a significant amount of time to reset
    DEFAULT_RESET_DELAY = 0.75 + BootTimeoutResettableDevice.DEFAULT_RESET_DELAY
