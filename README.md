mesos: Offer Filtering Allocator Module
===

[![Build Status](https://travis-ci.org/gettyimages/mesos_offer_filtering_allocator_module.svg?branch=master)](https://travis-ci.org/gettyimages/mesos_offer_filtering_allocator_module)

Motivation
---

Help operators to safely perform maintenance on portions of the Apache [Mesos](http://mesos.apache.org/documentation/latest/) cluster by:
- Providing a cluster state where resource offers are prevented for a given `agent`
- Providing manipulation of this state via an api endpoint `/allocator/filters`

Note that this module allows you to temporarily block offers for individual agents.
This is a (hopefully) temporary alternative to the [maintenance primitives](http://mesos.apache.org/documentation/latest/maintenance/)
mechanism in Mesos which only works when frameworks adopt it.

Use-case
- Isolate an agent from any new tasks, but let existing tasks on the agent keep running (to completion)


Installation
---

1. Install the module library (example: `libofferfilterallocator-1.0.1-vXXX.so`) on your masters
   - _location is your choice--you'll point to it in configuration_
   - pre-built binaries can be found on the [releases](../../releases) tab which consist of the module (`.so` file)
      and a `modules.json` file which assumes the module itself will be placed at `/usr/local/lib/`; modify
      as needed
2. Configure the `--modules` command-line arg, or `MESOS_MODULES` env variable to use the module
   - example: `--modules=file:///path-to-modules.json`
   - example: `MESOS_MODULES=file:///path-to-modules.json`
    ```
    {
      "libraries":
      [
        {
          "file": "/path/to/libofferfilterallocator-1.0.1-vXXX.so",
          "modules":
          [
            {
              "name": "im_getty_mesos_OfferFilteringAllocator",
              "parameters": [
                {
                  "key": "zk_url",
                  "value": "zk://127.0.0.1:2181/mesos-allocator-filters"
                }

            }
          ]
        }
      ]
    }
    ```
   - Alternatively, place the (updated to your environment) `modules.json` in the directory configured by `MESOS_MODULES_DIR` env varaible
     or `--modules-dir` command-line arg.
   - _Note: merge as needed to incoporate other modules_
3. Configure the `--allocator` command-line arg, or `MESOS_ALLOCATOR` env variable to use the custom allocator
   - example: `--allocator=im_getty_mesos_OfferFilteringAllocator`
   - example: `MESOS_ALLOCATOR=im_getty_mesos_OfferFilteringAllocator`
4. Start up `mesos` and view the documentation of the API at: `http(s)://your-mesos-master:5050/help/allocator/filters`

Usage
---

Offer filters are accessed via the exposed API endpoints (relative to mesos master(s))

---

> `GET /allocator/filters`

  List/Get the active allocator filters

  - example response when no filters are present:
    ```
    {"filters": []}
    ```

  - example response with filters present:
    ```
    {
      "filters": [
        {
          "agentId": "7532e174-d91a-49c4-85e6-389ea9fd73c3-S0",
          "hostname": "some-host.some-domain"
        }
      ]
    }
    ```

---

> `POST /allocator/filters`

> `Content-Type: application/json`

  - body:
    ```
    { "agentId": "VALUE"}
    ```
      or:
    ```
    { "hostname": "VALUE"}
    ```

  Add/Create an allocator filter for the specified agent

---

> `PUT /allocator/filters`

> `Content-Type: application/json`

  - body:
    ```
    {
      "filters": [
        {
          "agentId": "7532e174-d91a-49c4-85e6-389ea9fd73c3-S0",
          "hostname": "some-host.some-domain"
        },
        ...
      ]
    }
    ```

  Set the current state of all filters; either of `agentId` or `hostname` may be
  omitted on an individual filter; set `filters` to an empty array to clear all filters

---

> `DELETE /allocator/filters`

  - parameters:
     - `agentId` id of the agent for which to remove filters ",
     - `hostname` hostname of the agent for which to remove filters ",

    _(one of `agentId` or `hostname` is required)_

  Remove/Delete an allocator filter


All endpoints return:
  -  `200 OK` when the operation performed successfully,
    along with the current state of the allocator filters (see `GET /allocator/filters`).
  - `307 TEMPORARY_REDIRECT` redirect to the leading master when"
    current master is not the leader.
  - `503 SERVICE_UNAVAILABLE` if the leading master cannot be found.

---

Example (using the provided docker-compose cluster)
---

This example will demonstrate the application of how a filter interacts with existing tasks and new tasks using a simple cli application in Marathon.

0. Spin up the test cluster, and view the Marathon UI
    ```
    make run && while ! curl -s "http://$(make run ip):8080/" >/dev/null; do sleep 1; done && open "http://$(make run ip):8080/"
    ```

1. create a task in Marathon with 2 instances (this would normally result in one on each agent)
    ```
    curl -X POST "http://$(make run ip):8080/v2/apps" \
    -H 'Content-Type: application/json' -d '
    {
      "id": "/test1",
      "cmd": "while true; do echo \"hello\"; sleep 5; done",
      "cpus": 0.1,
      "mem": 32,
      "disk": 0,
      "instances": 2
    }
    '
    ```
    - confirm that there's one instance on each agent (i.e., offers created normally for all agents)

2. create a filter for one of the agents (`agent-one`) so that no new offers are sent
    ```
    curl -L -X POST "http://$(make run ip):5050/allocator/filters" \
    -H 'Content-Type: application/json' -d '
    { "hostname": "agent-one" }
    '
    ```

3. verify that tasks targeted at agent-one are not initiated
    ```
    curl -X POST "http://$(make run ip):8080/v2/apps" \
    -H 'Content-Type: application/json' -d '
    {
      "id": "/test2",
      "cmd": "while true; do echo \"hello\"; sleep 5; done",
      "cpus": 0.1,
      "mem": 32,
      "disk": 0,
      "instances": 1,
      "container": null,
      "constraints": [
        [
          "hostname",
          "LIKE",
          "agent-one"
        ]
      ]
    }
    '
    ```
    - view the Marathon UI and note that the task is in a `Waiting` status
    - note that the original task `/test1` still has an instance running on `agent-one`

4. remove the filter for agent-one
    ```
    curl -L -X DELETE "http://$(make run ip):5050/allocator/filters?hostname=agent-one"
    ```

5. confirm that the waiting tasks targeted for agent-one are now fulfilled/scheduled



---


Contributing
---

This project has been configured to build and test using docker, as it can be quite complicated to set up your C++ development environment.
It is assumed that you have a few bare essentials (`docker`, `docker-machine`), and uses docker to handle the rest.


#### Prerequisites:

  - `docker [1.9+]`
  - `docker-machine [0.8+]` (if running on OSX or Windows)
      - `docker-for-mac` and `docker-for-windows` are not easily configured with the `libprocess` internals
      - a machine named `mesos-modules` will be created (if not found); configure this name in `./docker-machine-env.sh`


#### Configuring:

The important configuration points are:
  - change module, library name and `VERSION` in the top section of `./CMakeLists.txt`
  - change the name of the docker machine at top of `docker/docker-machine-env.sh`
  - change the templated `#define` constants in `templates/config.h.in`

#### Building:
```
make build
```
To build for an alternate target version (than the default of 1.0.1), export the `MESOS_VERSION` env variable.
```
export MESOS_VERSION=0.28.2
```
Then all other commands will target that version.
_Note that specific docker-compose setups and build images have to be created to support a new version._

#### Testing:
```
make run
```
  - _runs a `docker-compose`-based mesos cluster on a single docker machine_
  - _view the live help docs: `open "http://$(make run ip):5050/help/allocator/filters"`_
  - _view the Marathon UI: `open "http://$(make run ip):8080/"`_

#### Developing:
```
make dev
```
  - builds the module from source
  - restarts the local cluster with new module
  - tails logs of the masters

----
