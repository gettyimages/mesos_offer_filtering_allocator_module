#include <iostream>
#include <string>

#include <mesos/mesos.hpp>
#include <mesos/module.hpp>
#include <mesos/module/allocator.hpp>

#include <stout/json.hpp>
#include <stout/os.hpp>
#include <stout/protobuf.hpp>
#include <stout/strings.hpp>
#include <stout/hashmap.hpp>
#include <stout/option.hpp>

#include <process/once.hpp>

#include <master/master.hpp>
#include <stout/lambda.hpp>


#include "config.h"
#include "offer_filter_module.hpp"

using std::map;
using std::string;
using std::vector;
using std::pair;
using std::ostringstream;

namespace http = process::http;

using namespace mesos;
using namespace mesos::modules;

using process::HELP;
using process::TLDR;
using process::DESCRIPTION;
using process::Future;
using mesos::state::ZooKeeperStorage;
using mesos::internal::master::allocator::HierarchicalDRFAllocatorProcess;
using mesos::allocator::Allocator;
using mesos::state::Variable;
using mesos::internal::state::Entry;


// Need a custom dispatch to deal with ()const types
template <typename R, typename T>
process::Future<R> dispatch(const process::PID<T>& pid, R (T::*method)()const)
{
    std::shared_ptr<process::Promise<R>> promise(new process::Promise<R>());

    std::shared_ptr<std::function<void(process::ProcessBase*)>> f(
        new std::function<void(process::ProcessBase*)>(
            [=](process::ProcessBase* process) {
                assert(process != nullptr);
                T* t = dynamic_cast<T*>(process);
                assert(t != nullptr);
                promise->set((t->*method)());
            }));

    process::internal::dispatch(pid, f, &typeid(method));

    return promise->future();
}

