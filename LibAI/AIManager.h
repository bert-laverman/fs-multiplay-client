/**
*/
#ifndef __NL_RAKIS_FS_AIMANAGER_H
#define __NL_RAKIS_FS_AIMANAGER_H

#include <map>
#include <vector>
#include <string>
#include <memory>
#include <functional>

#include "../LibRakis/File.h"
#include "../LibRakis/Log.h"

#include "../LibService/Service.h"
#include "../LibService/SettingsManager.h"
#include "../LibFsx/FsxData.h"
#include "../LibFsx/FsxEvent.h"

#include "AIAircraft.h"

namespace nl {
namespace rakis {
namespace fsx {

	class AIManager
		: public virtual nl::rakis::service::Service
	{
	public: // Types and constants
		typedef std::map<std::wstring, AIAircraft> AIMap;

		typedef std::function<void(const AIAircraft& aircraft)> AICallback;

		static AircraftIdData NO_AIRCRAFT;

		static AIManager& instance();

	private: // Members
		bool                          fsxConnected_;
		bool                          fsxStarted_;
		bool                          fsxPaused_;

		AIMap                 aiAircraft_;
		std::vector<AICallback> addCallbacks_;
		std::vector<AICallback> removeCallbacks_;

		// Light Events
		FsxEvent lightStrobe_;
		FsxEvent lightLanding_;
		FsxEvent lightTaxi_;
		FsxEvent lightBeacon_;
		FsxEvent lightNavigation_;
		FsxEvent lightLogo_;
		FsxEvent lightWing_;
		FsxEvent lightRecognition_;
		FsxEvent lightCabin_;

		// Control Events
		FsxEvent ctrlRudder_;
		FsxEvent ctrlElevator_;
		FsxEvent ctrlAileron_;
		FsxEvent ctrlRudderTrim_;
		FsxEvent ctrlElevatorTrim_;
		FsxEvent ctrlAileronTrim_;
		FsxEvent ctrlFlaps_;
		FsxEvent ctrlSpoilers_;
		FsxEvent ctrlGear_;
		FsxEvent ctrlDoor1_;

		// Helper methods
		void updateEngines(AIAircraft& ac, bool force =false);
		void updateLights(AIAircraft& ac);
		void updateControls(AIAircraft& ac);

		// FSX event handlers
		void onFsxConnect();
		void onFsxDisconnect();
		void onFsxStarted();
		void onFsxStopped();
		void onFsxPaused();
		void onFsxUnpaused();
		void onFsxAircraftLoaded();
		void onFsxObjectAdded();
		void onFsxObjectRemoved();

		static Logger log_;

	public: // Methods
		AIManager();
		virtual ~AIManager();

		virtual void doStart();
		virtual void doStop();

		void flushAIAircraft();

		void setAIAircraft(const std::wstring& callsign, const AircraftIdData& aiAircraft, bool setUpdated = true);
		void removeAIAircraft(std::wstring const& callsign) { auto aiAircraft = aiAircraft_.find(callsign); if (aiAircraft != aiAircraft_.end()) { aiAircraft_.erase(aiAircraft); } }
		bool haveAIAircraft(std::wstring const& callsign) const { auto it = aiAircraft_.find(callsign); return (it != aiAircraft_.end()); };
		const AircraftIdData& getAIAircraft(std::wstring const& callsign) const { auto it = aiAircraft_.find(callsign); return (it != aiAircraft_.end()) ? it->second.getAircraft() : NO_AIRCRAFT; };
		void foreachAIAircraft(std::function<void(const AIAircraft&)> doIt) { for (AIMap::const_iterator it = aiAircraft_.begin(); it != aiAircraft_.end(); it++) { doIt(it->second); } };

		void create(const std::wstring& callsign, const FullAircraftData& aircraft);
		void create(const std::wstring& callsign, const AircraftIdData& aircraft, const AircraftLocationData& location);
		void remove(const std::wstring& callsign);
		void removeAll();

		void collectAircraftInNeedOfUpdate(std::vector<std::wstring>& aircraftList);

		void updateLocation(const std::wstring& callsign, const AircraftLocationData& aircraft);
		void updateEngines(const std::wstring& callsign, const AircraftEngineData& data);
		void updateLights(const std::wstring& callsign);
		void updateLights(const std::wstring& callsign, const AircraftLightsData& data);
		void updateControls(const std::wstring& callsign, const AircraftControlsData& data);

		void addAIAircraftAddedListener(AICallback callback) { addCallbacks_.emplace_back(callback); }
		void addAIAircraftRemovedListener(AICallback callback) { removeCallbacks_.emplace_back(callback); }

	private:
		AIManager(AIManager const&)  =delete;
		AIManager& operator=(AIManager const&) =delete;
	};

} // namespace fsx
} // namespace rakis
} // namespace nl

#endif

