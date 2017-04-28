/**
*/
#include "stdafx.h"

#include <strstream>

#include "../LibFsx/SimConnectManager.h"
#include "../LibMultiplay/MultiplayManager.h"

#include "AIManager.h"

using namespace std;

using namespace boost::property_tree;

using namespace nl::rakis::service;
using namespace nl::rakis::fsx;
using namespace nl::rakis;

enum {
	DATAID_LATITUDE = 1,
	DATAID_LONGITUDE,
	DATAID_ALTITUDE,
	DATAID_PITCH,
	DATAID_BANK,
	DATAID_HEADING,
	DATAID_SIM_ON_GROUND,
	DATAID_TAS
};


/*static*/ AircraftIdData AIManager::NO_AIRCRAFT{
	"NOFILE.AIR",
	"Nothing",
	"Nothing",
	"",
	"",
	"",
	"No aircraft"
};

/*static*/ Logger AIManager::log_(Logger::getLogger("nl.rakis.fsx.AIManager"));


AIManager::AIManager()
	: Service("AIManager")
	, lightStrobe_("STROBES_SET", true)
	, lightLanding_("LANDING_LIGHTS_SET", true)
	, lightTaxi_("TOGGLE_TAXI_LIGHTS")
	, lightBeacon_("TOGGLE_BEACON_LIGHTS")
	, lightNavigation_("TOGGLE_NAV_LIGHTS")
	, lightLogo_("TOGGLE_LOGO_LIGHTS")
	, lightWing_("TOGGLE_WING_LIGHTS")
	, lightRecognition_("TOGGLE_RECOGNITION_LIGHTS")
	, lightCabin_("TOGGLE_CABIN_LIGHTS")

	, ctrlRudder_("RUDDER_SET", true)
	, ctrlElevator_("ELEVATOR_SET", true)
	, ctrlAileron_("AILERON_SET", true)
	, ctrlRudderTrim_("RUDDER_TRIM_SET", true)
	, ctrlElevatorTrim_("ELEVATOR_TRIM_SET", true)
	, ctrlAileronTrim_("AILERON_TRIM_SET", true)
	, ctrlFlaps_("FLAPS_SET", true)
	, ctrlSpoilers_("SPOILERS_SET", true)
	, ctrlGear_("GEAR_SET", true)
	, ctrlDoor1_("TOGGLE_AIRCRAFT_EXIT", false)
{
	log_.debug("AIManager()");

	setAutoStart(true);

	ostringstream buf;
	buf << aiAircraft_.size();
	setStatus("aiAircraftCount", buf.str());

	log_.debug("AIManager(): Done");
}

AIManager::~AIManager()
{
	log_.debug("~AIManager()");
}

AIManager& AIManager::instance()
{
	static AIManager instance_;

	return instance_;
}

void AIManager::setAIAircraft(const std::wstring& callsign, const AircraftIdData& aiAircraft, bool setUpdated)
{
	AIAircraft& ac(aiAircraft_[callsign]);
	ac.setAircraft(aiAircraft);
	if (!setUpdated) {
		ac.resetAircraftFlag();
	}
}

void AIManager::doStart()
{
	log_.debug("doStart()");
	SimConnectManager::instance().addSimConnectEventHandler("AIManager", [=](SimConnectEvent evt, DWORD arg1, const char* arg2) {
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
	});
	log_.debug("doStart(): Done");
}

void AIManager::doStop()
{
	log_.debug("doStop()");

	ptree aiAircraftTree;
	SettingsManager::instance().setTree("aiAircrafts", aiAircraftTree);

	log_.debug("doStop(): Done");
}

void AIManager::flushAIAircraft()
{
	log_.debug("flushAIAircraft()");

	removeAll();

	log_.debug("flushAIAircraft(): Done");
}

