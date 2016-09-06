#include <iostream>

#include <mesos/mesos.hpp>
#include <mesos/module.hpp>
#include <mesos/allocator/allocator.hpp>
#include <mesos/zookeeper/zookeeper.hpp>
#include <mesos/zookeeper/contender.hpp>

#include <mesos/state/state.hpp>
#include <mesos/state/zookeeper.hpp>

#include <master/allocator/mesos/allocator.hpp>
#include <master/allocator/mesos/hierarchical.hpp>

#include <stout/duration.hpp>
#include <stout/json.hpp>
#include <stout/os.hpp>

#include <stout/protobuf.hpp>

#include <process/help.hpp>
#include <process/owned.hpp>

#include "config.h"

using std::string;

using process::Future;
using process::Process;
using process::ProcessBase;
using process::Owned;
using process::HELP;
using process::TLDR;
using process::DESCRIPTION;
using process::AUTHENTICATION;
using process::UPID;
using process::PID;

using mesos::SlaveID;
using mesos::SlaveInfo;
using mesos::Unavailability;
using mesos::FrameworkID;
using mesos::Resources;
using mesos::internal::master::allocator::HierarchicalDRFAllocatorProcess;
using mesos::internal::master::allocator::DRFSorter;

using mesos::state::State;
using mesos::state::Storage;
using mesos::internal::state::Entry;

namespace http = process::http;
namespace internal = mesos::internal::master::allocator;


namespace gettyimages {
namespace mesos {
namespace modules {

class OfferFilteringHierarchicalDRFAllocatorProcess;

typedef internal::MesosAllocator<OfferFilteringHierarchicalDRFAllocatorProcess>
OfferFilteringHierarchicalDRFAllocator;


class OfferFilteringHierarchicalDRFAllocatorProcess : public HierarchicalDRFAllocatorProcess
{
public:
  OfferFilteringHierarchicalDRFAllocatorProcess()
  : ProcessBase("allocator")
  {
    route("/filters",
      HELP(
          TLDR("Manipulate resource filters for agents; filtered agents receive/send no offers"),
          DESCRIPTION(
              "---",
              "#### LIST/GET the active allocator filters: ",
              ">       GET /allocator/filters ",
              "",
              " *example response when no filters are present:*",
              ">       { \"filters\": []}",
              "",
              " *example response with filters present:*",
              ">       {",
              ">         \"filters\": [",
              ">           {",
              ">             \"agentId\": \"7532e174-d91a-49c4-85e6-389ea9fd73c3-S0\",",
              ">             \"hostname\": \"some-host.some-domain\"",
              ">           }",
              ">         ]",
              ">       }",
              "",
              "---",
              "",
              "#### ADD/CREATE an allocator filter for the specified agent: ",
              ">       POST /allocator/filters ",
              ">              body:  { \"agentId\": \"VALUE\"}",
              ">                or   { \"hostname\": \"VALUE\"}",
              "",
              "---",
              "",
              "#### REMOVE/DELETE an allocator filter: ",
              ">       DELETE /allocator/filters ",
              ">              parameters:",
              ">                agentId=VALUE     The id of the agent for which to remove filters ",
              ">                hostname=VALUE    The hostname of the agent for which to remove filters ",
              ">              (one of agentId or hostname is required) ",
              "",
              "---",
              "",
              "",
              "All endpoints return `200 OK` when the operation performed successfully, "
              "along with the current state of the allocator filters.",
              " ",
              "Returns `307 TEMPORARY_REDIRECT` redirect to the leading master when",
              "current master is not the leader.",
              " ",
              "Returns `503 SERVICE_UNAVAILABLE` if the leading master cannot be found.",
              " ",
              "An agent for which an allocator filter exists will receive no offers until that",
              "filter is removed.",
              " ",
              "---",
              "provided by: " PROJECT_NAME_STRING " (v: " VERSION_STRING " )"
              ),
          AUTHENTICATION(true),
          ""
      ),
      &OfferFilteringHierarchicalDRFAllocatorProcess::offerFilters);

  }

  virtual ~OfferFilteringHierarchicalDRFAllocatorProcess() {}

  Future<http::Response> offerFilters(const http::Request &request);

  virtual void recover(
      const int _expectedAgentCount,
      const hashmap<std::string, Quota>& quotas);

  virtual void addSlave(
      const SlaveID& slaveId,
      const SlaveInfo& slaveInfo,
      const Option<Unavailability>& unavailability,
      const Resources& total,
      const hashmap<FrameworkID, Resources>& used);

  virtual void activateSlave(
      const SlaveID& slaveId);

protected:

  Future<http::Response> addOfferFilter(const http::Request &request);

  Future<http::Response> getOfferFilters(const http::Request &request);

  Future<http::Response> updateOfferFilters(const http::Request &request);

  Future<http::Response> removeOfferFilter(const http::Request &request);

private:
  Future<http::Response> reportOfferFilters(const hashmap<string, string>& filteredAgents);

  Future<http::Response> persistAndReportOfferFilters();

  hashmap<string, string> getFilteredAgents();

  Option<SlaveID> findSlaveID(const string& hostname, const string& agentId);

  void persistFilteredAgents(const hashmap<string, string>& filteredAgents);

  void restoreFilteredAgents();

  Option<string> getLeader(const http::Request &request);

  State* state;
};

} // namespace modules
} // namespace mesos
} // namespace gettyimages



