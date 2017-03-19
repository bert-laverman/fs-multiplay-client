/**
*/
#include "stdafx.h"

#include <cwctype>
#include <chrono>
#include <strstream>
#include <iomanip>

#include <cpprest/json.h>
#include <cpprest/base_uri.h>

#include "MultiplayManager.h"
#include "JSONUtil.h"

#include "../LibAI/AIManager.h"

using namespace std;
using namespace std::chrono;

using namespace boost::property_tree;

using namespace nl::rakis;
using namespace nl::rakis::service;
using namespace nl::rakis::fsx;

using namespace web;
using namespace web::json;
using namespace web::http;
using namespace web::http::client;
using namespace web::websockets::client;
using namespace utility::conversions;

/*static*/ Logger     MultiplayManager::log_(Logger::getLogger("nl.rakis.fsx.MultiplayManager"));


/**
 * Constructor
 */
MultiplayManager::MultiplayManager()
	: Service("MultiplayManager")
	, connect_(false)
	, apiConnected_(false)
	, wsConnect_(false)
	, wsConnected_(false)
	, needSendAircraft_(false)
	, needSendLocation_(false) , lastLocationUpdate_()
	, needSendEngines_(false)  , lastEngineUpdate_()
	, needSendLights_(false)   , lastLightUpdate_()
	, needSendControls_(false) , lastControlsUpdate_()
{
	log_.trace("MultiplayManager()");

	setAutoStart(true);

	log_.trace("MultiplayManager(): Done");
}

MultiplayManager::~MultiplayManager()
{
	log_.trace("~MultiplayManager()");
}

MultiplayManager& MultiplayManager::instance()
{
	static MultiplayManager instance_;

	return instance_;
}

inline void assign(wstring& ws, const string& v) { ws.assign(v.begin(), v.end()); }

void MultiplayManager::doStart()
{
	log_.debug("doStart()");

	api_.start();

	SimConnectManager::instance().addSimConnectEventHandler("MultiplayManager", [=](SimConnectEvent evt, DWORD arg1, const char* arg2) {
		switch (evt) {
		case SCE_CONNECT:
			onFsxConnect();
			break;

		case SCE_DISCONNECT:
			onFsxDisconnect();
			break;

		case SCE_START:
			onFsxStarted();
			break;

		case SCE_STOP:
			onFsxStopped();
			break;

		case SCE_PAUSE:
			onFsxPaused();
			break;

		case SCE_UNPAUSE:
			onFsxUnpaused();
			break;

		case SCE_AIRCRAFTLOADED:
			onFsxAircraftLoaded();
			break;

		case SCE_OBJECTADDED:
			onFsxObjectAdded();
			break;

		case SCE_OBJECTREMOVED:
			onFsxObjectRemoved();
			break;

		}
		return false;
	});

	auto multiplayTree = SettingsManager::instance().getTree("multiplay");
	assign(serverUrl_, multiplayTree.get<string>("serverUrl", "https://fs.rakis.nl/api/"));
	log_.info("doStart(): serverUrl=\"", serverUrl_, "\"");
	assign(serverWsUrl_, multiplayTree.get<string>("serverWsUrl", "wss://fs.rakis.nl/live"));
	log_.info("doStart(): serverWsUrl=\"", serverWsUrl_, "\"");
	assign(username_, multiplayTree.get<string>("username", "bertl"));
	log_.info("doStart(): username=\"", username_, "\"");
	assign(password_, multiplayTree.get<string>("password", "test"));
	log_.debug("doStart(): password=\"", password_, "\"");
	assign(session_, multiplayTree.get<string>("session", "BSS"));
	log_.info("doStart(): session=\"", session_, "\"");
	forceCallsign_ = multiplayTree.get<bool>("forceCallsign", true);
	log_.info("doStart(): forceCallsign=", forceCallsign_);
	assign(callsign_, multiplayTree.get<string>("callsign", "PH-BLA"));
	log_.info("doStart(): callsign=\"", callsign_, "\"");

	api_.setServer(serverUrl_, serverWsUrl_, username_, password_, session_, callsign_);

	if (SimConnectManager::instance().isConnected() && !fsxConnected_) {
		log_.debug("doStart(): We're already connected to FSX");
		onFsxConnect();
	}
	if (fsxStarted_) {
		if (SimConnectManager::instance().isStopped()) {
			log_.debug("doStart(): FSX is in the STOPPED state");
			onFsxStopped();
		}
		else {
			log_.debug("doStart(): FSX is in the STARTED state (and so am I)");
		}
	}
	else {
		if (!SimConnectManager::instance().isStopped()) {
			log_.debug("doStart(): FSX is in the STARTED state");
			onFsxStarted();
		}
		else {
			log_.debug("doStart(): FSX is in the STOPPED state (and so am I)");
		}
	}

	log_.trace("doStart(): Done");
}