void AIManager::create(const std::wstring& callsign, const FullAircraftData& aircraft)
{
	log_.debug("create(): callsign=\"", callsign, "\"");
	for (auto aiIt = aiAircraft_.begin(); aiIt != aiAircraft_.end(); aiIt++) {
		log_.debug("create(): Already have \"", aiIt->first, "\" (title=\"", aiIt->second.getAircraft().title, "\")");
	}

	auto it = aiAircraft_.find(callsign);

	if ((it == aiAircraft_.end()) || (it->second.getAircraft() == NO_AIRCRAFT)) {
		string id(wstr2str(callsign));
		AIAircraft& ac(aiAircraft_[callsign]); // Creates it if not yet there (it == aiAircraft_.end())

		ac.setAircraft(aircraft.aircraft);
		ac.setLocation(aircraft.location);
		ac.setEngines(aircraft.engines);
		ac.setEnginesFlag();
		ac.setLights(aircraft.lights);
		ac.setControls(aircraft.controls);
		ac.setControlsFlag();

		SimConnectManager::instance().createAIAircraft(ac.getAircraft().title, id, ac.getLocation(), [=,&ac](SIMCONNECT_OBJECT_ID objId) {
			log_.debug("create[ObjectIdAssignedCallback](): received objId ", objId);
			ac.setObjId(objId);
			updateEngines(ac);
			updateLights(ac);
			updateControls(ac);
		});

		ac.resetAircraftFlag();
		ac.resetInNeedOfUpdate();
		ac.resetLocationFlag();
	}
	else {
		log_.warn("create(): Already have a \"", callsign, "\", not creating a duplicate");
	}
	log_.debug("create(): Done");
}

void AIManager::create(const std::wstring& callsign, const AircraftIdData& aircraft, const AircraftLocationData& location)
{
	log_.debug("create(): callsign=\"", callsign, "\"");
	for (auto aiIt = aiAircraft_.begin(); aiIt != aiAircraft_.end(); aiIt++) {
		log_.debug("create(): Already have \"", aiIt->first, "\" (title=\"", aiIt->second.getAircraft().title, "\")");
	}

	auto it = aiAircraft_.find(callsign);

	if ((it == aiAircraft_.end()) || (it->second.getAircraft() == NO_AIRCRAFT)) {
		string id(wstr2str(callsign));
		AIAircraft& ac(aiAircraft_[callsign]); // Creates it if not yet there (it == aiAircraft_.end())
		ac.setAircraft(aircraft);
		ac.setLocation(location);

		SimConnectManager::instance().createAIAircraft(ac.getAircraft().title, id, ac.getLocation(), [&ac](SIMCONNECT_OBJECT_ID objId) {
			log_.debug("create[ObjectIdAssignedCallback](): received objId ", objId);
			ac.setObjId(objId);
		});

		ac.resetAircraftFlag();
		ac.resetInNeedOfUpdate();
		ac.resetLocationFlag();
	}
	else {
		log_.warn("create(): Already have a \"", callsign, "\", not creating a duplicate");
	}
	log_.debug("create(): Done");
}

void AIManager::remove(const std::wstring& callsign)
{
	log_.debug("remove(): callsign=\"", callsign, "\"");

	auto it = aiAircraft_.find(callsign);

	if (it == aiAircraft_.end()) {
		log_.error("remove(): Don't know any \"", callsign, "\"");
	}
	else {
		const SIMCONNECT_OBJECT_ID objId(it->second.getObjId());
		aiAircraft_.erase(it);

		log_.debug("remove(): Asking for \"", callsign, "\" to be removed (objId=", objId, ")");
		SimConnectManager::instance().removeAIAircraft(objId);
	}

	log_.debug("remove(): Done");
}

void AIManager::removeAll()
{
	log_.debug("removeAll()");

	vector<SIMCONNECT_OBJECT_ID> ids;
	auto it = aiAircraft_.begin();
	while (it != aiAircraft_.end()) {
		ids.emplace_back(it->second.getObjId());
		it++;
	}
	aiAircraft_.clear();

	for (unsigned i = 0; i < ids.size(); i++) {
		log_.debug("removeAll(): Asking for objId ", ids [i], " to be removed");
		SimConnectManager::instance().removeAIAircraft(ids [i]);
	}

	log_.debug("removeAll(): Done");
}

void AIManager::collectAircraftInNeedOfUpdate(std::vector<std::wstring>& aircraftList)
{
	for (auto it = aiAircraft_.begin(); it != aiAircraft_.end(); it++) {
		if (it->second.isInNeedOfUpdate()) {
			aircraftList.push_back(it->first);
		}
	}
}

void AIManager::updateLocation(const std::wstring& callsign, const AircraftLocationData& location)
{
	log_.debug("updateLocation()");

	auto it = aiAircraft_.find(callsign);
	if (it != aiAircraft_.end()) {
		AIAircraft& ac(it->second);

		ac.setLocation(location);

// TODO what when we have a change in an existing aircraft
//		if (ac.isAircraftUpdated()) {
//			create(callsign, ac);
//			ac.resetAircraftFlag();
//		}
//		else 
		if (ac.isCreated() && ac.isLocationUpdated()) {
			SimConnectManager::instance().sendSimData(
				ac.getObjId(), MultiplayManager::instance().getLocationData().getDefId(),
				&(ac.getLocation()), sizeof(SIMCONNECT_DATA_INITPOSITION));
			ac.resetLocationFlag();
		}
	}

	log_.debug("updateLocation(): Done");
}

