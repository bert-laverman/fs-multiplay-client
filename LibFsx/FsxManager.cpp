/**
 */
#include "stdafx.h"

#include <strstream>

#include <SimConnect.h>

#include "../LibService/SettingsManager.h"

#include "../LibRakis/XMLFile.h"

#include "FsxManager.h"
#include "SimConnectManager.h"


using namespace std;

using namespace nl::rakis::service;
using namespace nl::rakis::fsx;
using namespace nl::rakis;


/*static*/ Logger FsxManager::log_(Logger::getLogger("nl.rakis.fsx.FsxManager"));


FsxManager::FsxManager()
    : Service("FsxManager")
{
    // Force init order
    //SettingsManager::instance().getAppName();

    log_.debug("FsxManager()");

    setAutoStart(true);

    log_.debug("FsxManager(): Done");
}

FsxManager::~FsxManager()
{
    log_.debug("~FsxManager()");
}

FsxManager& FsxManager::instance()
{
	static FsxManager        instance_;

	return instance_;
}

void FsxManager::addFsxData(FsxDataPtr serverData)
{
    log_.debug("addFsxData()");

    string name(serverData->getName());
    auto gaugeIt = data_.find(name);
    if (gaugeIt != data_.end()) {
        gaugeIt->second = serverData;
    }
    else {
        data_.insert(make_pair(name, serverData));
    }

    log_.debug("addFsxData(): Done");
}

void FsxManager::removeFsxData(string const& name)
{
    auto gauge = data_.find(name);
    if (gauge != data_.end()) {
        data_.erase(gauge);
    }
}

void FsxManager::doStart()
{
    log_.debug("doStart()");
	SettingsManager& settings(SettingsManager::instance());

	for (auto gPair : settings.getTree("events.modules")) {
		string name(gPair.second.get<string>("name"));
		XMLFile f(settings.getConfigPath().getChild(gPair.second.get<string>("file")));

		log_.debug("doStart(): Loading event module \"", name, "\" from \"", f.getPath1(), "\"");
		EventModule& mod(eventModules_[name]);

		// First scan, collect all third-party events
		XmlNode* evt = f.document().first_node("event", 0, false);
		while (evt != nullptr) {
			string evtName(getNodeText(evt, "name"));
			const char* evtOffsetText = getNodeText(evt, "third-party");
			int evtOffset = (evtOffsetText == nullptr) ? -1 : atoi(evtOffsetText);
			if (evtOffset != -1) {
				mod.map[evtName] = THIRD_PARTY_EVENT_ID_MIN + evtOffset;
			}

			evt = evt->next_sibling("event", 0, false);
		}
		size_t numEvents = mod.map.size();

		// Second scan, try to resolve aliases
		evt = f.document().first_node("event", 0, false);
		while (evt != nullptr) {
			string evtName(getNodeNonNullText(evt, "name"));
			string evtAlias(getNodeNonNullText(evt, "alias"));
			if (!evtAlias.empty()) {
				auto ref = mod.map.find(evtAlias);
				if (ref == mod.map.end()) {
					log_.error("doStart(): Cannot find \"", evtAlias, "\", referenced by \"", evtName, "\"");
				}
				else {
					mod.map[evtName] = ref->second;
				}
			}

			evt = evt->next_sibling("event", 0, false);
		}
		size_t numAliases = mod.map.size() - numEvents;
		log_.info("doStart(): Event Module \"", name, "\" loaded, ", numEvents, " event(s), ", numAliases, " alias(es)");
	}

	for (auto gPair : settings.getTree("events.aliases")) {
		string name(gPair.second.get<string>("name"));
		string module(gPair.second.get<string>("module"));

		modAliases_[name] = module;
	}
	log_.info("doStart(): ", modAliases_.size(), " Event Module alias(es) defined");

	setAutoStart(true);
    log_.debug("doStart(): Subscribing to SimConnectEvents");
    SimConnectManager::instance().addSimConnectEventHandler("FsxManager", [&](SimConnectEvent evt, DWORD data, const char* arg) {
        switch (evt) {
        case SCE_CONNECT: onConnect(); break;
        case SCE_DISCONNECT: onDisconnect(); break;
        case SCE_START: onSimStart(); break;
        case SCE_STOP: onSimStop(); break;
        case SCE_PAUSE: onSimPaused(); break;
        case SCE_UNPAUSE: onSimUnpaused(); break;
        case SCE_AIRCRAFTLOADED: onAircraftLoaded(arg); break;
		case SCE_OBJECTADDED: onObjectAdded(); break;
		case SCE_OBJECTREMOVED: onObjectRemoved(); break;
        }
        return false; // I'm not the last one to call
    });

    SimConnectManager::instance().start();

    log_.debug("doStart(): Done");
}

