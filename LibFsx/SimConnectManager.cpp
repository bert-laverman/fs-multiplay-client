/**
 * SimConnectManager.cpp
 */
#include "stdafx.h"

#include <thread>

#include "SimConnectManager.h"


using namespace std;

using namespace nl::rakis::fsx;
using namespace nl::rakis;


static const SIMCONNECT_CLIENT_DATA_DEFINITION_ID NR_OF_DEFINITIONS          = 128;
static const SIMCONNECT_CLIENT_DATA_DEFINITION_ID DEF_ID_OFFSET              = 16;
static const SIMCONNECT_DATA_REQUEST_ID           REQ_SYSTEMSTATE_ON_CONNECT = DEF_ID_OFFSET+NR_OF_DEFINITIONS;
static const SIMCONNECT_DATA_REQUEST_ID           REQ_ID_OFFSET              = REQ_SYSTEMSTATE_ON_CONNECT + 2;

static const SIMCONNECT_CLIENT_EVENT_ID           NR_OF_EVENTS               = 512;
static const SIMCONNECT_CLIENT_EVENT_ID           MENU_EVT_ID                = 256;
static const SIMCONNECT_CLIENT_EVENT_ID           EVT_ID_OFFSET              = MENU_EVT_ID + 1;

enum FIPSERVER_EVT_GROUP {
	MENU_EVTGRP_ID
};

static const size_t NR_OF_AI_AIRCRAFT = 32;

static const char* SYSSTATE_AIRPATH  = "AircraftLoaded";
static const char* SYSSTATE_MODE     = "DialogMode";
static const char* SYSSTATE_FLIGHT   = "FlightLoaded";
static const char* SYSSTATE_FPLAN    = "FlightPlan";
static const char* SYSSTATE_SIMSTATE = "Sim";

/*static*/ Logger SimConnectManager::log_(Logger::getLogger("nl.rakis.fsx.SimConnectManager"));

/*static*/ atomic<SIMCONNECT_DATA_REQUEST_ID>    SimConnectManager::nextReqId(REQ_ID_OFFSET);

enum EVENT_ID {
	EVENT_SIM,							// Request Sim status
	EVENT_SIM_START,					// "SimStart"
	EVENT_SIM_STOP,						// "SimStop"
	EVENT_AIRCRAFT_LOADED,				// "AircraftLoaded"
	EVENT_PAUSE,						// Request Pase status
	EVENT_PAUSED,						// "Paused"
	EVENT_UNPAUSED,						// "Unpaused"
	EVENT_OBJECT_ADDED,					// "ObjectAdded"
	EVENT_OBJECT_REMOVED				// "ObjectRemoved"
};

SimConnectManager::SimConnectManager()
	: fsxHdl_(0),
	keepRunning_(false), stopped_(true),
	aiListLowest_(0)
{
    log_.debug("SimConnectManager()");

    requestCallbacks_.reserve(NR_OF_DEFINITIONS);
    requestCallbacks_.resize(NR_OF_DEFINITIONS);
    for (size_t i=0; i < NR_OF_DEFINITIONS; i++) {
        requestCallbacks_ [i].active = false;
        requestCallbacks_ [i].defined = false;
        requestCallbacks_ [i].autoDestruct = false;
    }

	eventCallbacks_.reserve(NR_OF_EVENTS);
	eventCallbacks_.resize(NR_OF_EVENTS);
	for (size_t i = 0; i < NR_OF_EVENTS; i++) {
		eventCallbacks_[i].active = false;
		eventCallbacks_[i].defined = false;
	}

	requestCallbacks_ [0].active = true;
    requestCallbacks_ [0].defined = true;
    unDefineData(DEF_ID_OFFSET);

	aiList_.reserve(NR_OF_AI_AIRCRAFT);
	aiList_.resize(NR_OF_AI_AIRCRAFT);
	for (size_t i = 0; i < NR_OF_AI_AIRCRAFT; i++) {
		aiList_[i].active = false;
		aiList_[i].valid = false;
	}

	log_.debug("SimConnectManager(): Done");
}

SimConnectManager::~SimConnectManager()
{
	log_.debug("~SimConnectManager()");

    if (isRunning()) {
    	log_.debug("~SimConnectManager(): Request thread to stop");
        stopRunning();
    }
    if (!simConnectThread_.joinable()) {
    	log_.debug("~SimConnectManager(): Thread not joianble, detaching it");
        simConnectThread_.detach();
    }
    else {
    	log_.debug("~SimConnectManager(): Joining thread");
        simConnectThread_.join();
    }
	log_.debug("~SimConnectManager(): Done");
}

SimConnectManager& SimConnectManager::instance()
{
	static SimConnectManager instance_;

	return instance_;
}

void SimConnectManager::start()
{
	log_.debug("start()");

    keepRunning_ = true;
    simConnectThread_ = thread([=] { run(); });

    log_.debug("start(): Done");
}

bool SimConnectManager::connect()
{
	log_.trace("connect()");

	if (isConnected()) {
		log_.warn("connect(): already connected");
		return true;
	}
	HRESULT hr;

	log_.trace("connect(): Calling SimConnect_Open");
	hr = SimConnect_Open(&fsxHdl_, "SimConnectWorker", NULL, 0, 0, 0);

	if (SUCCEEDED(hr)) {
        log_.debug("connect(): Connection succeeded");
        
		// also request notifications on sim start and aircraft change
        log_.trace("connect(): Request system events");
		hr = SimConnect_SubscribeToSystemEvent(fsxHdl_, EVENT_SIM_START, "SimStart");
		if (FAILED(hr)) {
			log_.error("connect(): SubscribeToSystemEvent failed (hr=", hr, ")");
		}
		hr = SimConnect_SubscribeToSystemEvent(fsxHdl_, EVENT_SIM_STOP, "SimStop");
		if (FAILED(hr)) {
			log_.error("connect(): SubscribeToSystemEvent failed (hr=", hr, ")");
		}
        hr = SimConnect_SubscribeToSystemEvent(fsxHdl_, EVENT_PAUSED, "Paused");
		if (FAILED(hr)) {
			log_.error("connect(): SubscribeToSystemEvent failed (hr=", hr, ")");
		}
        hr = SimConnect_SubscribeToSystemEvent(fsxHdl_, EVENT_UNPAUSED, "UnPaused");
		if (FAILED(hr)) {
			log_.error("connect(): SubscribeToSystemEvent failed (hr=", hr, ")");
		}
        hr = SimConnect_SubscribeToSystemEvent(fsxHdl_, EVENT_AIRCRAFT_LOADED, "AircraftLoaded");
		if (FAILED(hr)) {
			log_.error("connect(): SubscribeToSystemEvent failed (hr=", hr, ")");
		}
		hr = SimConnect_SubscribeToSystemEvent(fsxHdl_, EVENT_OBJECT_ADDED, "ObjectAdded");
		if (FAILED(hr)) {
			log_.error("connect(): SubscribeToSystemEvent failed (hr=", hr, ")");
		}
		hr = SimConnect_SubscribeToSystemEvent(fsxHdl_, EVENT_OBJECT_REMOVED, "ObjectRemoved");
		if (FAILED(hr)) {
			log_.error("connect(): SubscribeToSystemEvent failed (hr=", hr, ")");
		}
		hr = SimConnect_RequestSystemState(fsxHdl_, REQ_SYSTEMSTATE_ON_CONNECT, "Sim");
        if (FAILED(hr)) {
            log_.error("connect(): SimConnect_RequestSystemState failed (hr=", hr, ")");
        }

		// Prepare our menu
		hr = SimConnect_MenuAddItem(fsxHdl_, "FipServer", MENU_EVT_ID, 123);
		if (FAILED(hr)) {
			log_.error("connect(): SimConnect_MenuAddItem failed (hr=", hr, ")");
		}
		//addMenu("Quit FipServer", [=]() { disconnect(); });

		requestSystemStateOnce(SSE_AIRPATH, [this](int iVal, float fVal, const char* sVal) -> bool {
            this->userAircraftIdData_.airFilePath = sVal;
            this->requestUserAircraftIdData();
            return false;
        });
    }
	else {
        log_.debug("connect(): Connection failed");
        fsxHdl_ = 0; // Force not-connected status
	}
	return isConnected();
}

void SimConnectManager::disconnect()
{
    log_.trace("disconnect()");
	if (isConnected()) {
        log_.trace("disconnect(): Calling SimConnect_Close()");
		HRESULT hr = SimConnect_Close(fsxHdl_);
		if (FAILED(hr)) {
			log_.error("disconnect(): Close failed (hr=", hr, ")");
		}
		fsxHdl_ = 0;
	}
	else {
        log_.warn("disconnect(): Already disconnected");
	}
}

void SimConnectManager::connectLoop()
{
	log_.trace("connectLoop()");

	while (isRunning() && !isConnected()) {
		if (connect()) {
			log_.trace("connectLoop(): Connection succeeded");
		}
		else if (isRunning()) {
            unique_lock<mutex> lck(connectWaitLock_);
            connectWaitCv_.wait_for(lck, chrono::seconds(5));
		}
	}
	log_.trace("connectLoop(): done");
}


void SimConnectManager::run()
{
	log_.trace("run()");
//    addSimConnectEventHandler("SimConnectManager", [this](SimConnectEvent evt, DWORD data, const char* arg) -> bool {
//        this->onSimConnectEvent(evt, data, arg);
//        return false;
//    });

    while (keepRunning_) {
		connectLoop();

		log_.trace("run(): Waiting for work");
		while (isRunning() && isConnected()) {
            if (!isStopped()) { // Only run tasks while FSX is not in stopped mode
                log_.trace("run(): Checking for tasks");
                Requestor work;
                {
                    unique_lock<mutex> reqLck(requestLock_);
                    if (!requests_.empty()) {
                        log_.trace("run(): Getting task");
                        work = move(requests_.front());
                        requests_.pop_front();
                    }
                }
                if (work.valid()) {
                    log_.trace("run(): Running task");
                    work();
                }
            }
            log_.trace("run(): Checking for messages from FSX");
			checkDispatch();

            if (isConnected()) {
                this_thread::sleep_for(chrono::milliseconds(50));
            }
		}
		log_.trace("run(): Not connected or not running, done for now");
	}
	// We're stopping, so let's disconnect
	disconnect();
	log_.info("run(): done running");
}

