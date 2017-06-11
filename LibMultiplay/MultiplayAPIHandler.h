/**
*/
#ifndef __NL_RAKIS_FSX_MULTIPLAYAPIHANDLER_H
#define __NL_RAKIS_FSX_MULTIPLAYAPIHANDLER_H

#include <string>
#include <mutex>
#include <thread>
#include <atomic>
#include <condition_variable>

#include <cpprest/http_client.h>
#include <cpprest/ws_client.h>

#include "../LibRakis/Log.h"

#include "../LibFsx/SimConnectTypes.h"

namespace nl {
namespace rakis {
namespace fsx {

class MultiplayAPIHandler
{
public: // Types and constants
	enum ApiState {
		APISTATE_INIT = 1,
		APISTATE_DISCONNECTED,
		APISTATE_DO_CONNECT,
		APISTATE_CONNECTING,
		APISTATE_CONNECT_FAILED,
		APISTATE_WAITING_FOR_AIRCRAFT_DATA,
		APISTATE_READY_TO_SEND_AIRCRAFT,
		APISTATE_AIRCRAFT_SENT,
		APISTATE_READY_TO_REQUEST_SESSION,
		APISTATE_REQUESTING_SESSION,
		APISTATE_READY_TO_REQUEST_SESSION_AIRCRAFT,
		APISTATE_REQUESTING_SESSION_AIRCRAFT,
		APISTATE_SESSION_LOADED,
		APISTATE_DO_OPEN_WS,
		APISTATE_WS_CONNECTING,
		APISTATE_SEND_WS_HELLO,
		APISTATE_CONNECTED,
		APISTATE_DO_DISCONNECT,
		APISTATE_DO_CLOSE_WS,
		APISTATE_UNLOAD_SESSION,
		APISTATE_RESEND_AIRCRAFT,
		APISTATE_WAITING_FOR_NEW_AIRCRAFT_DATA,
		APISTATE_READY_TO_RESEND_AIRCRAFT,
		APISTATE_AIRCRAFT_RESENT,
		APISTATE_READY_TO_RELOAD_SESSION_AIRCRAFT,
		APISTATE_RELOADING_SESSION_AIRCRAFT,
		APISTATE_SESSION_RELOADED,
	};

	enum MultiplayServerStatus {
		MPSTAT_DISCONNECTED = 1,
		MPSTAT_CONNECTING,
		MPSTAT_WS_CONNECTING,
		MPSTAT_CONNECTED,
		MPSTAT_CLOSING,
		MPSTAT_SYNC,
		MPSTAT_ERROR,
	};
	typedef std::function<void(MultiplayServerStatus status)> ServerStatusCallback;
	typedef std::vector<ServerStatusCallback> ServerStatusCallbackList;

	struct MultiplaySession {
		std::wstring name;
		std::wstring description;
		std::vector<std::wstring> aircraft;
	};
	typedef std::function<void(const MultiplaySession& session)> SessionDataCallback;

	typedef std::vector<std::wstring> StringVector;

private: // Members
	web::http::client::http_client client_;
	web::websockets::client::websocket_client ws_client_;

	std::wstring serverUrl_;
	std::wstring serverWsUrl_;
	std::wstring username_;
	std::wstring password_;
	std::wstring sessionName_;
	std::wstring callsign_;

	std::wstring token_;

	ApiState                 state_;
	MultiplayServerStatus    serverStatus_;

	ServerStatusCallbackList statusCallbacks_;
	std::mutex               statusCbLock_;

	std::thread              apiThread_;
	std::atomic<bool>        stopRunning_;
	std::mutex               runLock_;
	std::condition_variable  runCv_;
	std::atomic<unsigned>    sem_;

	bool                     haveAircraft_;
	bool                     aircraftUpdated_;
	AircraftIdData           aircraft_;
	bool                     haveLocation_;
	bool                     locationUpdated_;
	AircraftLocationData     location_;
	bool                     haveEngines_;
	bool                     enginesUpdated_;
	AircraftEngineData       engines_;
	bool                     haveLights_;
	bool                     lightsUpdated_;
	AircraftLightsData       lights_;
	bool                     haveControls_;
	bool                     controlsUpdated_;
	AircraftControlsData     controls_;

	MultiplaySession         session_;
	StringVector             callsignsToLoad_;

	static Logger log_;
	static Logger dataInLog_;
	static Logger dataOutLog_;

	// We love you Edsger...
	void passeer();
	void verhoog();

	void run();

	void doLogin();
	void doLogout();
	void doWaitForConnectRetry();
	void doSendAircraft(bool usePUT =false);
	void doLoadSession();
	void doLoadSessionAircraft();
	bool doCheckUpdatedCallsigns();
	void doReloadSessionAircraft();
	void doConnectWebSocket();
	void doListenOnWebSocket();
	void doSendHello();
	void doCloseWebSocket();

	void doSendData();
	void doSendAircraftMessage();
	void doSendLocation();
	void doSendEngines();
	void doSendLights();
	void doSendControls();

	void fireServerStatus();
	void transition(ApiState newState);
	void transition(ApiState currentState, ApiState newState);

	void onRemoteLocationUpdate(const std::wstring& callsign, const AircraftLocationData& data);
	void onRemoteEngineUpdate(const std::wstring& callsign, const AircraftEngineData& data);
	void onRemoteLightsUpdate(const std::wstring& callsign, const AircraftLightsData& data);
	void onRemoteControlsUpdate(const std::wstring& callsign, const AircraftControlsData& data);

	void onWebSocketMessage(const web::json::value& msg);
	void onWebSocketAircraftMessage(const web::json::value& msg);
	void onWebSocketAddMessage(const web::json::value& msg);
	void onWebSocketRemoveMessage(const web::json::value& msg);
	void onWebSocketLocationMessage(const web::json::value& msg);
	void onWebSocketEnginesMessage(const web::json::value& msg);
	void onWebSocketLightsMessage(const web::json::value& msg);
	void onWebSocketControlsMessage(const web::json::value& msg);

public: // Methods
	MultiplayAPIHandler();
	~MultiplayAPIHandler();

	void start();
	void stop();

	void login();
	void updateAircraft(const AircraftIdData& aircraft);
	void logout();

	void getSessionData(const std::wstring& session, SessionDataCallback callback);
	void getAircraftData(const std::wstring& callsign, AircraftDataCallback callback);
	void getFullAircraftData(const std::wstring& callsign, FullAircraftDataCallback callback);
	void getLocationData(const std::wstring& callsign, AircraftLocationCallback callback);

	void setServer(const std::wstring& url, const std::wstring& wsUrl, const std::wstring&user, const std::wstring&pwd, const std::wstring&session, const std::wstring& callsign);
	void sendData(const AircraftIdData& aircraft);
	void sendData(const AircraftLocationData& location);
	void sendData(const AircraftEngineData& engines);
	void sendData(const AircraftLightsData& lights);
	void sendData(const AircraftControlsData& controls);

	bool isLoggedIn() const { return (state_ == APISTATE_CONNECTED); }
	const std::wstring& getToken() const { return token_; }

	void addServerStatusCallback(ServerStatusCallback cb);
};

} // namespace fsx
} // namespace rakis
} // namespace nl

#endif
