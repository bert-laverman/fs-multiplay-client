/**
 * SimConnectManager.h
 */
#ifndef __NL_RAKIS_FSX_SIMCONNECTMANAGER_H
#define __NL_RAKIS_FSX_SIMCONNECTMANAGER_H

#include <future>
#include <mutex>
#include <atomic>
#include <condition_variable>

#include <map>
#include <deque>

#include <Windows.h>
#include <SimConnect.h>

#include "../LibRakis/Log.h"

#include "FsxData.h"
#include "FsxEvent.h"
#include "SimConnectTypes.h"

namespace nl {
namespace rakis {
namespace fsx {

class SimConnectManager
{
public: // Types and constants
    typedef std::function<void(SimConnectEvent,DWORD,const char*)> SimConnectEventHandler;
    typedef std::vector<SimConnectEventHandler>  SimConnectEventListeners;

    typedef std::packaged_task<SIMCONNECT_DATA_REQUEST_ID(void)>        Requestor;
    typedef std::deque<Requestor>                                       RequestQueue;

    typedef std::vector<SimConnectDataDefinition>                       DataCallbackList;
    typedef std::vector<SimConnectSystemStateRequest>                   SystemStateCallbackList;
    typedef std::vector<SimConnectClientDataDefinition>                 ClientDataCallbackList;

	typedef std::vector<SimConnectEventDefinition>                      EventCallbackList;

	typedef std::vector<SimConnectAIAircraft>                           AIAircraftList;

    static SimConnectManager& instance();

private: // Members
	HANDLE                   fsxHdl_;
    bool                     keepRunning_;
    bool                     stopped_;
    std::thread              simConnectThread_;

    std::mutex               connectWaitLock_;
    std::condition_variable  connectWaitCv_;

    std::mutex               requestLock_;
    RequestQueue             requests_;
    std::mutex               requestCallbackLock_;
    DataCallbackList         requestCallbacks_;
    SystemStateCallbackList  systemStateCallbacks_;
	std::mutex               clientDataCallbackLock_;
	ClientDataCallbackList   clientDataCallbacks_;

	std::mutex               eventLock_;
	EventCallbackList        eventCallbacks_;

	std::mutex               aiLock_;
	size_t                   aiListLowest_;
	AIAircraftList           aiList_;

	ConnectionData           connection_;

    SimConnectEventListeners sceListeners_;
    void fireSimConnectEvent(SimConnectEvent evt, DWORD data =0, const char* arg =nullptr);

    bool connect();
    void disconnect();
    void connectLoop();
    void run();
    void checkDispatch();

    bool onSimConnectEvent(SimConnectEvent evt, DWORD data, const char* arg);

	FsxData        userAircraft_;
    AircraftIdData userAircraftIdData_;
    void requestUserAircraftIdData();

    SIMCONNECT_DATA_REQUEST_ID registerSystemStateCallback(SystemStateCallback callback);
    void unregisterSystemStateCallback(SIMCONNECT_DATA_REQUEST_ID reqId);

	SIMCONNECT_DATA_REQUEST_ID getValidReqId(SIMCONNECT_DATA_REQUEST_ID& current);

	void fireSystemState(SIMCONNECT_RECV_SYSTEM_STATE* data);
	SIMCONNECT_DATA_DEFINITION_ID defineData(const SimConnectData* vars, SimDataCallback callback, SimDataItemCallback itemCallback, SimDataItemsDoneCallback doneCallback, bool selfDestruct = false);
	SIMCONNECT_DATA_DEFINITION_ID defineData(const std::vector<SimConnectData>& vars, SimDataCallback callback, SimDataItemCallback itemCallback, SimDataItemsDoneCallback doneCallback, bool selfDestruct = false);
	void fireSimDataCallback(SIMCONNECT_DATA_DEFINITION_ID defId, SIMCONNECT_RECV_CLIENT_DATA* data);
	void fireClientDataCallback(SIMCONNECT_CLIENT_DATA_DEFINITION_ID defId, SIMCONNECT_RECV_CLIENT_DATA* data);

    void requestSystemStateOnce(SimConnectSystemStateEvent evt, SystemStateCallback callback);

	static void logAIObjectAction(SIMCONNECT_RECV_EVENT_OBJECT_ADDREMOVE *obj);
    static void __stdcall scDispatchProc(SIMCONNECT_RECV* pData, DWORD cbData, void *pContext);

    static DWORD lastSendId(HANDLE fsxHdl);

    static Logger log_;

    static std::atomic<SIMCONNECT_DATA_REQUEST_ID>    nextReqId;

	static bool ignoreDataItem(const void* data, const SimConnectData& dataDef);
	static bool processDataItems(int entryNr, int outOfNr, const void* data, size_t size, const SimConnectDataDefinition* dataDef);
	static bool ignoreDataItemsDone(int entryNr, int outOfNr, size_t size, const SimConnectDataDefinition* dataDef);