void SimConnectManager::stopRunning()
{
	log_.debug("stopRunning()");
	keepRunning_ = false;
    {
        unique_lock<mutex> lck(connectWaitLock_);
        connectWaitCv_.notify_all();
    }
	log_.info("stopRunning(): Told worker to stop");
}


/*static*/ void SimConnectManager::logAIObjectAction(SIMCONNECT_RECV_EVENT_OBJECT_ADDREMOVE *obj)
{
	const char* verb = (obj->uEventID == EVENT_OBJECT_ADDED) ? "added" : "removed";

	switch (obj->eObjType) {
	case SIMCONNECT_SIMOBJECT_TYPE_USER:
		log_.debug("scDispatchProc(): The user's aircraft was ", verb, ". (ID=", obj->dwData, ")");
		break;
	case SIMCONNECT_SIMOBJECT_TYPE_ALL:
		log_.debug("scDispatchProc(): Some kind of AI object was ", verb, ". (ID=", obj->dwData, ")");
		break;
	case SIMCONNECT_SIMOBJECT_TYPE_AIRCRAFT:
		log_.debug("scDispatchProc(): An AI aircraft was ", verb, ". (ID=", obj->dwData, ")");
		break;
	case SIMCONNECT_SIMOBJECT_TYPE_HELICOPTER:
		log_.debug("scDispatchProc(): An AI helicopter was ", verb, ". (ID=", obj->dwData, ")");
		break;
	case SIMCONNECT_SIMOBJECT_TYPE_BOAT:
		log_.debug("scDispatchProc(): An AI boat was ", verb, ". (ID=", obj->dwData, ")");
		break;
	case SIMCONNECT_SIMOBJECT_TYPE_GROUND:
		log_.debug("scDispatchProc(): An AI groundvehicle was ", verb, ". (ID=", obj->dwData, ")");
		break;
	default:
		log_.debug("scDispatchProc(): An unknown AI type was ", verb, ". (type=", obj->eObjType, ", ID=", obj->dwData, ")");
		break;
	}
}

