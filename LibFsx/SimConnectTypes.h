/**
* SimConnectTypes.h
*/
#ifndef __NL_RAKIS_FSX_SIMCONNECTTYPES_H
#define __NL_RAKIS_FSX_SIMCONNECTTYPES_H

#include <vector>
#include <string>
#include <functional>

#include <Windows.h>
#include <SimConnect.h>

namespace nl {
namespace rakis {
namespace fsx {

	struct ConnectionData
	{
		std::string applicationName;
		int         applicationVersionMajor;
		int         applicationVersionMinor;
		int         applicationBuildMajor;
		int         applicationBuildMinor;
		int         simConnectVersionMajor;
		int         simConnectVersionMinor;
		int         simConnectBuildMajor;
		int         simConnectBuildMinor;
	};

	struct AircraftIdData;
	typedef std::function<bool(const std::wstring& callsign, const AircraftIdData& aircraftData)> AircraftDataCallback;
	struct AircraftIdData
	{
		std::string airFilePath;
		std::string atcType;
		std::string atcModel;
		std::string atcId;
		std::string atcAirline;
		std::string atcFlightNumber;
		std::string title;
		int         nrOfEngines;
		int         engineType;

		inline bool operator==(const AircraftIdData& rhs) const {
			return (atcId == rhs.atcId) && (title == rhs.title)
				&& (atcType == rhs.atcType) && (atcModel == rhs.atcModel)
				&& (atcAirline == rhs.atcAirline) && (atcFlightNumber == rhs.atcFlightNumber)
				&& (nrOfEngines == rhs.nrOfEngines) && (engineType == rhs.engineType);
		}
	};

	enum SimConnectEvent {
		SCE_CONNECT,
		SCE_DISCONNECT,
		SCE_START,
		SCE_STOP,
		SCE_PAUSE,
		SCE_UNPAUSE,
		SCE_AIRCRAFTLOADED,
		SCE_OBJECTADDED,
		SCE_OBJECTREMOVED,
	};

	enum SimConnectSystemStateEvent {
		SSE_AIRPATH,
		SSE_MODE,
		SSE_FLIGHT,
		SSE_FPLAN,
		SSE_SIMSTATE
	};

	struct SimConnectData {
		int                 id;
		std::string         key;
		std::string         name;
		std::string         units;
		SIMCONNECT_DATATYPE type;
		float               epsilon;

		SimConnectData() {};
		SimConnectData(const std::string& k, const std::string& n, const std::string& u, SIMCONNECT_DATATYPE tp = SIMCONNECT_DATATYPE_FLOAT64, float ep = 0.001f) : id(-1), key(k), name(n), units(u), type(tp), epsilon(ep) {};
		SimConnectData(const char* k, const char* n, char* u, SIMCONNECT_DATATYPE tp = SIMCONNECT_DATATYPE_FLOAT64, float ep = 0.001f) : id(-1), key(k), name(n), units(u), type(tp), epsilon(ep) {};
		SimConnectData(int i, const std::string& n, const std::string& u, SIMCONNECT_DATATYPE tp = SIMCONNECT_DATATYPE_FLOAT64, float ep = 0.001f) : id(i), key(""), name(n), units(u), type(tp), epsilon(ep) {};
		SimConnectData(int i, const char* n, char* u, SIMCONNECT_DATATYPE tp = SIMCONNECT_DATATYPE_FLOAT64, float ep = 0.001f) : id(i), key(""), name(n), units(u), type(tp), epsilon(ep) {};
	};

	struct SimConnectDataDefinition;
	typedef std::function<bool(int entryNr, int outOfNr, const void* data, size_t nrItems, const SimConnectDataDefinition* dataDef)> SimDataCallback;
	typedef std::function<bool(const void* data, const SimConnectData& dataDef)> SimDataItemCallback;
	typedef std::function<bool(int entryNr, int outOfNr, size_t nrItems, const SimConnectDataDefinition* dataDef)> SimDataItemsDoneCallback;
	struct SimConnectDataDefinition {
		bool                        active;
		bool                        defined;
		bool                        autoDestruct;
		SIMCONNECT_DATA_REQUEST_ID  reqId;
		std::vector<SimConnectData> dataDefs;
		SimDataCallback             callback;
		SimDataItemCallback         itemCallback;
		SimDataItemsDoneCallback    itemsDoneCallback;
	};