void MultiplayManager::doStop()
{
	log_.debug("doStop()");

	api_.stop();

	ptree multiplayTree;

	multiplayTree.put("serverUrl", wstr2str(serverUrl_));
	multiplayTree.put("username", wstr2str(username_));
	multiplayTree.put("password", wstr2str(password_));
	multiplayTree.put("session", wstr2str(session_));
	multiplayTree.put("forceCallsign", forceCallsign_);
	multiplayTree.put("callsign", wstr2str(callsign_));

	SettingsManager::instance().setTree("multiplay", multiplayTree);

	log_.trace("doStop(): Done");
}

void MultiplayManager::doStartRequestingData()
{
	log_.debug("doStartRequestingData()");

	SimConnectManager::instance().requestSimDataOnce(aircraftLocationData_);
	SimConnectManager::instance().requestSimDataWhenChanged(aircraftLocationData_);
	SimConnectManager::instance().requestSimDataOnce(aircraftEngineData_);
	SimConnectManager::instance().requestSimDataWhenChanged(aircraftEngineData_);
	SimConnectManager::instance().requestSimDataOnce(aircraftLightData_);
	SimConnectManager::instance().requestSimDataWhenChanged(aircraftLightData_);
	SimConnectManager::instance().requestSimDataOnce(aircraftControlsData_);
	SimConnectManager::instance().requestSimDataWhenChanged(aircraftControlsData_);
	SimConnectManager::instance().requestSimDataWhenChanged(aircraftEngineData_);

	log_.debug("doStartRequestingData(): Done");
}

void MultiplayManager::doStopRequestingData()
{
	log_.debug("doStopRequestingData()");

	SimConnectManager::instance().requestSimDataNoMore(aircraftLocationData_);
	SimConnectManager::instance().requestSimDataNoMore(aircraftEngineData_);
	SimConnectManager::instance().requestSimDataNoMore(aircraftLightData_);
	SimConnectManager::instance().requestSimDataNoMore(aircraftControlsData_);

	log_.debug("doStopRequestingData(): Done");
}

void MultiplayManager::onLocalStatusUpdate()
{
	log_.trace("onLocalStatusUpdate()");
}

void MultiplayManager::onLocalAircraftUpdate()
{
	log_.debug("onLocalAircraftUpdate()");

	api_.sendData(aircraftId_);
	doStartRequestingData();

	log_.debug("onLocalAircraftUpdate(): Done");
}


static void getValue(string& s, const void* data, const SimConnectData& dataDef)
{
	ostrstream os;
	switch (dataDef.type) {
	case SIMCONNECT_DATATYPE_FLOAT32:
		os << *static_cast<const float*>(data);
		break;
	case SIMCONNECT_DATATYPE_FLOAT64:
		os << *static_cast<const double*>(data);
		break;
	case SIMCONNECT_DATATYPE_INT32:
		os << *static_cast<const int32_t*>(data);
		break;
	case SIMCONNECT_DATATYPE_INT64:
		os << *static_cast<const int64_t*>(data);
		break;
	case SIMCONNECT_DATATYPE_STRINGV:
		os << static_cast<const char*>(data);
		break;
	case SIMCONNECT_DATATYPE_STRING8:
	{
		const char*p = static_cast<const char*>(data);
		for (unsigned i = 0; (i < 8) && (*p != '\0'); i++) {
			os << *p++;
		}
		break;
	}
	case SIMCONNECT_DATATYPE_STRING32:
	{
		const char*p = static_cast<const char*>(data);
		for (unsigned i = 0; (i < 32) && (*p != '\0'); i++) {
			os << *p++;
		}
		break;
	}
	case SIMCONNECT_DATATYPE_STRING64:
	{
		const char*p = static_cast<const char*>(data);
		for (unsigned i = 0; (i < 64) && (*p != '\0'); i++) {
			os << *p++;
		}
		break;
	}
	case SIMCONNECT_DATATYPE_STRING128:
	{
		const char*p = static_cast<const char*>(data);
		for (unsigned i = 0; (i < 128) && (*p != '\0'); i++) {
			os << *p++;
		}
		break;
	}
	case SIMCONNECT_DATATYPE_STRING256:
	{
		const char*p = static_cast<const char*>(data);
		for (unsigned i = 0; (i < 256) && (*p != '\0'); i++) {
			os << *p++;
		}
		break;
	}
	case SIMCONNECT_DATATYPE_STRING260:
	{
		const char*p = static_cast<const char*>(data);
		for (unsigned i = 0; (i < 260) && (*p != '\0'); i++) {
			os << *p++;
		}
		break;
	}
	default:
		os << "{type=" << dataDef.type << "}";
		break;
	}
	os << ends;
	s = os.str();
}