namespace gettyimages {
namespace mesos {
namespace modules {

const string FILTERED_AGENTS = "filtered-agents";
const string CURRENT_MASTER = ":";
const string DEFAULT_ZK_URL = "zk://127.0.0.1:2181/mesos-allocator-filters";


Try<Nothing> OfferFilteringHierarchicalDRFAllocatorProcess::configure(const zookeeper::URL* zk_url)
{
    if (zkUrl == NULL) {
        zkUrl = new zookeeper::URL(*zk_url);
        state = new State(new ZooKeeperStorage(zkUrl->servers, Seconds(10), zkUrl->path));
        LOG(INFO) << "Created ZooKeeperStorage using zk_url " << zkUrl->servers << zkUrl->path;
        return Try<Nothing>(Nothing());
    } else {
        return Try<Nothing>(Error("Attempt to reinitialize ignored"));
    }
}

Future<http::Response> OfferFilteringHierarchicalDRFAllocatorProcess::getOfferFilters(
    const http::Request &request)
{
    return toHttpResponse(getFilteredAgents());
}

hashmap<string, string> OfferFilteringHierarchicalDRFAllocatorProcess::getFilteredAgents() {
    hashmap<string, string> filteredAgents;
    foreachpair (const SlaveID& agentId, const Slave& agent, slaves) {
        if (!agent.activated) {
            filteredAgents[stringify(agentId)] = agent.hostname;
        }
    }
    return filteredAgents;
}

Future<http::Response> OfferFilteringHierarchicalDRFAllocatorProcess::addOfferFilter(
    const http::Request &request)
{
    auto contentType = request.headers.get("Content-Type");
    if (contentType.isSome() && contentType.get() != "application/json") {
        return http::UnsupportedMediaType("expected: application/json");
    }

    Try<JSON::Object> body_ = JSON::parse<JSON::Object>(request.body);
    if (body_.isError()) {
        return http::BadRequest(body_.error());
    }

    auto body = body_.get();

    string hostnameParam;
    if (body.values.count("hostname") > 0) {
        hostnameParam = body.values["hostname"].as<JSON::String>().value;
    }
    string agentIdParam;
    if (body.values.count("agentId") > 0) {
        agentIdParam = body.values["agentId"].as<JSON::String>().value;
    }

    if (!hostnameParam.empty() || !agentIdParam.empty()) {

        const SlaveID* agentIdToDeactivate = nullptr;
        foreachpair (const SlaveID& agentId, const Slave& slave, slaves) {
            if ( (hostnameParam.empty() || hostnameParam == slave.hostname) &&
                    (agentIdParam.empty() || agentIdParam == stringify(agentId)) ) {

                agentIdToDeactivate = &agentId;
                break;
            }
        }

        if (agentIdToDeactivate == NULL) {
            string msg = "";
            if (!hostnameParam.empty()) {
                msg += " hostname==" + hostnameParam;
            }
            if (!agentIdParam.empty()) {
                if (!hostnameParam.empty()) {
                    msg += " AND ";
                }
                msg += " agentId==" + agentIdParam;
            }
            return http::BadRequest("No such agent matching" + msg);
        } else {
            LOG(INFO) << "Adding filter for agent " << agentIdToDeactivate;
            deactivateSlave(*agentIdToDeactivate);
            return persistAndReportOfferFilters();
        }
    } else {
        return http::BadRequest("body requires 'agentId' and/or 'hostname' attributes");
    }
}

Future<http::Response> OfferFilteringHierarchicalDRFAllocatorProcess::removeOfferFilter(
    const http::Request &request)
{
    auto agentIdParam = request.url.query.get("agentId");
    auto hostnameParam = request.url.query.get("hostname");

    if (agentIdParam.isSome() && hostnameParam.isSome()) {
        return http::BadRequest("Specify parameter 'agentId' or 'hostname', but not both");
    } else if (agentIdParam.isNone() && hostnameParam.isNone()) {
        return http::BadRequest("One of parameters 'agentId' or 'hostname' is required");
    }

    foreachpair (const SlaveID& agentId, const Slave& agent, slaves) {
        if ( (agentIdParam.isSome() && agentIdParam.get() == stringify(agentId)) ||
                (hostnameParam.isSome() && hostnameParam.get() == agent.hostname) ) {
            if (agent.activated) {
                return http::NotFound("No filter exists for agent " + stringify(agentId));
            } else {
                LOG(INFO) << "Activating agent: (" << stringify(agentId) << "," << agent.hostname << ")";
                activateSlave(agentId);
                return persistAndReportOfferFilters();
            }
            break;
        }
    }

    string msg;
    if (agentIdParam.isSome()) {
        msg = "agent " + agentIdParam.get();
    } else {
        msg = "hostname " + hostnameParam.get();
    }

    return http::NotFound("No filter exists for " + msg);
}

Future<http::Response> OfferFilteringHierarchicalDRFAllocatorProcess::toHttpResponse(
    const hashmap<string, string>& filteredAgents = hashmap<string, string>())
{
    JSON::Array filters;
    if (!slaves.empty()) {
        foreachpair (const string& agentId, const string& hostname, filteredAgents) {
            JSON::Object filter;
            filter.values["hostname"] = hostname;
            filter.values["agentId"] = agentId;
            filters.values.push_back(filter);
        }
    }

    JSON::Object body;
    body.values["filters"] = std::move(filters);
    return http::OK(body);
}

Future<http::Response> OfferFilteringHierarchicalDRFAllocatorProcess::persistAndReportOfferFilters()
{
    hashmap<string, string> filteredAgents = getFilteredAgents();
    persistFilteredAgents(filteredAgents);
    return toHttpResponse(filteredAgents);
}

Option<SlaveID> OfferFilteringHierarchicalDRFAllocatorProcess::findSlaveID(
        const string& agentId, const string& hostname) {

    foreachpair (const SlaveID& agentId_, const Slave& agent, slaves) {
        if ( (hostname.empty() || hostname == agent.hostname) &&
                (agentId.empty() || agentId == stringify(agentId_)) ) {
            return Some(agentId_);
        }
    }
    return None();
}

Future<http::Response> OfferFilteringHierarchicalDRFAllocatorProcess::updateOfferFilters(
    const http::Request &request)
{
    auto contentType = request.headers.get("Content-Type");
    if (contentType.isSome() && contentType.get() != "application/json") {
        return http::UnsupportedMediaType("expected: application/json");
    }

    Try<JSON::Object> body_ = JSON::parse<JSON::Object>(request.body);
    if (body_.isError()) {
        return http::BadRequest(body_.error());
    }

    auto body = body_.get();
    if (body.values.count("filters") == 1) {

    //     hashset<SlaveID> slaveIds;

    //     auto filters = body.values["filters"].as<JSON::Array>();
    //     for ( auto const &filter : filters) {

    //     }
    //     foreachpair (const SlaveID& agentId, const Slave& agent, slaves) {

    //     }
    }

    return http::OK(JSON::parse(
        "{"
            "\"result\": \"ok (method not implemented)\""
        "}").get()
    );
}

// Create a single string value composed of all existing filters
void OfferFilteringHierarchicalDRFAllocatorProcess::persistFilteredAgents(const hashmap<string, string>& filteredAgents) {

    ostringstream filteredAgentsStream;

    foreachpair (const string& agentId, const string& hostname, filteredAgents) {
        filteredAgentsStream << "," << agentId << ":" << hostname;
    }

    string serialized = filteredAgents.empty() ? "" : filteredAgentsStream.str().substr(1, string::npos);

    state->fetch(FILTERED_AGENTS)
        .onReady([this, serialized](const Option<Variable>& option) {
            state->store(option.get().mutate(serialized));
        });
}

// Read agent filters from the state store, and re-apply deactivations to
// the associated slaves; this may be called before all agents have re-registered,
// which is why we also call this method up addSlave calls.
void OfferFilteringHierarchicalDRFAllocatorProcess::restoreFilteredAgents() {

    if (state != NULL) {
        state->fetch(FILTERED_AGENTS)
            .onReady([this](const Option<Variable>& option) {
                string serialized = option.get().value();
                if (!serialized.empty()) {
                    for (const string& agent: strings::split(serialized, ",")) {
                        vector<string> parts = strings::split(agent, ":");
                        Option<SlaveID> opt = findSlaveID(parts[0], parts[1]);
                        if (opt.isSome()) {
                            deactivateSlave(opt.get());
                            LOG(INFO) << "Restored filter for agent: (" << stringify(opt.get()) << "," << slaves[opt.get()].hostname << ")";
                        } else {
                            LOG(WARNING) << "Failed to locate filtered agent: (" << parts[0] << ", " << parts[1] << ")";
                        }
                    }
                }
            });
    } else {
        LOG(WARNING) << "State not initialized";
    }
}


Future<http::Response> OfferFilteringHierarchicalDRFAllocatorProcess::offerFilters(
    const http::Request &request)
{
    bool isLeader;
    Option<string> leader = getLeader(request);
    if (leader.isNone()) {
        // No leader; punt!
        return http::ServiceUnavailable("No leader elected");
    } else if (leader.get() != CURRENT_MASTER) {
        // Redirect the request to the current leader
        return http::TemporaryRedirect("//" + leader.get() + stringify(request.url));
    }

    if (request.method == "GET") {
        return getOfferFilters(request);
    } else if (request.method == "POST") {
        return addOfferFilter(request);
    } else if (request.method == "PUT") {
        return updateOfferFilters(request);
    } else if (request.method == "DELETE") {
        return removeOfferFilter(request);
    } else {
        return http::MethodNotAllowed({"GET","POST","PUT","DELETE"}, request.method);
    }
}

void OfferFilteringHierarchicalDRFAllocatorProcess::recover(
      const int _expectedAgentCount,
      const hashmap<std::string, Quota>& quotas)
{

    HierarchicalDRFAllocatorProcess::recover(_expectedAgentCount, quotas);
    restoreFilteredAgents();
}

// Gets {hostname}:{port} of the current leader; returns ":" when the current master is leader;
// returns None when no leader has been elected.
Option<string> OfferFilteringHierarchicalDRFAllocatorProcess::getLeader(const http::Request &request)
{
    process::PID<> pid;
    pid.id = "master";
    pid.address = process::address();

    Option<http::Headers> headers;
    if (request.headers.get("Authorization").isSome()) {
        headers = Some(http::Headers{{"Authorization", request.headers.get("Authorization").get() }});
    }

    auto response_ = http::get(pid, "state", None(), headers).get();
    if (response_.code == http::Status::TEMPORARY_REDIRECT) {
        // Follow redirect to current leader
        auto location = response_.headers["Location"];
        return Some(strings::split(location, "/")[2]);
    } else if (response_.code == http::Status::OK) {
        auto masterState_ = JSON::parse<JSON::Object>(response_.body);
        if (!masterState_.isError()) {
            auto pid = masterState_.get().values["pid"].as<JSON::String>().value;
            if (masterState_.get().values.count("leader") == 1) {
                auto leader = masterState_.get().values["leader"].as<JSON::String>().value;
                if (leader.empty()) {
                    return None();
                } else if (pid == leader) {
                    return Some(CURRENT_MASTER);
                } else {
                    vector<string> parts = strings::split(leader, "@");
                    return Some(parts[1]);
                }
            } else {
                return None();
            }
        } else {
            LOG(WARNING) << "Failed to recover master state: " << masterState_.error();
            return None();
        }
    } else {
        LOG(WARNING) << "Failed to recover master state: " << response_.status;
        return None();
    }
}

void OfferFilteringHierarchicalDRFAllocatorProcess::addSlave(
      const SlaveID& slaveId,
      const SlaveInfo& slaveInfo,
      const Option<Unavailability>& unavailability,
      const Resources& total,
      const hashmap<FrameworkID, Resources>& used)
{
    HierarchicalDRFAllocatorProcess::addSlave(slaveId, slaveInfo, unavailability, total, used);
    restoreFilteredAgents();
}

void OfferFilteringHierarchicalDRFAllocatorProcess::activateSlave(const SlaveID& slaveId)
{
    // TODO(mdeboer): need to compare this against existing filters and suppress
    // the call as necessary
    LOG(INFO) << "----> activateSlave called " << slaveId;
    HierarchicalDRFAllocatorProcess::activateSlave(slaveId);
}


} // namespace modules
} // namespace mesos
} // namespace gettyimages

