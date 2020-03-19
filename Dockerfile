FROM ubuntu:bionic-20200112 AS builder

# apt dependencies for build
RUN apt-get update && apt-get install -y build-essential git python3 python3-dev python3-pip zlib1g-dev wget libbz2-dev pkg-config libhdf5-dev liblzo2-dev libtokyocabinet-dev

# build cactus binaries
RUN mkdir -p /home/cactus
COPY . /home/cactus
RUN cd /home/cactus && make -j $(nproc)

# build cactus python3
RUN ln -s /usr/bin/python3 /usr/bin/python
RUN mkdir -p /wheels && cd /wheels && pip3 install -U pip && pip3 wheel /home/cactus

# Create a thinner final Docker image in which only the binaries and necessary data exist.
FROM ubuntu:bionic-20200112

# apt dependencies for runtime
RUN apt-get update && apt-get install -y --no-install-recommends git python3 python3-pip python3-distutils zlib1g libbz2-1.0 net-tools libhdf5-100 liblzo2-2 libtokyocabinet9

# copy cactus binaries from build image
COPY --from=builder /home/cactus/bin/* /usr/local/bin/

# copy temporary files for installing cactus
COPY --from=builder /home/cactus /tmp/cactus
COPY --from=builder /wheels /wheels

# install the python3 binaries then clean up
RUN pip3 install -U pip wheel setuptools && \
    pip3 install -f /wheels /tmp/cactus && \
	 pip3 install -f /wheels /tmp/cactus/submodules/sonLib && \
    rm -rf /wheels /root/.cache/pip/* /tmp/cactus && \
    apt-get remove -y git python3-pip && \
    apt-get auto-remove -y

# wrapper.sh is used when running using the docker image with --binariesMode local
RUN mkdir /opt/cactus/
COPY runtime/wrapper.sh /opt/cactus/
RUN chmod 777 /opt/cactus/wrapper.sh

# remember where we came from
ARG CACTUS_COMMIT

# UCSC convention is to work in /data
RUN mkdir /data
WORKDIR /data

