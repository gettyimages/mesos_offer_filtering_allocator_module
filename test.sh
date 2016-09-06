#!/bin/bash

action=$1
if [[ -z ${action} ]]; then
  action="start"
fi

source docker/docker-env.sh
export MESOS_ALLOCATOR_NAME=$(cat CMakeLists.txt | grep 'MODULE_NAME' | head -1 | awk -F '[() ]' '{print $3}')
export MESOS_VERSION=$(cat CMakeLists.txt | grep 'MESOS_VERSION' | head -1 | awk -F '[() ]' '{print $3}')
export DOCKER_COMPOSE="docker-compose --file docker/docker-compose.${MESOS_VERSION}.test.yml"

if [[ "start" == "${action}" ]]; then

  ${DOCKER_COMPOSE} up -d

elif [[ "stop" == "${action}" ]]; then

  shift
  ${DOCKER_COMPOSE} kill $*

elif [[ "rm" == "${action}" ]]; then

  shift
  ${DOCKER_COMPOSE} kill $*
  ${DOCKER_COMPOSE} rm -f $*

elif [[ "logs" == "${action}" ]]; then

  shift
  ${DOCKER_COMPOSE} logs -f $*

else

  echo "Unknown action: ${action}; supported actions are 'start', 'stop', 'logs'"
  exit 1
fi