# Change Log
All notable changes to this project will be documented in this file.


## [v1-RC1] - 2016-09-09

### Changed
- Remove automatic lookup of zk location from master, as this requires a possibly-authenticated http request.
  Instead, use a module parameter to configure zk_url parameter
- When determining the leader, reuse the `Authorization` header passed in from the client
- Tested against authenticated DC/OS (1.8)

## [v1-RC0] - 2016-09-07

### Added
- Multi-master failover support via zookeeper state persistence.