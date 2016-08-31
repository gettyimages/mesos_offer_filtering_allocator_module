#!/bin/bash
#
#
#
MACHINE_NAME='mesos-modules'
MACHINE_DRIVER=${MACHINE_DRIVER:-virtualbox}
if [[ -z $(docker-machine ls -q | grep "${MACHINE_NAME}") ]]; then
  echo "Looks like you don't have a machine named ${MACHINE_NAME}; creating..."
  docker-machine create -d ${MACHINE_DRIVER} ${MACHINE_NAME}
fi

if [[ -z $(docker-machine ls | grep "mesos" | grep "Running") ]]; then
  echo "Restarting machine ${MACHINE_NAME}..."
  docker-machine restart ${MACHINE_NAME}
fi

eval $(docker-machine env ${MACHINE_NAME})
export DOCKER_IP=$(docker-machine ip ${MACHINE_NAME})