void AIManager::updateLights(AIAircraft& ac)
{
	log_.debug("updateLights(AIAircraft&)");

	if (ac.isCreated()) {
		SimConnectManager& mgr(SimConnectManager::instance());

		while (ac.isCreated() && ac.isLightsUpdated()) {
			auto evt(ac.popLightEvent());
			switch (evt.first) {
			case LIGHT_STROBE:
				mgr.sendEvent(ac.getObjId(), lightStrobe_, evt.second ? 1 : 0); break;
			case LIGHT_LANDING:
				mgr.sendEvent(ac.getObjId(), lightLanding_, evt.second ? 1 : 0); break;
			case LIGHT_TAXI:
				mgr.sendEvent(ac.getObjId(), lightTaxi_, evt.second ? 1 : 0); break;
			case LIGHT_BEACON:
				mgr.sendEvent(ac.getObjId(), lightBeacon_, evt.second ? 1 : 0); break;
			case LIGHT_NAVIGATION:
				mgr.sendEvent(ac.getObjId(), lightNavigation_, evt.second ? 1 : 0); break;
			case LIGHT_LOGO:
				mgr.sendEvent(ac.getObjId(), lightLogo_, evt.second ? 1 : 0); break;
			case LIGHT_WING:
				mgr.sendEvent(ac.getObjId(), lightWing_, evt.second ? 1 : 0); break;
			case LIGHT_RECOGNITION:
				mgr.sendEvent(ac.getObjId(), lightRecognition_, evt.second ? 1 : 0); break;
			case LIGHT_CABIN:
				mgr.sendEvent(ac.getObjId(), lightCabin_, evt.second ? 1 : 0); break;
			default:
				break;
			}
		}
	}

	log_.trace("updateLights(): Done");
}

void AIManager::updateLights(const std::wstring& callsign)
{
	log_.debug("updateLights(\"", callsign, "\")");

	auto it = aiAircraft_.find(callsign);
	if (it != aiAircraft_.end()) {
		updateLights(it->second);
	}

	log_.debug("updateLights(): Done");
}

void AIManager::updateEngines(AIAircraft& ac, bool force)
{
	log_.trace("updateEngines(AIAircraft&)");

	if (ac.isCreated() && (force || ac.areEnginesUpdated())) {
		SimConnectManager::instance().sendSimData(ac.getObjId(), MultiplayManager::instance().getEngineData().getDefId(), &ac.getEngines(), sizeof(AircraftEngineData));
		ac.resetEnginesFlag();
	}
	log_.trace("updateEngines(): Done");
}

void AIManager::updateEngines(const std::wstring& callsign, const AircraftEngineData& data)
{
	log_.trace("updateEngines(\"", callsign, "\")");

	auto it = aiAircraft_.find(callsign);
	if (it != aiAircraft_.end()) {
		AIAircraft& ac(it->second);

		ac.setEngines(data);

		updateEngines(ac);
	}

	log_.trace("updateEngines(): Done");
}

void AIManager::updateLights(const std::wstring& callsign, const AircraftLightsData& data)
{
	log_.debug("updateLights(\"", callsign, "\")");

	auto it = aiAircraft_.find(callsign);
	if (it != aiAircraft_.end()) {
		AIAircraft& ac(it->second);

		ac.setLights(data);

		updateLights(ac);
	}

	log_.debug("updateLights(): Done");
}

static void sendIfUpdated(SimConnectManager& mgr, SIMCONNECT_OBJECT_ID objId, double& last, double val, FsxEvent& event)
{
	if (val != last) {
		mgr.sendEvent(objId, event, int(round(16383 * -val)));
		last = val;
	}
}

static void sendIfUpdated(SimConnectManager& mgr, SIMCONNECT_OBJECT_ID objId, int32_t& last, int32_t val, FsxEvent& event)
{
	if (val != last) {
		mgr.sendEvent(objId, event, val);
		last = val;
	}
}