// FSX event handlers
void MultiplayManager::onFsxConnect()
{
	log_.debug("onFsxConnect()");

	if (!fsxConnected_) {
		fsxConnected_ = true;

		SimConnectManager& mgr(SimConnectManager::instance());

		// Location data
		aircraftLocationData_.clear();
		aircraftLocationData_.add(DATAID_LATITUDE, "PLANE LATITUDE", "DEGREES", SIMCONNECT_DATATYPE_FLOAT64, 0.0000001f);
		aircraftLocationData_.add(DATAID_LONGITUDE, "PLANE LONGITUDE", "DEGREES", SIMCONNECT_DATATYPE_FLOAT64, 0.0000001f);
		aircraftLocationData_.add(DATAID_ALTITUDE, "PLANE ALTITUDE", "FEET", SIMCONNECT_DATATYPE_FLOAT64, 1.0f);
		aircraftLocationData_.add(DATAID_PITCH, "PLANE PITCH DEGREES", "DEGREES", SIMCONNECT_DATATYPE_FLOAT64, 0.5f);
		aircraftLocationData_.add(DATAID_BANK, "PLANE BANK DEGREES", "DEGREES", SIMCONNECT_DATATYPE_FLOAT64, 0.5f);
		aircraftLocationData_.add(DATAID_HEADING, "PLANE HEADING DEGREES TRUE", "DEGREES", SIMCONNECT_DATATYPE_FLOAT64, 0.5f);
		aircraftLocationData_.add(DATAID_SIM_ON_GROUND, "SIM ON GROUND", "BOOL", SIMCONNECT_DATATYPE_INT32);
		aircraftLocationData_.add(DATAID_TAS, "AIRSPEED TRUE", "KNOTS", SIMCONNECT_DATATYPE_INT32);
		mgr.defineData(aircraftLocationData_, [=](const void* data, const SimConnectData& dataDef) {
			log_.trace("[positionUpdateCallback](): received \"", dataDef.name, "\"");
			switch (dataDef.id) {
			case DATAID_LATITUDE: position_.latitude = *static_cast<const double*>(data); break;
			case DATAID_LONGITUDE: position_.longitude = *static_cast<const double*>(data); break;
			case DATAID_ALTITUDE: position_.altitude = *static_cast<const double*>(data); break;
			case DATAID_PITCH: position_.pitch = *static_cast<const double*>(data); break;
			case DATAID_BANK: position_.bank = *static_cast<const double*>(data); break;
			case DATAID_HEADING: position_.heading = *static_cast<const double*>(data); break;
			case DATAID_SIM_ON_GROUND: position_.onGround = DWORD(*static_cast<const int64_t*>(data)); break;
			case DATAID_TAS: position_.airspeed = DWORD(*static_cast<const int64_t*>(data)); break;
			default:
				log_.error("[positionUpdateCallback](): Unknown data element?");
				break;
			}
			return true;
		}, [=](int entryNr, int outOfNr, size_t size, const SimConnectDataDefinition* dataDef) {
			log_.trace("[positionUpdateCompleteCallback](): Data block complete");
			api_.sendData(position_);
			return true;
		});

		// Aircraft engine data
		aircraftEngineData_.clear();
		aircraftEngineData_.add(DATAID_ENIGINE1_COMBUSTION, "GENERAL ENG COMBUSTION:1", "Bool", SIMCONNECT_DATATYPE_INT32);
		aircraftEngineData_.add(DATAID_ENIGINE2_COMBUSTION, "GENERAL ENG COMBUSTION:2", "Bool", SIMCONNECT_DATATYPE_INT32);
		aircraftEngineData_.add(DATAID_ENIGINE3_COMBUSTION, "GENERAL ENG COMBUSTION:3", "Bool", SIMCONNECT_DATATYPE_INT32);
		aircraftEngineData_.add(DATAID_ENIGINE4_COMBUSTION, "GENERAL ENG COMBUSTION:4", "Bool", SIMCONNECT_DATATYPE_INT32);
		aircraftEngineData_.add(DATAID_ENIGINE1_THROTTLE, "GENERAL ENG THROTTLE LEVER POSITION:1", "Percent", SIMCONNECT_DATATYPE_INT32);
		aircraftEngineData_.add(DATAID_ENIGINE2_THROTTLE, "GENERAL ENG THROTTLE LEVER POSITION:2", "Percent", SIMCONNECT_DATATYPE_INT32);
		aircraftEngineData_.add(DATAID_ENIGINE3_THROTTLE, "GENERAL ENG THROTTLE LEVER POSITION:3", "Percent", SIMCONNECT_DATATYPE_INT32);
		aircraftEngineData_.add(DATAID_ENIGINE4_THROTTLE, "GENERAL ENG THROTTLE LEVER POSITION:4", "Percent", SIMCONNECT_DATATYPE_INT32);
		mgr.defineData(aircraftEngineData_, [=](const void* data, const SimConnectData& dataDef) {
			switch (dataDef.id) {
			case DATAID_ENIGINE1_COMBUSTION: aircraftEngines_.engineOn[0] = *static_cast<const int32_t*>(data) != 0; break;
			case DATAID_ENIGINE2_COMBUSTION: aircraftEngines_.engineOn[1] = *static_cast<const int32_t*>(data) != 0; break;
			case DATAID_ENIGINE3_COMBUSTION: aircraftEngines_.engineOn[2] = *static_cast<const int32_t*>(data) != 0; break;
			case DATAID_ENIGINE4_COMBUSTION: aircraftEngines_.engineOn[3] = *static_cast<const int32_t*>(data) != 0; break;
			case DATAID_ENIGINE1_THROTTLE: aircraftEngines_.throttle[0] = *static_cast<const int32_t*>(data); break;
			case DATAID_ENIGINE2_THROTTLE: aircraftEngines_.throttle[1] = *static_cast<const int32_t*>(data); break;
			case DATAID_ENIGINE3_THROTTLE: aircraftEngines_.throttle[2] = *static_cast<const int32_t*>(data); break;
			case DATAID_ENIGINE4_THROTTLE: aircraftEngines_.throttle[3] = *static_cast<const int32_t*>(data); break;
			default: log_.error("[aircraftEngineDataItemCallback](): Unknown dataId ", dataDef.id); break;
			}
			return true;
		}, [=](int entryNr, int outOfNr, size_t size, const SimConnectDataDefinition* dataDef) {
			log_.trace("[aircraftEngineDataCompleteCallback](): Data block complete.");
			api_.sendData(aircraftEngines_);
			return true;
		});

		// Aircraft light data
		aircraftLightData_.clear();
		aircraftLightData_.add(DATAID_LIGHT_STROBE, "LIGHT STROBE", "Bool", SIMCONNECT_DATATYPE_INT32);
		aircraftLightData_.add(DATAID_LIGHT_LANDING, "LIGHT LANDING", "Bool", SIMCONNECT_DATATYPE_INT32);
		aircraftLightData_.add(DATAID_LIGHT_TAXI, "LIGHT TAXI", "Bool", SIMCONNECT_DATATYPE_INT32);
		aircraftLightData_.add(DATAID_LIGHT_BEACON, "LIGHT BEACON", "Bool", SIMCONNECT_DATATYPE_INT32);
		aircraftLightData_.add(DATAID_LIGHT_NAV, "LIGHT NAV", "Bool", SIMCONNECT_DATATYPE_INT32);
		aircraftLightData_.add(DATAID_LIGHT_LOGO, "LIGHT LOGO", "Bool", SIMCONNECT_DATATYPE_INT32);
		aircraftLightData_.add(DATAID_LIGHT_WING, "LIGHT WING", "Bool", SIMCONNECT_DATATYPE_INT32);
		aircraftLightData_.add(DATAID_LIGHT_RECOGNITION, "LIGHT RECOGNITION", "Bool", SIMCONNECT_DATATYPE_INT32);
		aircraftLightData_.add(DATAID_LIGHT_CABIN, "LIGHT CABIN", "Bool", SIMCONNECT_DATATYPE_INT32);
		mgr.defineData(aircraftLightData_, [=](const void* data, const SimConnectData& dataDef) {
			switch (dataDef.id) {
			case DATAID_LIGHT_STROBE: aircraftLights_.strobeLightOn = *static_cast<const int32_t*>(data) != 0; break;
			case DATAID_LIGHT_LANDING: aircraftLights_.landingLightOn = *static_cast<const int32_t*>(data) != 0; break;
			case DATAID_LIGHT_TAXI: aircraftLights_.taxiLightOn = *static_cast<const int32_t*>(data) != 0; break;
			case DATAID_LIGHT_BEACON: aircraftLights_.beaconLightOn = *static_cast<const int32_t*>(data) != 0; break;
			case DATAID_LIGHT_NAV: aircraftLights_.navLightOn = *static_cast<const int32_t*>(data) != 0; break;
			case DATAID_LIGHT_LOGO: aircraftLights_.logoLightOn = *static_cast<const int32_t*>(data) != 0; break;
			case DATAID_LIGHT_WING: aircraftLights_.wingLightOn = *static_cast<const int32_t*>(data) != 0; break;
			case DATAID_LIGHT_RECOGNITION: aircraftLights_.recogLightOn = *static_cast<const int32_t*>(data) != 0; break;
			case DATAID_LIGHT_CABIN: aircraftLights_.cabinLightOn = *static_cast<const int32_t*>(data) != 0; break;
			default: log_.error("[aircraftLightDataItemCallback](): Unknown dataId ", dataDef.id); break;
			}
			return true;
		}, [=](int entryNr, int outOfNr, size_t size, const SimConnectDataDefinition* dataDef) {
			log_.debug("[aircraftLightDataCompleteCallback](): reqId=", dataDef->reqId);

			api_.sendData(aircraftLights_);
			return true;
		});

		// Aircraft controls data
		aircraftControlsData_.clear();
		aircraftControlsData_.add(DATAID_CONTROL_RUDDER, "RUDDER PEDAL POSITION", "Position", SIMCONNECT_DATATYPE_FLOAT64);
		aircraftControlsData_.add(DATAID_CONTROL_ELEVATOR, "ELEVATOR POSITION", "Position", SIMCONNECT_DATATYPE_FLOAT64);
		aircraftControlsData_.add(DATAID_CONTROL_AILERON, "AILERON POSITION", "Position", SIMCONNECT_DATATYPE_FLOAT64);
		aircraftControlsData_.add(DATAID_CONTROL_TRIM_RUDDER, "RUDDER TRIM PCT", "Percent over 100", SIMCONNECT_DATATYPE_FLOAT64);
		aircraftControlsData_.add(DATAID_CONTROL_TRIM_ELEVATOR, "ELEVATOR TRIM PCT", "Percent over 100", SIMCONNECT_DATATYPE_FLOAT64);
		aircraftControlsData_.add(DATAID_CONTROL_TRIM_AILERON, "AILERON TRIM PCT", "Percent over 100", SIMCONNECT_DATATYPE_FLOAT64);
		aircraftControlsData_.add(DATAID_CONTROL_SPOILERS, "SPOILERS HANDLE POSITION", "Position", SIMCONNECT_DATATYPE_FLOAT64);
		aircraftControlsData_.add(DATAID_CONTROL_FLAPS_HANDLE, "FLAPS HANDLE PERCENT", "Percent over 100", SIMCONNECT_DATATYPE_FLOAT64);
		aircraftControlsData_.add(DATAID_CONTROL_PARKING_BRAKE, "BRAKE PARKING POSITION", "Position", SIMCONNECT_DATATYPE_FLOAT64);
		aircraftControlsData_.add(DATAID_CONTROL_PRIMARY_DOOR, "EXIT OPEN:1", "Percent over 100", SIMCONNECT_DATATYPE_FLOAT64);
		aircraftControlsData_.add(DATAID_CONTROL_GEARS_HANDLE, "GEAR HANDLE POSITION", "Bool", SIMCONNECT_DATATYPE_INT32);
		mgr.defineData(aircraftControlsData_, [=](const void* data, const SimConnectData& dataDef) {
			switch (dataDef.id) {
			case DATAID_CONTROL_RUDDER: aircraftControls_.rudderPos = *static_cast<const double*>(data); break;
			case DATAID_CONTROL_ELEVATOR: aircraftControls_.elevatorPos = *static_cast<const double*>(data); break;
			case DATAID_CONTROL_AILERON: aircraftControls_.aileronPos = *static_cast<const double*>(data); break;
			case DATAID_CONTROL_TRIM_RUDDER: aircraftControls_.rudderTrimPos = *static_cast<const double*>(data); break;
			case DATAID_CONTROL_TRIM_ELEVATOR: aircraftControls_.elevatorTrimPos = *static_cast<const double*>(data); break;
			case DATAID_CONTROL_TRIM_AILERON: aircraftControls_.aileronTrimPos = *static_cast<const double*>(data); break;
			case DATAID_CONTROL_SPOILERS: aircraftControls_.spoilersPos = *static_cast<const double*>(data); break;
			case DATAID_CONTROL_FLAPS_HANDLE: aircraftControls_.flapsPos = *static_cast<const double*>(data); break;
			case DATAID_CONTROL_PARKING_BRAKE: aircraftControls_.parkingBrakePos = *static_cast<const double*>(data); break;
			case DATAID_CONTROL_PRIMARY_DOOR: aircraftControls_.doorPos = *static_cast<const double*>(data); break;
			case DATAID_CONTROL_GEARS_HANDLE: aircraftControls_.gearsDown = *static_cast<const int32_t*>(data); break;
			default: log_.error("[aircraftControlsDataItemCallback](): Unknown dataId ", dataDef.id); break;
			}
			return true;
		}, [=](int entryNr, int outOfNr, size_t size, const SimConnectDataDefinition* dataDef) {
			log_.trace("[aircraftControlsDataCompleteCallback](): Data block complete.");
			api_.sendData(aircraftControls_);
			return true;
		});

		log_.debug("onFsxConnect(): Adding menu items");
		SimConnectManager::instance().addMenu("Connect to server", [=]() { connect(); });
		SimConnectManager::instance().addMenu("Disconnect from server", [=]() { disconnect(); });
	}
	log_.debug("onFsxConnect(): Done");
}

