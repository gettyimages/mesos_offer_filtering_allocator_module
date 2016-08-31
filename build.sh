#!/bin/sh

docker build -t mesos-module-builder --file docker/Dockerfile.build .

docker run --rm -it -v $(pwd):/src mesos-module-builder