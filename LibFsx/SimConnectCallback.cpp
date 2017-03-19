/**
 * SimConnectCallback.cpp
 */
#include "stdafx.h"

#include "SimConnectCallback.h"


using namespace std;

using namespace nl::rakis::fsx;
using namespace nl::rakis;


/*static*/ Logger SimConnectCallback::log_(Logger::getLogger("nl.rakis.fsx.SimConnectCallback"));

/*static*/ void SimConnectCallback::NO_EXCEPTION_CALLBACK(DWORD exception, DWORD sendId, DWORD index)
{
    log_.warn("callback(): No exception handler for incoming SIMCONNECT_RECV_EXCEPTION");
    switch (exception) {
    case SIMCONNECT_EXCEPTION_NONE:
        log_.error("scDispatchProc(): No Exception");
        break;
    case SIMCONNECT_EXCEPTION_ERROR:
        log_.error("scDispatchProc(): Unknown Error");
        break;
    case SIMCONNECT_EXCEPTION_SIZE_MISMATCH:
        log_.error("scDispatchProc(): Size mismatch");
        break;
    case SIMCONNECT_EXCEPTION_UNRECOGNIZED_ID:
        log_.error("scDispatchProc(): Unrecognized ID");
        break;
    case SIMCONNECT_EXCEPTION_UNOPENED:
        log_.error("scDispatchProc(): Talking through a closed connection (shouldn't be possible, right?)");
        break;
    case SIMCONNECT_EXCEPTION_VERSION_MISMATCH:
        log_.error("scDispatchProc(): SimConnect API version mismatch");
        break;
    case SIMCONNECT_EXCEPTION_TOO_MANY_GROUPS:
        log_.error("scDispatchProc(): Too many groups (max = 20)");
        break;
    case SIMCONNECT_EXCEPTION_NAME_UNRECOGNIZED:
        log_.error("scDispatchProc(): Unrecognized event name");
        break;
    case SIMCONNECT_EXCEPTION_TOO_MANY_EVENT_NAMES:
        log_.error("scDispatchProc(): Too many event names (max = 1000)");
        break;
    case SIMCONNECT_EXCEPTION_EVENT_ID_DUPLICATE:
        log_.error("scDispatchProc(): Event ID already in use (after MapClientEventToSimEvent or SubscribeToSystemEvent)");
        break;
    case SIMCONNECT_EXCEPTION_TOO_MANY_MAPS:
        log_.error("scDispatchProc(): Too many mappings defined (max = 20)");
        break;
    case SIMCONNECT_EXCEPTION_TOO_MANY_OBJECTS:
        log_.error("scDispatchProc(): Too many objects (max = 1000)");
        break;
    case SIMCONNECT_EXCEPTION_TOO_MANY_REQUESTS:
        log_.error("scDispatchProc(): Too many requests (max = 1000)");
        break;
    case SIMCONNECT_EXCEPTION_WEATHER_INVALID_PORT:
        log_.error("scDispatchProc(): Invalid port number requested");
        break;
    case SIMCONNECT_EXCEPTION_WEATHER_INVALID_METAR:
        log_.error("scDispatchProc(): METAR data did not match required format");
        break;
    case SIMCONNECT_EXCEPTION_WEATHER_UNABLE_TO_GET_OBSERVATION:
        log_.error("scDispatchProc(): Weather observation not available");
        break;
    case SIMCONNECT_EXCEPTION_WEATHER_UNABLE_TO_CREATE_STATION:
        log_.error("scDispatchProc(): Weather station could not be created");
        break;
    case SIMCONNECT_EXCEPTION_WEATHER_UNABLE_TO_REMOVE_STATION:
        log_.error("scDispatchProc(): Weather station could not be removed");
        break;
    case SIMCONNECT_EXCEPTION_INVALID_DATA_TYPE:
        log_.error("scDispatchProc(): Invalid data type requested");
        break;
    case SIMCONNECT_EXCEPTION_INVALID_DATA_SIZE:
        log_.error("scDispatchProc(): Invalid data size requested");
        break;
    case SIMCONNECT_EXCEPTION_DATA_ERROR:
        log_.error("scDispatchProc(): Generic Data Error");
        break;
    case SIMCONNECT_EXCEPTION_INVALID_ARRAY:
        log_.error("scDispatchProc(): Invalid Array used with SetDataOnSimObject");
        break;
    case SIMCONNECT_EXCEPTION_CREATE_OBJECT_FAILED:
        log_.error("scDispatchProc(): Create AI object failed");
        break;
    case SIMCONNECT_EXCEPTION_LOAD_FLIGHTPLAN_FAILED:
        log_.error("scDispatchProc(): Specified flightplan could not be found or bad format");
        break;
    case SIMCONNECT_EXCEPTION_OPERATION_INVALID_FOR_OBJECT_TYPE:
        log_.error("scDispatchProc(): Cannot perform action on object");
        break;
    case SIMCONNECT_EXCEPTION_ILLEGAL_OPERATION:
        log_.error("scDispatchProc(): AI operation cannot be completed");
        break;
    case SIMCONNECT_EXCEPTION_ALREADY_SUBSCRIBED:
        log_.error("scDispatchProc(): Client already subscribed to event");
        break;
    case SIMCONNECT_EXCEPTION_INVALID_ENUM:
        log_.error("scDispatchProc(): Invalid Enum value in RequestDataOnSimObjectType");
        break;
    case SIMCONNECT_EXCEPTION_DEFINITION_ERROR:
        log_.error("scDispatchProc(): Data definition error in RequestDataOnSimObject");
        break;
    case SIMCONNECT_EXCEPTION_DUPLICATE_ID:
        log_.error("scDispatchProc(): ID already in use for AddTpDataDefinition, AddCClientEventToNotificationGroup, or MapClientDataNameToID");
        break;
    case SIMCONNECT_EXCEPTION_DATUM_ID:
        log_.error("scDispatchProc(): Unknown Datum ID in SetDataOnSimObject");
        break;
    case SIMCONNECT_EXCEPTION_OUT_OF_BOUNDS:
        log_.error("scDispatchProc(): Value out of range for RequestDataOnSimObjectType or CreateClientData");
        break;
    case SIMCONNECT_EXCEPTION_ALREADY_CREATED:
        log_.error("scDispatchProc(): Client Data Area name already in use for MapClientDataNameToID");
        break;
    case SIMCONNECT_EXCEPTION_OBJECT_OUTSIDE_REALITY_BUBBLE:
        log_.error("scDispatchProc(): Cannot create ATC controlled AI object outside of reality bubble");
        break;
    case SIMCONNECT_EXCEPTION_OBJECT_CONTAINER:
        log_.error("scDispatchProc(): Create AI object failed, bad container");
        break;
    case SIMCONNECT_EXCEPTION_OBJECT_AI:
        log_.error("scDispatchProc(): Create AI object failed due to AI system");
        break;
    case SIMCONNECT_EXCEPTION_OBJECT_ATC:
        log_.error("scDispatchProc(): Create AI object failed due to ATC problem");
        break;
    case SIMCONNECT_EXCEPTION_OBJECT_SCHEDULE:
        log_.error("scDispatchProc(): Create AI Object failed due to schedule problem");
        break;
    }
}

