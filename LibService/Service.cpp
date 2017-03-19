/**
 */
#include "stdafx.h"

#include "Service.h"

using namespace std;

using namespace nl::rakis::service;
using namespace nl::rakis;

const std::string Service::statusStatus("status");
const std::string Service::statusNew("New");
const std::string Service::statusStarting("Starting");
const std::string Service::statusRunning("Running");
const std::string Service::statusStopping("Stopping");
const std::string Service::statusStopped("Stopped");

/*static*/ Logger Service::log_(Logger::getLogger("nl.rakis.service.Service"));

Service::Service(std::string const& name)
    : name_(name)
    , autoStart_(false)
    , running_(false)
{
    setStatus(statusNew);
}

Service::~Service()
{
}

void Service::setStatus(string const& key, string const& value)
{
    log_.trace("setStatus(\"", key, "\", \"", value, "\")");
    status_.put(key, value);
}

void Service::addStatus(string const& key, string const& value)
{
    log_.trace("addStatus(\"", key, "\", \"", value, "\")");
    status_.add(key, value);
}

void Service::start()
{
    if (!isRunning()) {
        setStatus(statusStarting);
        doStart();
        setStatus(statusRunning);
        setRunning(true);
    }
}

void Service::doStart()
{
    log_.warn("doStart(): Unimplemented.");
}

void Service::stop()
{
    if (isRunning()) {
        setRunning(false);
        setStatus(statusStopping);
        doStop();
        setStatus(statusStopped);
    }
}

void Service::doStop()
{
    log_.warn("doStop(): Unimplemented.");
}
