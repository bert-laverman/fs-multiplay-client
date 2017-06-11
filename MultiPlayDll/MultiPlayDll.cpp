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

MULTIPLAYDLL_API int __stdcall srvmgr_isServiceRunning(const char* pszService)
{
	string service(pszService);

	return ServiceManager::instance().getService(service)->isRunning() ? 1 : 0;
}
