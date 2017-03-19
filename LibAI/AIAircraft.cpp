/**
*/
#include "stdafx.h"

#include <fstream>

#include "AIAircraft.h"

using namespace std;

using namespace nl::rakis;
using namespace nl::rakis::fsx;


/*static*/ Logger AIAircraft::log_(Logger::getLogger("nl.rakis.fsx.AIAircraft"));

AIAircraft::AIAircraft()
	: name_(L""), objId_(0), aircraftUpdated_(false), locationUpdated_(false)
{
	for (size_t i = 0; i < 4; i++) {
		engines_.engineOn[i] = 0;
		engines_.throttle[i] = 0;
	}
	lights_.beaconLightOn = false;
	lights_.cabinLightOn = false;
	lights_.landingLightOn = false;
	lights_.logoLightOn = false;
	lights_.navLightOn = false;
	lights_.recogLightOn = false;
	lights_.strobeLightOn = false;
	lights_.taxiLightOn = false;
	lights_.wingLightOn = false;
	controls_.aileronPos = 0;
	controls_.aileronTrimPos = 0;
	controls_.elevatorPos = 0;
	controls_.elevatorTrimPos = 0;
	controls_.rudderPos = 0;
	controls_.rudderTrimPos = 0;
	controls_.doorPos = 0;
	controls_.flapsPos = 0;
	controls_.parkingBrakePos = 0;
	controls_.gearsDown = false;
	lastControls_ = controls_;
}

AIAircraft::AIAircraft(wstring const& name)
	: name_(name), objId_(0), aircraftUpdated_(false), locationUpdated_(false)
{
	for (size_t i = 0; i < 4; i++) {
		engines_.engineOn[i] = 0;
		engines_.throttle[i] = 0;
	}
	lights_.beaconLightOn = false;
	lights_.cabinLightOn = false;
	lights_.landingLightOn = false;
	lights_.logoLightOn = false;
	lights_.navLightOn = false;
	lights_.recogLightOn = false;
	lights_.strobeLightOn = false;
	lights_.taxiLightOn = false;
	lights_.wingLightOn = false;
	controls_.aileronPos = 0;
	controls_.aileronTrimPos = 0;
	controls_.elevatorPos = 0;
	controls_.elevatorTrimPos = 0;
	controls_.rudderPos = 0;
	controls_.rudderTrimPos = 0;
	controls_.doorPos = 0;
	controls_.flapsPos = 0;
	controls_.parkingBrakePos = 0;
	controls_.gearsDown = false;
	lastControls_ = controls_;
}

AIAircraft::~AIAircraft()
{
}

void AIAircraft::setLocation(const AircraftLocationData& location)
{
	if (location_ != location) {
		location_ = location;
		locationUpdated_ = true;
	}
}

