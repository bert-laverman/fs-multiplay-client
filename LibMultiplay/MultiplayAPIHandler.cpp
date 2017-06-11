/**
*/
#include "stdafx.h"

#include <cwctype>
#include <chrono>
#include <strstream>
#include <iomanip>

#include <cpprest/json.h>
#include <cpprest/base_uri.h>

#include "../LibAI/AIManager.h"

#include "MultiplayAPIHandler.h"
#include "JSONUtil.h"

using namespace std;
using namespace std::chrono;

using namespace nl::rakis;
using namespace nl::rakis::fsx;

using namespace web;
using namespace web::json;
using namespace web::http;
using namespace web::http::client;
using namespace web::websockets::client;
using namespace utility::conversions;

static const wstring FIELD_ENGINES(L"engines");
static const wstring FIELD_LIGHTS(L"lights");
static const wstring FIELD_CONTROLS(L"controls");
static const wstring FIELD_LOCATION(L"location");

/*static*/ Logger     MultiplayAPIHandler::log_(Logger::getLogger("nl.rakis.fsx.MultiplayAPIHandler"));
/*static*/ Logger     MultiplayAPIHandler::dataInLog_(Logger::getLogger("nl.rakis.fsx.Multiplay.DataIn"));
/*static*/ Logger     MultiplayAPIHandler::dataOutLog_(Logger::getLogger("nl.rakis.fsx.Multiplay.DataOut"));

// SessionInfo fields
static const wstring FIELD_NAME(L"name");
static const wstring FIELD_DESC(L"description");
static const wstring FIELD_AIRCRAFT(L"aircraft");

static const char * stateName[] = {
	"<nostate>",
	"APISTATE_INIT",
	"APISTATE_DISCONNECTED",
	"APISTATE_DO_CONNECT",
	"APISTATE_CONNECTING",
	"APISTATE_CONNECT_FAILED",
	"APISTATE_WAITING_FOR_AIRCRAFT_DATA",
	"APISTATE_READY_TO_SEND_AIRCRAFT",
	"APISTATE_AIRCRAFT_SENT",
	"APISTATE_READY_TO_REQUEST_SESSION",
	"APISTATE_REQUESTING_SESSION",
	"APISTATE_READY_TO_REQUEST_SESSION_AIRCRAFT",
	"APISTATE_REQUESTING_SESSION_AIRCRAFT",
	"APISTATE_SESSION_LOADED",
	"APISTATE_DO_OPEN_WS",
	"APISTATE_WS_CONNECTING",
	"APISTATE_SEND_WS_HELLO",
	"APISTATE_CONNECTED",
	"APISTATE_DO_DISCONNECT",
	"APISTATE_DO_CLOSE_WS",
	"APISTATE_UNLOAD_SESSION",
	"APISTATE_RESEND_AIRCRAFT",
	"APISTATE_WAITING_FOR_NEW_AIRCRAFT_DATA",
	"APISTATE_READY_TO_RESEND_AIRCRAFT",
	"APISTATE_AIRCRAFT_RESENT",
	"APISTATE_READY_TO_RELOAD_SESSION_AIRCRAFT",
	"APISTATE_RELOADING_SESSION_AIRCRAFT",
	"APISTATE_SESSION_RELOADED",
};

MultiplayAPIHandler::MultiplayAPIHandler()
	: client_(uri(L"http://localhost"))
	, state_(APISTATE_INIT), serverStatus_(MPSTAT_DISCONNECTED)
	, haveAircraft_(false)
	, haveLocation_(false), locationUpdated_(false)
	, haveEngines_(false), enginesUpdated_(false)
	, haveLights_(false), lightsUpdated_(false)
	, haveControls_(false), controlsUpdated_(false)
{
}

MultiplayAPIHandler::~MultiplayAPIHandler()
{
}

/*
 */
void MultiplayAPIHandler::passeer()
{
	log_.trace("passeer(): sem=", sem_);
	unique_lock<mutex> lock(runLock_);
	while (sem_ == 0) {
		runCv_.wait(lock);
	}
	--sem_;
	log_.trace("passeer(): Done, sem=", sem_);
}

void MultiplayAPIHandler::verhoog()
{
	log_.trace("verhoog(): sem=", sem_);
	unique_lock<mutex> lock(runLock_);
	sem_++;
	runCv_.notify_one();
	log_.trace("verhoog(): Done, sem=", sem_);
}

/*
 */

void MultiplayAPIHandler::start()
{
	log_.debug("start()");

	stopRunning_ = false;
	apiThread_ = thread([=] { run(); });

	log_.debug("start(): Done");
}

void MultiplayAPIHandler::stop()
{
	log_.debug("stopRunning()");

	stopRunning_ = true;
	verhoog();

	log_.info("stopRunning(): Told worker to stop");
}