/*static*/ void __stdcall SimConnectManager::scDispatchProc(SIMCONNECT_RECV* pData, DWORD cbData, void *pContext)
{
	SimConnectManager* pThis = 0;
	if (pContext == 0) {
		log_.error("scDispatchProc(): SimConnect called me without context!");
        return;
	}
	else {
		pThis = (SimConnectManager*)pContext;
	}
    log_.trace("scDispatchProc()");

	if (pData == 0) {
		log_.error("scDispatchProc(): Received a NULL pointer as pData");
		return;
	}

    switch(pData->dwID) {
	case SIMCONNECT_RECV_ID_NULL:
	{
		log_.trace("scDispatchProc(): The documentation says this is 'Nothing useful'");
	}
	break;

	case SIMCONNECT_RECV_ID_EXCEPTION:
    {
        SIMCONNECT_RECV_EXCEPTION *except = (SIMCONNECT_RECV_EXCEPTION*) pData;
        log_.error("scDispatchProc(): SimConnect sent me an exception on packet #", except->dwSendID, ", parameter #", except->dwIndex);

        switch (except->dwException) {
        case SIMCONNECT_EXCEPTION_NONE:
            log_.error("scDispatchProc(): No Exception");
            break;
        case SIMCONNECT_EXCEPTION_ERROR:
            log_.error("scDispatchProc(): Unknown Error");
            break;
        case SIMCONNECT_EXCEPTION_SIZE_MISMATCH:
            log_.error("scDispatchProc(): Size mismatch");
            break;
        case SIMCONNECT_EXCEPTION_UNRECOGNIZED_ID:
            log_.error("scDispatchProc(): Unrecognized ID");
            break;
        case SIMCONNECT_EXCEPTION_UNOPENED:
            log_.error("scDispatchProc(): Talking through a closed connection (shouldn't be possible, right?)");
            break;
        case SIMCONNECT_EXCEPTION_VERSION_MISMATCH:
            log_.error("scDispatchProc(): SimConnect API version mismatch");
            break;
        case SIMCONNECT_EXCEPTION_TOO_MANY_GROUPS:
            log_.error("scDispatchProc(): Too many groups (max = 20)");
            break;
        case SIMCONNECT_EXCEPTION_NAME_UNRECOGNIZED:
            log_.error("scDispatchProc(): Unrecognized event name");
            break;
        case SIMCONNECT_EXCEPTION_TOO_MANY_EVENT_NAMES:
            log_.error("scDispatchProc(): Too many event names (max = 1000)");
            break;
        case SIMCONNECT_EXCEPTION_EVENT_ID_DUPLICATE:
            log_.error("scDispatchProc(): Event ID already in use (after MapClientEventToSimEvent or SubscribeToSystemEvent)");
            break;
        case SIMCONNECT_EXCEPTION_TOO_MANY_MAPS:
            log_.error("scDispatchProc(): Too many mappings defined (max = 20)");
            break;
        case SIMCONNECT_EXCEPTION_TOO_MANY_OBJECTS:
            log_.error("scDispatchProc(): Too many objects (max = 1000)");
            break;
        case SIMCONNECT_EXCEPTION_TOO_MANY_REQUESTS:
            log_.error("scDispatchProc(): Too many requests (max = 1000)");
            break;
        case SIMCONNECT_EXCEPTION_WEATHER_INVALID_PORT:
            log_.error("scDispatchProc(): Invalid port number requested");
            break;
        case SIMCONNECT_EXCEPTION_WEATHER_INVALID_METAR:
            log_.error("scDispatchProc(): METAR data did not match required format");
            break;
        case SIMCONNECT_EXCEPTION_WEATHER_UNABLE_TO_GET_OBSERVATION:
            log_.error("scDispatchProc(): Weather observation not available");
            break;
        case SIMCONNECT_EXCEPTION_WEATHER_UNABLE_TO_CREATE_STATION:
            log_.error("scDispatchProc(): Weather station could not be created");
            break;
        case SIMCONNECT_EXCEPTION_WEATHER_UNABLE_TO_REMOVE_STATION:
            log_.error("scDispatchProc(): Weather station could not be removed");
            break;
        case SIMCONNECT_EXCEPTION_INVALID_DATA_TYPE:
            log_.error("scDispatchProc(): Invalid data type requested");
            break;
        case SIMCONNECT_EXCEPTION_INVALID_DATA_SIZE:
            log_.error("scDispatchProc(): Invalid data size requested");
            break;
        case SIMCONNECT_EXCEPTION_DATA_ERROR:
            log_.error("scDispatchProc(): Generic Data Error");
            break;
        case SIMCONNECT_EXCEPTION_INVALID_ARRAY:
            log_.error("scDispatchProc(): Invalid Array used with SetDataOnSimObject");
            break;
        case SIMCONNECT_EXCEPTION_CREATE_OBJECT_FAILED:
            log_.error("scDispatchProc(): Create AI object failed");
            break;
        case SIMCONNECT_EXCEPTION_LOAD_FLIGHTPLAN_FAILED:
            log_.error("scDispatchProc(): Specified flightplan could not be found or bad format");
            break;
        case SIMCONNECT_EXCEPTION_OPERATION_INVALID_FOR_OBJECT_TYPE:
            log_.error("scDispatchProc(): Cannot perform action on object");
            break;
        case SIMCONNECT_EXCEPTION_ILLEGAL_OPERATION:
            log_.error("scDispatchProc(): AI operation cannot be completed");
            break;
        case SIMCONNECT_EXCEPTION_ALREADY_SUBSCRIBED:
            log_.error("scDispatchProc(): Client already subscribed to event");
            break;
        case SIMCONNECT_EXCEPTION_INVALID_ENUM:
            log_.error("scDispatchProc(): Invalid Enum value in RequestDataOnSimObjectType");
            break;
        case SIMCONNECT_EXCEPTION_DEFINITION_ERROR:
            log_.error("scDispatchProc(): Data definition error in RequestDataOnSimObject");
            break;
        case SIMCONNECT_EXCEPTION_DUPLICATE_ID:
            log_.error("scDispatchProc(): ID already in use for AddTpDataDefinition, AddCClientEventToNotificationGroup, or MapClientDataNameToID");
            break;
        case SIMCONNECT_EXCEPTION_DATUM_ID:
            log_.error("scDispatchProc(): Unknown Datum ID in SetDataOnSimObject");
            break;
        case SIMCONNECT_EXCEPTION_OUT_OF_BOUNDS:
            log_.error("scDispatchProc(): Value out of range for RequestDataOnSimObjectType or CreateClientData");
            break;
        case SIMCONNECT_EXCEPTION_ALREADY_CREATED:
            log_.error("scDispatchProc(): Client Data Area name already in use for MapClientDataNameToID");
            break;
        case SIMCONNECT_EXCEPTION_OBJECT_OUTSIDE_REALITY_BUBBLE:
            log_.error("scDispatchProc(): Cannot create ATC controlled AI object outside of reality bubble");
            break;
        case SIMCONNECT_EXCEPTION_OBJECT_CONTAINER:
            log_.error("scDispatchProc(): Create AI object failed, bad container");
            break;
        case SIMCONNECT_EXCEPTION_OBJECT_AI:
            log_.error("scDispatchProc(): Create AI object failed due to AI system");
            break;
        case SIMCONNECT_EXCEPTION_OBJECT_ATC:
            log_.error("scDispatchProc(): Create AI object failed due to ATC problem");
            break;
        case SIMCONNECT_EXCEPTION_OBJECT_SCHEDULE:
            log_.error("scDispatchProc(): Create AI Object failed due to schedule problem");
            break;
        }
    }
	break;

	case SIMCONNECT_RECV_ID_OPEN:
	{
		log_.trace("scDispatchProc(): Open");
		SIMCONNECT_RECV_OPEN *data = static_cast<SIMCONNECT_RECV_OPEN*>(pData);
		pThis->connection_.applicationName = data->szApplicationName;
		pThis->connection_.applicationVersionMajor = int(data->dwApplicationVersionMajor);
		pThis->connection_.applicationVersionMinor = int(data->dwApplicationVersionMinor);
		pThis->connection_.applicationBuildMajor = int(data->dwApplicationBuildMajor);
		pThis->connection_.applicationBuildMinor = int(data->dwApplicationBuildMinor);
		pThis->connection_.simConnectVersionMajor = int(data->dwSimConnectVersionMajor);
		pThis->connection_.simConnectVersionMinor = int(data->dwSimConnectVersionMinor);
		pThis->connection_.simConnectBuildMajor = int(data->dwSimConnectBuildMajor);
		pThis->connection_.simConnectBuildMinor = int(data->dwSimConnectBuildMinor);
		pThis->fireSimConnectEvent(SCE_CONNECT);
	}
	break;

	case SIMCONNECT_RECV_ID_QUIT:
	{
		log_.info("scDispatchProc(): FSX Quit");
		pThis->disconnect();
		pThis->fireSimConnectEvent(SCE_DISCONNECT);
	}
	break;

	case SIMCONNECT_RECV_ID_EVENT: // Subscribed events
	{
		log_.trace("scDispatchProc(): Received event");
		SIMCONNECT_RECV_EVENT *evt = (SIMCONNECT_RECV_EVENT*)pData;
		switch (evt->uEventID) {
		case EVENT_SIM_START:	// Track aircraft changes
			log_.trace("scDispatchProc(): SimStart event received");
			pThis->setStopped(false);
			pThis->fireSimConnectEvent(SCE_START);
			break;

		case EVENT_SIM_STOP:
			log_.trace("scDispatchProc(): SimStop event received");
			pThis->setStopped(true);
			pThis->fireSimConnectEvent(SCE_STOP);
			break;

		case EVENT_PAUSED:
			log_.trace("scDispatchProc(): SimPaused event received");
			pThis->fireSimConnectEvent(SCE_PAUSE);
			break;

		case EVENT_UNPAUSED:
			log_.trace("scDispatchProc(): SimUnPaused event received");
			pThis->fireSimConnectEvent(SCE_UNPAUSE);
			break;

		case EVENT_AIRCRAFT_LOADED:
			log_.trace("scDispatchProc(): SimAircraftLoaded event received");
			pThis->fireSimConnectEvent(SCE_AIRCRAFTLOADED, 0, ((SIMCONNECT_RECV_EVENT_FILENAME*)evt)->szFileName);
			break;

		case EVENT_OBJECT_ADDED:
			log_.trace("scDispatchProc(): ObjectAdded event received");
			pThis->fireSimConnectEvent(SCE_OBJECTADDED, evt->dwData);
			break;

		case EVENT_OBJECT_REMOVED:
			log_.trace("scDispatchProc(): ObjectRemoved event received");
			pThis->fireSimConnectEvent(SCE_OBJECTREMOVED, evt->dwData);
			break;

		default:
			if ((evt->uEventID >= EVT_ID_OFFSET) && (evt->uEventID < (EVT_ID_OFFSET + pThis->eventCallbacks_.size()))) {
				size_t index(evt->uEventID - EVT_ID_OFFSET);
				if (pThis->eventCallbacks_[index].active) {
					if (pThis->eventCallbacks_[index].isMenu) {
						pThis->eventCallbacks_[index].menuCallback();
					}
					else {
						log_.warn("scDispatchProc(): Ignoring non-menu event");
					}
				}
				else {
					log_.error("scDispatchProc(): Received client event for inactive event...");
				}
			}
			else {
				log_.warn("scDispatchProc(): Some unknown event received (uEventID=", evt->uEventID, ")");
			}
			break;
		}
	}
	break;

	case SIMCONNECT_RECV_ID_CLIENT_DATA:
	{
		log_.trace("scDispatchProc(): Received client data");
		SIMCONNECT_RECV_CLIENT_DATA *pObjData = (SIMCONNECT_RECV_CLIENT_DATA*)pData;
		pThis->fireClientDataCallback(pObjData->dwDefineID, pObjData);
	}
	break;

	case SIMCONNECT_RECV_ID_EVENT_OBJECT_ADDREMOVE:
	{
		log_.trace("scDispatchProc(): Object added or removed");
		SIMCONNECT_RECV_EVENT_OBJECT_ADDREMOVE *obj = static_cast<SIMCONNECT_RECV_EVENT_OBJECT_ADDREMOVE*>(pData);

		logAIObjectAction(obj);

		SimConnectEvent evt = (obj->uEventID == EVENT_OBJECT_ADDED) ? SCE_OBJECTADDED : SCE_OBJECTREMOVED;
		pThis->fireSimConnectEvent(evt, obj->dwData);
	}
	break;

	case SIMCONNECT_RECV_ID_EVENT_FILENAME:
	{
		log_.trace("scDispatchProc(): Filename received");
		SIMCONNECT_RECV_EVENT_FILENAME* data = static_cast<SIMCONNECT_RECV_EVENT_FILENAME*>(pData);
		log_.info("scDispatchProc(): Filename = \"", data->szFileName, "\"");
		pThis->userAircraftIdData_.airFilePath = data->szFileName;
		pThis->userAircraftIdData_.title = "";
		pThis->requestUserAircraftIdData();
	}
	break;

	case SIMCONNECT_RECV_ID_EVENT_FRAME:
	{
		log_.trace("scDispatchProc(): Frame rate info");
	}
	break;

	case SIMCONNECT_RECV_ID_SIMOBJECT_DATA:
    case SIMCONNECT_RECV_ID_SIMOBJECT_DATA_BYTYPE:
	{
		SIMCONNECT_RECV_CLIENT_DATA *pObjData = (SIMCONNECT_RECV_CLIENT_DATA*)pData;
		log_.trace("scDispatchProc(): Received simobject data (reqId=", pObjData->dwRequestID, ", defId=", pObjData->dwDefineID, ", flags=", pObjData->dwFlags, ")");

		pThis->fireSimDataCallback(pObjData->dwDefineID, pObjData);
	}
	break;

	case SIMCONNECT_RECV_ID_WEATHER_OBSERVATION:
	{
		log_.trace("scDispatchProc(): Weather observation received.");
	}
	break;

	case SIMCONNECT_RECV_ID_CLOUD_STATE:
	{
		log_.trace("scDispatchProc(): Cloud state received.");
	}
	break;

	case SIMCONNECT_RECV_ID_ASSIGNED_OBJECT_ID:
	{
		log_.trace("scDispatchProc(): AssignedObjectId");
		SIMCONNECT_RECV_ASSIGNED_OBJECT_ID *data = static_cast<SIMCONNECT_RECV_ASSIGNED_OBJECT_ID*>(pData);
		log_.trace("scDispatchProc(): Received ObjectId ", data->dwObjectID, " for request ", data->dwRequestID);
		size_t index = 0;
		{
			unique_lock<mutex> aiLck(pThis->aiLock_);
			while (index < NR_OF_AI_AIRCRAFT) {
				if ((pThis->aiList_[index].active) && (pThis->aiList_[index].reqId == data->dwRequestID)) {
					pThis->aiList_[index].id = data->dwObjectID;
					pThis->aiList_[index].valid = true;
					break;
				}
				index += 1;
			}
		}
		if (index < NR_OF_AI_AIRCRAFT) {
			pThis->aiList_[index].callback(data->dwObjectID);
			pThis->onObjectIdAssigned(pThis->aiList_[index]);
		}
		else {
			log_.warn("scDispatchProc(): Received ObjectId for unknown request");
		}
	}
	break;

	case SIMCONNECT_RECV_ID_SYSTEM_STATE: // Explicitly requested system states
	{
		log_.trace("scDispatchProc(): Received system state");
        SIMCONNECT_RECV_SYSTEM_STATE* pState = static_cast<SIMCONNECT_RECV_SYSTEM_STATE*>(pData);
        if (pState->dwRequestID == REQ_SYSTEMSTATE_ON_CONNECT) {
            if (pState->dwInteger == 1) {
                pThis->setStopped(false);
                pThis->fireSimConnectEvent(SCE_START);
            }
            else {
                pThis->setStopped(true);
                pThis->fireSimConnectEvent(SCE_STOP);
            }
        }
        else {
            pThis->fireSystemState(pState);
        }
	}
	break;

	default:
	{
		log_.debug("scDispatchProc(): Received uninteresting event (", pData->dwID, ")");
	}
	break;
	}

	log_.trace("scDispatchProc(): Check for control events to send");
//	if (pThis->m_ngxControl.Event == 0) {
//		if (pThis->getControl(pThis->m_ngxControl)) {
//			log_.debug("scDispatchProc(): Send control event");
//			HRESULT hr = SimConnect_SetClientData(pThis->m_hdl, PMDG_NGX_CONTROL_ID, PMDG_NGX_CONTROL_DEFINITION, 0, 0, sizeof(PMDG_NGX_Control), &pThis->m_ngxControl);
//			if (FAILED(hr)) {
//				log_.error("scDispatchProc(): Failed to send control (hr=", hr, ")");
//			}
//		}
//	}
    log_.trace("scDispatchProc(): done");
}

void SimConnectManager::checkDispatch()
{
    log_.trace("checkDispatch(): calling CallDispatch");
    HRESULT hr = SimConnect_CallDispatch(fsxHdl_, scDispatchProc, this);
	if (FAILED(hr)) {
		log_.error("checkDispatch(): CallDispatch failed (hr=", hr, "), closing connection");

		disconnect();
		fireSimConnectEvent(SCE_DISCONNECT);
	}
}

/*
 * SimConnectEventHandler stuff
 */

void SimConnectManager::addSimConnectEventHandler(const char* service, SimConnectEventHandler handler)
{
	log_.debug("addSimConnectEventHandler(): service=\"", service, "\"");

    sceListeners_.push_back(handler);
    log_.trace("addSimConnectEventHandler(): # of handlers is now ", sceListeners_.size());

    log_.trace("addSimConnectEventHandler(): Done");
}

void SimConnectManager::requestSimConnectStateOnce()
{
}

void SimConnectManager::fireSimConnectEvent(SimConnectEvent evt, DWORD data, const char* arg)
{
	log_.trace("fireSimConnectEvent()");
	if (evt == SCE_START) {
		log_.debug("fireSimConnectEvent(): ", sceListeners_.size(), "event handler(s)");
	}
    int nr = 0;
    for (SimConnectEventListeners::iterator i = sceListeners_.begin(); i != sceListeners_.end(); i++) {
		log_.trace("fireSimConnectEvent(): Calling handler #", ++nr, " (evt=", evt, ")");
        (*i).operator()(evt, data, arg);
    }

	log_.trace("fireSimConnectEvent(): Done");
}

/*
 * Very generic stuff
 */