	// AI related helpers
	void onObjectIdAssigned(const SimConnectAIAircraft& aircraft);
	void onObjectAdded(const SimConnectAIAircraft& aircraft);
	void onObjectRemoved(const SimConnectAIAircraft& aircraft);

	void detachAIAircraft(const SimConnectAIAircraft& data);

public: // Methods
    SimConnectManager();
    virtual ~SimConnectManager();

    bool isRunning() const { return keepRunning_; };
	bool isConnected() const { return (fsxHdl_ != 0); };
    bool isStopped() const { return stopped_; };

    void setStopped(bool stopped =true) {stopped_ = stopped; };

    void start();
    void stopRunning();

    ConnectionData const& getConnection()         const { return connection_; };
    AircraftIdData const& getUserAircraftIdData() const { return userAircraftIdData_; };

    void addSimConnectEventHandler(const char* service, SimConnectEventHandler handler);
	void requestSimConnectStateOnce();

	// FSX Data, Obsolete method
    SIMCONNECT_DATA_DEFINITION_ID defineData(const SimConnectData* vars, SimDataCallback callback, bool selfDestruct =false);
    SIMCONNECT_DATA_DEFINITION_ID defineData(const std::vector<SimConnectData>& vars, SimDataCallback callback, bool selfDestruct =false);
	SIMCONNECT_DATA_DEFINITION_ID defineData(const SimConnectData* vars, SimDataItemCallback callback, bool selfDestruct = false);
	SIMCONNECT_DATA_DEFINITION_ID defineData(const std::vector<SimConnectData>& vars, SimDataItemCallback callback, bool selfDestruct = false);
	SIMCONNECT_DATA_DEFINITION_ID defineData(const SimConnectData* vars, SimDataItemCallback callback, SimDataItemsDoneCallback doneCallback, bool selfDestruct = false);
	SIMCONNECT_DATA_DEFINITION_ID defineData(const std::vector<SimConnectData>& vars, SimDataItemCallback callback, SimDataItemsDoneCallback doneCallback, bool selfDestruct = false);
	void unDefineData(SIMCONNECT_DATA_DEFINITION_ID defId);

	void requestSimDataOnce(const SimConnectData* vars, SimDataCallback then, SIMCONNECT_DATA_DEFINITION_ID defId =0);

	void requestSimDataNoMore(const SIMCONNECT_DATA_DEFINITION_ID defId);
    void requestSimDataOnce(const SIMCONNECT_DATA_DEFINITION_ID defId);
    void requestSimDataWhenChanged(const SIMCONNECT_DATA_DEFINITION_ID defId);

	void sendSimData(SIMCONNECT_OBJECT_ID id, SIMCONNECT_DATA_DEFINITION_ID defId, const void* data, size_t size);

	// FSX Data, New method
	void defineData(FsxData& data, SimDataCallback callback, bool selfDestruct = false);
	void defineData(FsxData& data, SimDataItemCallback callback, bool selfDestruct = false);
	void defineData(FsxData& data, SimDataCallback callback, SimDataItemsDoneCallback doneCallback, bool selfDestruct = false);
	void defineData(FsxData& data, SimDataItemCallback callback, SimDataItemsDoneCallback doneCallback, bool selfDestruct = false);
	void unDefineData(FsxData& data);

	void requestSimDataNoMore(const FsxData& data);
	void requestSimDataOnce(const FsxData& data);
	void requestSimDataWhenChanged(const FsxData& data);

	void sendSimData(SIMCONNECT_OBJECT_ID id, FsxData& data, const void* dataPtr, size_t size);

	// Client data
	void mapClientDataName(SIMCONNECT_CLIENT_DATA_ID dataId, const char* dataName);
	void addToClientDataDef(SIMCONNECT_CLIENT_DATA_ID dataId, SIMCONNECT_CLIENT_DATA_DEFINITION_ID defId, size_t offset, size_t size);
	void requestClientDataOnce(SIMCONNECT_CLIENT_DATA_ID dataId, ClientDataCallback then);
	void requestClientDataNoMore(SIMCONNECT_CLIENT_DATA_ID dataId);
	void requestClientDataWhenChanged(SIMCONNECT_CLIENT_DATA_ID dataId, ClientDataCallback then);

	// Events
	void defineEvent(FsxEvent& evt);
	void sendEvent(const FsxEvent& evt, DWORD data = 0);
	void sendEvent(SIMCONNECT_OBJECT_ID id, const FsxEvent& evt, DWORD data = 0);
	bool addMenu(const std::string& menuItem, MenuCallback callback);
	void removeMenu(const std::string& menuItem);

	// AI Aircraft
	void createAIAircraft(const std::string& title, const std::string& callsign, const AircraftLocationData& state, AIAircraftObjIdCallback callback, bool unmanaged = true);
	void removeAIAircraft(SIMCONNECT_OBJECT_ID objId);

private:
    SimConnectManager(SimConnectManager const&) =delete;
    SimConnectManager& operator=(SimConnectManager const&) =delete;
};

} // namespace fsx
} // namespace rakis
} // namespace nl


#endif
