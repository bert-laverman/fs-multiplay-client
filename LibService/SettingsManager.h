/**
 */
#ifndef __NL_RAKIS_SERVICE_SETTINGSMANAGER_H
#define __NL_RAKIS_SERVICE_SETTINGSMANAGER_H

#include <map>
#include <mutex>
#include <string>
#include <future>

#include <boost/property_tree/ptree.hpp>

#include "../LibRakis/File.h"
#include "../LibRakis/Log.h"

#include "Service.h"

namespace nl {
namespace rakis {
namespace service {


class ServiceManager;

class SettingsManager
    : public virtual Service
{
public: // Statics, types, & constants
    inline static SettingsManager& instance() { return instance_; };

private: // members
    std::string                 appName_;
    File                        appPath_;
    File                        configPath_;
    File                        propFile_;
    boost::property_tree::ptree properties_;

    void load();

    // There can be only one
    static Logger    log_;
    static SettingsManager      instance_;

public: // methods
    explicit SettingsManager();
    virtual ~SettingsManager();

    virtual void doStart();
    virtual void doStop();

    void init(const char* appName, File const& appPath);

    std::string const& getAppName() const { return appName_; };
    const char*        getCAppName() const { return appName_.c_str(); };

    File const& getAppPath   () const { return appPath_; };
    File const& getConfigPath() const { return configPath_; };

    std::string getStr (std::string const& key) const;
    void        setStr (std::string const& key, std::string const& value);
    int         getInt (std::string const& key) const;
    void        setInt (std::string const& key, int value) { setStr(key, std::to_string(value)); };
    bool        getBool(std::string const& key) const;
    void        setBool(std::string const& key, bool value) { setStr(key, value ? "true" : "false"); };
    double      getDbl (std::string const& key) const;
    void        setDbl (std::string const& key, double value) { setStr(key, std::to_string(value)); };

    boost::property_tree::ptree const& getTree(std::string name);
    void setTree(std::string name, boost::property_tree::ptree const& tree);

    void unsafeLoad();
    void store();

private: // deleted members
    SettingsManager(SettingsManager const&);
};

} // namespace service
} // namespace rakis
} // namespace nl



#endif