#ifndef LUX_CHAT_HANDLER_H
#define LUX_CHAT_HANDLER_H

#include "LuxBase.h"
#include "LuxTypes.h"
#include "ChatModel.h"
#include "gui/WidgetTextBox.h"

class iLuxChatSubmissionSink
{
public:
	virtual ~iLuxChatSubmissionSink() {}
	virtual void ReportLocalChatEntry(const cChatEntry& aEntry) = 0;
};

class cLuxChatHandler : public iLuxUpdateable
{
public:
	cLuxChatHandler();
	~cLuxChatHandler();

	void OnStart();
	void Update(float afTimeStep);
	void OnDraw(float afFrameTime);
	bool HandleInput(cInput* apInput);
	bool IsComposerOpen() const { return mModel.IsComposerOpen(); }
	void SetSubmissionSink(iLuxChatSubmissionSink* apSink) { mpSubmissionSink = apSink; }

private:
	bool CanOpenComposer() const;
	void OpenComposer();
	void CloseComposer(bool abSubmit);
	void EnsureComposerFocus();
	void DrawLog();
	bool TextChanged(iWidget* apWidget, const cGuiMessageData& aData);
	kGuiCallbackDeclarationEnd(TextChanged);
	bool ComposerEntered(iWidget* apWidget, const cGuiMessageData& aData);
	kGuiCallbackDeclarationEnd(ComposerEntered);

	cChatModel mModel;
	cGuiSet* mpSet;
	cGuiSkin* mpSkin;
	cWidgetTextBox* mpComposer;
	cGuiGfxElement* mpUnderline;
	cGuiSet* mpPreviousFocus;
	iLuxChatSubmissionSink* mpSubmissionSink;
	unsigned long mlLastTick;
	bool mbWasLoading;
	bool mbConsumedComposerInput;
};

#endif
