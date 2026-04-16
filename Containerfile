
#Base on Ubuntu 24.04 LTS
FROM ubuntu:24.04

#Kill any prompts on apt
ENV DEBIAN_FRONTEND=noninteractive

#Let pip BREAK SYSTEM PACKAGES WOOOO SCARY
ENV PIP_BREAK_SYSTEM_PACKAGES=1

#Get our apt packages
RUN apt-get update && apt-get install -y \
    python3 \
    python3-pip \
    g++ \
    cmake \
    ninja-build \
    git \
    curl \
    build-essential python3-dev

#Using Pip get Pandas, Numpy, plotly and Jupyter, and pybind11
RUN pip install jupyter pandas numpy plotly scikit-build-core pybind11[global] pybind11-stubgen

#Fetch a copy of Nlohmann JSON for the CPP frontend
RUN mkdir -p /usr/local/include/nlohmann && \
    curl -o /usr/local/include/nlohmann/json.hpp https://raw.githubusercontent.com/nlohmann/json/refs/heads/develop/single_include/nlohmann/json.hpp

#Launch with bash as our shell
CMD ["/bin/bash"]