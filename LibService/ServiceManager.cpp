/**
 */
#include "stdafx.h"

#include <sstream>

#include "SettingsManager.h"
#include "ServiceManager.h"

using namespace std;

using namespace nl::rakis;
using namespace nl::rakis::service;


/*static*/ Logger ServiceManager::log_(Logger::getLogger("nl.rakis.service.ServiceManager"));

ServiceManager::ServiceManager()
    : Service("nl.rakis.service.ServiceManager")
{
	log_.debug("ServiceManager()");

    log_.debug("ServiceManager(): Done");
}

ServiceManager::~ServiceManager()
{
	log_.debug("~ServiceManager()");

    SettingsManager::instance().doStop();

    log_.info("~ServiceManager(): Done");
}

ServiceManager& ServiceManager::instance()
{
	static ServiceManager instance_;

	return instance_;
}
void ServiceManager::init()
{
    log_.debug("init()");
    log_.debug("init(): Done");
}

void ServiceManager::doStart()
{
    log_.trace("doStart()");

    log_.debug("doStart(): Starting all services");
    for (auto srv: services_) {
        if (srv.second->isAutoStart()) {
            srv.second->start();
        }
    }
    log_.debug("doStart(): All services started");

    log_.trace("doStart() done");
}

void ServiceManager::doStop()
{
    log_.trace("doStop()");

    log_.debug("doStop(): Stopping all services");
    for (auto srv: services_) {
        srv.second->stop();
    }
    log_.debug("doStop(): All services stopped");

    log_.trace("doStop() done");
}

void ServiceManager::addService(ServicePtr srv)
{
    log_.trace("addService()");

    log_.debug("addService(): srv->name = \"", srv->getName(), "\"");

    // TODO threadsafety!
    int index(services_.size());
    services_ [srv->getName()] = srv;

    ostringstream srvKeyBuilder;
    srvKeyBuilder << "services." << index;
    string srvKey = srvKeyBuilder.str();

    addStatus(srvKey + ".name", srv->getName());

    log_.debug("addService() done");
}

