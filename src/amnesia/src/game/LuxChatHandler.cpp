#include "LuxChatHandler.h"

#include "LuxEffectHandler.h"
#include "LuxInputHandler.h"
#include "LuxMainMenu.h"
#include "LuxMapHandler.h"
#include "LuxMessageHandler.h"

#include "gui/Gui.h"
#include "gui/GuiSet.h"
#include "gui/WidgetTextBox.h"
#include "graphics/FontData.h"
#include "scene/Viewport.h"
#include "system/Platform.h"

namespace
{
	const cVector2f gvChatFontSize(18, 18);
	const float gfChatLeft = 20.0f;
	const float gfChatWidth = 440.0f;
	const float gfChatComposerY = 557.0f;
	const float gfChatLineHeight = 21.0f;
	const float gfChatTop = 335.0f;
}

cLuxChatHandler::cLuxChatHandler()
	: iLuxUpdateable("LuxChatHandler"), mpSet(NULL), mpSkin(NULL), mpComposer(NULL),
	  mpUnderline(NULL), mpPreviousFocus(NULL), mpSubmissionSink(NULL), mlLastTick(0),
	  mbWasLoading(false), mbConsumedComposerInput(false)
{
}

cLuxChatHandler::~cLuxChatHandler()
{
}

void cLuxChatHandler::OnStart()
{
	cGui* pGui = gpBase->mpEngine->GetGui();
	mpSkin = pGui->CreateSkin("gui_main_menu.skin");
	mpSet = pGui->CreateSet("Chat", mpSkin);
	mpSet->SetVirtualSize(cVector2f(800, 600), -1000, 1000);
	mpSet->SetDrawPriority(2);
	mpSet->SetActive(true);
	mpSet->SetDrawMouse(false);

	mpComposer = mpSet->CreateWidgetTextBox(cVector3f(gfChatLeft - 3, gfChatComposerY - 2, 20),
		cVector2f(gfChatWidth + 6, 25), _W(""));
	mpComposer->SetDrawFrame(false);
	mpComposer->SetShowButtons(false);
	mpComposer->SetMaxTextLength(512);
	mpComposer->SetDefaultFontSize(gvChatFontSize);
	mpComposer->SetDefaultFontColor(cColor(0.9f, 0.9f, 0.86f, 1));
	mpComposer->AddCallback(eGuiMessage_TextChange, this, kGuiCallback(TextChanged));
	mpComposer->AddCallback(eGuiMessage_TextBoxEnter, this, kGuiCallback(ComposerEntered));
	mpComposer->SetVisible(false);
	mpComposer->SetEnabled(false);

	mpUnderline = pGui->CreateGfxFilledRect(cColor(1, 1), eGuiMaterial_Alpha);
	gpBase->mpMapHandler->GetViewport()->AddGuiSet(mpSet);
	gpBase->mpMainMenu->GetViewport()->AddGuiSet(mpSet);
	mlLastTick = cPlatform::GetApplicationTime();
}

void cLuxChatHandler::Update(float afTimeStep)
{
	const unsigned long now = cPlatform::GetApplicationTime();
	mModel.Tick((now - mlLastTick) / 1000.0);
	mlLastTick = now;

	const bool loading = gpBase->mpInputHandler->GetState() == eLuxInputState_LoadScreen;
	if(loading && !mbWasLoading)
	{
		if(mModel.IsComposerOpen()) CloseComposer(false);
		mModel.BeginLoading();
	}
	mbWasLoading = loading;

	if(!loading)
	{
		const eChatContext context = gpBase->mpMapHandler->MapIsLoaded() ?
			eChatContext_Game : eChatContext_StartupMenu;
		if(context != mModel.GetContext())
		{
			if(mModel.IsComposerOpen()) CloseComposer(false);
			mModel.SetContext(context);
		}
	}

	if(mModel.IsComposerOpen() && !CanOpenComposer()) CloseComposer(false);
	if(mModel.IsComposerOpen())
	{
		EnsureComposerFocus();
		mModel.SetDraft(mpComposer->GetText());
	}
}

bool cLuxChatHandler::CanOpenComposer() const
{
	const eLuxInputState state = gpBase->mpInputHandler->GetState();
	if(state == eLuxInputState_Game)
	{
		return gpBase->mpMapHandler->MapIsLoaded() &&
			!gpBase->mpMessageHandler->IsPauseMessageActive() &&
			!gpBase->mpEffectHandler->GetPlayerIsPaused();
	}
	if(state != eLuxInputState_MainMenu || gpBase->mpMapHandler->MapIsLoaded() ||
		gpBase->mpMainMenu->IsTransitioning()) return false;

	return !gpBase->mpMainMenu->HasConflictingChatInputOwner();
}

