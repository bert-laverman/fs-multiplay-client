/**
 * FsxManager
 */
#ifndef __NL_RAKIS_FS_FSXMANAGER_H
#define __NL_RAKIS_FS_FSXMANAGER_H

#include <map>
#include <string>
#include <functional>

#pragma warning(push)
#include <PMDG_NGX_SDK.h>
#pragma warning( disable : 4005 )
#include <PMDG_777X_SDK.h>
#pragma warning(pop)

#define PMDG_777X_CDU_0_NAME		"PMDG_777X_CDU_0"
#define PMDG_777X_CDU_1_NAME		"PMDG_777X_CDU_1"
#define PMDG_777X_CDU_2_NAME		"PMDG_777X_CDU_2"
#define PMDG_777X_CDU_0_ID			0x4E477835
#define PMDG_777X_CDU_1_ID			0x4E477836
#define PMDG_777X_CDU_2_ID			0x4E477837
#define PMDG_777X_CDU_0_DEFINITION	0x4E477838
#define PMDG_777X_CDU_1_DEFINITION	0x4E477839
#define PMDG_777X_CDU_2_DEFINITION	0x4E47783A
#define PMDG_777X_CDU_Cell			PMDG_NGX_CDU_Cell
#define PMDG_777X_CDU_Screen		PMDG_NGX_CDU_Screen

#define PMDG_NGX_CDU_FLAG_NONE 0

#include "../LibRakis/File.h"
#include "../LibRakis/Log.h"

#include "../LibService/Service.h"
#include "../LibService/SettingsManager.h"

#include "FsxData.h"

namespace nl {
namespace rakis {
namespace fsx {

struct EventModule {
	std::string                  name;
	std::map<std::string, DWORD> map;
};

class FsxManager
    : public virtual nl::rakis::service::Service
{
public: // Types and constants
    typedef std::map<std::string, FsxDataPtr> FsxDataMap;

    static FsxManager& instance();

private: // Members
	HANDLE                               fsxHdl_;
    FsxDataMap                           data_;
	std::map<std::string, EventModule>   eventModules_;
	std::map<std::string, std::string>   modAliases_;

	bool                     pmdgNgx_;
	PMDG_NGX_Data            pmdgNgxData_;
	PMDG_NGX_CDU_Screen      pmdgNgxCdu0_;
	PMDG_NGX_CDU_Screen      pmdgNgxCdu1_;

	bool                     pmdg777_;
	PMDG_777X_Data			 pmdg777Data_;
	PMDG_777X_CDU_Screen     pmdg777Cdu0_;
	PMDG_777X_CDU_Screen     pmdg777Cdu1_;
	PMDG_777X_CDU_Screen     pmdg777Cdu2_;

	static Logger log_;

public: // Methods
    FsxManager();
    virtual ~FsxManager();

    virtual void doStart();
    virtual void doStop();

    void onConnect();
    void onDisconnect();
    void onSimStart();
    void onSimStop();
    void onAircraftLoaded(const char* airFile);
    void onSimPaused();
    void onSimUnpaused();
	void onObjectAdded();
	void onObjectRemoved();

    void addFsxData(FsxDataPtr serverData);
    void removeFsxData(std::string const& name);
    bool haveFsxData(std::string const& id) const { auto it = data_.find(id); return (it != data_.end()); };
    FsxDataPtr getFsxData(std::string const& id) const { auto it = data_.find(id); return (it != data_.end()) ? it->second : nullptr; };
    void foreachFsxData(std::function<void(FsxDataPtr)> doIt) { for (auto serverData: data_) { doIt(serverData.second); } };

	void eventName(std::string& event, std::string const& name, std::string const& module);

	bool isPmdgNgx() const { return pmdgNgx_; };
	void onPmdgNgxData();
	void onPmdgNgxCdu0();
	void onPmdgNgxCdu1();

	bool isPmdg777() const { return pmdg777_; };
	void onPmdg777Data();
	void onPmdg777Cdu0();
	void onPmdg777Cdu1();
	void onPmdg777Cdu2();

	PMDG_NGX_CDU_Screen* getCduScreen(int unit);

private:
    FsxManager(FsxManager const&) =delete;
    FsxManager& operator=(FsxManager const&) =delete;
};

} // namespace fsx
} // namespace rakis
} // namespace nl

#endif