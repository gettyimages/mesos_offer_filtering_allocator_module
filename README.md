mesos: Offer Filtering Allocator Module
===

Motivation
---

Help operators to safely perform maintenance on portions of the Apache [Mesos](http://mesos.apache.org/documentation/latest/) cluster by:
- Providing a cluster state where resource offers are prevented for a given `agent`
- Providing manipulation of this state via an api endpoint `/offer-filters`

Note that this module allows you to temporarily block offers for individual agents.
This is a (hopefully) temporary alternative to the [maintenance primitives](http://mesos.apache.org/documentation/latest/maintenance/)
mechanism in Mesos which only works when frameworks adopt it.

Installation
---

1. Install the module library (example: `libofferfilterallocator-1.0.1-c52cdba.so`) on your masters
   - _location is your choice--you'll point to it in configuration_
2. Configure the `--modules` command-line arg, or `MESOS_MODULES` env variable to use the module
   - example: `--modules=file:///path-to-modules.json`
   - example: `MESOS_MODULES=file:///path-to-modules.json`
    ```
    {
      "libraries":
      [
        {
          "file": "/path/to/libofferfilterallocator-1.0.1-c52cdba.so",
          "modules":
          [
            {
              "name": "im_getty_mesos_OfferFilteringAllocator"
            }
          ]
        }
      ]
    }
    ```
   - _Note: merge as needed to incoporate other modules_
3. Configure the `--allocator` command-line arg, or `MESOS_ALLOCATOR` env variable to use the custom allocator
   - example: `--allocator=im_getty_mesos_OfferFilteringAllocator`
   - example: `MESOS_ALLOCATOR=im_getty_mesos_OfferFilteringAllocator`
4. Start up `mesos` and view the documentation of the API at: `http(s)://your-mesos-master:5050/help/allocator/filters`

Usage
---

  Offer filters are accessed via the exposed API endpoints (relative to mesos master(s))

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

  > `POST /allocator/filters`
  - body:
    ```
    { "agentId": "VALUE"}
    ```
      or:
    ```
    { "hostname": "VALUE"}
    ```

  Add/Create an allocator filter for the specified agent:

  > `DELETE /allocator/filters`
  - parameters:
     - `agentId` id of the agent for which to remove filters ",
     - `hostname` hostname of the agent for which to remove filters ",

    _(one of `agentId` or `hostname` is required)_

    Remove/Delete an allocator filter

  All endpoints return `200 OK` when the operation performed successfully,
  along with the current state of the allocator filters (see `GET /allocator/filters`).

  Returns `307 TEMPORARY_REDIRECT` redirect to the leading master when"
  current master is not the leader.

  Returns `503 SERVICE_UNAVAILABLE` if the leading master cannot be found.

---


Contributing
---

  #### Prerequisites

   - `docker-machine` [0.8+] (if running on OSX or Windows)
       - `docker-for-mac` and `docker-for-windows` are not easily configured with the `libprocess` internals
   - `docker` [1.9+]
       - a machine named `mesos-modules` will be created (if not found); configure this name in `./docker-machine-env.sh`

   #### Configuring
   Edit the top section of `./CMakeLists.txt`
   #### Building:
   TL;DR: run this -->
   ```
   ./build.sh
   ```
   #### Testing:
   TL;DR: run this -->
   ```
   ./test.sh
   ```
   Which will run a `docker-compose`-based mesos cluster on a single docker machine.
   Use `docker-machine ip mesos-modules` to find the ip of this machine.
   View the help docs: `open "http://$(docker-machine ip mesos-modules):5050/help/allocator/filters"`


----