static MultiplayAPIHandler::MultiplayServerStatus state2serverState[] = {
	MultiplayAPIHandler::MPSTAT_DISCONNECTED, // 0
	MultiplayAPIHandler::MPSTAT_DISCONNECTED, // APISTATE_INIT,
	MultiplayAPIHandler::MPSTAT_DISCONNECTED, // APISTATE_DISCONNECTED,
	MultiplayAPIHandler::MPSTAT_CONNECTING, // APISTATE_DO_CONNECT,
	MultiplayAPIHandler::MPSTAT_CONNECTING, // APISTATE_CONNECTING,
	MultiplayAPIHandler::MPSTAT_ERROR, // APISTATE_CONNECT_FAILED,
	MultiplayAPIHandler::MPSTAT_SYNC, // APISTATE_WAITING_FOR_AIRCRAFT_DATA,
	MultiplayAPIHandler::MPSTAT_SYNC, // APISTATE_READY_TO_SEND_AIRCRAFT,
	MultiplayAPIHandler::MPSTAT_SYNC, // APISTATE_AIRCRAFT_SENT,
	MultiplayAPIHandler::MPSTAT_SYNC, // APISTATE_READY_TO_REQUEST_SESSION,
	MultiplayAPIHandler::MPSTAT_SYNC, // APISTATE_REQUESTING_SESSION,
	MultiplayAPIHandler::MPSTAT_SYNC, // APISTATE_READY_TO_REQUEST_SESSION_AIRCRAFT,
	MultiplayAPIHandler::MPSTAT_SYNC, // APISTATE_REQUESTING_SESSION_AIRCRAFT,
	MultiplayAPIHandler::MPSTAT_SYNC, // APISTATE_SESSION_LOADED,
	MultiplayAPIHandler::MPSTAT_WS_CONNECTING, // APISTATE_DO_OPEN_WS,
	MultiplayAPIHandler::MPSTAT_WS_CONNECTING, // APISTATE_WS_CONNECTING,
	MultiplayAPIHandler::MPSTAT_WS_CONNECTING, // APISTATE_SEND_WS_HELLO,
	MultiplayAPIHandler::MPSTAT_CONNECTED, // APISTATE_CONNECTED,
	MultiplayAPIHandler::MPSTAT_CLOSING, // APISTATE_DO_DISCONNECT,
	MultiplayAPIHandler::MPSTAT_CLOSING, // APISTATE_DO_CLOSE_WS,
	MultiplayAPIHandler::MPSTAT_SYNC, // APISTATE_UNLOAD_SESSION,
	MultiplayAPIHandler::MPSTAT_SYNC, // APISTATE_RESEND_AIRCRAFT,
	MultiplayAPIHandler::MPSTAT_SYNC, // APISTATE_WAITING_FOR_NEW_AIRCRAFT_DATA,
	MultiplayAPIHandler::MPSTAT_SYNC, // APISTATE_READY_TO_RESEND_AIRCRAFT,
	MultiplayAPIHandler::MPSTAT_SYNC, // APISTATE_AIRCRAFT_RESENT,
	MultiplayAPIHandler::MPSTAT_SYNC, // APISTATE_READY_TO_RELOAD_SESSION_AIRCRAFT,
	MultiplayAPIHandler::MPSTAT_SYNC, // APISTATE_RELOADING_SESSION_AIRCRAFT,
	MultiplayAPIHandler::MPSTAT_SYNC, // APISTATE_SESSION_RELOADED,
};

void MultiplayAPIHandler::run()
{
	while (passeer(), !stopRunning_) {
		switch (state_) {
		case APISTATE_INIT:
			break;

		case APISTATE_DISCONNECTED:
		case APISTATE_CONNECTING:
		case APISTATE_WS_CONNECTING:
		case APISTATE_REQUESTING_SESSION:
			break;

		case APISTATE_DO_CONNECT:
			doLogin();
			break;

		case APISTATE_CONNECT_FAILED:
			doWaitForConnectRetry();
			break;

		case APISTATE_WAITING_FOR_AIRCRAFT_DATA:
			if (haveAircraft_ && haveLocation_ && haveEngines_ && haveLights_ && haveControls_) {
				transition(APISTATE_WAITING_FOR_AIRCRAFT_DATA, APISTATE_READY_TO_SEND_AIRCRAFT);
			}
			break;

		case APISTATE_READY_TO_SEND_AIRCRAFT:
			doSendAircraft();
			break;

		case APISTATE_AIRCRAFT_SENT:
			transition(APISTATE_AIRCRAFT_SENT, APISTATE_READY_TO_REQUEST_SESSION);
			break;

		case APISTATE_READY_TO_REQUEST_SESSION:
			doLoadSession();
			break;

		case APISTATE_READY_TO_REQUEST_SESSION_AIRCRAFT:
			doLoadSessionAircraft();
			break;

		case APISTATE_REQUESTING_SESSION_AIRCRAFT:
			break;

		case APISTATE_SESSION_LOADED:
			transition(APISTATE_SESSION_LOADED, APISTATE_DO_OPEN_WS);
			break;

		case APISTATE_DO_OPEN_WS:
			doConnectWebSocket();
			break;

		case APISTATE_SEND_WS_HELLO:
			doSendHello();
			break;

		case APISTATE_CONNECTED:
			if (doCheckUpdatedCallsigns()) {
				transition(APISTATE_CONNECTED, APISTATE_READY_TO_RELOAD_SESSION_AIRCRAFT);
			}
			else if (aircraftUpdated_) {
				transition(APISTATE_CONNECTED, APISTATE_RESEND_AIRCRAFT);
			}
			else {
				doSendData();
			}
			break;

		case APISTATE_DO_DISCONNECT:
			doLogout();
			break;

		case APISTATE_DO_CLOSE_WS:
			doCloseWebSocket();
			break;

		case APISTATE_RESEND_AIRCRAFT:
			transition(APISTATE_RESEND_AIRCRAFT, APISTATE_WAITING_FOR_NEW_AIRCRAFT_DATA);
			break;

		case APISTATE_WAITING_FOR_NEW_AIRCRAFT_DATA:
			if (haveAircraft_ && haveLocation_ && haveEngines_ && haveLights_ && haveControls_) {
				transition(APISTATE_WAITING_FOR_NEW_AIRCRAFT_DATA, APISTATE_READY_TO_RESEND_AIRCRAFT);
			}
			break;

		case APISTATE_READY_TO_RESEND_AIRCRAFT:
			doSendAircraft();
			break;

		case APISTATE_AIRCRAFT_RESENT:
			doSendAircraftMessage();
			break;

		case APISTATE_READY_TO_RELOAD_SESSION_AIRCRAFT:
			doReloadSessionAircraft();
			break;

		case APISTATE_RELOADING_SESSION_AIRCRAFT:
			break;

		case APISTATE_SESSION_RELOADED:
			transition(APISTATE_SESSION_RELOADED, APISTATE_CONNECTED);
			break;

		default:
			log_.error("run(): Unhandled state. (state_=", state_, ")");
			break;
		}
	}
}

/*
 */
