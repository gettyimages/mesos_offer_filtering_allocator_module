#!/bin/sh
export MESOS_VERSION=$(cat CMakeLists.txt | grep 'MESOS_VERSION' | head -1 | awk -F '[() ]' '{print $3}')

if ! docker build -t mesos-module-builder --file docker/Dockerfile.${MESOS_VERSION}.build .; then
  exit 1;
fi


if [ -z "$1" ]; then
  docker run --rm -it -v $(pwd):/src mesos-module-builder
else
  entrypoint=$1
  shift
  docker run --rm -it --entrypoint $entrypoint -v $(pwd):/src mesos-module-builder $*
fi