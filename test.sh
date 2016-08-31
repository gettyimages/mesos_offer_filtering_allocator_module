#!/bin/bash

action=$1
if [[ -z ${action} ]]; then
  action="start"
fi

if [[ "start" == "${action}" ]]; then
  source docker/docker-machine-env.sh
  export MESOS_ALLOCATOR_NAME=$(cat CMakeLists.txt | grep 'MODULE_NAME' | awk -F '[() ]' '{print $3}')

  docker-compose --file docker/docker-compose.test.yml up -d

elif [[ "stop" == "${action}" ]]; then
  source docker/docker-machine-env.sh
  export MESOS_ALLOCATOR_NAME=$(cat CMakeLists.txt | grep 'MODULE_NAME' | awk -F '[() ]' '{print $3}')

  docker-compose --file docker/docker-compose.test.yml kill
  docker-compose --file docker/docker-compose.test.yml rm -f

elif [[ "logs" == "${action}" ]]; then

  source docker/docker-machine-env.sh
  export MESOS_ALLOCATOR_NAME=$(cat CMakeLists.txt | grep 'MODULE_NAME' | awk -F '[() ]' '{print $3}')
  shift
  docker-compose --file docker/docker-compose.test.yml logs -f $*

else
  echo "Unknown action: ${action}; supported actions are 'start', 'stop', 'logs'"
  exit 1
fi