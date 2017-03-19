/**
 */
#ifndef __NL_RAKIS_SERVICE_SERVICEMANAGER_H
#define __NL_RAKIS_SERVICE_SERVICEMANAGER_H

#include <map>
#include <string>
#include <functional>
#include <future>

#include "../LibRakis/File.h"
#include "../LibRakis/Log.h"

#include "Service.h"
#include "SettingsManager.h"


namespace nl {
namespace rakis {
namespace service {

class ServiceManager
    : public virtual Service
{
public: // Statics, types & constants
    typedef std::map<std::string, ServicePtr> ServiceMap;

    static ServiceManager& instance();

private: // members
    ServiceMap               services_;
    std::future<void>        init_;

    static Logger log_;

    void init();

public: // methods
    ServiceManager();
    virtual ~ServiceManager();

    virtual void doStart();
    virtual void doStop();

    void addService(ServicePtr srv);
    void addService(Service& srv) { addService(&srv); };
    const ServicePtr getService(std::string const& naam) { auto srv = services_.find(naam); return (srv == services_.end()) ? nullptr : srv->second; };
    bool haveService(std::string const& name) const { auto srv = services_.find(name); return (srv == services_.end()); };
    void foreachService(std::function<void(ServicePtr)> doIt) { for (auto service: services_) { doIt(service.second); } };
};

typedef ServiceManager* ServiceManagerPtr;

} // namespace service
} // namespace rakis
} // namespace nl



#endif