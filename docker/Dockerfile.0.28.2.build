FROM ubuntu:14.04
MAINTAINER Matt DeBoer <matt.deboer@gmail.com>

# Install Dependencies
ENV MESOS_RELEASE_VERSION=0.28.2
ENV PROTOBUF_VERSION=2.5.0

RUN apt-get update -q --fix-missing
RUN apt-get -qy install software-properties-common
RUN apt-get update -qy
# RUN apt-cache policy cmake
RUN apt-get -qy install                   \
  linux-libc-dev                          \
  build-essential                         \
  autoconf                                \
  automake                                \
  ca-certificates                         \
  gdb                                     \
  wget                                    \
  git-core                                \
  libcurl4-nss-dev                        \
  libsasl2-dev                            \
  libtool                                 \
  libsvn-dev                              \
  libapr1-dev                             \
  libgoogle-glog-dev                      \
  libboost-dev                            \
  make                                    \
  python                                  \
  python2.7                               \
  libpython-dev                           \
  python-dev                              \
  python-setuptools                       \
  heimdal-clients                         \
  libsasl2-modules-gssapi-heimdal         \
  unzip                                   \
  --no-install-recommends

#
#  libprotobuf-dev                         \

# Install protobuf ${PROTOBUF_VERSION}
RUN wget https://github.com/google/protobuf/releases/download/v${PROTOBUF_VERSION}/protobuf-${PROTOBUF_VERSION}.tar.gz \
 && tar -xvzf protobuf-${PROTOBUF_VERSION}.tar.gz \
 && cd protobuf-${PROTOBUF_VERSION}/ \
 && ./configure CXXFLAGS=-std=c++11 --prefix=/usr \
 && make && make install

# Install the picojson headers
RUN wget https://raw.githubusercontent.com/kazuho/picojson/v1.3.0/picojson.h -O /usr/local/include/picojson.h

# Install CMake
RUN wget http://www.cmake.org/files/v3.6/cmake-3.6.1.tar.gz \
 && tar -xvzf cmake-3.6.1.tar.gz \
 && cd cmake-3.6.1/ \
 && ./configure \
 && make && make install \
 && update-alternatives --install /usr/bin/cmake cmake /usr/local/bin/cmake 1 --force \
 && cmake --version

# Prepare to build Mesos
RUN mkdir -p /mesos
RUN mkdir -p /tmp
RUN mkdir -p /usr/share/java/
RUN wget http://search.maven.org/remotecontent?filepath=com/google/protobuf/protobuf-java/2.5.0/protobuf-java-2.5.0.jar -O protobuf.jar
RUN mv protobuf.jar /usr/share/java/

WORKDIR /mesos

# Clone Mesos (master branch)
RUN git clone git://git.apache.org/mesos.git /mesos
RUN git checkout tags/${MESOS_RELEASE_VERSION}
# RUN git checkout master
RUN git log -n 1

# Bootstrap
RUN ./bootstrap

# Configure
RUN mkdir build && cd build && ../configure --disable-java --disable-python --disable-optimize --without-included-zookeeper --with-glog=/usr/local --with-protobuf=/usr --with-boost=/usr/local

# Build Mesos
RUN cd build && make -j 2 install

# Install GTest
RUN apt-get install -qy libgtest-dev --no-install-recommends \
 && cd /usr/src/gtest \
 && cmake CMakeLists.txt \
 && make \
 && cp *.a /usr/local/lib/

# Create build launch script
RUN printf '#!/bin/sh\n\
rm -rf build && \\\n\
mkdir -p build && \\\n\
cd build && \\\n\
cmake -DMESOS_VERSION:STRING=${MESOS_RELEASE_VERSION} .. && \\\n\
make\n'\
  >> /bin/build \
 && chmod +x /bin/build

#RUN ln -s /usr/lib/x86_64-linux-gnu/libprotobuf.so /usr/local/lib/libprotobuf.so
#RUN apt-get install -y libzookeeper-mt-dev

VOLUME /src
WORKDIR /src

CMD ["/bin/build"]