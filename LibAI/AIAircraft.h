/**
*/
#ifndef __NL_RAKIS_FSX_AIAIRCRAFT_H
#define __NL_RAKIS_FSX_AIAIRCRAFT_H

#include <string>
#include <deque>

#include "../LibRakis/Log.h"

#include "../LibFsx/SimConnectTypes.h"

namespace nl {
namespace rakis {
namespace fsx {

	enum AircraftLightCode {
		LIGHT_STROBE = 1,
		LIGHT_LANDING,
		LIGHT_TAXI,
		LIGHT_BEACON,
		LIGHT_NAVIGATION,
		LIGHT_LOGO,
		LIGHT_WING,
		LIGHT_RECOGNITION,
		LIGHT_CABIN,
	};

	class AIAircraft
	{
	public: // Types & constants
		typedef std::deque<std::pair<AircraftLightCode, bool>> LightEventQueue;

	private: // Members
		std::wstring          name_;
		SIMCONNECT_OBJECT_ID objId_;

		bool                 inNeedOfUpdate_;
		AircraftIdData       aircraft_;
		bool                 aircraftUpdated_;
		AircraftLocationData location_;
		bool                 locationUpdated_;
		AircraftEngineData   engines_;
		bool                 enginesUpdated_;
		AircraftLightsData   lights_;
		LightEventQueue      lightEvents_;
		AircraftControlsData controls_;
		AircraftControlsData lastControls_;
		bool                 controlsUpdated_;


		static ::nl::rakis::Logger log_;

	public: // Methods
		AIAircraft();
		AIAircraft(const std::wstring& name);
		~AIAircraft();

		std::wstring const& getName() const { return name_; };
		void setName(std::wstring const& name) { name_ = name; };
		SIMCONNECT_OBJECT_ID getObjId() const { return objId_; }
		void setObjId(SIMCONNECT_OBJECT_ID objId) { objId_ = objId; }
		bool isCreated() const { return objId_ != 0; }
		void setInNeedOfUpdate() { inNeedOfUpdate_ = true; }
		void resetInNeedOfUpdate() { inNeedOfUpdate_ = false; }
		bool isInNeedOfUpdate() const { return inNeedOfUpdate_; }

		const AircraftIdData& getAircraft() const { return aircraft_; }
		void setAircraft(const AircraftIdData& aircraft) { aircraft_ = aircraft; aircraftUpdated_ = true; }
		bool isAircraftUpdated() const { return aircraftUpdated_; }
		void resetAircraftFlag() { aircraftUpdated_ = false; }

		const AircraftLocationData& getLocation() const { return location_; }
		void setLocation(const AircraftLocationData& location);
		bool isLocationUpdated() const { return locationUpdated_; }
		void setLocationFlag() { locationUpdated_ = true; }
		void resetLocationFlag() { locationUpdated_ = false; }

		const AircraftLightsData& getLights() const { return lights_; }
		void setLight(AircraftLightCode light, bool onOff =true);
		void setLights(const AircraftLightsData& lights);
		std::pair<AircraftLightCode, bool> popLightEvent() { auto result = lightEvents_.front(); lightEvents_.pop_front(); return result; }
		bool isLightsUpdated() const { return !lightEvents_.empty(); }

		const AircraftEngineData& getEngines() const { return engines_; }
		void setEngines(const AircraftEngineData& engines);
		bool areEnginesUpdated() const { return enginesUpdated_; }
		void setEnginesFlag() { enginesUpdated_ = true; }
		void resetEnginesFlag() { enginesUpdated_ = false; }

		const AircraftControlsData& getControls() const { return controls_; }
		AircraftControlsData& getLastControls() { return lastControls_; }
		void setControls(const AircraftControlsData& controls);
		bool areControlsUpdated() const { return controlsUpdated_; }
		void setControlsFlag() { controlsUpdated_ = true; }
		void resetControlsFlag() { controlsUpdated_ = false; }

	private:
	};

} // namespace fsx
} // namespace rakis
} // namespace nl

#endif