void MultiplayManager::onFsxDisconnect()
{
	log_.trace("onFsxDisconnect()");

	fsxConnected_ = false;

	onLocalStatusUpdate();
}

void MultiplayManager::onFsxStarted()
{
	log_.debug("onFsxStarted()");

	if (!fsxStarted_) {
		log_.debug("onFsxStarted(): Need to do some stuff");
		fsxStarted_ = true;

		if (apiConnected_) {
			doStartRequestingData();
		}
		onLocalStatusUpdate();
	}
}

void MultiplayManager::onFsxStopped()
{
	log_.debug("onFsxStopped()");

	doStopRequestingData();

	fsxStarted_ = false;

	onLocalStatusUpdate();
}

void MultiplayManager::onFsxPaused()
{
	log_.trace("onFsxPaused()");

	fsxPaused_ = true;

	onLocalStatusUpdate();
}

void MultiplayManager::onFsxUnpaused()
{
	log_.trace("onFsxUnpaused()");

	fsxPaused_ = false;

	onLocalStatusUpdate();
}

void MultiplayManager::onFsxAircraftLoaded()
{
	log_.debug("onFsxAircraftLoaded()");

	aircraftId_ = SimConnectManager::instance().getUserAircraftIdData();

	onLocalAircraftUpdate();
}

void MultiplayManager::onFsxObjectAdded()
{
	log_.debug("onFsxObjectAdded(): fsxConnected=", (fsxConnected_?"true":"false"), ", apiConnected=", (fsxConnected_ ? "true" : "false"));
	if (fsxConnected_ && apiConnected_) {
		log_.debug("onFsxObjectAdded(): Re-requesting data");
		SimConnectManager::instance().requestSimDataWhenChanged(aircraftLocationData_);
	}
}

void MultiplayManager::onFsxObjectRemoved()
{
	log_.debug("onFsxObjectRemoved()");
	if (fsxConnected_ && apiConnected_) {
		SimConnectManager::instance().requestSimDataWhenChanged(aircraftLocationData_);
	}
}


void MultiplayManager::connect()
{
	log_.debug("connect()");

	if (fsxConnected_ && fsxStarted_) {
		doStartRequestingData();
		api_.login();
	}
	log_.debug("connect(): Done");
}

void MultiplayManager::disconnect()
{
	log_.debug("disconnect()");

	if (api_.isLoggedIn()) {
		doStopRequestingData();
		api_.logout();
	}
	log_.debug("disconnect(): Done");
}
