/**
 * MultiPlayDll
 */
#ifdef MULTIPLAYDLL_EXPORTS
#define MULTIPLAYDLL_API __declspec(dllexport)
#else
#define MULTIPLAYDLL_API __declspec(dllimport)
#endif

typedef void(__stdcall * CsSimConnectEventHandler)(int32_t iEvent);
typedef void(__stdcall * CsMultiplayEventHandler)(int32_t iEvent);
typedef void(__stdcall * CsMultiplayDataChangedHandler)();
typedef void(__stdcall * CsAIEventHandler)();

extern "C" {

	MULTIPLAYDLL_API void __stdcall srvmgr_start();
	MULTIPLAYDLL_API void __stdcall srvmgr_stop();
	MULTIPLAYDLL_API int  __stdcall srvmgr_isServiceRunning(const char* pszService);

	MULTIPLAYDLL_API void        __stdcall scmgr_addSimconnectHandler(CsSimConnectEventHandler handler);
	MULTIPLAYDLL_API int         __stdcall scmgr_isConnected();
	MULTIPLAYDLL_API const char* __stdcall scmgr_getSimName();
	MULTIPLAYDLL_API int         __stdcall scmgr_getSimVersionMajor();
	MULTIPLAYDLL_API int         __stdcall scmgr_getSimVersionMinor();
	MULTIPLAYDLL_API int         __stdcall scmgr_getSimBuildMajor();
	MULTIPLAYDLL_API int         __stdcall scmgr_getSimBuildMinor();
	MULTIPLAYDLL_API int         __stdcall scmgr_getLibVersionMajor();
	MULTIPLAYDLL_API int         __stdcall scmgr_getLibVersionMinor();
	MULTIPLAYDLL_API int         __stdcall scmgr_getLibBuildMajor();
	MULTIPLAYDLL_API int         __stdcall scmgr_getLibBuildMinor();
	MULTIPLAYDLL_API const char* __stdcall scmgr_getAirfile();
	MULTIPLAYDLL_API const char* __stdcall scmgr_getTitle();
	MULTIPLAYDLL_API const char* __stdcall scmgr_getAtcType();
	MULTIPLAYDLL_API const char* __stdcall scmgr_getAtcModel();
	MULTIPLAYDLL_API const char* __stdcall scmgr_getAtcId();
	MULTIPLAYDLL_API const char* __stdcall scmgr_getAtcAirline();
	MULTIPLAYDLL_API const char* __stdcall scmgr_getAtcFlightnumber();

	MULTIPLAYDLL_API void __stdcall mpmgr_addMultiplayHandler(CsMultiplayEventHandler handler);
	MULTIPLAYDLL_API void __stdcall mpmgr_addMultiplayDataChangedHandler(CsMultiplayDataChangedHandler handler);
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
	MULTIPLAYDLL_API int  __stdcall mpmgr_getOverrideCallsign();
	MULTIPLAYDLL_API void __stdcall mpmgr_setOverrideCallsign(int  overrideCallsign);

	MULTIPLAYDLL_API void __stdcall aimgr_addAIHandler(CsAIEventHandler handler);

}
