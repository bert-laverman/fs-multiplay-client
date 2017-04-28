// MultiPlayDll.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"

#include <string>
#include "../LibService/ServiceManager.h"
#include "../LibFsx/SimConnectManager.h"
#include "../LibMultiplay/MultiplayManager.h"

#include "MultiPlayDll.h"

using namespace std;

using namespace nl::rakis;
using namespace nl::rakis::fsx;
using namespace nl::rakis::service;


/*
 * ServiceManager
 */
MULTIPLAYDLL_API void __stdcall srvmgr_start()
{
	ServiceManager::instance().start();
}

MULTIPLAYDLL_API void __stdcall srvmgr_stop()
{
	ServiceManager::instance().stop();
}

MULTIPLAYDLL_API bool __stdcall srvmgr_isServiceRunning(const char* pszService)
{
	string service(pszService);

	return ServiceManager::instance().getService(service)->isRunning();
}

/*
 * MultiplayManager
 */
MULTIPLAYDLL_API void __stdcall mpmgr_connect()
{
	MultiplayManager::instance().connect();
}

MULTIPLAYDLL_API void __stdcall mpmgr_disconnect()
{
	MultiplayManager::instance().disconnect();
}

MULTIPLAYDLL_API const char* __stdcall mpmgr_getServerUrl()
{
	static string url;
	
	url = wstr2str(MultiplayManager::instance().getServerUrl());

	return url.c_str();
}

MULTIPLAYDLL_API void __stdcall mpmgr_setServerUrl(const char* serverUrl)
{
	string url(serverUrl);

	MultiplayManager::instance().setServerUrl(wstring(url.begin(), url.end()));
}

MULTIPLAYDLL_API const char* __stdcall mpmgr_getWsServerUrl()
{
	static string url;

	url = wstr2str(MultiplayManager::instance().getServerWsUrl());

	return url.c_str();
}

MULTIPLAYDLL_API void __stdcall mpmgr_setWsServerUrl(const char* wsServerUrl)
{
	string url(wsServerUrl);

	MultiplayManager::instance().setServerWsUrl(wstring(url.begin(), url.end()));
}

MULTIPLAYDLL_API const char* __stdcall mpmgr_getUsername()
{
	static string user;

	user = wstr2str(MultiplayManager::instance().getUsername());

	return user.c_str();
}

MULTIPLAYDLL_API void __stdcall mpmgr_setUsername(const char* username)
{
	string user(username);

	MultiplayManager::instance().setUsername(wstring(user.begin(), user.end()));
}

MULTIPLAYDLL_API const char* __stdcall mpmgr_getPassword()
{
	static string pwd;

	pwd = wstr2str(MultiplayManager::instance().getPassword());

	return pwd.c_str();
}

MULTIPLAYDLL_API void __stdcall mpmgr_setPassword(const char* password)
{
	string pwd(password);

	MultiplayManager::instance().setPassword(wstring(pwd.begin(), pwd.end()));
}

MULTIPLAYDLL_API const char* __stdcall mpmgr_getSession()
{
	static string sess;

	sess = wstr2str(MultiplayManager::instance().getSession());

	return sess.c_str();
}

MULTIPLAYDLL_API void __stdcall mpmgr_setSession(const char* session)
{
	string sess(session);

	MultiplayManager::instance().setSession(wstring(sess.begin(), sess.end()));
}

MULTIPLAYDLL_API const char* __stdcall mpmgr_getCallsign()
{
	static string csign;

	csign = wstr2str(MultiplayManager::instance().getCallsign());

	return csign.c_str();
}

MULTIPLAYDLL_API void __stdcall mpmgr_setCallsign(const char* callsign)
{
	string csign(callsign);

	MultiplayManager::instance().setCallsign(wstring(csign.begin(), csign.end()));
}

MULTIPLAYDLL_API bool __stdcall mpmgr_getOverrideCallsign()
{
	return MultiplayManager::instance().isCallsignOverride();
}

MULTIPLAYDLL_API void __stdcall mpmgr_setOverrideCallsign(bool overrideCallsign)
{
	MultiplayManager::instance().setCallsignOverride(overrideCallsign);
}


/*
 * SimConnectManager
 */
MULTIPLAYDLL_API void __stdcall scmgr_addSimconnectHandler(CsSimConnectEventHandler handler)
{
	SimConnectManager::instance().addSimConnectEventHandler("DllUser", [=](SimConnectEvent evt, DWORD dwData, const char* pszData) {
		handler(evt);
	});
}