void MultiplayAPIHandler::addServerStatusCallback(ServerStatusCallback cb)
{
	unique_lock<mutex> lock(statusCbLock_);
	statusCallbacks_.emplace_back(cb);
}

void MultiplayAPIHandler::fireServerStatus()
{
	unique_lock<mutex> lock(statusCbLock_);

	MultiplayServerStatus mpst(serverStatus_);
	for (ServerStatusCallbackList::const_iterator it = statusCallbacks_.begin(); it != statusCallbacks_.end(); it++) {
		(*it)(mpst);
	}
}

/*
 */
void MultiplayAPIHandler::transition(ApiState newState)
{
	log_.debug("transition(", stateName [newState], ")");
	state_ = newState;
	verhoog();

	MultiplayServerStatus mpst = state2serverState[newState];
	log_.debug("transition(): serverstatus=", serverStatus_, ", new=", newState);
	if (serverStatus_ != mpst) {
		serverStatus_ = mpst;
		fireServerStatus();
	}
}

void MultiplayAPIHandler::transition(ApiState currentState, ApiState newState)
{
	if (state_ == currentState) {
		log_.debug("transition(", stateName [currentState], ", ", stateName [newState], ")");
		MultiplayServerStatus mpst = state2serverState[newState];
		log_.debug("transition(): serverstatus=", serverStatus_, ", new=", newState);

		state_ = newState;
		verhoog();

		if (serverStatus_ != mpst) {
			serverStatus_ = mpst;
			fireServerStatus();
		}
	}
}

/*
 */
void MultiplayAPIHandler::doLogin()
{
	log_.debug("doLogin()");

	if (state_ == APISTATE_DO_CONNECT) {
		log_.info("doLogin(): Connecting to server as user \"", username_, "\" and joining session \"", sessionName_, "\"");

		json::value body;
		body[U("username")] = json::value(username_);
		body[U("password")] = json::value(password_);
		body[U("session")] = json::value(sessionName_);
		body[U("callsign")] = json::value(callsign_);

		auto resp = client_.request(methods::PUT, U("session/login"), body).get();

		log_.trace("doLogin(): Server returned ", resp.status_code());
		if (resp.status_code() == 200) {
			log_.debug("doLogin(): Login succeeded");
			token_ = L"bearer ";
			token_ += resp.extract_utf16string(true).get();
			log_.debug("doLogin(): Received bearer token \"", token_, "\"");

			transition(APISTATE_DO_CONNECT, APISTATE_WAITING_FOR_AIRCRAFT_DATA);
		}
		else {
			log_.error("doLogin(): Login failed");
			token_ = L"";
			transition(APISTATE_CONNECT_FAILED);
		}
	}
	log_.debug("doLogin(): Done");
}

void MultiplayAPIHandler::doLoadSession()
{
	log_.debug("doLoadSession()");

	if (state_ == APISTATE_READY_TO_REQUEST_SESSION) {
		transition(APISTATE_READY_TO_REQUEST_SESSION, APISTATE_REQUESTING_SESSION);
		getSessionData(sessionName_, [=](const MultiplaySession& session) {
			log_.debug("doLoadSession[sessionCallback](): ", session.aircraft.size(), " aircraft in session");
			session_ = session;

			transition(APISTATE_REQUESTING_SESSION, APISTATE_READY_TO_REQUEST_SESSION_AIRCRAFT);
		});
	}
	log_.debug("doLoadSession(): Done");
}

void MultiplayAPIHandler::doLoadSessionAircraft()
{
	log_.debug("doLoadSessionAircraft()");

	if (state_ == APISTATE_READY_TO_REQUEST_SESSION_AIRCRAFT) {
		transition(APISTATE_READY_TO_REQUEST_SESSION_AIRCRAFT, APISTATE_REQUESTING_SESSION_AIRCRAFT);
		for (auto it = session_.aircraft.begin(); it != session_.aircraft.end(); it++) {
			if (*it != callsign_) {
				getFullAircraftData(*it, [=](const wstring& callsign, const FullAircraftData& data) {
					if ((data.location.latitude == 0) && (data.location.longitude == 0) && (data.location.altitude == 0)) {
						log_.warn("doLoadSessionAircraft[getFullAircraftCallback](): Received no location for \"", callsign, "\"");
					}
					else {
						AIManager::instance().create(*it, data);
					}
				});
			}
		}
		transition(APISTATE_REQUESTING_SESSION_AIRCRAFT, APISTATE_SESSION_LOADED);
	}
	log_.debug("doLoadSessionAircraft(): Done");
}

bool MultiplayAPIHandler::doCheckUpdatedCallsigns()
{
	AIManager::instance().collectAircraftInNeedOfUpdate(callsignsToLoad_);
	return !callsignsToLoad_.empty();
}

void MultiplayAPIHandler::doReloadSessionAircraft()
{
	log_.debug("doReloadSessionAircraft()");

	if (state_ == APISTATE_READY_TO_RELOAD_SESSION_AIRCRAFT) {
		transition(APISTATE_RELOADING_SESSION_AIRCRAFT);
		while (!callsignsToLoad_.empty()) {
			wstring callsign(callsignsToLoad_.front());

			log_.debug("doReloadSessionAircraft(): processing \"", callsign, "\"");

			callsignsToLoad_.erase(callsignsToLoad_.begin());

			AIManager::instance().remove(callsign);
			getFullAircraftData(callsign, [=](const wstring& cs, const FullAircraftData& data) {
				if ((data.location.latitude == 0) && (data.location.longitude == 0) && (data.location.altitude == 0)) {
					log_.warn("doReloadSessionAircraft[getFullAircraftCallback](): Received no location for \"", cs, "\"");
				}
				else {
					AIManager::instance().create(cs, data);
				}
			});
		}
		transition(APISTATE_SESSION_RELOADED);
	}
	else {
		log_.debug("doReloadSessionAircraft(): state is ", stateName [state_], " (", state_, ")");
	}
	log_.debug("doReloadSessionAircraft(): Done");
}

