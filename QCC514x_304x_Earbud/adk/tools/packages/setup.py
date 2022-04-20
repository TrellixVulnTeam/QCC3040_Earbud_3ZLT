from setuptools import setup, find_packages

setup(
    name='adk_build_tools',
    version='0.0.1',
    python_requires='>=2.7',
    options = {'bdist_wheel': {'universal': True},},
    author='ADK team',
    author_email='DL-CAM-ADK-devkits@qti.qualcomm.com',
    description='ADK Build Tools',
    packages=find_packages(),
    package_data={'adk_build_tools' : [
        'buttonparsexml/ButtonParseXML.xsd',
        'gattdbifgen/dbgen_interface_generator.c',
        'gattdbifgen/dbgen_interface_generator.h',
        ]},
    entry_points = {
        'console_scripts': [
            'buttonparsexml = adk_build_tools.buttonparsexml.ButtonParseXML:main',
            'chaingen = adk_build_tools.chaingen.chaingen:main',
            'gattdbifgen = adk_build_tools.gattdbifgen.dbgen_interface_generator:main',
            'rulegen = adk_build_tools.rulegen.rulegen:main',
            'typegen = adk_build_tools.typegen.typegen:main',
            ]
        }
)