	typedef std::function<bool(int entryNr, int outOfNr, void* data, size_t nrItems)> ClientDataCallback;
	struct SimConnectClientDataDefinition {
		bool                                 active;
		bool                                 defined;
		bool                                 autoDestruct;
		std::string                          dataName;
		SIMCONNECT_DATA_REQUEST_ID           reqId;
		SIMCONNECT_CLIENT_DATA_ID            dataId;
		SIMCONNECT_CLIENT_DATA_DEFINITION_ID defId;
		ClientDataCallback                   callback;
	};

	typedef std::function<void()> MenuCallback;
	struct SimConnectEventDefinition {
		bool                                 active;
		bool                                 defined;
		bool                                 isMenu;
		std::string                          eventName;
		MenuCallback                         menuCallback;
	};

	/**
	 * System state
	 */
	typedef std::function<bool(int intValue, float floatValue, const char* stringValue)> SystemStateCallback;
	struct SimConnectSystemStateRequest {
		bool                        active;
		SIMCONNECT_DATA_REQUEST_ID  reqId;
		SystemStateCallback         callback;
	};

	/**
	* AircraftLocationData
	* @see SIMCONNECT_DATA_INITPOSITION
	*/
	struct AircraftLocationData;
	typedef std::function<bool(const std::wstring& callsign, const AircraftLocationData& aircraftData)> AircraftLocationCallback;
	struct AircraftLocationData {
		double latitude;
		double longitude;
		double altitude;
		double pitch;
		double bank;
		double heading;
		DWORD  onGround;
		DWORD  airspeed;

		inline bool operator==(const AircraftLocationData& rhs) {
			return (latitude == rhs.latitude) && (longitude == rhs.longitude)
				&& (altitude == rhs.altitude)
				&& (pitch == rhs.pitch) && (bank == rhs.bank)
				&& (heading == rhs.heading)
				&& (onGround == rhs.onGround)
				&& (airspeed == rhs.airspeed);
		}

		inline bool operator!=(const AircraftLocationData& rhs) {
			return (latitude != rhs.latitude) || (longitude != rhs.longitude)
				|| (altitude != rhs.altitude)
				|| (pitch != rhs.pitch) || (bank != rhs.bank)
				|| (heading != rhs.heading)
				|| (onGround != rhs.onGround)
				|| (airspeed != rhs.airspeed);
		}
	};

	/**
	 * AircraftEngineData
	 */
	struct AircraftEngineData {
		int32_t engineOn[4];
		int32_t throttle[4];
	};

	/**
	* AircraftLightsData
	*/
	struct AircraftLightsData {
		bool strobeLightOn;
		bool landingLightOn;
		bool taxiLightOn;
		bool beaconLightOn;
		bool navLightOn;
		bool logoLightOn;
		bool wingLightOn;
		bool recogLightOn;
		bool cabinLightOn;
	};

	/**
	* AircraftControlsData
	*/
	struct AircraftControlsData {
		double  rudderPos;
		double  elevatorPos;
		double  aileronPos;
		double  rudderTrimPos;
		double  elevatorTrimPos;
		double  aileronTrimPos;
		double  spoilersPos;
		double  flapsPos;
		double  parkingBrakePos;
		double  doorPos;
		int32_t gearsDown;
	};

	struct FullAircraftData;
	typedef std::function<void(const std::wstring& callsign, const FullAircraftData& data)> FullAircraftDataCallback;
	struct FullAircraftData {
		AircraftIdData       aircraft;
		AircraftLocationData location;
		AircraftEngineData   engines;
		AircraftLightsData   lights;
		AircraftControlsData controls;
	};

	/**
	* SimConnectAIAircraft
	*/
	typedef std::function<void(SIMCONNECT_OBJECT_ID objId)> AIAircraftObjIdCallback;
	struct SimConnectAIAircraft {
		bool                       active;
		bool                       valid;
		bool                       unmanaged;
		SIMCONNECT_OBJECT_ID       id;
		AIAircraftObjIdCallback    callback;
		SIMCONNECT_DATA_REQUEST_ID reqId;
		std::string                title;
		std::string                callsign;
		AircraftLocationData       state;
	};


} // namespace fsx
} // namespace rakis
} // namespace nl

#endif