void MultiplayAPIHandler::doLogout()
{
	if (state_ == APISTATE_DO_DISCONNECT) {
		log_.info("doLogout(): Logging out from server");

		uri_builder uri(U("session/logout"));

		log_.debug("getFullAircraftData(): Calling GET at \"", uri.to_string(), "\"");

		http_request req(methods::GET);
		req.headers().add(U("Authorization"), token_);
		req.set_request_uri(uri.to_string());
		auto resp = client_.request(req).get();

		log_.trace("doLogout(): Server returned ", resp.status_code());
		if (resp.status_code() == 200) {
			log_.debug("doLogout(): Logout succeeded");
		}
		else {
			log_.error("doLogout(): Logout failed");
		}

		token_.clear();
		AIManager::instance().flushAIAircraft();

		transition(APISTATE_DO_DISCONNECT, APISTATE_DO_CLOSE_WS);
	}
}

void MultiplayAPIHandler::doWaitForConnectRetry()
{
	log_.debug("doWaitForconnectRetry()");

	this_thread::sleep_for(seconds(5));
	transition(APISTATE_CONNECT_FAILED, APISTATE_DO_CONNECT);

	log_.trace("doWaitForconnectRetry(): Done");
}

void MultiplayAPIHandler::doSendAircraft(bool usePOST)
{
	log_.debug("doSendAircraft()");

	if ((state_ == APISTATE_READY_TO_SEND_AIRCRAFT) ||
		(state_ == APISTATE_READY_TO_RESEND_AIRCRAFT) )
	{
		value aircraftInfo;

		toJSON(aircraftInfo, aircraft_);

		if (haveLocation_) {
			value locationInfo;
			toJSON(locationInfo, location_);
			aircraftInfo[FIELD_LOCATION] = locationInfo;
			locationUpdated_ = false;
		}
		else {
			log_.warn("doSendAircraft(): No location data to send");
		}
		if (haveEngines_) {
			value engineInfo;
			toJSON(engineInfo, engines_);
			aircraftInfo[FIELD_ENGINES] = engineInfo;
			enginesUpdated_ = false;
		}
		else {
			log_.warn("doSendAircraft(): No engine data to send");
		}
		if (haveLights_) {
			value lightInfo;
			toJSON(lightInfo, lights_);
			aircraftInfo[FIELD_LIGHTS] = lightInfo;
			lightsUpdated_ = false;
		}
		else {
			log_.warn("doSendAircraft(): No lights data to send");
		}
		if (haveControls_) {
			value controlsInfo;
			toJSON(controlsInfo, controls_);
			aircraftInfo[FIELD_CONTROLS] = controlsInfo;
			controlsUpdated_ = false;
		}
		else {
			log_.warn("doSendAircraft(): No controls data to send");
		}

		log_.debug("doSendAircraft(): Sending aircraft with callsign \"", callsign_, "\"");
		dataOutLog_.debug("doSendAircraft(): ", aircraftInfo.serialize());

		uri_builder uri(usePOST ? U("aircraft") : U("aircraft/"));
		if (!usePOST) {
			uri.append_path(callsign_);
		}

		http_request req(usePOST ? methods::POST : methods::PUT);
		req.headers().add(U("Authorization"), token_);
		req.set_request_uri(uri.to_string());
		req.set_body(aircraftInfo);

		auto resp = client_.request(req).get();
		if ((resp.status_code() == 200) || (resp.status_code() == 201)) {
			log_.debug("doSendAircraft(): Aircraft successfully sent to server");

			aircraftUpdated_ = false;

			// One of these will trigger
			transition(APISTATE_READY_TO_SEND_AIRCRAFT, APISTATE_AIRCRAFT_SENT);
			transition(APISTATE_READY_TO_RESEND_AIRCRAFT, APISTATE_AIRCRAFT_RESENT);
		}
		else if (resp.status_code() == 401) {
			log_.debug("doSendAircraft(): Session lost");

			transition(APISTATE_CONNECT_FAILED);
		}
		else if (!usePOST && (resp.status_code() == 404)) {
			log_.debug("doSendAircraft(): Broken PUT!");

			doSendAircraft(true);
		}
		else {
			log_.error("doSendAircraft(): Aircraft update failed (HTTP response ", resp.status_code(), ")");
			token_ = L"";
			transition(APISTATE_CONNECT_FAILED);
		}
	}
	log_.debug("doSendAircraft(): Done");
}

void MultiplayAPIHandler::doConnectWebSocket()
{
	log_.debug("doConnectWebSocket()");

	if (state_ == APISTATE_DO_OPEN_WS) {
		log_.debug("doConnectWebSocket(): Setting up WebSocket client");
		websocket_client_config conf;
		conf.headers().add(L"Authorization", token_);
		ws_client_ = websocket_client(conf);

		transition(APISTATE_DO_OPEN_WS, APISTATE_WS_CONNECTING);

		log_.debug("doConnectWebSocket(): Connecting to WebSocket server (uri=\"", serverWsUrl_, "\"");
		ws_client_.connect(serverWsUrl_).then([=](pplx::task<void> conn_task) {
			conn_task.get();
			log_.debug("doConnectWebSocket()[]: WebSocket connection established.");

			doListenOnWebSocket();

			transition(APISTATE_WS_CONNECTING, APISTATE_SEND_WS_HELLO);
		});
	}
	log_.debug("doConnectWebSocket(): Done");
}

void MultiplayAPIHandler::doSendHello()
{
	log_.debug("doSendHello()");

	if (state_ == APISTATE_SEND_WS_HELLO) {
		log_.debug("doConnectWebSocket()[]: Sending \"hello\" on WebSocket");
		value helloMsg;
		helloMsg[FIELD_TYPE] = value(TYPE_HELLO);
		helloMsg[FIELD_TOKEN] = value(token_);
		helloMsg[FIELD_CALLSIGN] = value(callsign_);

		websocket_outgoing_message msg;
		msg.set_utf8_message(to_utf8string(helloMsg.serialize()));
		ws_client_.send(msg).get();

		transition(APISTATE_SEND_WS_HELLO, APISTATE_CONNECTED);
	}
	log_.debug("doSendHello(): Done");
}

