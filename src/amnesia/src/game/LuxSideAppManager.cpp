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
		}

		CloseProcessHandles(instance);
	}
#endif

	mvRunningApps.clear();
}

void cLuxSideAppManager::Update(float afTimeStep)
{
#ifdef WIN32
	(void)afTimeStep;

	size_t i = 0;
	while(i < mvRunningApps.size())
	{
		cSideAppInstance& instance = mvRunningApps[i];
		bool bRemove = false;

		if(instance.mProcessInfo.hProcess == NULL)
		{
			bRemove = true;
		}
		else
		{
			DWORD dwWaitResult = WaitForSingleObject(instance.mProcessInfo.hProcess, 0);
			if(dwWaitResult == WAIT_OBJECT_0)
			{
				DWORD dwExitCode = 0;
				GetExitCodeProcess(instance.mProcessInfo.hProcess, &dwExitCode);
				Log("LuxSideApps: '%s' exited (code %lu)\n", instance.mDefinition.msExecutableOriginal.c_str(), dwExitCode);
				HandleSideAppExit(instance);
				bRemove = true;
			}
			else if(dwWaitResult == WAIT_FAILED)
			{
				DWORD dwError = GetLastError();
				Warning("LuxSideApps: Wait failed for '%s' (error %lu)\n", instance.mDefinition.msExecutableOriginal.c_str(), dwError);
				HandleSideAppExit(instance);
				bRemove = true;
			}
		}

		if(bRemove)
		{
			CloseProcessHandles(instance);
			mvRunningApps.erase(mvRunningApps.begin() + i);
		}
		else
		{
			++i;
		}
	}
#else
	(void)afTimeStep;
#endif
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

void cLuxSideAppManager::HandleSideAppExit(cSideAppInstance& aInstance)
{
	switch(aInstance.mDefinition.mCrashBehavior)
	{
	case eLuxSideAppCrashBehavior_Restart:
		Log("LuxSideApps: Restarting '%s'\n", aInstance.mDefinition.msExecutableOriginal.c_str());
		StartSideApp(aInstance.mDefinition);
		break;
	case eLuxSideAppCrashBehavior_CrashGame:
		CrashGameForSideApp(aInstance.mDefinition);
		break;
	case eLuxSideAppCrashBehavior_Ignore:
	default:
		break;
	}
}

void cLuxSideAppManager::CloseProcessHandles(cSideAppInstance& aInstance)
{
#ifdef WIN32
	if(aInstance.mProcessInfo.hProcess)
	{
		CloseHandle(aInstance.mProcessInfo.hProcess);
		aInstance.mProcessInfo.hProcess = NULL;
	}

	if(aInstance.mProcessInfo.hThread)
	{
		CloseHandle(aInstance.mProcessInfo.hThread);
		aInstance.mProcessInfo.hThread = NULL;
	}
#else
	(void)aInstance;
#endif
}

void cLuxSideAppManager::CrashGameForSideApp(const cLuxSideAppDefinition& aDefinition)
{
	tString sChildName = cString::GetFileName(aDefinition.msExecutableOriginal);
	if(sChildName == "")
		sChildName = aDefinition.msExecutableOriginal;

	tString sMessage = "The child process \"" + sChildName + "\" closed unexpectedly";
	gpBase->msErrorMessage = cString::To16Char(sMessage);

	cPlatform::CreateMessageBox(_W("Error!"), gpBase->msErrorMessage.c_str());
	gpBase->mpEngine->Exit();
}
