// dllmain.cpp : Defines the entry point for the DLL application.
#include "stdafx.h"

#include<atomic>
#include <array>

#include "../LibService/SettingsManager.h"
#include "../LibService/ServiceManager.h"
#include "../LibFsx/FsxManager.h"
#include "../LibFsx/SimConnectManager.h"
#include "../LibAI/AIManager.h"
#include "../LibMultiplay/MultiplayManager.h"

using namespace nl::rakis;
using namespace nl::rakis::fsx;
using namespace nl::rakis::service;


using namespace std;

static atomic_flag initDone = ATOMIC_FLAG_INIT;

static void initialize()
{
	HMODULE hModule = ::GetModuleHandle(NULL);

	File appPath;
	size_t len;
	array<char, MAX_PATH> home;

	if (getenv_s(&len, home.data(), MAX_PATH, "FIPSERVER_HOME") == 0) {
		appPath = home.data();
	}
	else if (hModule != NULL)  {
		TCHAR modPath[MAX_PATH];
		GetModuleFileName(hModule, modPath, sizeof(modPath));
		File modFile(modPath);
		appPath = modFile.getParent();
	}

	if (appPath.exists() && appPath.isDirectory()) {
		File configPath(appPath.getChild("config"));
		File logProps(configPath.getChild("rakislog.properties"));

		if (logProps.exists()) {
			Logger::configure(logProps.getPath());
		}
		Logger log(Logger::getLogger("FipServer"));
		log.info("Starting up");

		SettingsManager& settings(SettingsManager::instance());
		settings.init("FIPServer", appPath);

		ServiceManager& srvMgr(ServiceManager::instance());
		srvMgr.addService(FsxManager::instance());
		srvMgr.addService(AIManager::instance());
		srvMgr.addService(MultiplayManager::instance());
	}
	else {
		MessageBox(NULL, L"FIPSERVER_HOME not set!\nI don't know where I am...", L"FSMultiplayClient DLL initialization failed", MB_OK);
	}
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
					 )
{
	if (!initDone.test_and_set()) {
		initialize();
	}

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}

