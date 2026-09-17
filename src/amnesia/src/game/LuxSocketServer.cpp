#include "LuxSocketServer.h"
#include "LuxMap.h"
#include "LuxMapHandler.h"
#include "LuxPlayer.h"
#include "LuxChatHandler.h"
#include "LuxInputHandler.h"
#include "LuxMainMenu.h"

#include <set>

namespace
{
	class cLuxGameInteractionGameAdapter : public iGameInteractionGameAdapter
	{
	public:
		virtual bool IsMapLoaded() const
		{
			return gpBase->mpMapHandler && gpBase->mpMapHandler->GetCurrentMap();
		}

		virtual cGameInteractionPosition GetPosition() const
		{
			iCharacterBody* pCharBody = gpBase->mpPlayer->GetCharacterBody();
			const cVector3f position = pCharBody->GetFeetPosition();
			return cGameInteractionPosition(position.x, position.y, position.z);
		}

		virtual cGameInteractionRotation GetRotation() const
		{
			iCharacterBody* pCharBody = gpBase->mpPlayer->GetCharacterBody();
			return cGameInteractionRotation(pCharBody->GetYaw(), pCharBody->GetPitch());
		}

		virtual std::string GetMapFile() const
		{
			return gpBase->mpMapHandler->GetCurrentMap()->GetFileName();
		}

		virtual void RunScript(const std::string& asScript)
		{
			gpBase->mpMapHandler->GetCurrentMap()->RunScript(asScript);
		}

		virtual bool DisplayChatEntry(const cChatEntry& aEntry)
		{
			return gpBase->mpChatHandler && gpBase->mpChatHandler->DisplayChatEntry(aEntry);
		}

		virtual std::vector<cGameInteractionCustomStory> GetCustomStories() const
		{
			std::vector<cGameInteractionCustomStory> vStories;
			std::set<tWString> setIdentifiers;
			tWStringList lstStoryFolders;
			cLuxCustomStorySettings::FindInstalledStoryFolders(lstStoryFolders);
			for(tWStringListIt it = lstStoryFolders.begin(); it != lstStoryFolders.end(); ++it)
			{
				// Like starting, the menu's earlier location wins when an identifier is installed twice.
				const tWString sIdentifier = cString::GetFileNameW(*it);
				if(!setIdentifiers.insert(sIdentifier).second) continue;

				cLuxCustomStorySettings story;
				if(story.CreateFromPath(*it))
					vStories.push_back(cGameInteractionCustomStory(sIdentifier, story.msName));
			}
			return vStories;
		}

		virtual eGameInteractionCustomStoryAvailability GetCustomStoryAvailability(
			const std::wstring& asIdentifier) const
		{
			cLuxCustomStorySettings story;
			return FindCustomStory(asIdentifier, story);
		}

		virtual void StartCustomStory(const std::wstring& asIdentifier)
		{
			// The game may have left the main menu since the Command was accepted.
			cLuxCustomStorySettings story;
			if(FindCustomStory(asIdentifier, story) != eGameInteractionCustomStoryAvailability_Available)
				return;

			if(gpBase->mpUserConfig==NULL)
			{
				gpBase->CreateProfile(gpBase->msDefaultProfileName);
				gpBase->SetProfile(gpBase->msDefaultProfileName);
				gpBase->InitUserConfig();
			}

			story.SetActive();
			gpBase->StartCustomStory();
		}

	private:
		static bool IsInMainMenu()
		{
			return gpBase->mpInputHandler->GetState() == eLuxInputState_MainMenu &&
				!gpBase->mpMapHandler->MapIsLoaded() && !gpBase->mpMainMenu->IsTransitioning();
		}

		static eGameInteractionCustomStoryAvailability FindCustomStory(const tWString& asIdentifier,
			cLuxCustomStorySettings& aStory)
		{
			if(!IsInMainMenu())
			{
				Log("Game Interaction Protocol: cannot start custom story '%ls': not in main menu\n",
					asIdentifier.c_str());
				return eGameInteractionCustomStoryAvailability_NotInMainMenu;
			}

			tWStringList lstStoryFolders;
			cLuxCustomStorySettings::FindInstalledStoryFolders(lstStoryFolders);
			for(tWStringListIt it = lstStoryFolders.begin(); it != lstStoryFolders.end(); ++it)
			{
				if(cString::GetFileNameW(*it) != asIdentifier) continue;
				// CreateFromPath logs why an installed story is invalid.
				return aStory.CreateFromPath(*it) ? eGameInteractionCustomStoryAvailability_Available :
					eGameInteractionCustomStoryAvailability_Invalid;
			}

			Log("Game Interaction Protocol: cannot start custom story '%ls': not found\n", asIdentifier.c_str());
			return eGameInteractionCustomStoryAvailability_NotFound;
		}
	};
}

cLuxSocketServer::cLuxSocketServer()
    : iLuxUpdateable("LuxSocketServer")
{
	mHost = "127.0.0.1";
	mPort = 5150;
	if (gpBase->mpChatHandler) gpBase->mpChatHandler->SetSubmissionSink(this);

	InitSocket();
    Log("cLuxSocketServer created!\n");
}

bool cLuxSocketServer::InitSocket()
{
	if (!mGateway.Listen(mHost, mPort))
	{
		Log("Game Interaction Protocol listener failed: %s\n", mGateway.GetDiagnostic().c_str());
		return false;
	}
	Log("Socket listening on %s:%d\n", mHost.c_str(), mPort);
	return true;
}

void cLuxSocketServer::Update(float afTimeStep)
{
	cLuxGameInteractionGameAdapter gameAdapter;
	mGateway.Update(gameAdapter);
	LogNewGatewayDiagnostic();
}

void cLuxSocketServer::PublishEvent(const cGameInteractionEvent& aEvent)
{
	mGateway.Report(aEvent);
	LogNewGatewayDiagnostic();
}

void cLuxSocketServer::ReportLocalChatEntry(const cChatEntry& aEntry)
{
	PublishEvent(cGameInteractionEvent(eGameInteractionEvent_LocalChatSubmitted,
		aEntry.GetAuthor(), aEntry.GetMessage()));
}

void cLuxSocketServer::LogNewGatewayDiagnostic()
{
	const tString& diagnostic = mGateway.GetDiagnostic();
	if (!diagnostic.empty() && diagnostic != mLastGatewayDiagnostic)
		Log("Game Interaction Protocol: %s\n", diagnostic.c_str());
	mLastGatewayDiagnostic = diagnostic;
}

void cLuxSocketServer::SetConnectionSettings(const tString& host, int port)
{
    mHost = host;
    mPort = port;

	Log("LuxSocketServer config changed: re-init\n");
	mGateway.Shutdown();
	mLastGatewayDiagnostic.clear();
    InitSocket();
}

cLuxSocketServer::~cLuxSocketServer()
{
	mGateway.Shutdown();
    Log("cLuxSocketServer destroyed!\n");
}