void FsxManager::doStop()
{
    log_.debug("doStop()");

    SimConnectManager::instance().stopRunning();

    log_.debug("doStop(): Done");
}

void FsxManager::onConnect()
{
    ConnectionData const& data(SimConnectManager::instance().getConnection());

    log_.info("onConnect()");
    log_.info("onConnect(): Simulator=\"", data.applicationName, "\", version=", data.applicationVersionMajor, ".", data.applicationVersionMinor
       , " (Build ", data.applicationBuildMajor, ".", data.applicationBuildMinor, ")");
    log_.info("onConnect(): SimConnect version=", data.simConnectVersionMajor, ".", data.simConnectVersionMinor
       , " (Build ", data.simConnectBuildMajor, ".", data.simConnectBuildMinor, ")");
}

void FsxManager::onDisconnect()
{
    log_.info("onDisconnect()");
}

void FsxManager::onSimStart()
{
    log_.trace("onSimStart()");
}

void FsxManager::onSimStop()
{
	log_.trace("onSimStop()");
}

void FsxManager::onSimPaused()
{
	log_.trace("onSimPaused()");
}

void FsxManager::onSimUnpaused()
{
	log_.trace("onSimUnpaused()");
}

void FsxManager::onObjectAdded()
{
	log_.trace("onObjectAdded()");
}

void FsxManager::onObjectRemoved()
{
	log_.trace("onObjectRemoved()");
}