typedef pplx::task<bool> BoolTask;
typedef pplx::task<void> VoidTask;
typedef function<BoolTask(void)> BoolTaskFunc;

static BoolTask doWhileOnce(BoolTaskFunc func) {
	pplx::task_completion_event<bool> ev;

	func().then([=](bool continueOk) { ev.set(continueOk); });

	return pplx::create_task(ev);
}

static BoolTask asyncDoWhileBool(BoolTaskFunc func) {
	return doWhileOnce(func).then([=](bool continueOk) ->BoolTask {
		return continueOk ? asyncDoWhileBool(func) : pplx::task_from_result(false);
	});
}

static VoidTask asyncDoWhile(BoolTaskFunc func) {
	return asyncDoWhileBool(func).then([](bool) {});
}

void MultiplayAPIHandler::doListenOnWebSocket()
{
	log_.debug("doListenOnWebSocket()");

	pplx::create_task([=]() {
		asyncDoWhile([=]() {
			return ws_client_.receive().then([=](pplx::task<websocket_incoming_message> in_msg_task) {
				log_.debug("doListenOnWebSocket()[]: INCOMING!!");
				return in_msg_task.get().extract_string().then([=](pplx::task<string> str_tsk) {
					onWebSocketMessage(value::parse(utility::conversions::to_string_t(str_tsk.get())));
				});

			}).then([](VoidTask end_task) {
				try {
					end_task.get();
					return true;
				}
				catch (websocket_exception) {
					//TODO error handling or eat the exception as needed.
				}
				catch (...) {
					//TODO error handling or eat the exception as needed.
				}

				// We are here means we encountered some exception.
				log_.error("doListenOnWebSocket()[]: Error processing incoming message");

				// Return false to break the asynchronous loop.
				return false;
			});
		});
	});
	log_.debug("doListenOnWebSocket(): Done");
}

void MultiplayAPIHandler::doCloseWebSocket()
{
	log_.debug("doCloseWebSocket()");

	if (state_ == APISTATE_DO_CLOSE_WS) {
		log_.info("doCloseWebSocket(): Closing websocket");

		ws_client_.close().then([](VoidTask tsk) {
			tsk.get();
		});

		transition(APISTATE_DO_CLOSE_WS, APISTATE_DISCONNECTED);
	}
	log_.debug("doCloseWebSocket(): Done");
}

void MultiplayAPIHandler::doSendData()
{
	log_.trace("doSendData()");

	if (locationUpdated_) {
		log_.debug("doSendData(): We have updated location data");
		doSendLocation();
	}
	if (enginesUpdated_) {
		log_.debug("doSendData(): We have updated engine data");
		doSendEngines();
	}
	if (lightsUpdated_) {
		log_.debug("doSendData(): We have updated lights data");
		doSendLights();
	}
	if (controlsUpdated_) {
		log_.debug("doSendData(): We have updated controls data");
		doSendControls();
	}
	log_.trace("doSendData(): Done");
}

void MultiplayAPIHandler::doSendAircraftMessage()
{

	log_.trace("doSendAircraftMessage()");

	if (state_ == APISTATE_AIRCRAFT_RESENT) {
		log_.trace("doSendAircraftMessage(): Sending aircraft update message for \"", callsign_, "\"");

		value aircraftUpdate;
		aircraftUpdate[FIELD_CALLSIGN] = value(callsign_);
		aircraftUpdate[FIELD_TYPE] = value(TYPE_AIRCRAFT);
		aircraftUpdate[FIELD_SESSION] = value(sessionName_);

		const string jsonMsg(to_utf8string(aircraftUpdate.serialize()));
		dataOutLog_.debug(jsonMsg);

		websocket_outgoing_message msg;
		msg.set_utf8_message(jsonMsg);
		try {
			ws_client_.send(msg).wait();
		}
		catch (...) {
			log_.error("doSendAircraftMessage(): Some kind of exception was thrown by websocket_client::send()");
		}
		transition(APISTATE_AIRCRAFT_RESENT, APISTATE_CONNECTED);
	}
	log_.trace("doSendAircraftMessage(): Done");
}

void MultiplayAPIHandler::doSendLocation()
{
	log_.trace("doSendLocation()");

	if (state_ == APISTATE_CONNECTED) {
		log_.trace("doSendLocation(): Sending location data for \"", callsign_, "\"");

		value location;
		location[FIELD_CALLSIGN] = value(callsign_);
		location[FIELD_TYPE] = value(TYPE_LOCATION);

		toJSON(location, location_);
		locationUpdated_ = false;

		const string jsonMsg(to_utf8string(location.serialize()));
		dataOutLog_.debug(jsonMsg);

		websocket_outgoing_message msg;
		msg.set_utf8_message(jsonMsg);
		try {
			ws_client_.send(msg).wait();
		}
		catch (...) {
			log_.error("doSendLocation(): Some kind of exception was thrown by websocket_client::send()");
		}
	}
	log_.trace("doSendLocation(): Done");
}

void MultiplayAPIHandler::doSendEngines()
{
	log_.trace("doSendEngines()");

	if (state_ == APISTATE_CONNECTED) {
		log_.trace("doSendEngines(): Sending engine data for \"", callsign_, "\"");

		value engineData;
		engineData[FIELD_CALLSIGN] = value(callsign_);
		engineData[FIELD_TYPE] = value(TYPE_ENGINES);

		toJSON(engineData, engines_);
		enginesUpdated_ = false;

		const string jsonMsg(to_utf8string(engineData.serialize()));
		dataOutLog_.debug(jsonMsg);

		websocket_outgoing_message msg;
		msg.set_utf8_message(jsonMsg);
		try {
			ws_client_.send(msg).wait();
		}
		catch (...) {
			log_.error("doSendEngines(): Some kind of exception was thrown by websocket_client::send()");
		}
	}
	log_.trace("doSendEngines(): Done");
}

