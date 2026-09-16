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

class cChatEntry
{
public:
	cChatEntry();
	cChatEntry(const std::wstring& asAuthor, const std::wstring& asMessage);

	const std::wstring& GetAuthor() const { return msAuthor; }
	const std::wstring& GetMessage() const { return msMessage; }

private:
	std::wstring msAuthor;
	std::wstring msMessage;
};

class cChatModel
{
public:
	cChatModel();

	static eChatEntryValidation TryCreateEntry(const std::wstring& asAuthor,
		const std::wstring& asMessage, cChatEntry& aEntry);

	eChatEntryValidation AddEntry(eChatContext aContext, const std::wstring& asAuthor,
		const std::wstring& asMessage);
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
	eChatSubmitResult SubmitComposer();

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
