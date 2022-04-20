############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2020 Qualcomm Technologies, Inc. and/or its subsidiaries.
# All rights reserved.
#
############################################################################
"""Module to provide JSON schema validation."""

import copy

from jsonschema import Draft7Validator, validators


def extend_with_default(validator_class):
    """Extend validator with default processing.

    Args:
        validator_class (jsonschema.validator): Validator class to extend.

    Returns:
        jsonschema.validator: Extended validator class.
    """
    validate_properties = validator_class.VALIDATORS['properties']

    def set_defaults(validator, properties, instance, schema):
        """Set property value to default if not present.

        Args:
            validator (jsonschema.validator): Validator to extend.
            properties (list(dict)): List of properties.
            instance (dict): Validation instance.
            schema (jsonschema.schema): Schema to validate.
        """
        for prop in properties:
            subschema = properties[prop]
            if 'default' in subschema:
                instance.setdefault(prop, copy.deepcopy(subschema['default']))

        for error in validate_properties(validator, properties, instance,
                                         schema):
            yield error

    return validators.extend(validator_class, {'properties': set_defaults})


VALIDATOR = extend_with_default(Draft7Validator)