void AIManager::updateControls(AIAircraft& ac)
{
	log_.debug("updateControls(AIAircraft&)");

	if (ac.isCreated() && ac.areControlsUpdated()) {
		SimConnectManager& mgr(SimConnectManager::instance());

		const SIMCONNECT_OBJECT_ID objId(ac.getObjId());
		const AircraftControlsData& ctrls(ac.getControls());
		AircraftControlsData& last(ac.getLastControls());

		sendIfUpdated(mgr, objId, last.rudderPos, ctrls.rudderPos, ctrlRudder_);
		sendIfUpdated(mgr, objId, last.elevatorPos, ctrls.elevatorPos, ctrlElevator_);
		sendIfUpdated(mgr, objId, last.aileronPos, ctrls.aileronPos, ctrlAileron_);
		sendIfUpdated(mgr, objId, last.rudderTrimPos, ctrls.rudderTrimPos, ctrlRudderTrim_);
		sendIfUpdated(mgr, objId, last.elevatorTrimPos, ctrls.elevatorTrimPos, ctrlElevatorTrim_);
		sendIfUpdated(mgr, objId, last.aileronTrimPos, ctrls.aileronTrimPos, ctrlAileronTrim_);
		sendIfUpdated(mgr, objId, last.flapsPos, ctrls.flapsPos, ctrlFlaps_);
		sendIfUpdated(mgr, objId, last.spoilersPos, ctrls.spoilersPos, ctrlSpoilers_);
		sendIfUpdated(mgr, objId, last.gearsDown, ctrls.gearsDown, ctrlGear_);
		if (abs(ctrls.doorPos - last.doorPos) > 0.5) {
			log_.debug("updateControls(AIAircraft&): last.doorPos=", last.doorPos, ", ctrls.doorPos=", ctrls.doorPos);
			mgr.sendEvent(objId, ctrlDoor1_);
			last.doorPos = ctrls.doorPos;
		}

		ac.resetControlsFlag();
	}
	log_.trace("updateControls(): Done");
}

void AIManager::updateControls(const std::wstring& callsign, const AircraftControlsData& data)
{
	log_.trace("updateControls(\"", callsign, "\")");

	auto it = aiAircraft_.find(callsign);
	if (it != aiAircraft_.end()) {
		AIAircraft& ac(it->second);

		ac.setControls(data);

		updateControls(ac);
	}

	log_.trace("updateControls(): Done");
}


// FSX event handlers
void AIManager::onFsxConnect()
{
	log_.trace("onFsxConnect()");
	SimConnectManager& mgr(SimConnectManager::instance());

	fsxConnected_ = true;

	mgr.defineEvent(lightStrobe_);
	mgr.defineEvent(lightLanding_);
	mgr.defineEvent(lightTaxi_);
	mgr.defineEvent(lightBeacon_);
	mgr.defineEvent(lightNavigation_);
	mgr.defineEvent(lightLogo_);
	mgr.defineEvent(lightWing_);
	mgr.defineEvent(lightRecognition_);
	mgr.defineEvent(lightCabin_);

	mgr.defineEvent(ctrlRudder_);
	mgr.defineEvent(ctrlElevator_);
	mgr.defineEvent(ctrlAileron_);
	mgr.defineEvent(ctrlRudderTrim_);
	mgr.defineEvent(ctrlElevatorTrim_);
	mgr.defineEvent(ctrlAileronTrim_);
	mgr.defineEvent(ctrlFlaps_);
	mgr.defineEvent(ctrlSpoilers_);
	mgr.defineEvent(ctrlGear_);
	mgr.defineEvent(ctrlDoor1_);

	log_.trace("onFsxConnect(): Done");
}

void AIManager::onFsxDisconnect()
{
	log_.trace("onFsxDisconnect()");

	fsxConnected_ = false;
}

void AIManager::onFsxStarted()
{
	log_.debug("onFsxStarted()");

	fsxStarted_ = true;
}

void AIManager::onFsxStopped()
{
	log_.debug("onFsxStopped()");

	fsxStarted_ = false;
}

void AIManager::onFsxPaused()
{
	log_.trace("onFsxPaused()");

	fsxPaused_ = true;
}

void AIManager::onFsxUnpaused()
{
	log_.trace("onFsxUnpaused()");

	fsxPaused_ = false;
}

void AIManager::onFsxAircraftLoaded()
{
	log_.trace("onFsxAircraftLoaded()");
}

void AIManager::onFsxObjectAdded()
{
	log_.debug("onFsxObjectAdded(): fsxConnected=", (fsxConnected_ ? "true" : "false"), ", apiConnected=", (fsxConnected_ ? "true" : "false"));
}

void AIManager::onFsxObjectRemoved()
{
	log_.debug("onFsxObjectRemoved()");
}

