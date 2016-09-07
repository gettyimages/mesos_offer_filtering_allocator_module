#!/bin/sh
export MESOS_VERSION=$(cat CMakeLists.txt | grep 'MESOS_VERSION' | head -1 | awk -F '[() ]' '{print $3}')

entrypoint=""
extra_args=""
release=""

if [ "$1" == "release" ]; then
  release="true"
elif [ ! -z "$1" ]; then
  entrypoint="--entrypoint $1"
  shift
  extra_args=$*
fi

echo "Running:: docker run --rm -it $entrypoint -v $(pwd):/src mattdeboer/mesos-module-development:${MESOS_VERSION} ${extra_args}"
if ! docker run --rm -it $entrypoint -v $(pwd):/src mattdeboer/mesos-module-development:${MESOS_VERSION} ${extra_args}; then
  exit 1
fi

if [ ! -z "${release}" ]; then
  source ./build/build-vars.sh
  cd ./build && tar -cvzf ${LIB_NAME}.tar.gz ./${LIB_FILE} ./modules.json
fi