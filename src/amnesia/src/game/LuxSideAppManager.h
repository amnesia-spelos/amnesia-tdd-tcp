#ifndef LUX_SIDE_APP_MANAGER_H
#define LUX_SIDE_APP_MANAGER_H

#include <vector>

#include "LuxBase.h"

class cLuxSideAppManager : public iLuxUpdateable
{
public:
	cLuxSideAppManager();
	~cLuxSideAppManager();

	void StartSideApps(const std::vector<cLuxSideAppDefinition>& aDefinitions);
	void ShutdownSideApps();
	void Update(float afTimeStep);

private:
	struct cSideAppInstance;

	void StartSideApp(const cLuxSideAppDefinition& aDefinition);
	void HandleSideAppExit(cSideAppInstance& aInstance);
	void CloseProcessHandles(cSideAppInstance& aInstance);
	void CrashGameForSideApp(const cLuxSideAppDefinition& aDefinition);

	std::vector<cSideAppInstance> mvRunningApps;
};

#endif