void FsxManager::onAircraftLoaded(const char* airPath)
{
	if (airPath == nullptr) {
		log_.debug("onAircraftLoaded(): no airPath");
	}
	else {
		log_.info("onAircraftLoaded(): airPath=\"", airPath, "\"");
	}

    const AircraftIdData& aircraft(SimConnectManager::instance().getUserAircraftIdData());

	log_.info("onAircraftLoaded(): Aircraft airPath      = \"", aircraft.airFilePath, "\"");
	log_.info("onAircraftLoaded(): Aircraft ATC Type     = \"", aircraft.atcType, "\"");
	log_.info("onAircraftLoaded(): Aircraft ATC Model    = \"", aircraft.atcModel, "\"");
    log_.info("onAircraftLoaded(): Aircraft ATC Id       = \"", aircraft.atcId, "\"");
    log_.info("onAircraftLoaded(): Aircraft ATC Airline  = \"", aircraft.atcAirline, "\"");
    log_.info("onAircraftLoaded(): Aircraft ATC Flight # = \"", aircraft.atcFlightNumber, "\"");
    log_.info("onAircraftLoaded(): Aircraft Title        = \"", aircraft.title, "\"");

	if (!aircraft.airFilePath.empty()) {
		pmdgNgx_ = false;
		pmdg777_ = false;

		File airFile(aircraft.airFilePath);
		File airDir(airFile.getParent());
		string dirName(airDir.getName1());

		pmdgNgx_ = (dirName.substr(0, 9).compare("PMDG 737-") == 0);
		if (isPmdgNgx()) {
			log_.info("onAircraftLoaded(): PMDG 737 NGX detected!");
			SimConnectManager& mgr(SimConnectManager::instance());

			// Callback for NGX data
			log_.info("onAircraftLoaded(): Setting up for PMDG_NGX_Data structure");
			mgr.mapClientDataName(PMDG_NGX_DATA_ID, PMDG_NGX_DATA_NAME);
			mgr.addToClientDataDef(PMDG_NGX_DATA_ID, PMDG_NGX_DATA_DEFINITION, 0, sizeof(pmdgNgxData_));
			log_.info("onAircraftLoaded(): Request sending PMDG_NGX_Data structure when changed");
			mgr.requestClientDataWhenChanged(PMDG_NGX_DATA_ID, [this](int entryNr, int outOfNr, void* data, size_t size) -> bool {
				memcpy(&pmdgNgxData_, data, sizeof(pmdgNgxData_));
				onPmdgNgxData();
				return true;
			});

			// Callback for NGX left CDU (CDU 0)
			log_.info("onAircraftLoaded(): Setting up for PMDG_NGX_CDU_Screen structure (left CDU)");
			mgr.mapClientDataName(PMDG_NGX_CDU_0_ID, PMDG_NGX_CDU_0_NAME);
			mgr.addToClientDataDef(PMDG_NGX_CDU_0_ID, PMDG_NGX_CDU_0_DEFINITION, 0, sizeof(pmdgNgxCdu0_));
			log_.info("onAircraftLoaded(): Request sending left CDU screen when changed");
			mgr.requestClientDataWhenChanged(PMDG_NGX_CDU_0_ID, [this](int entryNr, int outOfNr, void* data, size_t size) -> bool {
				memcpy(&pmdgNgxCdu0_, data, sizeof(pmdgNgxCdu0_));
				onPmdgNgxCdu0();
				return true;
			});

			// Callback for NGX right CDU (CDU 1)
			log_.info("onAircraftLoaded(): Setting up for PMDG_NGX_CDU_Screen structure (right CDU)");
			mgr.mapClientDataName(PMDG_NGX_CDU_1_ID, PMDG_NGX_CDU_1_NAME);
			mgr.addToClientDataDef(PMDG_NGX_CDU_1_ID, PMDG_NGX_CDU_1_DEFINITION, 0, sizeof(pmdgNgxCdu1_));
			log_.info("onAircraftLoaded(): Request sending right CDU screen when changed");
			mgr.requestClientDataWhenChanged(PMDG_NGX_CDU_1_ID, [this](int entryNr, int outOfNr, void* data, size_t size) -> bool {
				memcpy(&pmdgNgxCdu1_, data, sizeof(pmdgNgxCdu1_));
				onPmdgNgxCdu1();
				return true;
			});
		}

		pmdg777_ = (dirName.substr(0, 9).compare("PMDG 777-") == 0);
		if (isPmdg777()) {
			log_.info("onAircraftLoaded(): PMDG 777 detected!");
			SimConnectManager& mgr(SimConnectManager::instance());

			// Callback for NGX data
			log_.info("onAircraftLoaded(): Setting up for PMDG_777X_Data structure");
			mgr.mapClientDataName(PMDG_777X_DATA_ID, PMDG_777X_DATA_NAME);
			mgr.addToClientDataDef(PMDG_777X_DATA_ID, PMDG_777X_DATA_DEFINITION, 0, sizeof(pmdg777Data_));
			log_.info("onAircraftLoaded(): Request sending PMDG_777X_Data structure when changed");
			mgr.requestClientDataWhenChanged(PMDG_777X_DATA_ID, [this](int entryNr, int outOfNr, void* data, size_t size) -> bool {
				memcpy(&pmdg777Data_, data, sizeof(pmdg777Data_));
				onPmdg777Data();
				return true;
			});

			// Callback for 777 left CDU (CDU 0)
			log_.info("onAircraftLoaded(): Setting up for PMDG_777X_CDU_Screen structure (left CDU)");
			mgr.mapClientDataName(PMDG_777X_CDU_0_ID, PMDG_777X_CDU_0_NAME);
			mgr.addToClientDataDef(PMDG_777X_CDU_0_ID, PMDG_777X_CDU_0_DEFINITION, 0, sizeof(pmdg777Cdu0_));
			log_.info("onAircraftLoaded(): Request sending left CDU screen when changed");
			mgr.requestClientDataWhenChanged(PMDG_777X_CDU_0_ID, [this](int entryNr, int outOfNr, void* data, size_t size) -> bool {
				memcpy(&pmdg777Cdu0_, data, sizeof(pmdg777Cdu0_));
				onPmdg777Cdu0();
				return true;
			});

			// Callback for 777 right CDU (CDU 1)
			log_.info("onAircraftLoaded(): Setting up for PMDG_777X_CDU_Screen structure (right CDU)");
			mgr.mapClientDataName(PMDG_777X_CDU_1_ID, PMDG_777X_CDU_1_NAME);
			mgr.addToClientDataDef(PMDG_777X_CDU_1_ID, PMDG_777X_CDU_1_DEFINITION, 0, sizeof(pmdg777Cdu1_));
			log_.info("onAircraftLoaded(): Request sending right CDU screen when changed");
			mgr.requestClientDataWhenChanged(PMDG_777X_CDU_1_ID, [this](int entryNr, int outOfNr, void* data, size_t size) -> bool {
				memcpy(&pmdg777Cdu1_, data, sizeof(pmdg777Cdu1_));
				onPmdg777Cdu1();
				return true;
			});

			// Callback for 777 center CDU (CDU 2)
			log_.info("onAircraftLoaded(): Setting up for PMDG_777X_CDU_Screen structure (right CDU)");
			mgr.mapClientDataName(PMDG_777X_CDU_2_ID, PMDG_777X_CDU_2_NAME);
			mgr.addToClientDataDef(PMDG_777X_CDU_2_ID, PMDG_777X_CDU_2_DEFINITION, 0, sizeof(pmdg777Cdu2_));
			log_.info("onAircraftLoaded(): Request sending center CDU screen when changed");
			mgr.requestClientDataWhenChanged(PMDG_777X_CDU_2_ID, [this](int entryNr, int outOfNr, void* data, size_t size) -> bool {
				memcpy(&pmdg777Cdu2_, data, sizeof(pmdg777Cdu2_));
				onPmdg777Cdu2();
				return true;
			});
		}
	}
}

