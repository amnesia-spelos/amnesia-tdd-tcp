#include "LuxSideAppManager.h"

#include <vector>
#include <cstring>

#ifdef WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

struct cLuxSideAppManager::cSideAppInstance
{
	cLuxSideAppDefinition mDefinition;
#ifdef WIN32
	PROCESS_INFORMATION mProcessInfo;
#endif
};

cLuxSideAppManager::cLuxSideAppManager()
	: iLuxUpdateable("LuxSideAppManager")
{
}

cLuxSideAppManager::~cLuxSideAppManager()
{
	ShutdownSideApps();
}

void cLuxSideAppManager::StartSideApps(const std::vector<cLuxSideAppDefinition>& aDefinitions)
{
	ShutdownSideApps();

	for(size_t i=0; i<aDefinitions.size(); ++i)
	{
		StartSideApp(aDefinitions[i]);
	}
}

void cLuxSideAppManager::ShutdownSideApps()
{
#ifdef WIN32
	for(size_t i=0; i<mvRunningApps.size(); ++i)
	{
		cSideAppInstance& instance = mvRunningApps[i];

		if(instance.mProcessInfo.hProcess)
		{
			if(WaitForSingleObject(instance.mProcessInfo.hProcess, 0) == WAIT_TIMEOUT)
			{
				TerminateProcess(instance.mProcessInfo.hProcess, 0);
				WaitForSingleObject(instance.mProcessInfo.hProcess, 2000);
			}

			CloseHandle(instance.mProcessInfo.hProcess);
			instance.mProcessInfo.hProcess = NULL;
		}

		if(instance.mProcessInfo.hThread)
		{
			CloseHandle(instance.mProcessInfo.hThread);
			instance.mProcessInfo.hThread = NULL;
		}
	}
#endif

	mvRunningApps.clear();
}

void cLuxSideAppManager::StartSideApp(const cLuxSideAppDefinition& aDefinition)
{
#ifdef WIN32
	if(aDefinition.msExecutableFullPath == _W(""))
		return;

	STARTUPINFOW StartupInfo;
	PROCESS_INFORMATION ProcessInfo;
	memset(&StartupInfo, 0, sizeof(STARTUPINFOW));
	memset(&ProcessInfo, 0, sizeof(PROCESS_INFORMATION));
	StartupInfo.cb = sizeof(STARTUPINFOW);
	StartupInfo.dwFlags |= STARTF_USESHOWWINDOW;
	StartupInfo.wShowWindow = SW_SHOWMINNOACTIVE;

	tWString sCommand = _W("\"") + aDefinition.msExecutableFullPath + _W("\"");
	std::vector<wchar_t> vCommand(sCommand.length()+1);
	for(size_t i=0; i<sCommand.length(); ++i)
		vCommand[i] = sCommand[i];
	vCommand[sCommand.length()] = 0;

	tWString sCurrentDir = cString::GetFilePathW(aDefinition.msExecutableFullPath);
	std::vector<wchar_t> vCurrentDir;
	wchar_t* pCurrentDir = NULL;
	if(sCurrentDir != _W(""))
	{
		vCurrentDir.resize(sCurrentDir.length()+1);
		for(size_t i=0; i<sCurrentDir.length(); ++i)
			vCurrentDir[i] = sCurrentDir[i];
		vCurrentDir[sCurrentDir.length()] = 0;
		pCurrentDir = &vCurrentDir[0];
	}

	if(CreateProcessW(NULL, &vCommand[0], NULL, NULL, FALSE, 0, NULL, pCurrentDir, &StartupInfo, &ProcessInfo))
	{
		cSideAppInstance instance;
		instance.mDefinition = aDefinition;
		instance.mProcessInfo = ProcessInfo;
		mvRunningApps.push_back(instance);

		Log("LuxSideApps: Started '%s'\n", aDefinition.msExecutableOriginal.c_str());
	}
	else
	{
		DWORD dwError = GetLastError();
		Warning("LuxSideApps: Failed to start '%s' (error %lu)\n", aDefinition.msExecutableOriginal.c_str(), dwError);
	}
#else
	(void)aDefinition;
	Warning("LuxSideApps: Side applications are not supported on this platform.\n");
#endif
}
