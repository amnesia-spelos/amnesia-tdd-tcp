#ifndef BUILD_VERSION_H
#define BUILD_VERSION_H

#include <string>

// The log header line naming this game executable's Build Version (issue #71) and the Protocol Versions
// it speaks: the legacy protocol, for Sessions that never negotiate, and the one negotiable Protocol
// Version. The Build Version is git describe output captured at build time, or "unknown".
// Engine-independent; cLuxBase::Init logs it once the main config is loaded.
std::string MakeBuildVersionLogLine(const std::string& asBuildVersion, unsigned int alSupportedProtocolVersion);

#endif // BUILD_VERSION_H
