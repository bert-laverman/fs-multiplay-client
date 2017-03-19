/**
 * Service.h
 */
#ifndef __NL_RAKIS_SERVICE_H
#define __NL_RAKIS_SERVICE_H

#include <string>

#include "../LibRakis/Log.h"

#include <boost/property_tree/ptree.hpp>

namespace nl {
namespace rakis {
namespace service {

class Service
{
public: // Types & constants
    static const std::string statusStatus;
    static const std::string statusNew;
    static const std::string statusStarting;
    static const std::string statusRunning;
    static const std::string statusStopping;
    static const std::string statusStopped;

private: // members
    std::string name_;
    boost::property_tree::ptree status_;
    bool autoStart_;
    bool running_;

	static Logger log_;

public: // methods
    Service(std::string const& name);
    virtual ~Service();

    void start();
    virtual void doStart();
    void stop();
    virtual void doStop();

    std::string const& getName()   const { return name_; };

    boost::property_tree::ptree const& getFullStatus() const { return status_; };
    std::string getStatus(std::string const& key) const { return status_.get<std::string>(key); };
    void setStatus(std::string const& key, std::string const& value);
    void addStatus(std::string const& key, std::string const& value);

    std::string getStatus() const { return getStatus(statusStatus); };
    void setStatus(std::string const& status) { setStatus(statusStatus, status); };

    bool isAutoStart() const { return autoStart_; };
    void setAutoStart(bool autoStart) { autoStart_ = autoStart; };

    bool isRunning() const { return running_; };
    void setRunning(bool running) { running_ = running; };
};

typedef Service* ServicePtr;

} // namespace service
} // namespace rakis
} // namespace nl


#endif
