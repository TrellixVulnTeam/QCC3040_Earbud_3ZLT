# Copyright (c) 2020 - 2021 Qualcomm Technologies International, Ltd.

class WizardError(RuntimeError):
    pass


class WizardThreadException(RuntimeError):
    def __init__(self, exc_info, *args):
        self.exc_info = exc_info
        super(WizardThreadException, self).__init__(*args)
