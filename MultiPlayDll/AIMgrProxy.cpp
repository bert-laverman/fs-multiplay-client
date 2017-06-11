// MultiPlayDll.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"

#include <string>
#include "../LibAI/AIManager.h"

#include "MultiPlayDll.h"

using namespace std;

using namespace nl::rakis;
using namespace nl::rakis::fsx;
using namespace nl::rakis::service;


/*
* MultiplayManager
*/
MULTIPLAYDLL_API void __stdcall aimgr_addAIHandler(CsAIEventHandler handler)
{
//	AIManager::instance().addAICallback([=]() {
//		handler();
//	});
}
