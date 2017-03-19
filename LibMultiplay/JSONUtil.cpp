/**
*/
#include "stdafx.h"

#include <cwctype>
#include <chrono>
#include <strstream>
#include <iomanip>

#include <cpprest/json.h>
#include <cpprest/base_uri.h>

#include "JSONUtil.h"

#include "../LibAI/AIManager.h"

using namespace std;
using namespace std::chrono;

using namespace web;
using namespace web::json;
using namespace utility::conversions;

namespace nl {
namespace rakis {
namespace fsx {


/*
* Utility stuff for JSON
*/

// AircraftInfo fields
static const wstring FIELD_ATC_TYPE(L"atcType");
static const wstring FIELD_ATC_MODEL(L"atcModel");
static const wstring FIELD_ATC_ID(L"atcId");
static const wstring FIELD_ATC_AIRLINE(L"atcAirline");
static const wstring FIELD_ATC_FLIGHT_NUM(L"atcFlightNumber");
static const wstring FIELD_TITLE(L"title");
// LocationInfo fields
static const wstring FIELD_LATITUDE(L"latitude");
static const wstring FIELD_LONGITUDE(L"longitude");
static const wstring FIELD_ALTITUDE(L"altitude");
static const wstring FIELD_PITCH(L"pitch");
static const wstring FIELD_BANK(L"bank");
static const wstring FIELD_HEADING(L"heading");
static const wstring FIELD_ON_GROUND(L"onGround");
static const wstring FIELD_AIRSPEED(L"airspeed");
// EnginesInfo fields
static const wstring FIELD_COMBUSTION(L"eng");
static const wstring FIELD_THROTTLE(L"thrt");
// LightsInfo fields
static const wstring FIELD_STROBES(L"strb");
static const wstring FIELD_LANDING(L"land");
static const wstring FIELD_TAXI(L"taxi");
static const wstring FIELD_BEACON(L"bcn");
static const wstring FIELD_NAVIGATION(L"nav");
static const wstring FIELD_LOGO(L"logo");
static const wstring FIELD_WING(L"wing");
static const wstring FIELD_RECOGNITION(L"recg");
static const wstring FIELD_CABIN(L"cabn");
// ControlsInfo fields
static const wstring FIELD_RUDDER(L"rdr");
static const wstring FIELD_ELEVATOR(L"ele");
static const wstring FIELD_AILERON(L"ail");
static const wstring FIELD_RUDDER_TRIM(L"rdrtr");
static const wstring FIELD_ELEVATOR_TRIM(L"eletr");
static const wstring FIELD_AILERON_TRIM(L"ailtr");
static const wstring FIELD_SPOILERS(L"spl");
static const wstring FIELD_FLAPS(L"flp");
static const wstring FIELD_GEARS(L"grs");
static const wstring FIELD_BREAKS(L"brk");
static const wstring FIELD_DOOR1(L"dr1");


static value as_string(double dbl) {
	wostringstream os;
	os << fixed << setprecision(10) << dbl;
	return value(os.str());
}
static value as_string(DWORD dw) {
	wostringstream os;
	os << dw;
	return value(os.str());
}
static double as_double(const wstring& str) {
	double result;
	wistringstream is(str);
	is >> result;
	return result;
}
static int as_integer(const wstring& str) {
	int result;
	wistringstream is(str);
	is >> result;
	return result;
}
static long as_long(const wstring& str) {
	long result;
	wistringstream is(str);
	is >> result;
	return result;
}


static void assign(int& fld, value val) {
	if (val.is_null()) {
		fld = 0;
	}
	else if (val.is_number()) {
		fld = val.as_integer();
	}
	else if (val.is_string()) {
		fld = as_integer(val.as_string());
	}
	else {
		val = 0;
	}
}
static void assign(int64_t& fld, value val) {
	if (val.is_null()) {
		fld = 0;
	}
	else if (val.is_number()) {
		fld = val.as_integer();
	}
	else if (val.is_string()) {
		fld = as_integer(val.as_string());
	}
	else {
		val = 0;
	}
}
static void assign(double& fld, value val) {
	if (val.is_null()) {
		fld = 0.0;
	}
	else if (val.is_number()) {
		fld = val.as_double();
	}
	else if (val.is_string()) {
		fld = as_double(val.as_string());
	}
	else {
		val = 0.0;
	}
}
static void assign(DWORD& fld, value val) {
	if (val.is_null()) {
		fld = 0;
	}
	else if (val.is_number()) {
		fld = val.as_integer();
	}
	else if (val.is_string()) {
		fld = as_long(val.as_string());
	}
	else {
		val = 0;
	}
}
static const wstring TRUE_VAL(L"tTrRuUeE");
static void assign(bool& fld, value val) {
	if (!val.is_null() && val.is_boolean()) {
		fld = val.as_bool();
	}
	else if (!val.is_null() && val.is_string()) {
		wstring b(val.as_string());

		fld = (b.length() == 4)
			&& ((b[0] == TRUE_VAL[0]) || (b[0] == TRUE_VAL[1]))
			&& ((b[1] == TRUE_VAL[2]) || (b[1] == TRUE_VAL[3]))
			&& ((b[2] == TRUE_VAL[4]) || (b[2] == TRUE_VAL[5]))
			&& ((b[3] == TRUE_VAL[6]) || (b[3] == TRUE_VAL[7]));
	}
	else {
		fld = false;
	}
}

/*
* safe field handling
*/
void getString(string& data, const value& obj, const wstring& field) {
	if (obj.has_field(field)) {
		data = wstr2str(obj.at(field).as_string());
	}
}
void getString(wstring& data, const value& obj, const wstring& field) {
	if (obj.has_field(field)) {
		data = obj.at(field).as_string();
	}
}

template<typename T>
static void assign(T& fld, value obj, const wstring& prop) {
	if (obj.has_field(prop)) {
		assign(fld, obj.at(prop));
	}
}

/*
* whole records
*/
void fromJSON(AircraftIdData& data, const value& jsonData)
{
	getString(data.atcType, jsonData, FIELD_ATC_TYPE);
	getString(data.atcModel, jsonData, FIELD_ATC_MODEL);
	getString(data.atcId, jsonData, FIELD_ATC_ID);
	getString(data.atcAirline, jsonData, FIELD_ATC_AIRLINE);
	getString(data.atcFlightNumber, jsonData, FIELD_ATC_FLIGHT_NUM);
	getString(data.title, jsonData, FIELD_TITLE);
}
void toJSON(value& jsonData, const AircraftIdData& data)
{
	jsonData[FIELD_ATC_TYPE] = value(str2wstr(data.atcType));
	jsonData[FIELD_ATC_MODEL] = value(str2wstr(data.atcModel));
	jsonData[FIELD_ATC_ID] = value(str2wstr(data.atcId));
	jsonData[FIELD_ATC_AIRLINE] = value(str2wstr(data.atcAirline));
	jsonData[FIELD_ATC_FLIGHT_NUM] = value(str2wstr(data.atcFlightNumber));
	jsonData[FIELD_TITLE] = value(str2wstr(data.title));
}

void fromJSON(AircraftLocationData& data, const value& jsonData)
{
	assign(data.latitude, jsonData, FIELD_LATITUDE);
	assign(data.longitude, jsonData, FIELD_LONGITUDE);
	assign(data.altitude, jsonData, FIELD_ALTITUDE);
	assign(data.pitch, jsonData, FIELD_PITCH);
	assign(data.bank, jsonData, FIELD_BANK);
	assign(data.heading, jsonData, FIELD_HEADING);
	assign(data.onGround, jsonData, FIELD_ON_GROUND);
	assign(data.airspeed, jsonData, FIELD_AIRSPEED);
}
void toJSON(value& jsonData, const AircraftLocationData& data)
{
	jsonData[FIELD_LATITUDE] = as_string(data.latitude);
	jsonData[FIELD_LONGITUDE] = as_string(data.longitude);
	jsonData[FIELD_ALTITUDE] = as_string(data.altitude);
	jsonData[FIELD_PITCH] = as_string(data.pitch);
	jsonData[FIELD_BANK] = as_string(data.bank);
	jsonData[FIELD_HEADING] = as_string(data.heading);
	jsonData[FIELD_ON_GROUND] = data.onGround != 0;
	jsonData[FIELD_AIRSPEED] = as_string(data.airspeed);
}

void fromJSON(AircraftEngineData& data, const value& jsonData)
{
	auto comb = jsonData.at(FIELD_COMBUSTION).as_array();
	assign(data.engineOn[0], comb[0]);
	assign(data.engineOn[1], comb[1]);
	assign(data.engineOn[2], comb[2]);
	assign(data.engineOn[3], comb[3]);
	auto thrt = jsonData.at(FIELD_THROTTLE).as_array();
	assign(data.throttle[0], thrt[0]);
	assign(data.throttle[1], thrt[1]);
	assign(data.throttle[2], thrt[2]);
	assign(data.throttle[3], thrt[3]);
}
void toJSON(value& jsonData, const AircraftEngineData& data)
{
	vector<value> arr;
	arr.emplace_back(value(data.engineOn[0]));
	arr.emplace_back(value(data.engineOn[1]));
	arr.emplace_back(value(data.engineOn[2]));
	arr.emplace_back(value(data.engineOn[3]));
	jsonData[FIELD_COMBUSTION] = value::array(arr);

	arr.clear();
	arr.emplace_back(value(data.throttle[0]));
	arr.emplace_back(value(data.throttle[1]));
	arr.emplace_back(value(data.throttle[2]));
	arr.emplace_back(value(data.throttle[3]));
	jsonData[FIELD_THROTTLE] = value::array(arr);
}

void fromJSON(AircraftLightsData& data, const value& jsonData)
{
	assign(data.strobeLightOn, jsonData.at(FIELD_STROBES));
	assign(data.landingLightOn, jsonData.at(FIELD_LANDING));
	assign(data.taxiLightOn, jsonData.at(FIELD_TAXI));
	assign(data.beaconLightOn, jsonData.at(FIELD_BEACON));
	assign(data.navLightOn, jsonData.at(FIELD_NAVIGATION));
	assign(data.logoLightOn, jsonData.at(FIELD_LOGO));
	assign(data.wingLightOn, jsonData.at(FIELD_WING));
	assign(data.recogLightOn, jsonData.at(FIELD_RECOGNITION));
	assign(data.cabinLightOn, jsonData.at(FIELD_CABIN));
}
void toJSON(value& jsonData, const AircraftLightsData& data)
{
	jsonData[FIELD_STROBES] = value(data.strobeLightOn);
	jsonData[FIELD_LANDING] = value(data.landingLightOn);
	jsonData[FIELD_TAXI] = value(data.taxiLightOn);
	jsonData[FIELD_BEACON] = value(data.beaconLightOn);
	jsonData[FIELD_NAVIGATION] = value(data.navLightOn);
	jsonData[FIELD_LOGO] = value(data.logoLightOn);
	jsonData[FIELD_WING] = value(data.wingLightOn);
	jsonData[FIELD_RECOGNITION] = value(data.recogLightOn);
	jsonData[FIELD_CABIN] = value(data.cabinLightOn);
}

void fromJSON(AircraftControlsData& data, const value& jsonData)
{
	assign(data.rudderPos, jsonData.at(FIELD_RUDDER));
	assign(data.elevatorPos, jsonData.at(FIELD_ELEVATOR));
	assign(data.aileronPos, jsonData.at(FIELD_AILERON));
	assign(data.rudderTrimPos, jsonData.at(FIELD_RUDDER_TRIM));
	assign(data.elevatorTrimPos, jsonData.at(FIELD_ELEVATOR_TRIM));
	assign(data.aileronTrimPos, jsonData.at(FIELD_AILERON_TRIM));
	assign(data.spoilersPos, jsonData.at(FIELD_SPOILERS));
	assign(data.flapsPos, jsonData.at(FIELD_FLAPS));
	assign(data.gearsDown, jsonData.at(FIELD_GEARS));
	assign(data.parkingBrakePos, jsonData.at(FIELD_BREAKS));
	assign(data.doorPos, jsonData.at(FIELD_DOOR1));
}
void toJSON(value& jsonData, AircraftControlsData& data)
{
	jsonData[FIELD_RUDDER] = value(data.rudderPos);
	jsonData[FIELD_ELEVATOR] = value(data.elevatorPos);
	jsonData[FIELD_AILERON] = value(data.aileronPos);
	jsonData[FIELD_RUDDER_TRIM] = value(data.rudderTrimPos);
	jsonData[FIELD_ELEVATOR_TRIM] = value(data.elevatorTrimPos);
	jsonData[FIELD_AILERON_TRIM] = value(data.aileronTrimPos);
	jsonData[FIELD_SPOILERS] = value(data.spoilersPos);
	jsonData[FIELD_FLAPS] = value(data.flapsPos);
	jsonData[FIELD_GEARS] = value(data.gearsDown);
	jsonData[FIELD_BREAKS] = value(data.parkingBrakePos);
	jsonData[FIELD_DOOR1] = value(data.doorPos);
}

} // namespace fsx
} // namespace rakis
} // namespace nl
