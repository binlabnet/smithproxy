# 
FROM ubuntu:18.10

# Set the working directory to /app
WORKDIR /app

RUN apt update && apt install -y \
wget curl \
python-pip \
libcli1.9 libconfig++9v5 libssl1.0 libunwind8 libconfig++ && \
apt install -y iptables python-ldap python-pyparsing python-posix-ipc python-soappy python-m2crypto telnet iproute2 \
libconfig-dev libcli-dev libunwind-dev libssl1.0-dev \
debootstrap devscripts build-essential lintian debhelper dh-virtualenv vim nano \
git g++ cmake make && pip install pylibconfig2

# download master, containing fresh build scripts - build scripts are always maintained in master
RUN \
    git clone https://github.com/astibal/smithproxy.git smithproxy

CMD cd /app/smithproxy/tools/build-scripts && cat README.txt && echo && /bin/bash