void MultiplayAPIHandler::doSendLights()
{
	log_.trace("doSendLights()");

	if (state_ == APISTATE_CONNECTED) {
		log_.trace("doSendLights(): Sending light data for \"", callsign_, "\"");

		value lightsData;
		lightsData[FIELD_CALLSIGN] = value(callsign_);
		lightsData[FIELD_TYPE] = value(TYPE_LIGHTS);

		toJSON(lightsData, lights_);
		lightsUpdated_ = false;

		const string jsonMsg(to_utf8string(lightsData.serialize()));
		dataOutLog_.debug(jsonMsg);

		websocket_outgoing_message msg;
		msg.set_utf8_message(jsonMsg);
		try {
			ws_client_.send(msg).wait();
		}
		catch (...) {
			log_.error("doSendLights(): Some kind of exception was thrown by websocket_client::send()");
		}
	}
	log_.trace("doSendLights(): Done");
}

void MultiplayAPIHandler::doSendControls()
{
	log_.trace("doSendControls()");

	if (state_ == APISTATE_CONNECTED) {
		log_.trace("doSendControls(): Sending controls data for \"", callsign_, "\"");

		value controlsData;
		controlsData[FIELD_CALLSIGN] = value(callsign_);
		controlsData[FIELD_TYPE] = value(TYPE_CONTROLS);

		toJSON(controlsData, controls_);
		controlsUpdated_ = false;

		const string jsonMsg(to_utf8string(controlsData.serialize()));
		dataOutLog_.debug(jsonMsg);

		websocket_outgoing_message msg;
		msg.set_utf8_message(jsonMsg);
		try {
			ws_client_.send(msg).wait();
		}
		catch (...) {
			log_.error("doSendControls(): Some kind of exception was thrown by websocket_client::send()");
		}
	}
	log_.trace("doSendControls(): Done");
}

/*
 */

void MultiplayAPIHandler::onRemoteLocationUpdate(const std::wstring& callsign, const AircraftLocationData& data)
{
	log_.trace("onRemoteLocationUpdate(): callsign=\"", callsign, "\", lat=", data.latitude, " , lon=", data.longitude);

	AIManager& mgr(AIManager::instance());

	mgr.updateLocation(callsign, data);

	log_.trace("onRemoteLocationUpdate(): Done");
}

void MultiplayAPIHandler::onRemoteEngineUpdate(const std::wstring& callsign, const AircraftEngineData& data)
{
	log_.trace("onRemoteEngineUpdate(): callsign=\"", callsign, "\"");

	AIManager& mgr(AIManager::instance());

	mgr.updateEngines(callsign, data);

	log_.trace("onRemoteEngineUpdate(): Done");
}

void MultiplayAPIHandler::onRemoteLightsUpdate(const std::wstring& callsign, const AircraftLightsData& data)
{
	log_.trace("onRemoteLightsUpdate(): callsign=\"", callsign, "\"");

	AIManager& mgr(AIManager::instance());

	mgr.updateLights(callsign, data);

	log_.trace("onRemoteLightsUpdate(): Done");
}

void MultiplayAPIHandler::onRemoteControlsUpdate(const std::wstring& callsign, const AircraftControlsData& data)
{
	log_.debug("onRemoteControlsUpdate(): callsign=\"", callsign, "\"");

	AIManager& mgr(AIManager::instance());

	mgr.updateControls(callsign, data);

	log_.trace("onRemoteControlsUpdate(): Done");
}

void MultiplayAPIHandler::onWebSocketMessage(const value& msg)
{
	dataInLog_.debug(msg.serialize());

	if (msg.has_field(FIELD_CALLSIGN) && (msg.at(FIELD_CALLSIGN).as_string() == callsign_)) {
		log_.debug("onWebSocketMessage(): Ignoring reflected message");
	}
	else {
		wstring type(msg.at(FIELD_TYPE).as_string());
		if (type == TYPE_LOCATION) {
			onWebSocketLocationMessage(msg);
		}
		else if (type == TYPE_ADD) {
			onWebSocketAddMessage(msg);
		}
		else if (type == TYPE_REMOVE) {
			onWebSocketRemoveMessage(msg);
		}
		else if (type == TYPE_AIRCRAFT) {
			onWebSocketAircraftMessage(msg);
		}
		else if (type == TYPE_ENGINES) {
			onWebSocketEnginesMessage(msg);
		}
		else if (type == TYPE_LIGHTS) {
			onWebSocketLightsMessage(msg);
		}
		else if (type == TYPE_CONTROLS) {
			onWebSocketControlsMessage(msg);
		}
		else {
			log_.error("onWebSocketMessage(): Unknown message type \"", type, "\"");
		}
	}
	log_.debug("onWebSocketMessage(): Done");
}

void MultiplayAPIHandler::onWebSocketAircraftMessage(const web::json::value& msg)
{
	log_.debug("onWebSocketAircraftMessage()", msg.serialize());

	wstring msgCallsign(msg.at(FIELD_CALLSIGN).as_string());
	wstring session(msg.at(FIELD_SESSION).as_string());

	if (msg.at(FIELD_SESSION).as_string() == sessionName_) {
		log_.info("onWebSocketAircraftMessage(): New callsign \"", msgCallsign, "\" in session");

		callsignsToLoad_.push_back(msgCallsign);
		verhoog();
	}
	else {
		log_.debug("onWebSocketAircraftMessage(): Ignoring new callsign \"", msgCallsign, "\" in session \"", session, "\"");
	}

	log_.debug("onWebSocketAircraftMessage(): Done");
}

