from setuptools import setup, find_packages

setup(name = 'jiminy_py',
      version = '0.8',
      description = 'Package containing python-native helper methods for Jiminy Open Source.',
      author = 'Alexis Duburcq',
      packages=find_packages('src'),
      package_dir={'': 'src'},
      zip_safe = False)
