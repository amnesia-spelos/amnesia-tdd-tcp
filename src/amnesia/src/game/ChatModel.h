#ifndef LUX_CHAT_MODEL_H
#define LUX_CHAT_MODEL_H

#include <string>
#include <vector>

enum eChatEntryValidation
{
	eChatEntryValidation_Valid,
	eChatEntryValidation_InvalidAuthor,
	eChatEntryValidation_InvalidMessage
};

enum eChatContext
{
	eChatContext_StartupMenu,
	eChatContext_Game
};

enum eChatSubmitResult
{
	eChatSubmitResult_Added,
	eChatSubmitResult_Empty,
	eChatSubmitResult_Invalid
};

// A Chat Color, or none when that part of a Chat Entry keeps its default color.
class cChatColor
{
public:
	cChatColor();
	cChatColor(unsigned char acRed, unsigned char acGreen, unsigned char acBlue);

	bool IsSet() const { return mbSet; }
	unsigned char GetRed() const { return mcRed; }
	unsigned char GetGreen() const { return mcGreen; }
	unsigned char GetBlue() const { return mcBlue; }
	bool operator==(const cChatColor& aColor) const;

private:
	bool mbSet;
	unsigned char mcRed;
	unsigned char mcGreen;
	unsigned char mcBlue;
};

class cChatEntry
{
public:
	cChatEntry();
	cChatEntry(const std::wstring& asAuthor, const std::wstring& asMessage,
		const cChatColor& aAuthorColor = cChatColor(), const cChatColor& aMessageColor = cChatColor());

	const std::wstring& GetAuthor() const { return msAuthor; }
	const std::wstring& GetMessage() const { return msMessage; }
	const cChatColor& GetAuthorColor() const { return mAuthorColor; }
	const cChatColor& GetMessageColor() const { return mMessageColor; }

private:
	std::wstring msAuthor;
	std::wstring msMessage;
	cChatColor mAuthorColor;
	cChatColor mMessageColor;
};

class cChatModel
{
public:
	cChatModel();

	// Chat Colors never affect validation.
	static eChatEntryValidation TryCreateEntry(const std::wstring& asAuthor,
		const std::wstring& asMessage, cChatEntry& aEntry, const cChatColor& aAuthorColor = cChatColor(),
		const cChatColor& aMessageColor = cChatColor());

	eChatEntryValidation AddEntry(eChatContext aContext, const std::wstring& asAuthor,
		const std::wstring& asMessage, const cChatColor& aAuthorColor = cChatColor(),
		const cChatColor& aMessageColor = cChatColor());
	const std::vector<cChatEntry>& GetEntries(eChatContext aContext) const;

	void SetContext(eChatContext aContext);
	eChatContext GetContext() const { return mContext; }
	void BeginLoading();

	void Tick(double afElapsedSeconds);
	float GetOpacity() const;

	void OpenComposer();
	void CancelComposer();
	bool IsComposerOpen() const { return mbComposerOpen; }
	void SetDraft(const std::wstring& asDraft);
	const std::wstring& GetDraft() const;
	eChatSubmitResult SubmitComposer(const std::wstring& asAuthor);

private:
	struct cContextState
	{
		std::vector<cChatEntry> mvEntries;
		std::wstring msDraft;
		double mfElapsedSinceEntry;

		cContextState() : mfElapsedSinceEntry(0.0) {}
		void Clear();
	};

	cContextState& State(eChatContext aContext);
	const cContextState& State(eChatContext aContext) const;

	cContextState mStartupMenuState;
	cContextState mGameState;
	eChatContext mContext;
	bool mbComposerOpen;
};

#endif