/*static*/ DWORD SimConnectManager::lastSendId(HANDLE fsxHdl)
{
    DWORD sendId;
    HRESULT hr = SimConnect_GetLastSentPacketID(fsxHdl, &sendId);
    if (FAILED(hr)) {
        log_.error("requestSimDataOnce()[Requestor]: SimConnect_GetLastSentPacketID failed (hr=", hr, ")");
        return 0;
    }
    return sendId;
}

/*
 * My own event handler(s)
 */

bool SimConnectManager::onSimConnectEvent(SimConnectEvent evt, DWORD data, const char* arg)
{
	log_.trace("onSimConnectEvent(evt=", evt, ")");
    switch (evt) {
    case SCE_AIRCRAFTLOADED:
        requestUserAircraftIdData();
        break;

    case SCE_CONNECT:
        break;

    case SCE_DISCONNECT:
        break;

    case SCE_PAUSE:
        break;

    case SCE_UNPAUSE:
        break;

    case SCE_START:
        break;

    case SCE_STOP:
        break;

	case SCE_OBJECTADDED:
		break;

	case SCE_OBJECTREMOVED:
		break;

    }
	log_.trace("onSimConnectEvent(): Done, returning false");
    return false; // I'm not the last one to call
}

/*
 * Stuff for requesting data
 */

SIMCONNECT_DATA_REQUEST_ID SimConnectManager::getValidReqId(SIMCONNECT_DATA_REQUEST_ID& current) {
	if (current == 0) {
		current = nextReqId++;
	}
	return current;
}

/*static*/ bool SimConnectManager::ignoreDataItem(const void* data, const SimConnectData& dataDef)
{
	log_.trace("ignoreDataItem(): name=\"", dataDef.name, "\"");
	return true;
}

/*static*/ bool SimConnectManager::processDataItems(int entryNr, int outOfNr, const void* data, size_t size, const SimConnectDataDefinition* dataDef)
{
	log_.trace("processDataItems(): ", size, " values received, ", dataDef->dataDefs.size(), " expected.");
	bool result = false;

	for (size_t index = 0; (index < size) && (index < dataDef->dataDefs.size()); index++) {
		result = dataDef->itemCallback(data, dataDef->dataDefs[index]);

		switch (dataDef->dataDefs[index].type) {
		case SIMCONNECT_DATATYPE_INT32:
		case SIMCONNECT_DATATYPE_FLOAT32:
			data = (static_cast<const uint8_t*>(data) + 4);
			break;

		case SIMCONNECT_DATATYPE_INT64:
		case SIMCONNECT_DATATYPE_FLOAT64:
		case SIMCONNECT_DATATYPE_STRING8:
			data = (static_cast<const uint8_t*>(data) + 8);
			break;

		case SIMCONNECT_DATATYPE_STRING32:
			data = (static_cast<const uint8_t*>(data) + 32);
			break;

		case SIMCONNECT_DATATYPE_STRING64:
			data = (static_cast<const uint8_t*>(data) + 64);
			break;

		case SIMCONNECT_DATATYPE_STRING128:
			data = (static_cast<const uint8_t*>(data) + 128);
			break;

		case SIMCONNECT_DATATYPE_STRING256:
			data = (static_cast<const uint8_t*>(data) + 256);
			break;

		case SIMCONNECT_DATATYPE_STRING260:
			data = (static_cast<const uint8_t*>(data) + 260);
			break;

		default:
			log_.error("processDataItems(): Don't know how to handle data with type ", dataDef->dataDefs[index].type);
		}
	}

	log_.trace("processDataItems(): Calling itemsDoneCallback.");
	result = dataDef->itemsDoneCallback(entryNr, outOfNr, size, dataDef);

	log_.trace("processDataItems(): Done");
	return result;
}

/*static*/ bool SimConnectManager::ignoreDataItemsDone(int entryNr, int outOfNr, size_t size, const SimConnectDataDefinition* dataDef)
{
	//IGNORE
	return true;
}

SIMCONNECT_DATA_DEFINITION_ID SimConnectManager::defineData(const SimConnectData* vars, SimDataCallback callback, SimDataItemCallback itemCallback, SimDataItemsDoneCallback doneCallback, bool autoDestruct)
{
	log_.trace("defineData(): Request definition of data");
	SIMCONNECT_DATA_DEFINITION_ID defId = 0;
	size_t index = 0;
	log_.trace("defineData(): Search for a free Define ID");
	{
		unique_lock<mutex> reqLck(requestCallbackLock_);
		while ((index < NR_OF_DEFINITIONS) && requestCallbacks_[index].active) { index++; }
		if (index < NR_OF_DEFINITIONS) {
			requestCallbacks_[index].active = true;
			requestCallbacks_[index].reqId = 0;
		}
	}
	if (index < NR_OF_DEFINITIONS) {
		defId = DEF_ID_OFFSET + index;
		log_.trace("defineData(): Request definition of block with nr ", defId);

		DataCallbackList::reference data = requestCallbacks_[defId - DEF_ID_OFFSET];
		while (!vars->name.empty()) {
			data.dataDefs.push_back(*vars);
			vars++;
		}
		data.itemCallback = itemCallback;
		data.callback = callback;
		data.itemsDoneCallback = doneCallback;
		data.autoDestruct = autoDestruct;

		const HANDLE fsxHdl = fsxHdl_;
		{
			unique_lock<mutex> lck(requestLock_);
			data.reqId = nextReqId++;
			requests_.push_back(move(Requestor([this, defId, data, fsxHdl]() -> SIMCONNECT_DATA_REQUEST_ID {
				auto it = data.dataDefs.begin();
				size_t index = 0;
				while (it != data.dataDefs.end()) {
					log_.trace("defineData()[Requestor]: AddToDataDefinition: defId=", defId, ", name=\"", it->name, "\"");
					HRESULT hr = SimConnect_AddToDataDefinition(fsxHdl, defId, it->name.c_str(), (it->units.empty() ? nullptr : it->units.c_str()), it->type, it->epsilon, index);
					if (FAILED(hr)) {
						log_.error("defineData()[Requestor]: SimConnect_AddToDataDefinition failed (hr=", hr, ")");
						return 0;
					}
					log_.debug("defineData()[Requestor]: defId=", defId, ", name=\"", it->name, "\", sendId=", lastSendId(fsxHdl));
					index++;
					it++;
				}
				requestCallbacks_[defId - DEF_ID_OFFSET].defined = true;
				return data.reqId;
			})));
		}
	}
	else {
		log_.error("defineData(): No more definition blcoks free");
	}
	log_.trace("defineData(): Done");
	return defId;
}

SIMCONNECT_DATA_DEFINITION_ID SimConnectManager::defineData(const SimConnectData* vars, SimDataCallback callback, bool autoDestruct)
{
	return defineData(vars, callback, ignoreDataItem, ignoreDataItemsDone, autoDestruct);
}

SIMCONNECT_DATA_DEFINITION_ID SimConnectManager::defineData(const SimConnectData* vars, SimDataItemCallback callback, bool autoDestruct)
{
	return defineData(vars, processDataItems, callback, ignoreDataItemsDone, autoDestruct);
}

SIMCONNECT_DATA_DEFINITION_ID SimConnectManager::defineData(const SimConnectData* vars, SimDataItemCallback callback, SimDataItemsDoneCallback doneCallback, bool autoDestruct)
{
	return defineData(vars, processDataItems, callback, doneCallback, autoDestruct);
}

SIMCONNECT_DATA_DEFINITION_ID SimConnectManager::defineData(const vector<SimConnectData>& vars, SimDataCallback callback, SimDataItemCallback itemCallback, SimDataItemsDoneCallback doneCallback, bool autoDestruct)
{
	log_.trace("defineData(): Request definition of data");
	SIMCONNECT_DATA_DEFINITION_ID defId = 0;
	size_t index = 0;
	log_.trace("defineData(): Search for a free Define ID");
	{
		unique_lock<mutex> reqLck(requestCallbackLock_);
		while ((index < NR_OF_DEFINITIONS) && requestCallbacks_[index].active) { index++; }
		if (index < NR_OF_DEFINITIONS) {
			requestCallbacks_[index].active = true;
			requestCallbacks_[index].reqId = 0;
		}
	}
	if (index < NR_OF_DEFINITIONS) {
		defId = DEF_ID_OFFSET + index;
		log_.trace("defineData(): Request definition of block with nr ", defId);

		DataCallbackList::reference data = requestCallbacks_[defId - DEF_ID_OFFSET];
		data.dataDefs = vars;
		data.itemCallback = itemCallback;
		data.callback = callback;
		data.itemsDoneCallback = doneCallback;
		data.autoDestruct = autoDestruct;

		const HANDLE fsxHdl = fsxHdl_;
		{
			unique_lock<mutex> lck(requestLock_);
			data.reqId = nextReqId++;
			requests_.push_back(move(Requestor([this, defId, data, fsxHdl]() -> SIMCONNECT_DATA_REQUEST_ID {
				auto it = data.dataDefs.begin();
				size_t index = 0;
				while (it != data.dataDefs.end()) {
					log_.trace("defineData()[Requestor]: AddToDataDefinition: defId=", defId, ", name=\"", it->name, "\"");
					HRESULT hr = SimConnect_AddToDataDefinition(fsxHdl, defId, it->name.c_str(), (it->units.empty() ? nullptr : it->units.c_str()), it->type, it->epsilon, index);
					if (FAILED(hr)) {
						log_.error("defineData()[Requestor]: SimConnect_AddToDataDefinition failed (hr=", hr, ")");
						return 0;
					}
					log_.debug("defineData()[Requestor]: defId=", defId, ", name=\"", it->name, "\", sendId=", lastSendId(fsxHdl));
					index++;
					it++;
				}
				requestCallbacks_[defId - DEF_ID_OFFSET].defined = true;
				return data.reqId;
			})));
		}
	}
	else {
		log_.error("defineData(): No more definition blcoks free");
	}
	log_.trace("defineData(): Done");
	return defId;
}

SIMCONNECT_DATA_DEFINITION_ID SimConnectManager::defineData(const vector<SimConnectData>& vars, SimDataCallback callback, bool autoDestruct)
{
	return defineData(vars, callback, ignoreDataItem, ignoreDataItemsDone, autoDestruct);
}

SIMCONNECT_DATA_DEFINITION_ID SimConnectManager::defineData(const vector<SimConnectData>& vars, SimDataItemCallback callback, bool autoDestruct)
{
	return defineData(vars, processDataItems, callback, ignoreDataItemsDone, autoDestruct);
}

