# Use an official Python runtime as a parent image
FROM ubuntu:18.04

# Set the working directory to /app
WORKDIR /app

RUN apt update && apt install -y \
wget \
python-pip dnsutils

RUN apt install -y wget dnsutils; \
\
SX1=`dig +short latest.ubuntu1804.deb.smithproxy.org TXT | tr -d '"'` ; echo "marked for download: $SX1"; \
SX2=`dig +short latest-pylibconfig2.ubuntu1804.deb.smithproxy.org TXT | tr -d '"'`; echo "marked for download: $SX2"; \
\
apt install -y libcli1.9 libconfig++9v5 libssl1.0.0 libunwind8 python-pip && \
apt install -y iptables python-ldap python-pyparsing python-posix-ipc python-soappy python-m2crypto telnet iproute2 && \
apt install -y iptables python-ldap python-pyparsing python-posix-ipc python-soappy python-m2crypto telnet iproute2 && \
apt install -y python3 python3-pip python3-cryptography python3-pyroute2 && \
wget $SX2 && dpkg -i `basename $SX2` && \
wget $SX1 && dpkg -i `basename $SX1` && \
apt remove -y g++ gcc perl manpages && apt -y autoremove



# Define environment variable

# Run smithproxy when the container launches
CMD echo "Starting smithproxy .... " && \
    ( /etc/init.d/smithproxy start ) > /dev/null 2>&1 && \
    sleep 10 && \
    echo "SSL MITM CA cert (add to trusted CA's):" && \
    cat /etc/smithproxy/certs/default/ca-cert.pem && smithproxy_cli && \
    bash
