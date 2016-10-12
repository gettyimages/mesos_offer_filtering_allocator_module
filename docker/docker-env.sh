#!/bin/bash
#
# Determine if we're not running on linux
# and take appropriate steps to configure docker-machine
#
if [[ "$(uname -a | awk '{print $1}')" != "Linux" ]]; then
  MACHINE_NAME='mesos-modules'
  MACHINE_DRIVER=${MACHINE_DRIVER:-virtualbox}
  if [[ -z $(docker-machine ls -q | grep "${MACHINE_NAME}") ]]; then
    echo "Looks like you don't have a machine named ${MACHINE_NAME}; creating..."
    docker-machine create -d ${MACHINE_DRIVER} ${MACHINE_NAME}
  fi

  if [[ -z $(docker-machine ls | grep "mesos" | grep "Running" | grep -v "Unknown") ]]; then
    echo "Restarting machine ${MACHINE_NAME}..."
    if ! docker-machine restart ${MACHINE_NAME}; then
      echo "Failed to restart the machine ${MACHINE_NAME}; recreate it instead..."
      docker-machine rm -f ${MACHINE_NAME} && docker-machine create -d ${MACHINE_DRIVER} ${MACHINE_NAME}
    fi
  fi

  if ! eval $(docker-machine env ${MACHINE_NAME}); then
    echo "Failed to setup docker-machine environment for ${MACHINE_NAME}; punt!"
    exit 1
  fi

  docker-machine ssh ${MACHINE_NAME} "sudo ntpclient -s -h pool.ntp.org >/dev/null"
  if ! export DOCKER_IP=$(docker-machine ip ${MACHINE_NAME}); then
    echo "Failed to get docker-machine ip for ${MACHINE_NAME}; punt!"
    exit 1
  fi
else
  export DOCKER_IP=0.0.0.0
  export DOCKER_USER="$(id -u $USER):$(id -g $USER)"
fi