void FsxManager::onPmdgNgxData()
{
	log_.trace("onPmdgNgxData()");

	log_.trace("onPmdgNgxData(): Done");
}

void FsxManager::onPmdgNgxCdu0()
{
	log_.info("onPmdgNgxCdu0()");

	//for (size_t row = 0; row < CDU_ROWS; row++) {
	//	log_.info("onPmdgNgxCdu0(): Row ", ((row < 9) ? " " : ""), (row+1), " \""
	//		<< char(pmdgNgxCdu0_.Cells[0][row].Symbol), char(pmdgNgxCdu0_.Cells[1][row].Symbol), char(pmdgNgxCdu0_.Cells[2][row].Symbol)
	//		<< char(pmdgNgxCdu0_.Cells[3][row].Symbol), char(pmdgNgxCdu0_.Cells[4][row].Symbol), char(pmdgNgxCdu0_.Cells[5][row].Symbol)
	//		<< char(pmdgNgxCdu0_.Cells[6][row].Symbol), char(pmdgNgxCdu0_.Cells[7][row].Symbol), char(pmdgNgxCdu0_.Cells[8][row].Symbol)
	//		<< char(pmdgNgxCdu0_.Cells[9][row].Symbol), char(pmdgNgxCdu0_.Cells[10][row].Symbol), char(pmdgNgxCdu0_.Cells[11][row].Symbol)
	//		<< char(pmdgNgxCdu0_.Cells[12][row].Symbol), char(pmdgNgxCdu0_.Cells[13][row].Symbol), char(pmdgNgxCdu0_.Cells[14][row].Symbol)
	//		<< char(pmdgNgxCdu0_.Cells[15][row].Symbol), char(pmdgNgxCdu0_.Cells[16][row].Symbol), char(pmdgNgxCdu0_.Cells[17][row].Symbol)
	//		<< char(pmdgNgxCdu0_.Cells[18][row].Symbol), char(pmdgNgxCdu0_.Cells[19][row].Symbol), char(pmdgNgxCdu0_.Cells[20][row].Symbol)
	//		<< char(pmdgNgxCdu0_.Cells[21][row].Symbol), char(pmdgNgxCdu0_.Cells[22][row].Symbol), char(pmdgNgxCdu0_.Cells[23][row].Symbol)
	//		<< "\"");
	//}

	log_.trace("onPmdgNgxCdu0(): Done");
}

void FsxManager::onPmdgNgxCdu1()
{
	log_.info("onPmdgNgxCdu1()");

	//for (size_t row = 0; row < CDU_ROWS; row++) {
	//	log_.info("onPmdgNgxCdu1(): Row ", ((row < 9) ? " " : ""), (row + 1), " \""
	//		<< char(pmdgNgxCdu1_.Cells[0][row].Symbol), char(pmdgNgxCdu1_.Cells[1][row].Symbol), char(pmdgNgxCdu1_.Cells[2][row].Symbol)
	//		<< char(pmdgNgxCdu1_.Cells[3][row].Symbol), char(pmdgNgxCdu1_.Cells[4][row].Symbol), char(pmdgNgxCdu1_.Cells[5][row].Symbol)
	//		<< char(pmdgNgxCdu1_.Cells[6][row].Symbol), char(pmdgNgxCdu1_.Cells[7][row].Symbol), char(pmdgNgxCdu1_.Cells[8][row].Symbol)
	//		<< char(pmdgNgxCdu1_.Cells[9][row].Symbol), char(pmdgNgxCdu1_.Cells[10][row].Symbol), char(pmdgNgxCdu1_.Cells[11][row].Symbol)
	//		<< char(pmdgNgxCdu1_.Cells[12][row].Symbol), char(pmdgNgxCdu1_.Cells[13][row].Symbol), char(pmdgNgxCdu1_.Cells[14][row].Symbol)
	//		<< char(pmdgNgxCdu1_.Cells[15][row].Symbol), char(pmdgNgxCdu1_.Cells[16][row].Symbol), char(pmdgNgxCdu1_.Cells[17][row].Symbol)
	//		<< char(pmdgNgxCdu1_.Cells[18][row].Symbol), char(pmdgNgxCdu1_.Cells[19][row].Symbol), char(pmdgNgxCdu1_.Cells[20][row].Symbol)
	//		<< char(pmdgNgxCdu1_.Cells[21][row].Symbol), char(pmdgNgxCdu1_.Cells[22][row].Symbol), char(pmdgNgxCdu1_.Cells[23][row].Symbol)
	//		<< "\"");
	//}

	log_.trace("onPmdgNgxCdu1(): Done");
}