void MultiplayAPIHandler::onWebSocketAddMessage(const web::json::value& msg)
{
	log_.debug("onWebSocketAddMessage()", msg.serialize());

	wstring msgCallsign(msg.at(FIELD_CALLSIGN).as_string());
	wstring session(msg.at(FIELD_SESSION).as_string());

	if (msg.at(FIELD_SESSION).as_string() == sessionName_) {
		log_.info("onWebSocketAddMessage(): New callsign \"", msgCallsign, "\" in session");

		callsignsToLoad_.push_back(msgCallsign);
		verhoog();
	}
	else {
		log_.debug("onWebSocketAddMessage(): Ignoring new callsign \"", msgCallsign, "\" in session \"", session, "\"");
	}

	log_.debug("onWebSocketAddMessage(): Done");
}

void MultiplayAPIHandler::onWebSocketRemoveMessage(const web::json::value& msg)
{
	log_.debug("onWebSocketRemoveMessage()", msg.serialize());

	wstring callsign(msg.at(FIELD_CALLSIGN).as_string());
	wstring session(msg.at(FIELD_SESSION).as_string());

	if (msg.at(FIELD_SESSION).as_string() == sessionName_) {
		log_.info("onWebSocketRemoveMessage(): Callsign \"", callsign, "\" is leaving the session");
		AIManager& mgr(AIManager::instance());

		if (mgr.getAIAircraft(callsign) == AIManager::NO_AIRCRAFT) {
			log_.info("onWebSocketRemoveMessage(): We have no local AI for \"", callsign, "\"");
		}
		else {
			mgr.remove(callsign);
			verhoog();
		}

	}
	else {
		log_.debug("onWebSocketRemoveMessage(): Ignoring leaving callsign \"", callsign, "\" in session \"", session, "\"");
	}

	log_.debug("onWebSocketRemoveMessage(): Done");
}

void MultiplayAPIHandler::onWebSocketLocationMessage(const web::json::value& msg)
{
	log_.debug("onWebSocketLocationMessage()", msg.serialize());

	AircraftLocationData data;
	fromJSON(data, msg);

	onRemoteLocationUpdate(msg.at(FIELD_CALLSIGN).as_string(), data);

	log_.debug("onWebSocketLocationMessage(): Done");
}

void MultiplayAPIHandler::onWebSocketEnginesMessage(const web::json::value& msg)
{
	log_.trace("onWebSocketEnginesMessage()", msg.serialize());

	AircraftEngineData data;

	fromJSON(data, msg);

	onRemoteEngineUpdate(msg.at(FIELD_CALLSIGN).as_string(), data);

	log_.trace("onWebSocketEnginesMessage(): Done");
}

void MultiplayAPIHandler::onWebSocketLightsMessage(const web::json::value& msg)
{
	log_.debug("onWebSocketLightsMessage()", msg.serialize());

	AircraftLightsData data;

	fromJSON(data, msg);

	onRemoteLightsUpdate(msg.at(FIELD_CALLSIGN).as_string(), data);

	log_.debug("onWebSocketLightsMessage(): Done");
}

void MultiplayAPIHandler::onWebSocketControlsMessage(const web::json::value& msg)
{
	log_.trace("onWebSocketControlsMessage()", msg.serialize());

	AircraftControlsData data;

	fromJSON(data, msg);

	onRemoteControlsUpdate(msg.at(FIELD_CALLSIGN).as_string(), data);

	log_.trace("onWebSocketControlsMessage(): Done");
}

/*
 */
void MultiplayAPIHandler::getSessionData(const std::wstring& session, SessionDataCallback callback)
{
	log_.info("getSessionData()");

	uri_builder uri(U("session/"));
	uri.append_path(session);
	uri.append_query(U("_expand=aircraft"));

	log_.debug("getSessionData(): Calling GET at \"", uri.to_string(), "\"");

	http_request req(methods::GET);
	req.headers().add(U("Authorization"), token_);
	req.set_request_uri(uri.to_string());

	auto resp = client_.request(req).get();
	if (resp.status_code() == 200) {
		log_.info("getSessionData(): Received session data for \"", session, "\"");

		value sessionInfo(resp.extract_json().get());
		MultiplaySession result;

		getString(result.name, sessionInfo, FIELD_NAME);
		getString(result.description, sessionInfo, FIELD_DESC);
		if (sessionInfo.has_field(FIELD_AIRCRAFT)) {
			auto aircraft = sessionInfo.at(FIELD_AIRCRAFT).as_array();
			for (auto it = aircraft.begin(); it != aircraft.end(); it++) {
				wstring callsign(it->as_string());
				if (callsign != callsign_) {
					result.aircraft.push_back(callsign);
				}
			}
		}
		callback(result);
	}
	else if (resp.status_code() == 401) {
		log_.error("getSessionData(): Lost authorization. resetting connection");
		transition(APISTATE_DISCONNECTED);
	}
	else{
		log_.error("getSessionData(): Retrieving session data failed");
		transition(APISTATE_DO_DISCONNECT);
	}

	log_.info("getSessionData(): Done");
}

void MultiplayAPIHandler::getAircraftData(const std::wstring& callsign, AircraftDataCallback callback)
{
	log_.debug("getAircraftData()");

	uri_builder uri(U("aircraft/"));
	uri.append_path(callsign);

	log_.debug("getAircraftData(): Calling GET at \"", uri.to_string(), "\"");

	http_request req(methods::GET);
	req.headers().add(U("Authorization"), token_);
	req.set_request_uri(uri.to_string());

	auto resp = client_.request(req).get();
	if (resp.status_code() == 200) {
		log_.debug("getAircraftData(): Received aircraft data for \"", callsign, "\"");

		AircraftIdData result;

		fromJSON(result, resp.extract_json().get());

		callback(callsign, result);
	}
	else if (resp.status_code() == 401) {
		log_.error("getAircraftData(): Lost authorization. resetting connection");
		transition(APISTATE_DISCONNECTED);
	}
	else {
		log_.error("getAircraftData(): Retrieving session data failed");
		transition(APISTATE_DO_DISCONNECT);
	}

	log_.debug("getAircraftData(): Done");
}

