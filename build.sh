#!/bin/sh
export MESOS_VERSION=$(cat CMakeLists.txt | grep 'MESOS_VERSION' | head -1 | awk -F '[() ]' '{print $3}')

docker build -t mesos-module-builder --file docker/Dockerfile.${MESOS_VERSION}.build .

docker run --rm -it -v $(pwd):/src mesos-module-builder