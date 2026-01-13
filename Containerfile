
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
    ninja-build

#Using Pip get Pandas, Numpy, plotly and Jupyter
RUN pip install jupyter pandas numpy plotly

#Launch with bash as our shell
CMD ["/bin/bash"]