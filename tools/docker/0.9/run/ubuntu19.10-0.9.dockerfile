#
FROM astibal/smithproxy:ubuntu19.10-0.9-base

LABEL org.smithproxy.docker.image="astibal/smithproxy:ubuntu19.10-0.9-run"

# Set the working directory to /app
WORKDIR /app


RUN git clone https://github.com/astibal/smithproxy.git -b master smithproxy && \
cd smithproxy && \
git clone https://github.com/astibal/socle.git -b master socle && \
git clone https://github.com/astibal/argparse.git -b develop ext/argparse && \
mkdir build && cd build && cmake .. -DCMAKE_BUILD_TYPE=Release && make install

# Define environment variable

# Run smithproxy when the container launches
CMD echo "Starting smithproxy .... " && ( /etc/init.d/smithproxy start ) > /dev/null 2>&1 && sleep 10 && \
    echo "SSL MITM CA cert (add to trusted CAs):" && cat /etc/smithproxy/certs/default/ca-cert.pem && smithproxy_cli && bash