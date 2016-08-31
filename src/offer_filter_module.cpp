#include <iostream>
#include <string>

#include <mesos/mesos.hpp>
#include <mesos/module.hpp>
#include <mesos/module/anonymous.hpp>
#include <mesos/module/allocator.hpp>

#include <stout/json.hpp>
#include <stout/os.hpp>
#include <stout/protobuf.hpp>

#include "config.h"
#include "offer_filter_module.hpp"

using std::map;
using std::string;
using std::vector;
using std::pair;

namespace http = process::http;

using namespace mesos;
using namespace mesos::modules;

using process::HELP;
using process::TLDR;
using process::DESCRIPTION;
using mesos::internal::master::allocator::internal::HierarchicalAllocatorProcess;
using mesos::internal::master::allocator::HierarchicalDRFAllocatorProcess;
using mesos::allocator::Allocator;

template<>
Future<http::Response> OfferFilteringHierarchicalDRFAllocatorProcess::getOfferFilters(
    const http::Request &request)
{
    JSON::Array filters;
    if (!slaves.empty()) {
        foreachpair (const SlaveID& agentId, const Slave& agent, slaves) {
            if (!agent.activated) {
                JSON::Object filter;
                filter.values["hostname"] = agent.hostname;
                filter.values["agentId"] = stringify(agentId);
                filters.values.push_back(filter);
            }
        }
    }

    JSON::Object body;
    body.values["filters"] = std::move(filters);
    return http::OK(body);
}

template<>
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
            LOG(INFO) << "Adding filter for agent " << agentIdToDeactivate ;
            deactivateSlave(*agentIdToDeactivate);
            return getOfferFilters(request);
        }
    } else {
        return http::BadRequest("body requires 'agentId' and/or 'hostname' attributes");
    }
}

template<>
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
                LOG(INFO) << "Activating agent " << agent.hostname << ":" << stringify(agentId);
                activateSlave(agentId);
                return getOfferFilters(request);
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

template<>
Option<SlaveID> OfferFilteringHierarchicalDRFAllocatorProcess::findSlaveID(
        const string& hostname, const string& agentId) {

    foreachpair (const SlaveID& agentId_, const Slave& agent, slaves) {
        if ( (hostname.empty() || hostname == agent.hostname) &&
                (agentId.empty() || agentId == stringify(agentId_)) ) {
            return Some(agentId_);
        }
    }
    return None();
}

template<>
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

    // auto body = body_.get();
    // auto filters_ = body.find("filters");
    // if (filters_.isSome()) {

    //     hashset<SlaveID> slaveIds;

    //     auto filters = body.values["filters"].as<JSON::Array>();
    //     for ( auto const &filter : filters) {

    //     }
    //     foreachpair (const SlaveID& agentId, const Slave& agent, slaves) {

    //     }
    // }

    return http::OK(JSON::parse(
        "{"
            "\"result\": \"ok (method not implemented)\""
        "}").get()
    );
}

template<>
Future<http::Response> OfferFilteringHierarchicalDRFAllocatorProcess::offerFilters(
    const http::Request &request)
{
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


namespace {

    // Called by the main() of master at startup
    Allocator* create(const Parameters& parameters)
    {
        for (int i = 0; i < parameters.parameter_size(); ++i) {
            Parameter parameter = parameters.parameter(i);
            LOG(INFO) << parameter.key() << ": " << parameter.value();
        }

        Try<Allocator*> allocator = OfferFilteringHierarchicalDRFAllocator::create();
        if (allocator.isError()) {
            return nullptr;
        }
        LOG(INFO) << "Created new " MODULE_NAME_STRING " instance";
        return allocator.get();
    }

}

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