/**
* FsxEvent.h
*/
#ifndef __NL_RAKIS_FSX_FSXEVENT_H
#define __NL_RAKIS_FSX_FSXEVENT_H

#include <string>

#include "SimConnectTypes.h"

namespace nl {
namespace rakis {
namespace fsx {

	class FsxEvent
	{
	public: // Constants & Typedefs

	private: // Members
		std::string                 name_;
		bool                        parameterized_;
		DWORD                       lastParam_;
		DWORD                       param_;
		SIMCONNECT_CLIENT_EVENT_ID  evtId_;
		
	public: // Methods
		FsxEvent() : name_("FsxEvent"), parameterized_(false), evtId_(0) {}
		FsxEvent(const char* name, bool parameterized =false) : name_(name), parameterized_(parameterized), evtId_(0), param_(0), lastParam_(0) {}
		FsxEvent(std::string const& name, bool parameterized = false) : name_(name), parameterized_(parameterized), evtId_(0), param_(0), lastParam_(0) {}
		FsxEvent(const char* name, DWORD param) : name_(name), parameterized_(true), evtId_(0), param_(param), lastParam_(param) {}
		FsxEvent(std::string const& name, DWORD param) : name_(name), parameterized_(true), evtId_(0), param_(param), lastParam_(param) {}
		virtual ~FsxEvent() {};

		std::string const& getName() const { return name_; }
		void setName(std::string const& name) { name_ = name; }

		bool isDefined() const { return evtId_ != 0; }

		bool isParameterized() const { return parameterized_; }
		void setParameterized(bool parameterized) { parameterized_ = parameterized; }

		void setParam(DWORD param) { if (param != param_) { lastParam_ = param_; param_ = param; } }
		bool hasParamChanged() const { return param_ != lastParam_; }
		void resetParam() { lastParam_ = param_; }

		SIMCONNECT_CLIENT_EVENT_ID getEvtId() const { return evtId_; }
		void setEvtId(SIMCONNECT_CLIENT_EVENT_ID evtId) { evtId_ = evtId; }


	private: // deleted implementations

	};

} // namespace fsx
} // namespace rakis
} // namespace nl

#endif
