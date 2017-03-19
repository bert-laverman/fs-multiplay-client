// FSMultiplayClient.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <array>

#include <Windows.h>

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

int main()
{
	HMODULE hModule = ::GetModuleHandle(NULL);

	if (hModule != NULL)
	{
		File appPath;
		size_t len;
		array<char, MAX_PATH> home;

		if (getenv_s(&len, home.data(), MAX_PATH, "FIPSERVER_HOME") == 0) {
			appPath = home.data();
		}
		else {
			TCHAR modPath[MAX_PATH];
			GetModuleFileName(hModule, modPath, sizeof(modPath));
			File modFile(modPath);
			appPath = modFile.getParent();
		}
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

		srvMgr.start();

		SimConnectManager& scMgr(SimConnectManager::instance());
		while (!scMgr.isConnected()) {
			Sleep(10000);
		}
		while (scMgr.isConnected()) {
			Sleep(10000);
		}
	}
	return 0;
}