SimConnectCallback::~SimConnectCallback()
{
}

void SimConnectCallback::callback(SIMCONNECT_RECV* data)
{
    if (data->dwID == SIMCONNECT_RECV_ID_EXCEPTION) {
        SIMCONNECT_RECV_EXCEPTION* exceptData(static_cast<SIMCONNECT_RECV_EXCEPTION*>(data));
        exceptCallback_(exceptData->dwException, exceptData->dwSendID, exceptData->dwIndex);
    }
}

SimConnectClientDataCallback::~SimConnectClientDataCallback()
{
}

void SimConnectClientDataCallback::callback(SIMCONNECT_RECV* data)
{
    if ((data->dwID == SIMCONNECT_RECV_ID_CLIENT_DATA) || (data->dwID == SIMCONNECT_RECV_ID_SIMOBJECT_DATA) || (data->dwID == SIMCONNECT_RECV_ID_SIMOBJECT_DATA_BYTYPE)) {
        SIMCONNECT_RECV_SIMOBJECT_DATA* objectData(static_cast<SIMCONNECT_RECV_SIMOBJECT_DATA*>(data));
        if (callback_ != nullptr) {
            // void(DWORD entryNr, DWORD outOfNr, DWORD count, DWORD* data)
//            callback_(objectData->dwentrynumber, objectData->dwoutof, objectData->dwDefineCount, &(objectData->dwData));
        }
        else {
            log_.warn("callback(): No callback for data");
        }
    }
    else {
        SimConnectCallback::callback(data);
    }
}