void AIAircraft::setLight(AircraftLightCode light, bool onOff)
{
	log_.trace("setLight(): light=", light, ", onOff=", (onOff ? "true" : "false"));
	switch (light) {
	case LIGHT_STROBE:
	{
		if (lights_.strobeLightOn != onOff) {
			log_.debug("setLight(): STROBES set to ", (onOff ? "true" : "false"));
			lights_.strobeLightOn = onOff;
			lightEvents_.emplace_back(light, onOff);
		}
		break;
	}

	case LIGHT_LANDING:
	{
		if (lights_.landingLightOn != onOff) {
			log_.debug("setLight(): LANDING LIGHTS set to ", (onOff ? "true" : "false"));
			lights_.landingLightOn = onOff;
			lightEvents_.emplace_back(light, onOff);
		}
		break;
	}

	case LIGHT_TAXI:
	{
		if (lights_.taxiLightOn != onOff) {
			log_.debug("setLight(): TAXI LIGHTS set to ", (onOff ? "true" : "false"));
			lights_.taxiLightOn = onOff;
			lightEvents_.emplace_back(light, onOff);
		}
		break;
	}

	case LIGHT_BEACON:
	{
		if (lights_.beaconLightOn != onOff) {
			log_.debug("setLight(): BEACON LIGHTS set to ", (onOff ? "true" : "false"));
			lights_.beaconLightOn = onOff;
			lightEvents_.emplace_back(light, onOff);
		}
		break;
	}

	case LIGHT_NAVIGATION:
	{
		if (lights_.navLightOn != onOff) {
			log_.debug("setLight(): NAV LIGHTS set to ", (onOff ? "true" : "false"));
			lights_.navLightOn = onOff;
			lightEvents_.emplace_back(light, onOff);
		}
		break;
	}

	case LIGHT_LOGO:
	{
		if (lights_.logoLightOn != onOff) {
			log_.debug("setLight(): LOGO LIGHTS set to ", (onOff ? "true" : "false"));
			lights_.logoLightOn = onOff;
			lightEvents_.emplace_back(light, onOff);
		}
		break;
	}

	case LIGHT_WING:
	{
		if (lights_.wingLightOn != onOff) {
			log_.debug("setLight(): WING LIGHTS set to ", (onOff ? "true" : "false"));
			lights_.wingLightOn = onOff;
			lightEvents_.emplace_back(light, onOff);
		}
		break;
	}

	case LIGHT_RECOGNITION:
	{
		if (lights_.recogLightOn != onOff) {
			log_.debug("setLight(): RECOGNITION LIGHTS set to ", (onOff ? "true" : "false"));
			lights_.recogLightOn = onOff;
			lightEvents_.emplace_back(light, onOff);
		}
		break;
	}

	case LIGHT_CABIN:
	{
		if (lights_.cabinLightOn != onOff) {
			log_.debug("setLight(): CABIN LIGHTS set to ", (onOff ? "true" : "false"));
			lights_.cabinLightOn = onOff;
			lightEvents_.emplace_back(light, onOff);
		}
		break;
	}

	default:
		break;
	}
}

void AIAircraft::setLights(const AircraftLightsData& lights)
{
	setLight(LIGHT_STROBE, lights.strobeLightOn);
	setLight(LIGHT_LANDING, lights.landingLightOn);
	setLight(LIGHT_TAXI, lights.taxiLightOn);
	setLight(LIGHT_BEACON, lights.beaconLightOn);
	setLight(LIGHT_NAVIGATION, lights.navLightOn);
	setLight(LIGHT_LOGO, lights.logoLightOn);
	setLight(LIGHT_WING, lights.wingLightOn);
	setLight(LIGHT_RECOGNITION, lights.recogLightOn);
	setLight(LIGHT_CABIN, lights.cabinLightOn);
}

void AIAircraft::setEngines(const AircraftEngineData& engines)
{
	enginesUpdated_ = enginesUpdated_
		|| (engines_.engineOn[0] != engines.engineOn[0])
		|| (engines_.engineOn[1] != engines.engineOn[1])
		|| (engines_.engineOn[2] != engines.engineOn[2])
		|| (engines_.engineOn[3] != engines.engineOn[3])
		|| (engines_.throttle[0] != engines.throttle[0])
		|| (engines_.throttle[1] != engines.throttle[1])
		|| (engines_.throttle[2] != engines.throttle[2])
		|| (engines_.throttle[3] != engines.throttle[3]);
	engines_ = engines;
}

void AIAircraft::setControls(const AircraftControlsData& controls)
{
	controlsUpdated_ = controlsUpdated_
		|| (controls.aileronPos != controls_.aileronPos)
		|| (controls.aileronTrimPos != controls_.aileronTrimPos)
		|| (controls.elevatorPos != controls_.elevatorPos)
		|| (controls.elevatorTrimPos != controls_.elevatorTrimPos)
		|| (controls.rudderPos != controls_.rudderPos)
		|| (controls.rudderTrimPos != controls_.rudderTrimPos)
		|| (controls.spoilersPos != controls_.spoilersPos)
		|| (controls.flapsPos != controls_.flapsPos)
//		|| (controls.gearsDown != controls_.gearsDown)
		|| (controls.doorPos != controls_.doorPos)
		|| (controls.parkingBrakePos != controls_.parkingBrakePos);
	controls_ = controls;
}