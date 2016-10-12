#!/bin/bash
DEFAULT_MESOS_VERSION=$(cat CMakeLists.txt | grep 'MESOS_VERSION' | head -1 | awk -F '[() ]' '{print $3}')
export MESOS_VERSION=${MESOS_VERSION:-$DEFAULT_MESOS_VERSION}
export DOCKER_USER="$(id -u $USER):$(id -g $USER)"

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

if ! docker inspect -f '{{.Id}}' "mattdeboer/mesos-module-development:${MESOS_VERSION}" >/dev/null 2>/dev/null; then
  if ! docker pull "mattdeboer/mesos-module-development:${MESOS_VERSION}"; then
    docker build -t "mattdeboer/mesos-module-development:${MESOS_VERSION}" --file docker/Dockerfile.${MESOS_VERSION}.build docker;
  fi
fi

if ! docker run --rm -it ${entrypoint} -v $(pwd):/src mattdeboer/mesos-module-development:${MESOS_VERSION} ${extra_args}; then

  exit 1

fi

if [ ! -z "${release}" ]; then

  source ./build/build-vars.sh
  echo "Creating ${LIB_NAME}.tar.gz"
  cd ./build && tar -cvzf ${LIB_NAME}.tar.gz ./${LIB_FILE} ./modules.json && cd ..
  mkdir ./dist && mv ./build/${LIB_NAME}.tar.gz ./dist/

fi
