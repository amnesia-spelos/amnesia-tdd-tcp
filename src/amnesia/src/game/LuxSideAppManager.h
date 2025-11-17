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

private:
	void StartSideApp(const cLuxSideAppDefinition& aDefinition);

	struct cSideAppInstance;

	std::vector<cSideAppInstance> mvRunningApps;
};

#endif
