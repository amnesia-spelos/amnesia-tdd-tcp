#include "BuildVersion.h"

#include <sstream>

std::string MakeBuildVersionLogLine(const std::string& asBuildVersion, unsigned int alSupportedProtocolVersion)
{
	std::ostringstream line;
	line << "Amnesia TDD TCP " << asBuildVersion << " (Protocol Versions: legacy, " << alSupportedProtocolVersion << ")";
	return line.str();
}