void FsxManager::onPmdg777Data()
{
	log_.trace("onPmdg777Data()");

	log_.trace("onPmdg777Data(): Done");
}

void FsxManager::onPmdg777Cdu0()
{
	log_.info("onPmdg777Cdu0()");

	//for (size_t row = 0; row < CDU_ROWS; row++) {
	//	log_.info("onPmdg777Cdu0(): Row ", ((row < 9) ? " " : ""), (row + 1), " \""
	//		<< char(pmdg777Cdu0_.Cells[0][row].Symbol), char(pmdg777Cdu0_.Cells[1][row].Symbol), char(pmdg777Cdu0_.Cells[2][row].Symbol)
	//		<< char(pmdg777Cdu0_.Cells[3][row].Symbol), char(pmdg777Cdu0_.Cells[4][row].Symbol), char(pmdg777Cdu0_.Cells[5][row].Symbol)
	//		<< char(pmdg777Cdu0_.Cells[6][row].Symbol), char(pmdg777Cdu0_.Cells[7][row].Symbol), char(pmdg777Cdu0_.Cells[8][row].Symbol)
	//		<< char(pmdg777Cdu0_.Cells[9][row].Symbol), char(pmdg777Cdu0_.Cells[10][row].Symbol), char(pmdg777Cdu0_.Cells[11][row].Symbol)
	//		<< char(pmdg777Cdu0_.Cells[12][row].Symbol), char(pmdg777Cdu0_.Cells[13][row].Symbol), char(pmdg777Cdu0_.Cells[14][row].Symbol)
	//		<< char(pmdg777Cdu0_.Cells[15][row].Symbol), char(pmdg777Cdu0_.Cells[16][row].Symbol), char(pmdg777Cdu0_.Cells[17][row].Symbol)
	//		<< char(pmdg777Cdu0_.Cells[18][row].Symbol), char(pmdg777Cdu0_.Cells[19][row].Symbol), char(pmdg777Cdu0_.Cells[20][row].Symbol)
	//		<< char(pmdg777Cdu0_.Cells[21][row].Symbol), char(pmdg777Cdu0_.Cells[22][row].Symbol), char(pmdg777Cdu0_.Cells[23][row].Symbol)
	//		<< "\"");
	//}

	log_.trace("onPmdg777Cdu0(): Done");
}

void FsxManager::onPmdg777Cdu1()
{
	log_.info("onPmdg777Cdu1()");

	//for (size_t row = 0; row < CDU_ROWS; row++) {
	//	log_.info("onPmdg777Cdu1(): Row ", ((row < 9) ? " " : ""), (row + 1), " \""
	//		<< char(pmdg777Cdu1_.Cells[0][row].Symbol), char(pmdg777Cdu1_.Cells[1][row].Symbol), char(pmdg777Cdu1_.Cells[2][row].Symbol)
	//		<< char(pmdg777Cdu1_.Cells[3][row].Symbol), char(pmdg777Cdu1_.Cells[4][row].Symbol), char(pmdg777Cdu1_.Cells[5][row].Symbol)
	//		<< char(pmdg777Cdu1_.Cells[6][row].Symbol), char(pmdg777Cdu1_.Cells[7][row].Symbol), char(pmdg777Cdu1_.Cells[8][row].Symbol)
	//		<< char(pmdg777Cdu1_.Cells[9][row].Symbol), char(pmdg777Cdu1_.Cells[10][row].Symbol), char(pmdg777Cdu1_.Cells[11][row].Symbol)
	//		<< char(pmdg777Cdu1_.Cells[12][row].Symbol), char(pmdg777Cdu1_.Cells[13][row].Symbol), char(pmdg777Cdu1_.Cells[14][row].Symbol)
	//		<< char(pmdg777Cdu1_.Cells[15][row].Symbol), char(pmdg777Cdu1_.Cells[16][row].Symbol), char(pmdg777Cdu1_.Cells[17][row].Symbol)
	//		<< char(pmdg777Cdu1_.Cells[18][row].Symbol), char(pmdg777Cdu1_.Cells[19][row].Symbol), char(pmdg777Cdu1_.Cells[20][row].Symbol)
	//		<< char(pmdg777Cdu1_.Cells[21][row].Symbol), char(pmdg777Cdu1_.Cells[22][row].Symbol), char(pmdg777Cdu1_.Cells[23][row].Symbol)
	//		<< "\"");
	//}

	log_.trace("onPmdg777Cdu1(): Done");
}