SIMCONNECT_DATA_DEFINITION_ID SimConnectManager::defineData(const vector<SimConnectData>& vars, SimDataItemCallback callback, SimDataItemsDoneCallback doneCallback, bool autoDestruct)
{
	return defineData(vars, processDataItems, callback, doneCallback, autoDestruct);
}


void SimConnectManager::unDefineData(SIMCONNECT_DATA_DEFINITION_ID defId)
{
	log_.trace("unDefineData(): Request clearing definition for defId=", defId);
    const HANDLE fsxHdl = fsxHdl_;
    DataCallbackList::reference data = requestCallbacks_ [defId-DEF_ID_OFFSET];
    {
        unique_lock<mutex> lck(requestLock_);
        requests_.push_back(move(Requestor([this,defId,data,fsxHdl]() -> SIMCONNECT_DATA_REQUEST_ID {
			log_.trace("unDefineData()[Requestor]: ClearDataDefinition: defId=", defId);
            HRESULT hr = SimConnect_ClearDataDefinition(fsxHdl, defId);
            if (FAILED(hr)) {
                log_.error("unDefineData()[Requestor]: SimConnect_ClearDataDefinition failed (hr=", hr, ")");
                return 0;
            }
            log_.debug("unDefineData()[Requestor]: defId=", defId, ", sendId=", lastSendId(fsxHdl));

			requestCallbacks_[defId - DEF_ID_OFFSET].reqId = 0;
			requestCallbacks_[defId - DEF_ID_OFFSET].defined = false;
			requestCallbacks_[defId - DEF_ID_OFFSET].active = false;
            return 0;
        })));
    }
	log_.trace("unDefineData(): Done");
}

void SimConnectManager::requestSimDataNoMore(const SIMCONNECT_DATA_DEFINITION_ID defId)
{
	log_.trace("requestSimDataNoMore()");

	SIMCONNECT_DATA_REQUEST_ID reqId(getValidReqId(requestCallbacks_ [defId-DEF_ID_OFFSET].reqId));
    const HANDLE fsxHdl = fsxHdl_;
    {
        unique_lock<mutex> lck(requestLock_);
        requests_.push_back(move(Requestor([reqId,defId,fsxHdl]() -> SIMCONNECT_DATA_REQUEST_ID {
			log_.trace("requestSimDataNoMore()[Requestor]: RequestDataOnSimObject: defId=", defId);
            HRESULT hr = SimConnect_RequestDataOnSimObject(fsxHdl, reqId, defId, SIMCONNECT_OBJECT_ID_USER, SIMCONNECT_PERIOD_NEVER);
            if (FAILED(hr)) {
                log_.error("requestSimDataNoMore()[Requestor]: SimConnect_RequestDataOnSimObject failed (hr=", hr, ")");
                return 0;
            }
            log_.debug("requestSimDataNoMore()[Requestor]: reqId=", reqId, ", defId=", defId, ", sendId=", lastSendId(fsxHdl));

            return reqId;
        })));
    }
	log_.trace("requestSimDataNoMore(): Done");
}

void SimConnectManager::requestSimDataOnce(const SIMCONNECT_DATA_DEFINITION_ID defId)
{
	log_.trace("requestSimDataOnce()");

	SIMCONNECT_DATA_REQUEST_ID reqId(getValidReqId(requestCallbacks_[defId - DEF_ID_OFFSET].reqId));
	const HANDLE fsxHdl = fsxHdl_;
    {
        unique_lock<mutex> lck(requestLock_);
        requests_.push_back(move(Requestor([reqId,defId,fsxHdl]() -> SIMCONNECT_DATA_REQUEST_ID {
			log_.trace("requestSimDataOnce()[Requestor]: RequestDataOnSimObject: defId=", defId);
            HRESULT hr = SimConnect_RequestDataOnSimObject(fsxHdl, reqId, defId, SIMCONNECT_OBJECT_ID_USER, SIMCONNECT_PERIOD_ONCE);
            if (FAILED(hr)) {
                log_.error("requestSimDataOnce()[Requestor]: SimConnect_RequestDataOnSimObject failed (hr=", hr, ")");
                return 0;
            }
            log_.debug("requestSimDataOnce()[Requestor]: reqId=", reqId, ", defId=", defId, ", sendId=", lastSendId(fsxHdl));

            return reqId;
        })));
    }
	log_.trace("requestSimDataOnce()");
}

void SimConnectManager::requestSimDataOnce(const SimConnectData* vars, SimDataCallback callback, SIMCONNECT_DATA_DEFINITION_ID defId)
{
	requestSimDataOnce((defId == 0) ? defineData(vars, callback, true) : defId);
}

void SimConnectManager::requestSimDataWhenChanged(const SIMCONNECT_DATA_DEFINITION_ID defId)
{
	log_.trace("requestSimDataWhenChanged()");
	SIMCONNECT_DATA_REQUEST_ID reqId(getValidReqId(requestCallbacks_[defId - DEF_ID_OFFSET].reqId));
	const HANDLE fsxHdl = fsxHdl_;
    {
        unique_lock<mutex> lck(requestLock_);

		log_.trace("requestSimDataWhenChanged(): Adding request task for: defId=", defId, ", reqId=", reqId);
		requests_.push_back(move(Requestor([reqId,defId,fsxHdl]() -> SIMCONNECT_DATA_REQUEST_ID {
			log_.trace("requestSimDataWhenChanged()[Requestor]: RequestDataOnSimObject: defId=", defId, ", reqId=", reqId);
            HRESULT hr = SimConnect_RequestDataOnSimObject(fsxHdl, reqId, defId, SIMCONNECT_OBJECT_ID_USER, SIMCONNECT_PERIOD_VISUAL_FRAME, SIMCONNECT_DATA_REQUEST_FLAG_CHANGED);
            if (FAILED(hr)) {
                log_.error("requestSimDataWhenChanged()[Requestor]: SimConnect_RequestDataOnSimObject failed (hr=", hr, ")");
                return 0;
            }
            log_.debug("requestSimDataWhenChanged()[Requestor]: reqId=", reqId, ", defId=", defId, ", sendId=", lastSendId(fsxHdl));

            return reqId;
        })));
    }
	log_.trace("requestSimDataWhenChanged(): Done");
}

void SimConnectManager::fireSimDataCallback(SIMCONNECT_DATA_DEFINITION_ID defId, SIMCONNECT_RECV_CLIENT_DATA* data)
{
	log_.trace("fireSimDataCallback(", defId, ", ...)");

	size_t callbackIdx(NR_OF_DEFINITIONS);
	{
		unique_lock<mutex> cb(requestCallbackLock_);
		if ((defId > DEF_ID_OFFSET) && (defId < DEF_ID_OFFSET + NR_OF_DEFINITIONS)) {
			callbackIdx = defId - DEF_ID_OFFSET;
		}
	}
	if (callbackIdx < NR_OF_DEFINITIONS) {
		if (!requestCallbacks_[callbackIdx].active) {
			log_.error("fireSimDataCallback(): Received data for inactive define block!");
		}
		else if (!requestCallbacks_[callbackIdx].defined) {
			log_.error("fireSimDataCallback(): Received data for define block which is not yet registered!");
		}
		else {
			log_.trace("fireSimDataCallback(): Sending data to callback");
			requestCallbacks_[callbackIdx].callback(data->dwentrynumber, data->dwoutof, &data->dwData, data->dwDefineCount, &(requestCallbacks_[callbackIdx]));

			if (requestCallbacks_[callbackIdx].autoDestruct) {
				log_.debug("fireSimDataCallback(): AutoDestruct is set, calling undefineData()");
				unDefineData(defId);
			}
		}
	}
	log_.trace("fireSimDataCallback(): Done");
}

void SimConnectManager::sendSimData(SIMCONNECT_OBJECT_ID id, SIMCONNECT_DATA_DEFINITION_ID defId, const void* data, size_t size)
{
	log_.trace("sendSimData()");

	const HANDLE fsxHdl = fsxHdl_;
	{
		unique_lock<mutex> lck(requestLock_);

		requests_.push_back(move(Requestor([fsxHdl, id, defId, data, size]() -> SIMCONNECT_DATA_REQUEST_ID {
			log_.debug("sendSimData()[Requestor]: SimConnect_SetDataOnSimObject: objId=", id, ", defId=", defId, ", ", size, " byte(s)");
			HRESULT hr = SimConnect_SetDataOnSimObject(fsxHdl, defId, id, 0, 0, size, const_cast<void*>(data));
			if (FAILED(hr)) {
				log_.error("sendSimData(): SimConnect_SetDataOnSimObject failed (hr=", hr, ")");
			}
			log_.debug("sendSimData(): defId=", defId, ", sendId=", lastSendId(fsxHdl));
			return 0;
		})));
	}
	log_.trace("sendSimData(): Done");
}

void SimConnectManager::defineData(FsxData& data, SimDataCallback callback, bool selfDestruct)
{
	data.setDefId(defineData(data.getData(), callback, ignoreDataItem, ignoreDataItemsDone, selfDestruct));
}

void SimConnectManager::defineData(FsxData& data, SimDataItemCallback callback, bool selfDestruct)
{
	data.setDefId(defineData(data.getData(), processDataItems, callback, ignoreDataItemsDone, selfDestruct));
}

void SimConnectManager::defineData(FsxData& data, SimDataCallback callback, SimDataItemsDoneCallback doneCallback, bool selfDestruct)
{
	data.setDefId(defineData(data.getData(), callback, ignoreDataItem, doneCallback, selfDestruct));
}

void SimConnectManager::defineData(FsxData& data, SimDataItemCallback callback, SimDataItemsDoneCallback doneCallback, bool selfDestruct)
{
	data.setDefId(defineData(data.getData(), processDataItems, callback, doneCallback, selfDestruct));
}

void SimConnectManager::unDefineData(FsxData& data)
{
	unDefineData(data.getDefId());
}

void SimConnectManager::requestSimDataNoMore(const FsxData& data)
{
	requestSimDataNoMore(data.getDefId());
}

void SimConnectManager::requestSimDataOnce(const FsxData& data)
{
	requestSimDataOnce(data.getDefId());
}

void SimConnectManager::requestSimDataWhenChanged(const FsxData& data)
{
	requestSimDataWhenChanged(data.getDefId());
}

