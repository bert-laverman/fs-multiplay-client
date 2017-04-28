/**
 * MultiPlayDll
 */
#ifdef MULTIPLAYDLL_EXPORTS
#define MULTIPLAYDLL_API __declspec(dllexport)
#else
#define MULTIPLAYDLL_API __declspec(dllimport)
#endif

typedef void(__stdcall * CsSimConnectEventHandler)(int32_t iEvent);

extern "C" {

	MULTIPLAYDLL_API void __stdcall srvmgr_start();
	MULTIPLAYDLL_API void __stdcall srvmgr_stop();
	MULTIPLAYDLL_API bool __stdcall srvmgr_isServiceRunning(const char* pszService);

	MULTIPLAYDLL_API void __stdcall mpmgr_connect();
	MULTIPLAYDLL_API void __stdcall mpmgr_disconnect();
	MULTIPLAYDLL_API const char* __stdcall mpmgr_getServerUrl();
	MULTIPLAYDLL_API void __stdcall mpmgr_setServerUrl(const char* serverUrl);
	MULTIPLAYDLL_API const char* __stdcall mpmgr_getWsServerUrl();
	MULTIPLAYDLL_API void __stdcall mpmgr_setWsServerUrl(const char* wsServerUrl);
	MULTIPLAYDLL_API const char* __stdcall mpmgr_getUsername();
	MULTIPLAYDLL_API void __stdcall mpmgr_setUsername(const char* username);
	MULTIPLAYDLL_API const char* __stdcall mpmgr_getPassword();
	MULTIPLAYDLL_API void __stdcall mpmgr_setPassword(const char* password);
	MULTIPLAYDLL_API const char* __stdcall mpmgr_getSession();
	MULTIPLAYDLL_API void __stdcall mpmgr_setSession(const char* session);
	MULTIPLAYDLL_API const char* __stdcall mpmgr_getCallsign();
	MULTIPLAYDLL_API void __stdcall mpmgr_setCallsign(const char* callsign);
	MULTIPLAYDLL_API bool __stdcall mpmgr_getOverrideCallsign();
	MULTIPLAYDLL_API void __stdcall mpmgr_setOverrideCallsign(bool overrideCallsign);

	MULTIPLAYDLL_API void __stdcall scmgr_addSimconnectHandler(CsSimConnectEventHandler handler);
}