void FsxManager::onPmdg777Cdu2()
{
	log_.info("onPmdg777Cdu2()");

	//for (size_t row = 0; row < CDU_ROWS; row++) {
	//	log_.info("onPmdg777Cdu2(): Row ", ((row < 9) ? " " : ""), (row + 1), " \""
	//		<< char(pmdg777Cdu2_.Cells[0][row].Symbol), char(pmdg777Cdu2_.Cells[1][row].Symbol), char(pmdg777Cdu2_.Cells[2][row].Symbol)
	//		<< char(pmdg777Cdu2_.Cells[3][row].Symbol), char(pmdg777Cdu2_.Cells[4][row].Symbol), char(pmdg777Cdu2_.Cells[5][row].Symbol)
	//		<< char(pmdg777Cdu2_.Cells[6][row].Symbol), char(pmdg777Cdu2_.Cells[7][row].Symbol), char(pmdg777Cdu2_.Cells[8][row].Symbol)
	//		<< char(pmdg777Cdu2_.Cells[9][row].Symbol), char(pmdg777Cdu2_.Cells[10][row].Symbol), char(pmdg777Cdu2_.Cells[11][row].Symbol)
	//		<< char(pmdg777Cdu2_.Cells[12][row].Symbol), char(pmdg777Cdu2_.Cells[13][row].Symbol), char(pmdg777Cdu2_.Cells[14][row].Symbol)
	//		<< char(pmdg777Cdu2_.Cells[15][row].Symbol), char(pmdg777Cdu2_.Cells[16][row].Symbol), char(pmdg777Cdu2_.Cells[17][row].Symbol)
	//		<< char(pmdg777Cdu2_.Cells[18][row].Symbol), char(pmdg777Cdu2_.Cells[19][row].Symbol), char(pmdg777Cdu2_.Cells[20][row].Symbol)
	//		<< char(pmdg777Cdu2_.Cells[21][row].Symbol), char(pmdg777Cdu2_.Cells[22][row].Symbol), char(pmdg777Cdu2_.Cells[23][row].Symbol)
	//		<< "\"");
	//}

	log_.trace("onPmdg777Cdu2(): Done");
}