void SimConnectManager::sendSimData(SIMCONNECT_OBJECT_ID id, FsxData& data, const void* dataPtr, size_t size)
{
	sendSimData(id, data.getDefId(), dataPtr, size);
}

/*
 */

void SimConnectManager::mapClientDataName(SIMCONNECT_CLIENT_DATA_ID dataId, const char* dataName)
{
	log_.debug("mapClientDataName(", dataId, ", \"", dataName, "\")");

	size_t index;
	{
		unique_lock<mutex> lck(clientDataCallbackLock_);
		for (index = 0; index < clientDataCallbacks_.size(); index++) {
			if (clientDataCallbacks_[index].dataId == dataId) {
				break;
			}
		}
		if (index == clientDataCallbacks_.size()) {
			clientDataCallbacks_.resize(index + 1);
			clientDataCallbacks_[index].dataId = dataId;
			clientDataCallbacks_[index].active = true;
		}
	}
	ClientDataCallbackList::reference data(clientDataCallbacks_[index]);
	data.defined = false;
	data.dataName = dataName;
	const HANDLE fsxHdl = fsxHdl_;
	{
		unique_lock<mutex> lck(requestLock_);
		data.reqId = nextReqId++;
		requests_.push_back(move(Requestor([data, dataName, fsxHdl]() -> SIMCONNECT_DATA_REQUEST_ID {
			log_.error("mapClientDataName()[Requestor]: SimConnect_MapClientDataNameToID  - map \"", dataName, "\" to ", data.dataId, " (reqId=", data.reqId, ")");
			HRESULT hr = SimConnect_MapClientDataNameToID(fsxHdl, dataName, data.dataId);
			if (FAILED(hr)) {
				log_.error("mapClientDataName()[Requestor]: SimConnect_MapClientDataNameToID failed (hr=", hr, ")");
				return 0;
			}
			return data.reqId;
		})));
	}
	log_.debug("mapClientDataName(): Done");
}

void SimConnectManager::addToClientDataDef(SIMCONNECT_CLIENT_DATA_ID dataId, SIMCONNECT_CLIENT_DATA_DEFINITION_ID defId, size_t offset, size_t size)
{
	size_t index;
	{
		unique_lock<mutex> lck(clientDataCallbackLock_);
		for (index = 0; index < clientDataCallbacks_.size(); index++) {
			if (clientDataCallbacks_[index].dataId == dataId) {
				break;
			}
		}
		if (index == clientDataCallbacks_.size()) {
			log_.error("addToClientDataDef(): Unknown Data id ", dataId);
			return;
		}
	}
	ClientDataCallbackList::reference data(clientDataCallbacks_[index]);
	data.defId = defId;
	data.defined = true;
	const HANDLE fsxHdl = fsxHdl_;
	{
		unique_lock<mutex> lck(requestLock_);
		requests_.push_back(move(Requestor([data, offset, size, fsxHdl]() -> SIMCONNECT_DATA_REQUEST_ID {
			log_.error("mapClientDataName()[Requestor]: SimConnect_AddToClientDataDefinition  - Add ", size, " byte(s) to ", data.dataId, " (reqId=", data.reqId, ")");
			HRESULT hr = SimConnect_AddToClientDataDefinition(fsxHdl, data.defId, offset, size);
			if (FAILED(hr)) {
				log_.error("mapClientDataName()[Requestor]: SimConnect_AddToClientDataDefinition failed (hr=", hr, ")");
				return 0;
			}
			return data.reqId;
		})));
	}
}

void SimConnectManager::requestClientDataOnce(SIMCONNECT_CLIENT_DATA_ID dataId, ClientDataCallback then)
{
	size_t index;
	{
		unique_lock<mutex> lck(clientDataCallbackLock_);
		for (index = 0; index < clientDataCallbacks_.size(); index++) {
			if (clientDataCallbacks_[index].dataId == dataId) {
				break;
			}
		}
		if (index == clientDataCallbacks_.size()) {
			log_.error("requestClientDataOnce(): Unknown Data id ", dataId);
			return;
		}
	}
	ClientDataCallbackList::reference data(clientDataCallbacks_[index]);
	const HANDLE fsxHdl = fsxHdl_;
	{
		unique_lock<mutex> lck(requestLock_);
		requests_.push_back(move(Requestor([data, fsxHdl]() -> SIMCONNECT_DATA_REQUEST_ID {
			log_.debug("requestClientDataOnce()[Requestor]: RequestClientData: dataId=", data.dataId, " (reqId=", data.reqId, ")");
			HRESULT hr = SimConnect_RequestClientData(fsxHdl, data.dataId, data.reqId, data.defId, SIMCONNECT_CLIENT_DATA_PERIOD_ONCE);
			if (FAILED(hr)) {
				log_.error("requestClientDataOnce()[Requestor]: SimConnect_RequestClientData failed (hr=", hr, ")");
				return 0;
			}
			log_.debug("requestClientDataOnce()[Requestor]: reqId=", data.reqId, ", defId=", data.defId, ", sendId=", lastSendId(fsxHdl));

			return data.reqId;
		})));
	}
}

void SimConnectManager::requestClientDataNoMore(SIMCONNECT_CLIENT_DATA_ID dataId)
{
	size_t index;
	{
		unique_lock<mutex> lck(clientDataCallbackLock_);
		for (index = 0; index < clientDataCallbacks_.size(); index++) {
			if (clientDataCallbacks_[index].dataId == dataId) {
				break;
			}
		}
		if (index == clientDataCallbacks_.size()) {
			log_.error("requestClientDataNoMore(): Unknown Data id ", dataId);
			return;
		}
	}
	ClientDataCallbackList::reference data(clientDataCallbacks_[index]);
	const HANDLE fsxHdl = fsxHdl_;
	{
		unique_lock<mutex> lck(requestLock_);
		requests_.push_back(move(Requestor([data, fsxHdl]() -> SIMCONNECT_DATA_REQUEST_ID {
			log_.debug("requestClientDataNoMore()[Requestor]: RequestClientData: dataId=", data.dataId);
			HRESULT hr = SimConnect_RequestClientData(fsxHdl, data.dataId, data.reqId, data.defId, SIMCONNECT_CLIENT_DATA_PERIOD_NEVER);
			if (FAILED(hr)) {
				log_.error("requestClientDataNoMore()[Requestor]: SimConnect_RequestClientData failed (hr=", hr, ")");
				return 0;
			}
			log_.debug("requestClientDataNoMore()[Requestor]: reqId=", data.reqId, ", defId=", data.defId, ", sendId=", lastSendId(fsxHdl));

			return data.reqId;
		})));
	}
}

void SimConnectManager::requestClientDataWhenChanged(SIMCONNECT_CLIENT_DATA_ID dataId, ClientDataCallback then)
{
	size_t index;
	{
		unique_lock<mutex> lck(clientDataCallbackLock_);
		for (index = 0; index < clientDataCallbacks_.size(); index++) {
			if (clientDataCallbacks_[index].dataId == dataId) {
				break;
			}
		}
		if (index == clientDataCallbacks_.size()) {
			log_.error("requestClientDataWhenChanged(): Unknown Data id ", dataId);
			return;
		}
	}
	ClientDataCallbackList::reference data(clientDataCallbacks_[index]);
	data.callback = move(then);
	const HANDLE fsxHdl = fsxHdl_;
	{
		unique_lock<mutex> lck(requestLock_);
		requests_.push_back(move(Requestor([data, fsxHdl]() -> SIMCONNECT_DATA_REQUEST_ID {
			log_.debug("requestClientDataWhenChanged()[Requestor]: RequestClientData: dataId=", data.dataId);
			HRESULT hr = SimConnect_RequestClientData(fsxHdl, data.dataId, data.reqId, data.defId, SIMCONNECT_CLIENT_DATA_PERIOD_ON_SET, SIMCONNECT_CLIENT_DATA_REQUEST_FLAG_CHANGED);
			if (FAILED(hr)) {
				log_.error("requestClientDataWhenChanged()[Requestor]: SimConnect_RequestClientData failed (hr=", hr, ")");
				return 0;
			}
			log_.debug("requestClientDataWhenChanged()[Requestor]: reqId=", data.reqId, ", defId=", data.defId, ", sendId=", lastSendId(fsxHdl));

			return data.reqId;
		})));
	}
}

void SimConnectManager::fireClientDataCallback(SIMCONNECT_CLIENT_DATA_DEFINITION_ID defId, SIMCONNECT_RECV_CLIENT_DATA* data)
{
	log_.trace("fireClientDataCallback(", defId, ", ...)");

	size_t index;
	{
		unique_lock<mutex> cb(clientDataCallbackLock_);
		for (index = 0; index < clientDataCallbacks_.size(); index++) {
			if (clientDataCallbacks_[index].defId == defId) {
				break;
			}
		}
		if (index == clientDataCallbacks_.size()) {
			log_.error("fireClientDataCallback(): Unknown definition id ", defId);
			return;
		}
	}
	ClientDataCallbackList::reference cbData(clientDataCallbacks_[index]);
	if (!cbData.active) {
		log_.error("fireClientDataCallback(): Received data for inactive request block!");
	}
	else if (!cbData.defined) {
		log_.error("fireClientDataCallback(): Received data for request block which is not yet registered!");
	}
	else {
		log_.trace("fireClientDataCallback(): Sending data to callback");
		cbData.callback(data->dwentrynumber, data->dwoutof, &data->dwData, data->dwDefineCount);
	}
	log_.trace("fireClientDataCallback(): Done");
}

/*
 * Events
 */