void MultiplayAPIHandler::getFullAircraftData(const std::wstring& callsign, FullAircraftDataCallback callback)
{
	log_.debug("getFullAircraftData()");

	uri_builder uri(U("aircraft/"));
	uri.append_path(callsign);
	uri.append_query(U("_expand=location,engines,lights,controls"));

	log_.debug("getFullAircraftData(): Calling GET at \"", uri.to_string(), "\"");

	http_request req(methods::GET);
	req.headers().add(U("Authorization"), token_);
	req.set_request_uri(uri.to_string());

	auto resp = client_.request(req).get();
	if (resp.status_code() == 200) {
		log_.debug("getFullAircraftData(): Received aircraft data for \"", callsign, "\"");

		wstring objStr(resp.extract_string().get());
		dataInLog_.debug("getFullAircraftData(): ", objStr);
		error_code err;
		value aircraftInfo;
		aircraftInfo = value::parse(objStr);
		FullAircraftData result;

		fromJSON(result.aircraft, aircraftInfo);
		if (aircraftInfo.has_field(FIELD_LOCATION)) {
			fromJSON(result.location, aircraftInfo.at(FIELD_LOCATION));
		}
		if (aircraftInfo.has_field(FIELD_ENGINES)) {
			fromJSON(result.engines, aircraftInfo.at(FIELD_ENGINES));
		}
		if (aircraftInfo.has_field(FIELD_LIGHTS)) {
			fromJSON(result.lights, aircraftInfo.at(FIELD_LIGHTS));
		}
		if (aircraftInfo.has_field(FIELD_CONTROLS)) {
			fromJSON(result.controls, aircraftInfo.at(FIELD_CONTROLS));
		}
		callback(callsign, result);
	}
	else if (resp.status_code() == 401) {
		log_.error("getFullAircraftData(): Lost authorization. resetting connection");
		transition(APISTATE_DISCONNECTED);
	}
	else {
		log_.error("getFullAircraftData(): Retrieving session data failed");
		transition(APISTATE_DO_DISCONNECT);
	}

	log_.debug("getFullAircraftData(): Done");
}

void MultiplayAPIHandler::getLocationData(const std::wstring& callsign, AircraftLocationCallback callback)
{
	log_.debug("getLocationData()");

	uri_builder uri(U("location/"));
	uri.append_path(callsign);

	log_.trace("getLocationData(): Calling GET at \"", uri.to_string(), "\"");

	http_request req(methods::GET);
	req.headers().add(U("Authorization"), token_);
	req.set_request_uri(uri.to_string());

	auto resp = client_.request(req).get();
	if (resp.status_code() == 200) {
		log_.trace("getLocationData(): Received location data for \"", callsign, "\"");

		error_code err;
		value locationInfo(resp.extract_json().get());
		AircraftLocationData result;

		fromJSON(result, locationInfo);

		callback(callsign, result);
	}
	else if (resp.status_code() == 401) {
		log_.error("getLocationData(): Lost authorization. resetting connection");
		transition(APISTATE_DISCONNECTED);
	}
	else {
		log_.error("getLocationData(): Retrieving session data failed");
		transition(APISTATE_DO_DISCONNECT);
	}

	log_.trace("getLocationData(): Done");
}

/*
 */
void MultiplayAPIHandler::login()
{
	transition(APISTATE_DISCONNECTED, APISTATE_DO_CONNECT);
}

void MultiplayAPIHandler::logout()
{
	transition(APISTATE_DO_DISCONNECT);
}

void MultiplayAPIHandler::updateAircraft(const AircraftIdData& aircraft)
{
	log_.debug("updateAircraft(): title=\"", aircraft.title, "\"");

	aircraft_ = aircraft;
	aircraftUpdated_ = true;
	haveAircraft_ = true;

	haveLocation_ = false;
	haveEngines_ = false;
	haveLights_ = false;
	haveControls_ = false;

	transition(APISTATE_CONNECTED, APISTATE_RESEND_AIRCRAFT);

	log_.debug("updateAircraft(): Done");
}

/*
 */
void MultiplayAPIHandler::setServer(const wstring& url, const wstring& wsUrl, const wstring&user, const wstring&pwd, const wstring&session, const wstring& callsign)
{
	if (state_ = APISTATE_INIT) {
		serverUrl_ = url;
		serverWsUrl_ = wsUrl;
		username_ = user;
		password_ = pwd;
		sessionName_ = session;
		callsign_ = callsign;

		client_ = http_client(uri(serverUrl_));

		transition(APISTATE_INIT, APISTATE_DISCONNECTED);
	}
}

/*
 */
void MultiplayAPIHandler::sendData(const AircraftIdData& aircraft)
{
	aircraft_ = aircraft;
	if (!haveAircraft_) {
		haveAircraft_ = true;
		log_.debug("sendData(): Received initial AircraftIdData");
	}
	aircraftUpdated_ = true;
	verhoog();
}
void MultiplayAPIHandler::sendData(const AircraftLocationData& location)
{
	location_ = location;
	if (!haveLocation_) {
		haveLocation_ = true;
		log_.debug("sendData(): Received initial AircraftLocationData");
	}
	locationUpdated_ = true;
	verhoog();
}
void MultiplayAPIHandler::sendData(const AircraftEngineData& engines)
{
	engines_ = engines;
	if (!haveEngines_) {
		haveEngines_ = true;
		log_.debug("sendData(): Received initial AircraftEngineData");
	}
	enginesUpdated_ = true;
	verhoog();
}
void MultiplayAPIHandler::sendData(const AircraftLightsData& lights)
{
	lights_ = lights;
	if (!haveLights_) {
		haveLights_ = true;
		log_.debug("sendData(): Received initial AircraftLightsData");
	}
	lightsUpdated_ = true;
	verhoog();
}
void MultiplayAPIHandler::sendData(const AircraftControlsData& controls)
{
	controls_ = controls;
	if (!haveControls_) {
		haveControls_ = true;
		log_.debug("sendData(): Received initial AircraftControlsData");
	}
	controlsUpdated_ = true;
	verhoog();
}
