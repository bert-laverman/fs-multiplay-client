// MultiPlayDll.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"

#include <string>
#include "../LibFsx/SimConnectManager.h"

#include "MultiPlayDll.h"

using namespace std;

using namespace nl::rakis;
using namespace nl::rakis::fsx;


/*
* SimConnectManager
*/
MULTIPLAYDLL_API void __stdcall scmgr_addSimconnectHandler(CsSimConnectEventHandler handler)
{
	SimConnectManager::instance().addSimConnectEventHandler("DllUser", [=](SimConnectEvent evt, DWORD dwData, const char* pszData) {
		handler(evt);
	});
}

MULTIPLAYDLL_API int        __stdcall scmgr_isConnected()
{
	return SimConnectManager::instance().isConnected() ? 1 : 0;
}

MULTIPLAYDLL_API const char* __stdcall scmgr_getSimName()
{
	return SimConnectManager::instance().getConnection().applicationName.c_str();
}

MULTIPLAYDLL_API int         __stdcall scmgr_getSimVersionMajor()
{
	return SimConnectManager::instance().getConnection().applicationVersionMajor;
}

MULTIPLAYDLL_API int         __stdcall scmgr_getSimVersionMinor()
{
	return SimConnectManager::instance().getConnection().applicationVersionMinor;
}

MULTIPLAYDLL_API int         __stdcall scmgr_getSimBuildMajor()
{
	return SimConnectManager::instance().getConnection().applicationBuildMajor;
}

MULTIPLAYDLL_API int         __stdcall scmgr_getSimBuildMinor()
{
	return SimConnectManager::instance().getConnection().applicationBuildMinor;
}

MULTIPLAYDLL_API int         __stdcall scmgr_getLibVersionMajor()
{
	return SimConnectManager::instance().getConnection().simConnectVersionMajor;
}

MULTIPLAYDLL_API int         __stdcall scmgr_getLibVersionMinor()
{
	return SimConnectManager::instance().getConnection().simConnectVersionMinor;
}

MULTIPLAYDLL_API int         __stdcall scmgr_getLibBuildMajor()
{
	return SimConnectManager::instance().getConnection().simConnectBuildMajor;
}

MULTIPLAYDLL_API int         __stdcall scmgr_getLibBuildMinor()
{
	return SimConnectManager::instance().getConnection().simConnectBuildMinor;
}

MULTIPLAYDLL_API const char* __stdcall scmgr_getAirfile()
{
	return SimConnectManager::instance().getUserAircraftIdData().airFilePath.c_str();
}

MULTIPLAYDLL_API const char* __stdcall scmgr_getTitle()
{
	return SimConnectManager::instance().getUserAircraftIdData().title.c_str();
}

MULTIPLAYDLL_API const char* __stdcall scmgr_getAtcType()
{
	return SimConnectManager::instance().getUserAircraftIdData().atcType.c_str();
}

MULTIPLAYDLL_API const char* __stdcall scmgr_getAtcModel()
{
	return SimConnectManager::instance().getUserAircraftIdData().atcModel.c_str();
}

MULTIPLAYDLL_API const char* __stdcall scmgr_getAtcId()
{
	return SimConnectManager::instance().getUserAircraftIdData().atcId.c_str();
}

MULTIPLAYDLL_API const char* __stdcall scmgr_getAtcAirline()
{
	return SimConnectManager::instance().getUserAircraftIdData().atcAirline.c_str();
}

MULTIPLAYDLL_API const char* __stdcall scmgr_getAtcFlightnumber()
{
	return SimConnectManager::instance().getUserAircraftIdData().atcFlightNumber.c_str();
}