namespace {

using gettyimages::mesos::modules::OfferFilteringHierarchicalDRFAllocatorProcess;
using gettyimages::mesos::modules::OfferFilteringHierarchicalDRFAllocator;
using gettyimages::mesos::modules::DEFAULT_ZK_URL;

// Called by the main() of master at startup
Allocator* create(const Parameters& parameters)
{
    string zk_url = DEFAULT_ZK_URL;
    for (int i = 0; i < parameters.parameter_size(); ++i) {
        Parameter parameter = parameters.parameter(i);
        if (parameter.key() == "zk_url") {
            zk_url = parameter.value();
            break;
        }
    }

    Try<zookeeper::URL> url = zookeeper::URL::parse(zk_url);
    if (url.isError()) {
        LOG(ERROR)
            << "Failed to parse 'zk_url' parameter: '" << zk_url << "'; "
            << "multi-master failover state will be unavailable for offer filters";
    }

    Try<Allocator*> allocator = OfferFilteringHierarchicalDRFAllocator::create();
    if (allocator.isError()) {
        return nullptr;
    }

    process::PID<OfferFilteringHierarchicalDRFAllocatorProcess> pid;
    pid.id = gettyimages::mesos::modules::ALLOCATOR_PROCESS_ID;
    pid.address = process::address();

    Try<Nothing> configured = process::dispatch(pid, &OfferFilteringHierarchicalDRFAllocatorProcess::configure,
        const_cast<zookeeper::URL*>(&(url.get()))).get();
    if (configured.isError()) {
         LOG(ERROR) << "Failed to configure " MODULE_NAME_STRING ": " << configured.error();
        return nullptr;
    }
    LOG(INFO) << "Created new " MODULE_NAME_STRING " instance";
    return allocator.get();
}

} // namespace ::

// Our module definition
Module<Allocator> MODULE_NAME(
    MESOS_MODULE_API_VERSION,
    MESOS_VERSION,
    "Matt DeBoer",
    "matt.deboer@gmail.com",
    "Offer Filtering Allocator Module"
    "See: https://github.com/gettyimages/mesos_offer_filtering_allocator_module",
    [] () {return true;},
    create);