bool cLuxChatHandler::HandleInput(cInput* apInput)
{
	if(mbConsumedComposerInput)
	{
		mbConsumedComposerInput = false;
		return true;
	}
	if(mModel.IsComposerOpen())
	{
		if(apInput->BecameTriggerd(eLuxAction_Exit)) CloseComposer(false);
		return true;
	}
	if(apInput->BecameTriggerd(eLuxAction_Chat) && CanOpenComposer())
	{
		OpenComposer();
		return true;
	}
	return false;
}

void cLuxChatHandler::OpenComposer()
{
	mModel.OpenComposer();
	mpComposer->SetText(_W(""));
	mpComposer->SetVisible(true);
	mpComposer->SetEnabled(true);
	mpSet->SetDrawMouse(true);
	mpPreviousFocus = gpBase->mpEngine->GetGui()->GetFocusedSet();
	EnsureComposerFocus();
}

void cLuxChatHandler::EnsureComposerFocus()
{
	cGui* pGui = gpBase->mpEngine->GetGui();
	if(pGui->GetFocusedSet() != mpSet) pGui->SetFocus(mpSet);
	if(mpSet->GetFocusedWidget() != mpComposer)
	{
		mpSet->SetFocusedWidget(mpComposer);
		mpComposer->SetSelectedText((int)mpComposer->GetText().size(), 0);
	}
}

void cLuxChatHandler::CloseComposer(bool abSubmit)
{
	mModel.SetDraft(mpComposer->GetText());
	if(abSubmit)
	{
		const eChatSubmitResult result = mModel.SubmitComposer();
		if(result == eChatSubmitResult_Invalid) return;
		if(result == eChatSubmitResult_Added && mpSubmissionSink)
			mpSubmissionSink->ReportLocalChatEntry(mModel.GetEntries(mModel.GetContext()).back());
	}
	else
	{
		mModel.CancelComposer();
	}
	mpComposer->SetVisible(false);
	mpComposer->SetEnabled(false);
	mpSet->SetDrawMouse(false);
	mpSet->SetFocusedWidget(NULL);
	if(mpPreviousFocus) gpBase->mpEngine->GetGui()->SetFocus(mpPreviousFocus);
	mpPreviousFocus = NULL;
}

bool cLuxChatHandler::TextChanged(iWidget* apWidget, const cGuiMessageData& aData)
{
	mModel.SetDraft(mpComposer->GetText());
	return true;
}
kGuiCallbackDeclaredFuncEnd(cLuxChatHandler, TextChanged);

bool cLuxChatHandler::ComposerEntered(iWidget* apWidget, const cGuiMessageData& aData)
{
	CloseComposer(true);
	mbConsumedComposerInput = true;
	return true;
}
kGuiCallbackDeclaredFuncEnd(cLuxChatHandler, ComposerEntered);

void cLuxChatHandler::OnDraw(float afFrameTime)
{
	const eLuxInputState state = gpBase->mpInputHandler->GetState();
	const bool startupMenu = state == eLuxInputState_MainMenu && !gpBase->mpMapHandler->MapIsLoaded();
	const bool gameplay = state == eLuxInputState_Game && gpBase->mpMapHandler->MapIsLoaded();
	if(!startupMenu && !gameplay) return;
	DrawLog();
	if(mModel.IsComposerOpen())
	{
		mpSet->DrawGfx(mpUnderline, cVector3f(gfChatLeft, gfChatComposerY + 22, 19),
			cVector2f(gfChatWidth, 1), cColor(0.65f, 0.65f, 0.60f, 1));
	}
}

void cLuxChatHandler::DrawLog()
{
	const float alpha = mModel.GetOpacity();
	if(alpha <= 0) return;

	const std::vector<cChatEntry>& entries = mModel.GetEntries(mModel.GetContext());
	float y = gfChatComposerY - 7;
	for(std::vector<cChatEntry>::const_reverse_iterator it = entries.rbegin(); it != entries.rend(); ++it)
	{
		const tWString prefix = _W("<") + it->GetAuthor() + _W(">:");
		const tWString line = prefix + _W(" ") + it->GetMessage();
		tWStringVec rows;
		gpBase->mpDefaultFont->GetWordWrapRows(gfChatWidth, gfChatLineHeight,
			gvChatFontSize, line, &rows);
		for(tWStringVec::reverse_iterator row = rows.rbegin(); row != rows.rend(); ++row)
		{
			y -= gfChatLineHeight;
			if(y < gfChatTop) return;
			mpSet->DrawFont(*row, gpBase->mpDefaultFont, cVector3f(gfChatLeft, y, 18),
				gvChatFontSize, cColor(0.9f, 0.9f, 0.86f, alpha), eFontAlign_Left);
			if(row + 1 == rows.rend())
			{
				mpSet->DrawFont(prefix, gpBase->mpDefaultFont, cVector3f(gfChatLeft, y, 18.1f),
					gvChatFontSize, cColor(0.55f, 0.62f, 0.65f, alpha), eFontAlign_Left);
			}
		}
	}
}