static PMDG_NGX_CDU_Screen dummyScreen = {
	{
		{
			{ 'R', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },{ 'R', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },{ 'R', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },{ 'R', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },
			{ 'R', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },{ 'R', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },{ 'R', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },{ 'R', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },
			{ 'R', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },{ 'R', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },{ 'R', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },{ 'R', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },
			{ 'R', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_UNUSED },{ 'R', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_UNUSED }
		},
		{
			{ 'o', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },{ 'o', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },{ 'o', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },{ 'o', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },
			{ 'o', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },{ 'o', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },{ 'o', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },{ 'o', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },
			{ 'o', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },{ 'o', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },{ 'o', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },{ 'o', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },
			{ 'o', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_UNUSED },{ 'o', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_UNUSED }
		},
		{
			{ 'w', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },{ 'w', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },{ 'w', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },{ 'w', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },
			{ 'w', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },{ 'w', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },{ 'w', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },{ 'w', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },
			{ 'w', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },{ 'w', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },{ 'w', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },{ 'w', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },
			{ 'w', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_UNUSED },{ 'w', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_UNUSED }
		},
		{
			{ ' ', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },{ ' ', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },{ ' ', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },{ ' ', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },
			{ ' ', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },{ ' ', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },{ ' ', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },{ ' ', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },
			{ ' ', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },{ ' ', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },{ ' ', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },{ ' ', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },
			{ ' ', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_UNUSED },{ ' ', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_UNUSED }
		},
		{
			{ ' ', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },{ ' ', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },{ ' ', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },{ ' ', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },
			{ ' ', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },{ ' ', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },{ ' ', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },{ ' ', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },
			{ ' ', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },{ '1', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },{ '1', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },{ '1', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },
			{ '1', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_UNUSED },{ '1', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_UNUSED }
		},
		{
			{ '1', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },{ '2', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },{ '3', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },{ '4', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },
			{ '5', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },{ '6', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },{ '7', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },{ '8', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },
			{ '9', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },{ '0', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },{ '1', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },{ '2', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },
			{ '3', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_UNUSED },{ '4', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_UNUSED }
		},
		{
			{ ' ', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },{ ' ', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },{ ' ', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },{ ' ', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },
			{ ' ', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },{ ' ', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },{ ' ', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },{ ' ', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },
			{ ' ', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },{ ' ', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },{ ' ', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },{ ' ', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },
			{ ' ', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_UNUSED },{ ' ', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_UNUSED }
		},
		{
			{ 'a', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },{ 'r', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },{ 'A', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },{ 'R', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },
			{ 'a', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },{ 'r', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },{ 'A', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },{ 'R', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },
			{ 'a', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },{ 'r', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },{ 'A', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },{ 'R', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },
			{ 'T', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_UNUSED },{ 'T', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_UNUSED }
		},
		{
			{ 'b', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },{ 's', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },{ 'B', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },{ 'S', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },
			{ 'b', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },{ 's', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },{ 'B', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },{ 'S', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },
			{ 'b', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },{ 's', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },{ 'B', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },{ 'S', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },
			{ 'T', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_UNUSED },{ 'T', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_UNUSED }
		},
		{
			{ 'c', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },{ 't', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },{ 'C', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },{ 'T', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },
			{ 'c', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },{ 't', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },{ 'C', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },{ 'T', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },
			{ 'c', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },{ 't', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },{ 'C', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },{ 'T', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },
			{ 'T', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_UNUSED },{ 'T', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_UNUSED }
		},
		{
			{ 'd', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },{ 'u', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },{ 'D', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },{ 'U', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },
			{ 'd', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },{ 'u', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },{ 'D', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },{ 'U', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },
			{ 'd', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },{ 'u', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },{ 'D', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },{ 'U', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },
			{ 'T', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_UNUSED },{ 'T', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_UNUSED }
		},
		{
			{ 'e', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },{ 'v', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },{ 'E', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },{ 'V', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },
			{ 'e', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },{ 'v', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },{ 'E', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },{ 'V', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },
			{ 'e', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },{ 'v', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },{ 'E', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },{ 'V', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },
			{ 'T', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_UNUSED },{ 'T', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_UNUSED }
		},
		{
			{ 'f', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },{ 'w', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },{ 'F', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },{ 'W', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },
			{ 'f', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },{ 'w', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },{ 'F', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },{ 'W', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },
			{ 'f', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },{ 'w', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },{ 'F', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },{ 'W', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },
			{ 'T', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_UNUSED },{ 'T', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_UNUSED }
		},
		{
			{ 'g', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },{ 'x', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },{ 'G', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },{ 'X', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },
			{ 'g', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },{ 'x', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },{ 'G', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },{ 'X', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },
			{ 'g', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },{ 'x', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },{ 'G', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },{ 'X', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },
			{ 'T', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_UNUSED },{ 'T', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_UNUSED }
		},
		{
			{ 'h', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },{ 'y', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },{ 'H', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },{ 'Y', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },
			{ 'h', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },{ 'y', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },{ 'H', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },{ 'Y', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },
			{ 'h', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },{ 'y', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },{ 'H', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },{ 'Y', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },
			{ 'T', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_UNUSED },{ 'T', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_UNUSED }
		},
		{
			{ 'i', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },{ 'z', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },{ 'I', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },{ 'Z', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },
			{ 'i', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },{ 'z', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },{ 'I', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },{ 'Z', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },
			{ 'i', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },{ 'z', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },{ 'I', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },{ 'Z', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },
			{ 'T', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_UNUSED },{ 'T', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_UNUSED }
		},
		{
			{ 'j', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },{ '<', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },{ 'J', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },{ '(', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },
			{ 'j', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },{ '<', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },{ 'J', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },{ '(', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },
			{ 'j', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },{ '<', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },{ 'J', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },{ '(', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },
			{ 'T', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_UNUSED },{ 'T', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_UNUSED }
		},
		{
			{ 'k', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },{ '>', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },{ 'K', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },{ ')', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },
			{ 'k', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },{ '>', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },{ 'K', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },{ ')', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },
			{ 'k', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },{ '>', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },{ 'K', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },{ ')', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },
			{ 'T', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_UNUSED },{ 'T', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_UNUSED }
		},
		{
			{ 'l', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },{ '0', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },{ 'L', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },{ '5', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },
			{ 'l', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },{ '0', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },{ 'L', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },{ '5', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },
			{ 'l', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },{ '0', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },{ 'L', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },{ '5', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },
			{ 'T', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_UNUSED },{ 'T', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_UNUSED }
		},
		{
			{ 'm', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },{ '1', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },{ 'M', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },{ '6', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },
			{ 'm', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },{ '1', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },{ 'M', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },{ '6', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },
			{ 'm', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },{ '1', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },{ 'M', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },{ '6', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },
			{ 'T', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_UNUSED },{ 'T', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_UNUSED }
		},
		{
			{ 'n', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },{ '2', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },{ 'N', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },{ '7',PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },
			{ 'n', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },{ '2', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },{ 'N', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },{ '7',PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },
			{ 'n', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },{ '2', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },{ 'N', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },{ '7',PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },
			{ 'T', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_UNUSED },{ 'T', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_UNUSED }
		},
		{
			{ 'o', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },{ '3', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },{ 'O', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },{ '8', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },
			{ 'o', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },{ '3', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },{ 'O', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },{ '8', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },
			{ 'o', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },{ '3', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },{ 'O', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },{ '8', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },
			{ 'T', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_UNUSED },{ 'T', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_UNUSED }
		},
		{
			{ 'p', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },{ '4', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },{ 'P', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },{ '9', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },
			{ 'p', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },{ '4', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },{ 'P', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },{ '9', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },
			{ 'p', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },{ '4', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },{ 'P', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },{ '9', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },
			{ 'T', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_UNUSED },{ 'T', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_UNUSED }
		},
		{
			{ 'q', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },{0XA1, PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },{ 'Q', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },{ 0XA2,PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_NONE },
			{ 'q', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },{ 0XA1, PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },{ 'Q', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },{ 0XA2,PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_REVERSE },
			{ 'q', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },{ 0XA1, PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },{ 'Q', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },{ 0XA2,PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_SMALL_FONT },
			{ 'q', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_UNUSED },{ ':', PMDG_NGX_CDU_COLOR_WHITE, PMDG_NGX_CDU_FLAG_UNUSED }
		}
	}, true
};