void SimConnectManager::defineEvent(FsxEvent& evt)
{
	log_.trace("defineEvent(): Request definition of data");

	if (evt.isDefined()) {
		log_.warn("defineEvent(): Event \"", evt.getName(), "\" is already defined (evtId=", evt.getEvtId(), ")");
	}

	SIMCONNECT_CLIENT_EVENT_ID evtId = 0;
	size_t index = 0;
	log_.trace("defineEvent(): Search for a free Event ID");
	{
		unique_lock<mutex> evtLck(eventLock_);

		while ((index < NR_OF_EVENTS) && eventCallbacks_[index].active) {
			if (eventCallbacks_[index].eventName == evt.getName()) {
				break;
			}
			index++;
		}
		if (index < NR_OF_EVENTS) {
			eventCallbacks_[index].active = true;
			evtId = EVT_ID_OFFSET + index;
		}
	}
	// (evtId == NO_EVENT) || (evtId == EVT_ID_OFFSET + index)

	if (evtId != 0) {
		evt.setEvtId(evtId);

		if (!eventCallbacks_[index].defined) {
			log_.trace("defineEvent(): Request definition of event with nr ", evtId);

			EventCallbackList::reference data = eventCallbacks_[index];
			data.isMenu = false;
			data.eventName = evt.getName();

			const HANDLE fsxHdl = fsxHdl_;
			{
				unique_lock<mutex> lck(requestLock_);
				requests_.push_back(move(Requestor([this, evtId, data, fsxHdl]() -> SIMCONNECT_DATA_REQUEST_ID {
					log_.trace("defineEvent()[Requestor]: MapClientEventToSimEvent: evtId=", evtId, ", name=\"", data.eventName, "\"");
					string name;
					name.reserve(data.eventName.size());
					for (auto c : data.eventName) { int b(wctob(c)); if (b != EOF) { name.push_back(char(b)); } }
					HRESULT hr = SimConnect_MapClientEventToSimEvent(fsxHdl, evtId, name.c_str());
					if (FAILED(hr)) {
						log_.error("defineEvent()[Requestor]: SimConnect_MapClientEventToSimEvent failed (hr=", hr, ")");
						return 0;
					}
					log_.debug("defineEvent()[Requestor]: evtId=", evtId, ", name=\"", data.eventName, "\", sendId=", lastSendId(fsxHdl));
					eventCallbacks_[evtId - EVT_ID_OFFSET].defined = true;
					return evtId;
				})));
			}
		}
	}
	else {
		log_.error("defineEvent(): No more event blocks free");
	}
	log_.trace("defineEvent(): Done");
}

void SimConnectManager::sendEvent(const FsxEvent& evt, DWORD evtData)
{
	sendEvent(0, evt, evtData);
}

void SimConnectManager::sendEvent(SIMCONNECT_OBJECT_ID objId, const FsxEvent& evt, DWORD evtData)
{
	log_.trace("sendEvent()");

	if (!evt.isDefined()) {
		log_.error("sendEvent(): Event \"", evt.getName(), "\" is defined yet");
		return;
	}

	SIMCONNECT_CLIENT_EVENT_ID evtId(evt.getEvtId());
	size_t index = evtId - EVT_ID_OFFSET;
	if ((index > NR_OF_EVENTS) || !eventCallbacks_[index].active || !eventCallbacks_[index].defined) {
		log_.error("sendEvent(): Unknown or inactive event (evtId=", evtId, ")");
	}
	else {
		const HANDLE fsxHdl = fsxHdl_;
		{
			unique_lock<mutex> lck(requestLock_);
			requests_.push_back(move(Requestor([this, objId, evtId, evtData, fsxHdl]() -> SIMCONNECT_DATA_REQUEST_ID {
				log_.trace("sendEvent()[Requestor]: TransmitClientEvent: evtId=", evtId, ", evtData=", evtData);
				HRESULT hr = SimConnect_TransmitClientEvent(fsxHdl, objId, evtId, evtData, SIMCONNECT_GROUP_PRIORITY_HIGHEST, SIMCONNECT_EVENT_FLAG_GROUPID_IS_PRIORITY);
				if (FAILED(hr)) {
					log_.error("sendEvent()[Requestor]: SimConnect_TransmitClientEvent failed (hr=", hr, ")");
					return 0;
				}
				log_.debug("sendEvent()[Requestor]: evtId=", evtId, ", evtData=", evtData, ", sendId=", lastSendId(fsxHdl));

				return evtId;
			})));
		}
	}
	log_.trace("sendEvent(): Done");
}

bool SimConnectManager::addMenu(const std::string& menuItem, MenuCallback callback)
{
	log_.debug("addMenu(): menuItem=\"", menuItem, "\"");

	SIMCONNECT_CLIENT_EVENT_ID evtId = 0;
	size_t index = 0;
	log_.trace("addMenu(): Search for a free Event ID");
	{
		unique_lock<mutex> reqLck(eventLock_);

		while ((index < NR_OF_EVENTS) && eventCallbacks_[index].active) {
			index++;
		}
		if (index < NR_OF_EVENTS) {
			eventCallbacks_[index].active = true;
			evtId = EVT_ID_OFFSET + index;
		}
	}
	// (evtId == 0) || (evtId == EVT_ID_OFFSET + index)

	if (evtId != 0) {
		if (!eventCallbacks_[index].defined) {
			log_.debug("addMenu(): Menuitem gets eventid ", evtId);

			EventCallbackList::reference data = eventCallbacks_[index];
			data.isMenu = true;
			data.eventName = menuItem;
			data.menuCallback = callback;

			const HANDLE fsxHdl = fsxHdl_;
			{
				unique_lock<mutex> lck(requestLock_);
				requests_.push_back(move(Requestor([this, evtId, data, fsxHdl]() -> SIMCONNECT_DATA_REQUEST_ID {
					log_.debug("addMenu()[Requestor]: MapClientEventToSimEvent: evtId=", evtId, ", name=\"", data.eventName, "\"");
					HRESULT hr = SimConnect_MapClientEventToSimEvent(fsxHdl, evtId);
					if (FAILED(hr)) {
						log_.error("addMenu()[Requestor]: SimConnect_MapClientEventToSimEvent failed (hr=", hr, ")");
						return 0;
					}
					hr = SimConnect_MenuAddSubItem(fsxHdl, MENU_EVT_ID, data.eventName.c_str(), evtId, 123);
					if (FAILED(hr)) {
						log_.error("addMenu()[Requestor]: SimConnect_MenuAddSubItem failed (hr=", hr, ")");
						return 0;
					}
					hr = SimConnect_AddClientEventToNotificationGroup(fsxHdl_, MENU_EVTGRP_ID, evtId);
					if (FAILED(hr)) {
						log_.error("connect(): SimConnect_AddClientEventToNotificationGroup failed (hr=", hr, ")");
					}
					hr = SimConnect_SetNotificationGroupPriority(fsxHdl_, MENU_EVTGRP_ID, SIMCONNECT_GROUP_PRIORITY_HIGHEST);
					if (FAILED(hr)) {
						log_.error("connect(): SimConnect_SetNotificationGroupPriority failed (hr=", hr, ")");
					}

					log_.debug("addMenu()[Requestor]: evtId=", evtId, ", sendId=", lastSendId(fsxHdl));
					eventCallbacks_[evtId - EVT_ID_OFFSET].defined = true;
					return evtId;
				})));
			}
		}
		else {
			log_.error("addMenu(): Free event was already defined?");
		}
	}
	else {
		log_.error("addMenu(): No more event blocks free");
	}
	log_.debug("addMenu(): Done");
	return (evtId != 0);
}

void SimConnectManager::removeMenu(const std::string& menuItem)
{
	//TODO
}

/*
 * AI related
 */

void SimConnectManager::detachAIAircraft(const SimConnectAIAircraft& data)
{
	const HANDLE fsxHdl = fsxHdl_;
	unique_lock<mutex> lck(requestLock_);
	requests_.push_back(move(Requestor([fsxHdl, data]() -> SIMCONNECT_DATA_REQUEST_ID {
		log_.debug("detachAIAircraft()[Requestor]: AIReleaseControl: title=\"", data.title, "\", callsign =\"", data.callsign, "\", id=", data.id, ", reqId=", data.reqId);

		HRESULT hr = SimConnect_AIReleaseControl(fsxHdl, data.id, data.reqId);
		if (FAILED(hr)) {
			log_.error("detachAIAircraft()[Requestor]: SimConnect_AIReleaseControl failed (hr=", hr, ")");
			return 0;
		}
		log_.debug("detachAIAircraft()[Requestor]: reqId=", data.reqId, ", sendId=", lastSendId(fsxHdl));

		return data.reqId;
	})));
}

void SimConnectManager::createAIAircraft(const string& title, const string& callsign, const AircraftLocationData& state, AIAircraftObjIdCallback callback, bool unmanaged)
{
	log_.trace("createAIAircraft()");
	size_t index = 0;

	log_.trace("createAIAircraft(): Search for a free index");
	{
		unique_lock<mutex> aiLck(aiLock_);
		index = aiListLowest_;
		while ((index < NR_OF_AI_AIRCRAFT) && aiList_[index].active) { index++; }
		if (index < NR_OF_AI_AIRCRAFT) {
			log_.debug("createAIAircraft(): Using record at index ", index);
			aiListLowest_ = index + 1;
			aiList_[index].active = true;
			aiList_[index].reqId = nextReqId++;
		}
		else {
			log_.error("createAIAircraft(): No room for new AI Aircraft");
			return;
		}
	}
	if (index < NR_OF_AI_AIRCRAFT) {
		const HANDLE fsxHdl = fsxHdl_;
		SimConnectAIAircraft& data(aiList_[index]);
		data.callback = callback;
		data.callsign = callsign;
		data.title = title;
		data.state = state;
		data.unmanaged = unmanaged;

		{
			unique_lock<mutex> lck(requestLock_);
			requests_.push_back(move(Requestor([fsxHdl, &data]() -> SIMCONNECT_DATA_REQUEST_ID {
				log_.debug("createAIAircraft()[Requestor]: AICreateNonATCAircraft: title=\"", data.title, "\", callsign=\"", data.callsign, "\"");
				log_.trace("createAIAircraft()[Requestor]: AICreateNonATCAircraft: latitude=", data.state.latitude, ", longitude=", data.state.longitude);
				log_.trace("createAIAircraft()[Requestor]: AICreateNonATCAircraft: altitude=", data.state.altitude, ", airspeed=", data.state.airspeed);

				SIMCONNECT_DATA_INITPOSITION pos;
				pos.Airspeed = data.state.airspeed;
				pos.Altitude = data.state.altitude;
				pos.Bank = data.state.bank;
				pos.Heading = data.state.heading;
				pos.Latitude = data.state.latitude;
				pos.Longitude = data.state.longitude;
				pos.OnGround = data.state.onGround ? 1 : 0;
				pos.Pitch = data.state.pitch;

				HRESULT hr = SimConnect_AICreateNonATCAircraft(fsxHdl, data.title.c_str(), data.callsign.c_str(), pos, data.reqId);
				if (FAILED(hr)) {
					log_.error("createAIAircraft()[Requestor]: SimConnect_AICreateNonATCAircraft failed (hr=", hr, ")");
					return 0;
				}
				log_.debug("createAIAircraft()[Requestor]: reqId=", data.reqId, ", sendId=", lastSendId(fsxHdl));

				return data.reqId;
			})));
		}
	}
	else {
		log_.error("createAIAircraft(): Too many AI aircraft");
	}
	log_.trace("createAIAircraft(): Done");
}

