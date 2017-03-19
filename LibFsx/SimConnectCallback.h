/**
 * SimConnectManager.h
 */
#ifndef __NL_RAKIS_FSX_SIMCONNECTCALLBACK_H
#define __NL_RAKIS_FSX_SIMCONNECTCALLBACK_H

#include <functional>

#include <Windows.h>
#include <SimConnect.h>

#include "../LibRakis/Log.h"

namespace nl {
namespace rakis {
namespace fsx {

class SimConnectManager;
typedef std::function<void (DWORD exception, DWORD sendId, DWORD index)> ExceptionCallback;

class SimConnectCallback {
public: // Constants and typedefs

    static void NO_EXCEPTION_CALLBACK(DWORD exception, DWORD sendId, DWORD index);

protected:
    static Logger log_;

private: // members
    SimConnectManager* mgr_;
    bool               once_;

    ExceptionCallback exceptCallback_;

public: // Methods
    explicit SimConnectCallback(SimConnectManager* mgr =nullptr, bool once =false, ExceptionCallback exceptCallback =NO_EXCEPTION_CALLBACK) : mgr_(mgr), once_(once), exceptCallback_(exceptCallback) {};
    virtual ~SimConnectCallback();

    virtual void callback(SIMCONNECT_RECV*);

    bool isOnce() const { return once_; };

private: // Blocked implementations
};
typedef SimConnectCallback* SimConnectCallbackPtr;

class SimConnectAirportListCallback
    : public virtual SimConnectCallback
{
};
class SimConnectCloudStateCallback
    : public virtual SimConnectCallback
{
};
class SimConnectAssignedObjectIdCallback
    : public virtual SimConnectCallback
{
};

typedef std::function<bool(DWORD entryNr, DWORD outOfNr, DWORD count, void* data)> ClientDataCallback;
class SimConnectClientDataCallback
    : public virtual SimConnectCallback
{
public: // Constants and typedefs

private: // members
    ClientDataCallback callback_;

public: // Methods
    explicit SimConnectClientDataCallback(SimConnectManager* mgr, bool once, ClientDataCallback callback/*, ExceptionCallback exceptCallback =NO_EXCEPTION_CALLBACK*/) : SimConnectCallback(mgr, once/*, exceptCallback*/), callback_(callback) {};
    virtual ~SimConnectClientDataCallback();

    virtual void callback(SIMCONNECT_RECV*);

private: // Blocked implementations
};

} // namespace fsx
} // namespace rakis
} // namespace nl


#endif
