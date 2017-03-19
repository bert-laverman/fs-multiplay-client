/**
*/
#ifndef __NL_RAKIS_FSX_JSONUTIL_H
#define __NL_RAKIS_FSX_JSONUTIL_H

#include <cpprest/http_client.h>
#include <cpprest/ws_client.h>

#include "../LibFsx/SimConnectTypes.h"

namespace nl {
namespace rakis {
namespace fsx {

	const std::wstring TYPE_AIRCRAFT(L"Aircraft");
	const std::wstring TYPE_LOCATION(L"Location");
	const std::wstring TYPE_ENGINES(L"Engines");
	const std::wstring TYPE_LIGHTS(L"Lights");
	const std::wstring TYPE_CONTROLS(L"Controls");
	const std::wstring TYPE_HELLO(L"hello");
	const std::wstring TYPE_ADD(L"add");
	const std::wstring TYPE_REMOVE(L"remove");

	const std::wstring FIELD_TYPE(L"type");
	const std::wstring FIELD_CALLSIGN(L"callsign");
	const std::wstring FIELD_SESSION(L"session");
	const std::wstring FIELD_TOKEN(L"token");

	void getString(std::string& data, const web::json::value& obj, const std::wstring& field);
	void getString(std::wstring& data, const web::json::value& obj, const std::wstring& field);

	void fromJSON(AircraftIdData& data, const web::json::value& jsonData);
	void toJSON(web::json::value& jsonData, const AircraftIdData& data);
	void fromJSON(AircraftLocationData& data, const web::json::value& jsonData);
	void toJSON(web::json::value& jsonData, const AircraftLocationData& data);
	void fromJSON(AircraftEngineData& data, const web::json::value& jsonData);
	void toJSON(web::json::value& jsonData, const AircraftEngineData& data);
	void fromJSON(AircraftLightsData& data, const web::json::value& jsonData);
	void toJSON(web::json::value& jsonData, const AircraftLightsData& data);
	void fromJSON(AircraftControlsData& data, const web::json::value& jsonData);
	void toJSON(web::json::value& jsonData, AircraftControlsData& data);

} // namespace fsx
} // namespace rakis
} // namespace nl

#endif
