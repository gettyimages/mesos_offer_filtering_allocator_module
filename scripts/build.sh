#!/bin/sh
export MESOS_VERSION=$(cat CMakeLists.txt | grep 'MESOS_VERSION' | head -1 | awk -F '[() ]' '{print $3}')

entrypoint=""
args=""
release=""
if [ "$1" == "release" ]; then
  release=true
elif [ ! -z "$1" ]; then
  entrypoint=$1
  shift
  entrypoint="--entrypoint ${entrypoint}"
fi

docker run --rm -it $entrypoint -v $(pwd):/src mattdeboer/mesos-module-development:${MESOS_VERSION} $args

if [ ! -z "$release" ]; then
  source ./build/build-vars.sh
  cd ./build && tar -cvzf ${LIB_NAME}.tar.gz ./${LIB_FILE} ./modules.json
fi