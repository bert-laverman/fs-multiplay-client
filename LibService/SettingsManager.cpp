/**
 * SettingsManager
 */
#include "stdafx.h"

#include <boost/property_tree/json_parser.hpp>

#include "ServiceManager.h"
#include "SettingsManager.h"

using namespace std;

using namespace boost::property_tree;

using namespace nl::rakis;
using namespace nl::rakis::service;

static const string settingsManagerName("nl.rakis.service.SettingsManager");


/*static*/ Logger SettingsManager::log_(Logger::getLogger("nl.rakis.service.SettingsManager"));
/*static*/ SettingsManager SettingsManager::instance_;

SettingsManager::SettingsManager()
    : Service(settingsManagerName)
{
}

SettingsManager::~SettingsManager()
{
}

void SettingsManager::init(const char* appName, File const& appPath)
{
    if (appPath.exists() && appPath.isDirectory() && !appPath_.exists()) {
        appName_ = appName;
        appPath_ = appPath;
        configPath_ = appPath.getChild("config");
		unsafeLoad();
    }
}

void SettingsManager::doStart()
{
    log_.info("doStart(): Loading settings");
    load();
    log_.trace("doStart(): Done");
}

void SettingsManager::doStop()
{
    log_.info("doStop(): Saving settings");
    store();
    log_.trace("doStop(): Done");
}

ptree const& SettingsManager::getTree(string name)
{
    static ptree emptyResult;

    try {
        return properties_.get_child(name);
    }
    catch (...) {
    }
    return emptyResult;
}

void SettingsManager::setTree(string name, ptree const& tree)
{
    properties_.erase(name);
    properties_.put_child(name, tree);
}

void SettingsManager::unsafeLoad()
{
    log_.trace("load(): Loading settings");
    File settingsFile(configPath_.getChild("settings.json"));

    try {
        log_.info("load(): Reading \"", settingsFile.getPath(), "\"");
        read_json(settingsFile.getPath1(), properties_);
    }
    catch (...) {
        log_.error("load(): Failed to load \"", settingsFile.getPath(), "\"");
    }
    log_.trace("load(): done");
}

void SettingsManager::load()
{
    unsafeLoad();
}

void SettingsManager::store()
{
    log_.trace("store(): Saving settings");
    File settingsFile(configPath_.getChild("settings.json"));

    try {
        log_.info("store(): Writing \"", settingsFile.getPath(), "\"");
        write_json(settingsFile.getPath1(), properties_);
    }
    catch (...) {
        log_.error("store(): Failed to write \"", settingsFile.getPath(), "\"");
    }
    log_.trace("store(): done");
}

std::string SettingsManager::getStr (std::string const& key) const
{
    string result("");
    try {
        result = properties_.get<string>(key);
    }
    catch (...) {
        log_.warn("getStr(): \"", key, "\" not found.");
    }
    return result;
}

void SettingsManager::setStr (std::string const& key, std::string const& value)
{
    try {
        properties_.put(key, value);
    }
    catch (...) {
        log_.warn("setStr(): Failed to set \"", key, "\" to \"", value, "\".");
    }
}


int SettingsManager::getInt (std::string const& key) const
{
    int result;
    
    try {
        result = stoi(getStr(key));
    }
    catch(...) {
        log_.warn("getInt(): Could not convert value of \"", key, "\" to integer.");
        result = 0;
    }
    return result;
}


bool SettingsManager::getBool(std::string const& key) const
{
    bool result;
    
    try {
        result = getStr(key) == "true";
    }
    catch(...) {
        log_.warn("getBool(): Could not convert value of \"", key, "\" to bool.");
        result = 0;
    }
    return result;
}


double SettingsManager::getDbl (std::string const& key) const
{
    double result;
    
    try {
        result = stod(getStr(key));
    }
    catch(...) {
        log_.warn("getInt(): Could not convert value of \"", key, "\" to double.");
        result = 0.0;
    }
    return result;
}

