/**
 * FsxData.h
 */
#ifndef __NL_RAKIS_FSX_FSXDATA_H
#define __NL_RAKIS_FSX_FSXDATA_H

#include <string>

#include "SimConnectTypes.h"

namespace nl {
namespace rakis {
namespace fsx {

class FsxData;
typedef FsxData* FsxDataPtr;

class FsxData
{
public: // Constants & Typedefs

private: // Members
    std::string                   name_;
	SIMCONNECT_DATA_DEFINITION_ID defId_;
	std::vector<SimConnectData>   data_;

public: // Methods
	FsxData() : name_("FsxData"), defId_(0) {};
	FsxData(const char* name) : name_(name), defId_(0) {};
	FsxData(std::string const& name) : name_(name), defId_(0) {};
	virtual ~FsxData() {};

	std::string const& getName() const { return name_; };
	void setName(std::string const& name) { name_ = name; };
	SIMCONNECT_DATA_DEFINITION_ID getDefId() const { return defId_; };
	void setDefId(SIMCONNECT_DATA_DEFINITION_ID defId) { defId_ = defId; };
	bool isDefined() const { return defId_ != 0; }

	void clear() { data_.clear(); }
	void add(const SimConnectData& dataDef) { data_.emplace_back(dataDef); }
	void add(const std::string& k, const std::string& n, const std::string& u, int tp = SIMCONNECT_DATATYPE_FLOAT64, float ep = 0.001f) { data_.emplace_back(k, n, u, static_cast<SIMCONNECT_DATATYPE>(tp), ep); }
	void add(const char* k, const char* n, char* u, int tp = SIMCONNECT_DATATYPE_FLOAT64, float ep = 0.001f) { data_.emplace_back(k, n, u, static_cast<SIMCONNECT_DATATYPE>(tp), ep); }
	void add(int id, const std::string& n, const std::string& u, int tp = SIMCONNECT_DATATYPE_FLOAT64, float ep = 0.001f) { data_.emplace_back(id, n, u, static_cast<SIMCONNECT_DATATYPE>(tp), ep); }
	void add(int id, const char* n, char* u, int tp = SIMCONNECT_DATATYPE_FLOAT64, float ep = 0.001f) { data_.emplace_back(id, n, u, static_cast<SIMCONNECT_DATATYPE>(tp), ep); }

	const std::vector<SimConnectData>& getData() const { return data_;  }

private: // deleted implementations
    FsxData(FsxData const&) = delete;
    FsxData& operator=(FsxData const&) = delete;

};

} // namespace fsx
} // namespace rakis
} // namespace nl

#endif
