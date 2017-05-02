#include <iostream>
#include <string>
#include <utility>

#include <mesos/mesos.hpp>
#include <mesos/module.hpp>

#include <stout/json.hpp>
#include <stout/os.hpp>
#include <stout/protobuf.hpp>
#include <stout/strings.hpp>
#include <stout/hashmap.hpp>
#include <stout/option.hpp>
#include <stout/error.hpp>
#include <stout/try.hpp>
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

#ifdef MESOS__0_28_2

using mesos::internal::state::ZooKeeperStorage;
using mesos::internal::state::Variable;
using mesos::master::allocator::Allocator;

#else

using mesos::state::ZooKeeperStorage;
using mesos::state::Variable;
using mesos::allocator::Allocator;

#endif

using process::Future;
using mesos::internal::master::allocator::HierarchicalDRFAllocatorProcess;


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
    if (state == nullptr) {
        state = new State(new ZooKeeperStorage(zk_url->servers, Seconds(10), zk_url->path));
        LOG(INFO) << "Created ZooKeeperStorage using zk_url " << zk_url->servers << zk_url->path;
        return Try<Nothing>(Nothing());
    } else {
        LOG(INFO) << "Ignored duplicate configuration attempt";
    }
}

Future<http::Response> OfferFilteringHierarchicalDRFAllocatorProcess::getOfferFilters(
    const http::Request &request)
{
    return http::OK(getFilteredAgentsJSON());
}

JSON::Object OfferFilteringHierarchicalDRFAllocatorProcess::getFilteredAgentsJSON() {

    JSON::Array filters;

    foreachpair (const SlaveID& agentId, const Slave& agent, this->slaves) {
        if (!agent.activated) {
            JSON::Object filter;
            filter.values["hostname"] = agent.hostname;
            filter.values["agentId"] = stringify(agentId);
            filters.values.push_back(filter);
        }
    }

    JSON::Object body;
    body.values["filters"] = std::move(filters);
    return body;
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
        foreachpair (const SlaveID& agentId, const Slave& slave, this->slaves) {
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

    foreachpair (const SlaveID& agentId, const Slave& agent, this->slaves) {
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

Future<http::Response> OfferFilteringHierarchicalDRFAllocatorProcess::persistAndReportOfferFilters()
{
    JSON::Object agentsJson = getFilteredAgentsJSON();
    persistFilteredAgents(agentsJson);
    return http::OK(agentsJson);
}

Option<SlaveID> OfferFilteringHierarchicalDRFAllocatorProcess::findSlaveID(
        const string& agentId, const string& hostname) {

    foreachpair (const SlaveID& agentId_, const Slave& agent, this->slaves) {
        if ( (hostname.empty() || hostname == agent.hostname) &&
                (agentId.empty() || agentId == stringify(agentId_)) ) {
            return Some(agentId_);
        }
    }
    return None();
}

pair<hashset<SlaveID>, string> OfferFilteringHierarchicalDRFAllocatorProcess::parseFilters(JSON::Object json)
{
    hashset<SlaveID> slaveIds;
    ostringstream errMsg;

    auto filters = json.values["filters"].as<JSON::Array>();
    for ( auto const &filter : filters.values) {
        auto values = filter.as<JSON::Object>().values;

        string agentId;
        std::map<std::string, JSON::Value>::const_iterator itAgentId = values.find("agentId");
        if (itAgentId != values.end()) {
            agentId = itAgentId->second.as<JSON::String>().value;
        }

        string hostname;
        std::map<std::string, JSON::Value>::const_iterator itHostname = values.find("hostname");
        if (itHostname != values.end()) {
            hostname = itHostname->second.as<JSON::String>().value;
        }

        auto slaveId = findSlaveID(agentId, hostname);
        if (slaveId.isNone()) {
            errMsg << ", ";
            if (!hostname.empty()) {
                errMsg << "[hostname == " << hostname << "]";
            }
            if (!hostname.empty() && !agentId.empty()) {
                errMsg << " && ";
            }
            if (!agentId.empty()) {
                errMsg << "[agentId: " << agentId << "]";
            }
        } else {
            slaveIds.insert(slaveId.get());
        }
    }

    string error = !errMsg.str().empty() ? errMsg.str().substr(2) : "";
    return std::make_pair(slaveIds, error);
}

void OfferFilteringHierarchicalDRFAllocatorProcess::applyFilters(hashset<SlaveID> agentIds)
{
    foreachpair (const SlaveID& agentId, const Slave& agent, this->slaves) {
        hashset<SlaveID>::const_iterator it = agentIds.find(agentId);
        if (it != agentIds.end()) {
            deactivateSlave(agentId);
        } else {
            activateSlave(agentId);
        }
    }
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
        auto filters = parseFilters(body);
        if (!filters.second.empty()) {
            return http::BadRequest("One or more agents specified do not exist: " + filters.second);
        } else {
            applyFilters(filters.first);
        }
    }

    return persistAndReportOfferFilters();
}



// Store filters as JSON structure
void OfferFilteringHierarchicalDRFAllocatorProcess::persistFilteredAgents(const JSON::Object& filteredAgents) {

    string serialized = stringify(filteredAgents);

    state->fetch(FILTERED_AGENTS)
        .onReady([this, serialized](const Option<Variable>& option) {
            state->store(option.get().mutate(serialized));
        });
}

// Read agent filters from the state store, and re-apply deactivations to
// the associated slaves; this may be called before all agents have re-registered,
// which is why we also call this method upon addSlave calls.
void OfferFilteringHierarchicalDRFAllocatorProcess::restoreFilteredAgents() {

    if (state != NULL) {
        state->fetch(FILTERED_AGENTS)
            .onReady([this](const Option<Variable>& option) {
                string serialized = option.get().value();
                if (!serialized.empty()) {
                    LOG(INFO) << "Attepting to restore filtered agents: " << serialized;
                    Try<JSON::Object> body_ = JSON::parse<JSON::Object>(serialized);
                    if (body_.isError()) {
                        LOG(ERROR) << "Failed to parse serialized filters '" << serialized << "': " << body_.error();
                    } else {
                        auto body = body_.get();
                        if (body.values.count("filters") == 1) {

                            auto filters = parseFilters(body);
                            if (!filters.second.empty()) {
                                LOG(WARNING) << "One or more filtered agents could not be resolved: " << filters.second;
                            }
                            // This event must take place AFTER the new leader has sufficiently recovered
                            // when it occurrs too soon, it can result in failed recovery!
                            applyFilters(filters.first);
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
    } else {
        LOG(INFO) << "Using 'zk_url': " << zk_url;
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
    NULL,
    create);