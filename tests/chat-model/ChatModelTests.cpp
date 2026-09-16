#include "ChatModel.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace
{
	void Expect(bool abCondition, const char* apDescription)
	{
		if (abCondition) return;
		std::cerr << "FAIL: " << apDescription << "\n";
		exit(1);
	}

	std::wstring Repeated(const std::wstring& asText, size_t aCount)
	{
		std::wstring result;
		for (size_t index = 0; index < aCount; ++index) result += asText;
		return result;
	}
}

int main()
{
	cChatEntry entry;
	Expect(cChatModel::TryCreateEntry(L"  Daniel  ", L"  hello  ", entry) ==
		eChatEntryValidation_Valid, "valid Chat Entry is accepted");
	Expect(entry.GetAuthor() == L"Daniel", "Chat Author is trimmed");
	Expect(entry.GetMessage() == L"hello", "message is trimmed");
	Expect(cChatModel::TryCreateEntry(L"\x3000\x00A0" L"Alice" L"\x2003", L"\x2002" L" hello " L"\x202F" L"world " L"\x3000", entry) ==
		eChatEntryValidation_Valid, "Unicode whitespace is trimmed");
	Expect(entry.GetAuthor() == L"Alice", "all Unicode edge whitespace is removed");
	Expect(entry.GetMessage() == L"hello \x202Fworld", "internal Unicode spacing is preserved");
	Expect(cChatModel::TryCreateEntry(L"Alice", L"e\x0301", entry) ==
		eChatEntryValidation_Valid && entry.GetMessage() == L"e\x0301",
		"Unicode text is not normalized");

	Expect(cChatModel::TryCreateEntry(L" ", L"hello", entry) ==
		eChatEntryValidation_InvalidAuthor, "empty Chat Author is rejected");
	Expect(cChatModel::TryCreateEntry(L"Alice", L"\t", entry) ==
		eChatEntryValidation_InvalidMessage, "empty message is rejected");
	Expect(cChatModel::TryCreateEntry(L"A:lice", L"hello", entry) ==
		eChatEntryValidation_InvalidAuthor, "colon in Chat Author is rejected");
	Expect(cChatModel::TryCreateEntry(L"Alice", L"hello: world", entry) ==
		eChatEntryValidation_Valid, "colon in message is preserved");
	Expect(cChatModel::TryCreateEntry(L"Ali" L"\x0001" L"ce", L"hello", entry) ==
		eChatEntryValidation_InvalidAuthor, "control character in Chat Author is rejected");
	Expect(cChatModel::TryCreateEntry(L"Alice", L"hello\rworld", entry) ==
		eChatEntryValidation_InvalidMessage, "CR in message is rejected");
	Expect(cChatModel::TryCreateEntry(L"Alice", L"\nhello", entry) ==
		eChatEntryValidation_InvalidMessage, "LF is rejected even at an edge");

	const std::wstring scalarPair = L"\xD83D\xDE00";
	Expect(cChatModel::TryCreateEntry(Repeated(scalarPair, 32), Repeated(scalarPair, 256), entry) ==
		eChatEntryValidation_Valid, "Unicode scalar limits count surrogate pairs once");
	Expect(cChatModel::TryCreateEntry(Repeated(scalarPair, 33), L"hello", entry) ==
		eChatEntryValidation_InvalidAuthor, "Chat Author over 32 scalars is rejected");
	Expect(cChatModel::TryCreateEntry(L"Alice", Repeated(scalarPair, 257), entry) ==
		eChatEntryValidation_InvalidMessage, "message over 256 scalars is rejected");
	Expect(cChatModel::TryCreateEntry(L"\xD83D", L"hello", entry) ==
		eChatEntryValidation_InvalidAuthor, "unpaired high surrogate is rejected");
	Expect(cChatModel::TryCreateEntry(L"Alice", L"\xDE00", entry) ==
		eChatEntryValidation_InvalidMessage, "unpaired low surrogate is rejected");

	cChatModel model;
	for (int index = 0; index < 11; ++index)
	{
		const wchar_t digit = static_cast<wchar_t>(L'0' + index);
		Expect(model.AddEntry(eChatContext_StartupMenu, L"Author", std::wstring(1, digit)) ==
			eChatEntryValidation_Valid, "valid external Chat Entry is added");
	}
	Expect(model.GetEntries(eChatContext_StartupMenu).size() == 10,
		"Chat Log retains ten whole entries");
	Expect(model.GetEntries(eChatContext_StartupMenu).front().GetMessage() == L"1" &&
		model.GetEntries(eChatContext_StartupMenu).back().GetMessage()[0] == L':' ,
		"Chat Log evicts the oldest entry and preserves order");

	model.OpenComposer();
	model.SetDraft(L"menu draft");
	model.SetContext(eChatContext_Game);
	Expect(model.GetEntries(eChatContext_StartupMenu).empty() && model.GetDraft().empty(),
		"transition clears the prior context and draft");
	model.AddEntry(eChatContext_Game, L"Alice", L"old game");
	model.BeginLoading();
	Expect(model.GetEntries(eChatContext_Game).empty(), "loading clears pre-transition game entries");
	model.AddEntry(eChatContext_Game, L"Alice", L"upcoming game");
	Expect(model.GetEntries(eChatContext_Game).size() == 1,
		"post-clear arrivals belong to the upcoming game context");
	cChatModel loadingModel;
	loadingModel.BeginLoading();
	loadingModel.AddEntry(eChatContext_Game, L"Alice", L"arrived during loading");
	loadingModel.Tick(12.0);
	loadingModel.SetContext(eChatContext_Game);
	Expect(loadingModel.GetOpacity() == 0.0f,
		"upcoming game entries continue expiring while loading from the startup context");

	model.Tick(10.0);
	Expect(model.GetOpacity() == 1.0f, "Chat Log remains fully visible for ten seconds");
	model.Tick(1.0);
	Expect(model.GetOpacity() == 0.5f, "Chat Log fades linearly over two seconds");
	model.OpenComposer();
	model.Tick(100.0);
	Expect(model.GetOpacity() == 1.0f, "composer keeps the Chat Log visible and pauses timing");
	model.CancelComposer();
	Expect(model.GetOpacity() == 0.5f, "closing composer resumes the paused fade");
	model.Tick(1.0);
	Expect(model.GetOpacity() == 0.0f, "Chat Log is hidden after the fade");
	model.AddEntry(eChatContext_Game, L"Alice", L"new");
	Expect(model.GetOpacity() == 1.0f, "new Chat Entry restores visibility");

	model.OpenComposer();
	Expect(model.IsComposerOpen() && model.GetDraft().empty(), "opening composer starts empty");
	model.SetDraft(L"  /world  ");
	Expect(model.SubmitComposer() == eChatSubmitResult_Added, "valid draft submits");
	Expect(!model.IsComposerOpen() && model.GetEntries(eChatContext_Game).back().GetAuthor() == L"Daniel" &&
		model.GetEntries(eChatContext_Game).back().GetMessage() == L"/world",
		"local submission adds the literal Chat Author and preserves slash text");
	model.OpenComposer();
	model.SetDraft(L"   ");
	Expect(model.SubmitComposer() == eChatSubmitResult_Empty && !model.IsComposerOpen(),
		"empty submission closes without adding an entry");
	const size_t entryCount = model.GetEntries(eChatContext_Game).size();
	model.OpenComposer();
	model.SetDraft(L"cancelled");
	model.CancelComposer();
	model.OpenComposer();
	Expect(model.GetDraft().empty() && model.GetEntries(eChatContext_Game).size() == entryCount,
		"cancel and reopen clear the draft without submitting");
	model.SetDraft(L"bad\nmessage");
	Expect(model.SubmitComposer() == eChatSubmitResult_Invalid && model.IsComposerOpen(),
		"invalid non-empty submission stays open for correction");

	std::cout << "Chat model cases passed\n";
	return 0;
}