void SimConnectManager::removeAIAircraft(SIMCONNECT_OBJECT_ID objId)
{
	log_.debug("removeAIAircraft(): objId=", objId);
	{
		unique_lock<mutex> aiLck(aiLock_);
		size_t index = 0;
		while (index < NR_OF_AI_AIRCRAFT) {
			if (aiList_[index].active && (aiList_[index].id == objId)) {
				log_.debug("removeAIAircraft(): Cleaning up record at index ", index);
				SimConnectAIAircraft& aircraft(aiList_[index]);
				aircraft.active = false;
				aircraft.callback = nullptr;
				aircraft.callsign.clear();
				aircraft.id = 0;
				aircraft.reqId = 0;
				aircraft.title.clear();
				aircraft.unmanaged = false;
				aircraft.valid = false;

				if (aiListLowest_ > index) {
					aiListLowest_ = index;
				}
				break;
			}
			index++;
		}
	}

	const HANDLE fsxHdl = fsxHdl_;
	const SIMCONNECT_DATA_REQUEST_ID reqId(nextReqId++);

	{
		unique_lock<mutex> lck(requestLock_);
		requests_.push_back(move(Requestor([fsxHdl, reqId, objId]() -> SIMCONNECT_DATA_REQUEST_ID {
			log_.trace("removeAIAircraft()[Requestor]: AIRemoveObject: objId=", objId, ", reqId=", reqId);

			HRESULT hr = SimConnect_AIRemoveObject(fsxHdl, objId, reqId);
			if (FAILED(hr)) {
				log_.error("removeAIAircraft()[Requestor]: SimConnect_AIRemoveObject failed (hr=", hr, ")");
				return 0;
			}
			log_.debug("removeAIAircraft()[Requestor]: reqId=", reqId, ", sendId=", lastSendId(fsxHdl));

			return reqId;
		})));
	}
	log_.debug("removeAIAircraft(): Done");
}

void SimConnectManager::onObjectIdAssigned(const SimConnectAIAircraft& aircraft)
{
	log_.trace("onObjectIdAssigned()");

	if (aircraft.unmanaged) {
		detachAIAircraft(aircraft);
	}
	log_.trace("onObjectIdAssigned(): Done");
}

void SimConnectManager::onObjectAdded(const SimConnectAIAircraft& aircraft)
{
}

void SimConnectManager::onObjectRemoved(const SimConnectAIAircraft& aircraft)
{
}

/*
 * Aircraft data we want to track and provide
 */

enum {
	DATAID_ATC_TYPE = 1,
	DATAID_ATC_MODEL,
	DATAID_ATC_ID,
	DATAID_ATC_AIRLINE,
	DATAID_ATC_FLIGHTNUMBER,
	DATAID_TITLE,
	DATAID_NUM_OF_ENGINES,
	DATAID_ENGINE_TYPE,
};
typedef char String256Overlay[256];
struct AircraftDataOverlay {
    String256Overlay atcType;
    String256Overlay atcModel;
    String256Overlay atcId;
    String256Overlay atcAirline;
    String256Overlay atcFlightNumber;
    String256Overlay title;
};

void SimConnectManager::requestUserAircraftIdData()
{
    if (!userAircraft_.isDefined()) {
		userAircraft_.clear();
		userAircraft_.add(DATAID_ATC_TYPE, "ATC TYPE", "", SIMCONNECT_DATATYPE_STRING256);
		userAircraft_.add(DATAID_ATC_MODEL, "ATC MODEL", "", SIMCONNECT_DATATYPE_STRING256);
		userAircraft_.add(DATAID_ATC_ID, "ATC ID", "", SIMCONNECT_DATATYPE_STRING256);
		userAircraft_.add(DATAID_ATC_AIRLINE, "ATC AIRLINE", "", SIMCONNECT_DATATYPE_STRING256);
		userAircraft_.add(DATAID_ATC_FLIGHTNUMBER, "ATC FLIGHT NUMBER", "", SIMCONNECT_DATATYPE_STRING256);
		userAircraft_.add(DATAID_TITLE, "TITLE", "", SIMCONNECT_DATATYPE_STRING256);
		userAircraft_.add(DATAID_NUM_OF_ENGINES, "NUMBER OF ENGINES", "Number", SIMCONNECT_DATATYPE_INT32);
		userAircraft_.add(DATAID_ENGINE_TYPE, "ENGINE TYPE", "Number", SIMCONNECT_DATATYPE_INT32);
		defineData(userAircraft_, [=](const void* data, const SimConnectData& dataDef) {
			switch (dataDef.id) {
			case DATAID_ATC_TYPE: userAircraftIdData_.atcType = static_cast<const char*>(data); break;
			case DATAID_ATC_MODEL: userAircraftIdData_.atcModel = static_cast<const char*>(data); break;
			case DATAID_ATC_ID: userAircraftIdData_.atcId = static_cast<const char*>(data); break;
			case DATAID_ATC_AIRLINE: userAircraftIdData_.atcAirline = static_cast<const char*>(data); break;
			case DATAID_ATC_FLIGHTNUMBER: userAircraftIdData_.atcFlightNumber = static_cast<const char*>(data); break;
			case DATAID_TITLE: userAircraftIdData_.title = static_cast<const char*>(data); break;
			case DATAID_NUM_OF_ENGINES: userAircraftIdData_.nrOfEngines = *static_cast<const int32_t*>(data); break;
			case DATAID_ENGINE_TYPE: userAircraftIdData_.engineType = *static_cast<const int32_t*>(data); break;
			default: log_.error("[userAircraftCallback](): Unknown dataId ", dataDef.id); break;
			}
			return true;
		}, [=](int entryNr, int outOfNr, size_t size, const SimConnectDataDefinition* dataDef) {
			fireSimConnectEvent(SCE_AIRCRAFTLOADED);

			return true;
		});
    }
    requestSimDataOnce(userAircraft_);
}

/*
 * Stuff for system state events
 */

SIMCONNECT_DATA_REQUEST_ID SimConnectManager::registerSystemStateCallback(SystemStateCallback callback)
{
    log_.trace("registerSystemStateCallback()");
    const SIMCONNECT_DATA_REQUEST_ID reqId(nextReqId++);
    size_t index;
    {
        unique_lock<mutex> reqLck(requestCallbackLock_);
        index = systemStateCallbacks_.size();
        systemStateCallbacks_.emplace_back();
        systemStateCallbacks_ [index].active = false;
    }
    systemStateCallbacks_ [index].reqId = reqId;
    systemStateCallbacks_ [index].callback = callback;
    systemStateCallbacks_ [index].active = true;

    log_.debug("registerSystemStateCallback(): Done (reqId=", reqId, ")");
    return reqId;
}

void SimConnectManager::unregisterSystemStateCallback(SIMCONNECT_DATA_REQUEST_ID reqId)
{
    log_.debug("unregisterSystemStateCallback(reqId= ", reqId, ")");
    {
        unique_lock<mutex> reqLck(requestCallbackLock_);
        for (auto it=systemStateCallbacks_.begin(); it != systemStateCallbacks_.end(); it++) {
            if (it->reqId == reqId) {
                log_.trace("unregisterSystemStateCallback(): Found it");
                systemStateCallbacks_.erase(it);
                break;
            }
        }
    }
    log_.trace("unregisterSystemStateCallback(): Done");
}


void SimConnectManager::fireSystemState(SIMCONNECT_RECV_SYSTEM_STATE* data)
{
    const SIMCONNECT_DATA_REQUEST_ID reqId(data->dwRequestID);
    SystemStateCallback callback;

    log_.debug("fireSystemState(reqId= ", reqId, ")");
    {
        unique_lock<mutex> reqLck(requestCallbackLock_);
        for (size_t index=0; index < systemStateCallbacks_.size(); index++) {
            if ((systemStateCallbacks_ [index].reqId == reqId) && systemStateCallbacks_ [index].active) {
                log_.trace("fireSystemState(): Found it");
                callback = systemStateCallbacks_ [index].callback;
                systemStateCallbacks_ [index].active = false; // once only
                break;
            }
        }
    }
    if (callback) {
        log_.debug("fireSystemState(): Calling callback");
        callback(data->dwInteger, data->fFloat, data->szString);
        unregisterSystemStateCallback(reqId);
    }
    else {
        log_.warn("fireSystemState(): Received system state for request I don't know about");
    }
    log_.trace("fireSystemState(): Done");
}

void SimConnectManager::requestSystemStateOnce(SimConnectSystemStateEvent evt, SystemStateCallback callback)
{
    log_.debug("requestSystemStateOnce(evt= ", evt, ")");
    const char* stateName(nullptr);
    switch (evt) {
    case SSE_AIRPATH:  stateName = SYSSTATE_AIRPATH;  break;
    case SSE_FLIGHT:   stateName = SYSSTATE_FLIGHT;   break;
    case SSE_FPLAN:    stateName = SYSSTATE_FPLAN;    break;
    case SSE_MODE:     stateName = SYSSTATE_MODE;     break;
    case SSE_SIMSTATE: stateName = SYSSTATE_SIMSTATE; break;
    }

    const SIMCONNECT_DATA_REQUEST_ID reqId(registerSystemStateCallback(callback));
    const HANDLE fsxHdl = fsxHdl_;
    {
        unique_lock<mutex> lck(requestLock_);
        requests_.push_back(move(Requestor([reqId,stateName,fsxHdl]() -> SIMCONNECT_DATA_REQUEST_ID {
            log_.debug("requestSystemStateOnce()[Requestor]: RequestSystemState: reqId=", reqId);
            HRESULT hr = SimConnect_RequestSystemState(fsxHdl, reqId, stateName);
            if (FAILED(hr)) {
                log_.error("requestSystemStateOnce()[Requestor]: SimConnect_RequestSystemState failed (hr=", hr, ")");
                return 0;
            }
            log_.debug("requestSystemStateOnce()[Requestor]: reqId=", reqId, ", sendId=", lastSendId(fsxHdl));

            return reqId;
        })));
    }

    log_.debug("requestSystemStateOnce(): Done");
}


