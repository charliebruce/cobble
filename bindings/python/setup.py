from setuptools import setup, find_packages
import platform

platform_install_requires = []

# To use a consistent encoding
from codecs import open
from os import path

here = path.abspath(path.dirname(__file__))

# Get the long description from the README file
with open(path.join(here, 'README.md'), encoding='utf-8') as f:
    long_description = f.read()

setup(name              = 'cobble',
      version           = '0.0.1',
      author            = 'Charlie Bruce',
      author_email      = 'charliebruce@gmail.com',
      description       = 'Python library for interacting with Bluetooth LE devices in a cross-platform way',
      long_description  = long_description,
      license           = 'MIT',
      url               = 'https://github.com/charliebruce/cobble/',
      install_requires  = ['future'] + platform_install_requires,
      packages          = find_packages(),
)