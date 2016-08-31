#include <iostream>

#include <mesos/mesos.hpp>
#include <mesos/module.hpp>

#include <mesos/module/anonymous.hpp>
#include <mesos/allocator/allocator.hpp>

#include <master/allocator/mesos/allocator.hpp>
#include <master/allocator/mesos/hierarchical.hpp>

#include <stout/duration.hpp>
#include <stout/json.hpp>
#include <stout/os.hpp>
#include <stout/protobuf.hpp>
#include <process/id.hpp>
#include <process/help.hpp>

#include "config.h"

using std::string;
using process::Future;
using process::Process;
using process::ProcessBase;
using mesos::SlaveID;
using mesos::FrameworkID;
using mesos::Resources;
using mesos::internal::master::allocator::internal::HierarchicalAllocatorProcess;
using mesos::internal::master::allocator::DRFSorter;
using process::HELP;
using process::TLDR;
using process::DESCRIPTION;
using process::AUTHENTICATION;


namespace http = process::http;
namespace internal = mesos::internal::master::allocator;

template <
    typename RoleSorter,
    typename FrameworkSorter,
    typename QuotaRoleSorter>
class OfferFilteringHierarchicalAllocatorProcess;

typedef OfferFilteringHierarchicalAllocatorProcess<DRFSorter, DRFSorter, DRFSorter>
OfferFilteringHierarchicalDRFAllocatorProcess;

typedef internal::MesosAllocator<OfferFilteringHierarchicalDRFAllocatorProcess>
OfferFilteringHierarchicalDRFAllocator;


template <
    typename RoleSorter,
    typename FrameworkSorter,
    typename QuotaRoleSorter>
class OfferFilteringHierarchicalAllocatorProcess : public HierarchicalAllocatorProcess
{
public:
  OfferFilteringHierarchicalAllocatorProcess()
    : ProcessBase("allocator"),
      internal::internal::HierarchicalAllocatorProcess(
          [this]() -> internal::Sorter* {
            return new RoleSorter(this->self(), "allocator/mesos/roles/");
          },
          []() -> internal::Sorter* { return new FrameworkSorter(); },
          []() -> internal::Sorter* { return new QuotaRoleSorter(); })
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
              &OfferFilteringHierarchicalAllocatorProcess::offerFilters);
          }

  virtual ~OfferFilteringHierarchicalAllocatorProcess() {}

  Future<http::Response> offerFilters(const http::Request &request);

protected:

  Future<http::Response> addOfferFilter(const http::Request &request);

  Future<http::Response> getOfferFilters(const http::Request &request);

  Future<http::Response> updateOfferFilters(const http::Request &request);

  Future<http::Response> removeOfferFilter(const http::Request &request);

private:
  Option<SlaveID> findSlaveID(const string& hostname, const string& agentId);
};