PMDG_NGX_CDU_Screen* FsxManager::getCduScreen(int unit)
{
	return isPmdgNgx() ? ((unit == 1) ? &pmdgNgxCdu1_ : &pmdgNgxCdu0_)
		: (isPmdg777() ? ((unit == 2) ? &pmdg777Cdu2_ : ((unit == 1) ? &pmdg777Cdu1_ : &pmdg777Cdu0_)) : &dummyScreen);
};

inline const char* safeStr(const char* s, const char* def) { return (s == nullptr) ? def : s; }
inline const char* safeStr(const char* s, const char* val, const char* def) { return (s == nullptr) ? def : val; }

void FsxManager::eventName(string& event, string const& name, string const& module)
{
	log_.debug("eventName(..., ", name, "\", \"", module, "\");");

	event = name;
	if (!module.empty()) {
		auto modAliasIt = modAliases_.find(module);
		string mod(module);
		if (modAliasIt != modAliases_.end()) {
			mod = modAliasIt->second;
			log_.debug("eventName(): Module \"", module, "\" aliased to \"", mod, "\"");
		}
		auto evtModIt = eventModules_.find(mod);
		if (evtModIt == eventModules_.end()) {
			log_.error("eventName(): Unknown module \"", mod, "\"");
		}
		else {
			auto dwIt = evtModIt->second.map.find(event);

			if (dwIt == evtModIt->second.map.end()) {
				log_.error("eventName(): Unknown event \"", event, "\" in module \"", mod, "\"");
			}
			else {
				ostringstream os;
				os << "#" << dwIt->second;
				event = os.str();
				log_.debug("eventName(): Event \"", name, "\" resolved to \"", event, "\"");
			}
		}
	}
}