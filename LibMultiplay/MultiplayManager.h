/**
*/
#ifndef __NL_RAKIS_FS_MULTIPLAYMANAGER_H
#define __NL_RAKIS_FS_MULTIPLAYMANAGER_H

#include <chrono>
#include <map>
#include <vector>
#include <string>
#include <memory>
#include <functional>

#include <cpprest/http_client.h>
#include <cpprest/ws_client.h>

#include "../LibFsx/FsxData.h"
#include "../LibFsx/SimConnectManager.h"

#include "../LibRakis/File.h"
#include "../LibRakis/Log.h"

#include "../LibService/Service.h"
#include "../LibService/SettingsManager.h"

#include "MultiplayAPIHandler.h"

namespace nl {
namespace rakis {
namespace fsx {

	typedef enum {
		DATAID_LATITUDE = 1,
		DATAID_LONGITUDE,
		DATAID_ALTITUDE,
		DATAID_PITCH,
		DATAID_BANK,
		DATAID_HEADING,
		DATAID_SIM_ON_GROUND,
		DATAID_TAS,
	} AircraftLocationDataId;
	typedef enum {
		DATAID_ENIGINE1_COMBUSTION = 1,
		DATAID_ENIGINE2_COMBUSTION,
		DATAID_ENIGINE3_COMBUSTION,
		DATAID_ENIGINE4_COMBUSTION,
		DATAID_ENIGINE1_THROTTLE,
		DATAID_ENIGINE2_THROTTLE,
		DATAID_ENIGINE3_THROTTLE,
		DATAID_ENIGINE4_THROTTLE,
	} AircraftEngineDataId;
	typedef enum {
		DATAID_LIGHT_STROBE = 1,
		DATAID_LIGHT_LANDING,
		DATAID_LIGHT_TAXI,
		DATAID_LIGHT_BEACON,
		DATAID_LIGHT_NAV,
		DATAID_LIGHT_LOGO,
		DATAID_LIGHT_WING,
		DATAID_LIGHT_RECOGNITION,
		DATAID_LIGHT_CABIN,
		DATAID_LIGHT_ENIGINE1_COMBUSTION,
	} AircraftLightDataId;
	typedef enum {
		DATAID_CONTROL_RUDDER = 1,
		DATAID_CONTROL_ELEVATOR,
		DATAID_CONTROL_AILERON,
		DATAID_CONTROL_TRIM_RUDDER,
		DATAID_CONTROL_TRIM_ELEVATOR,
		DATAID_CONTROL_TRIM_AILERON,
		DATAID_CONTROL_SPOILERS,
		DATAID_CONTROL_FLAPS_HANDLE,
		DATAID_CONTROL_PARKING_BRAKE,
		DATAID_CONTROL_PRIMARY_DOOR,
		DATAID_CONTROL_GEARS_HANDLE,
	} AircraftControlsDataId;

	class MultiplayManager
		: public virtual nl::rakis::service::Service
	{
	public: // Types and constants
		typedef std::chrono::system_clock::time_point Timestamp;

		static MultiplayManager& instance();

	private: // Members
		bool                          connect_;
		bool						  apiConnected_;
		bool                          wsConnect_;
		bool						  wsConnected_;
		bool                          fsxConnected_;
		bool                          fsxStarted_;
		bool                          fsxPaused_;

		// Local aircraft
		AircraftIdData       aircraftId_;
		AircraftLocationData position_;
		AircraftEngineData	 aircraftEngines_;
		AircraftLightsData   aircraftLights_;
		AircraftControlsData aircraftControls_;

		FsxData aircraftLocationData_;
		FsxData aircraftEngineData_;
		FsxData aircraftLightData_;
		FsxData aircraftControlsData_;

		// Multiplay settings
		std::wstring serverUrl_;
		std::wstring serverWsUrl_;
		std::wstring username_;
		std::wstring password_;
		std::wstring session_;
		bool         forceCallsign_;
		std::wstring callsign_;

		MultiplayAPIHandler api_;

		// When did we last...
		bool needSendAircraft_;
		bool needSendLocation_;
		Timestamp lastLocationUpdate_;
		bool needSendEngines_;
		Timestamp lastEngineUpdate_;
		bool needSendLights_;
		Timestamp lastLightUpdate_;
		bool needSendControls_;
		Timestamp lastControlsUpdate_;

		// Statics
		static Logger log_;


		// Multiplayer Server commands and event handlers
		void doStartRequestingData();
		void doStopRequestingData();

		void onLocalStatusUpdate();
		void onLocalAircraftUpdate();

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

	public: // Methods
		MultiplayManager();
		virtual ~MultiplayManager();

		virtual void doStart();
		virtual void doStop();

		void connect();
		void disconnect();

		bool isApiConnected() const { return apiConnected_; }
		bool isWsConnected() const { return wsConnected_; }
		const std::wstring& getServerUrl() const { return serverUrl_; }
		void setServerUrl(const std::wstring& serverUrl) { serverUrl_ = serverUrl; }
		const std::wstring& getServerWsUrl() const { return serverWsUrl_; }
		void setServerWsUrl(const std::wstring& serverWsUrl) { serverWsUrl_ = serverWsUrl; }
		const std::wstring& getUsername() const{ return username_; }
		void setUsername(const std::wstring& username) { username_ = username; }
		const std::wstring& getPassword() const{ return password_; }
		void setPassword(const std::wstring& password) { password_ = password; }
		const std::wstring& getSession() const{ return session_; }
		void setSession(const std::wstring& session) { session_ = session; }
		const std::wstring& getCallsign() const { return callsign_; }
		void setCallsign(const std::wstring& callsign) { callsign_ = callsign; }
		bool isCallsignOverride() const { return forceCallsign_; }
		void setCallsignOverride(bool forceCallsign) { forceCallsign_ = forceCallsign; }

		// AIManager wants these
		const FsxData& getLocationData() const { return aircraftLocationData_; }
		const FsxData& getEngineData() const { return aircraftEngineData_; }
		const FsxData& getControlsData() const { return aircraftControlsData_; }

	private:
		MultiplayManager(MultiplayManager const&) = delete;
		MultiplayManager& operator=(MultiplayManager const&) = delete;
	};

} // namespace fsx
} // namespace rakis
} // namespace nl

#endif

