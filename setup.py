
from setuptools import setup, find_packages

setup(
    name="sahfte",
    version="0.1.0",
    packages=find_packages(),
    package_data={"sahfte": ["*.so", "*.pyi", "py.typed"]}
)