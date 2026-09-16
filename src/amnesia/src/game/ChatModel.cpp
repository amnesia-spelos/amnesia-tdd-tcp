#include "ChatModel.h"

#include <climits>

namespace
{
	bool IsUnicodeWhitespace(unsigned int aScalar)
	{
		return (aScalar >= 0x0009 && aScalar <= 0x000D) || aScalar == 0x0020 ||
			aScalar == 0x0085 || aScalar == 0x00A0 || aScalar == 0x1680 ||
			(aScalar >= 0x2000 && aScalar <= 0x200A) || aScalar == 0x2028 ||
			aScalar == 0x2029 || aScalar == 0x202F || aScalar == 0x205F ||
			aScalar == 0x3000;
	}

	bool IsControl(unsigned int aScalar)
	{
		return aScalar <= 0x001F || (aScalar >= 0x007F && aScalar <= 0x009F);
	}

	std::wstring Trim(const std::wstring& asText)
	{
		std::wstring::size_type first = 0;
		while (first < asText.size() && IsUnicodeWhitespace(
			static_cast<unsigned int>(asText[first]))) ++first;
		std::wstring::size_type last = asText.size();
		while (last > first && IsUnicodeWhitespace(
			static_cast<unsigned int>(asText[last - 1]))) --last;
		return asText.substr(first, last - first);
	}

	bool InspectText(const std::wstring& asText, size_t aMaxScalars, bool abRejectColon)
	{
		size_t scalarCount = 0;
		for (std::wstring::size_type index = 0; index < asText.size(); ++index)
		{
			unsigned int scalar = static_cast<unsigned int>(asText[index]);
#if WCHAR_MAX <= 0xFFFF
			if (scalar >= 0xD800 && scalar <= 0xDBFF)
			{
				if (index + 1 >= asText.size()) return false;
				const unsigned int low = static_cast<unsigned int>(asText[index + 1]);
				if (low < 0xDC00 || low > 0xDFFF) return false;
				scalar = 0x10000 + ((scalar - 0xD800) << 10) + (low - 0xDC00);
				++index;
			}
			else if (scalar >= 0xDC00 && scalar <= 0xDFFF) return false;
#else
			if (scalar >= 0xD800 && scalar <= 0xDFFF) return false;
#endif
			if (scalar > 0x10FFFF || IsControl(scalar) || (abRejectColon && scalar == ':'))
				return false;
			if (++scalarCount > aMaxScalars) return false;
		}
		return true;
	}
}

cChatEntry::cChatEntry()
{
}

cChatEntry::cChatEntry(const std::wstring& asAuthor, const std::wstring& asMessage)
	: msAuthor(asAuthor), msMessage(asMessage)
{
}

eChatEntryValidation cChatModel::TryCreateEntry(const std::wstring& asAuthor,
	const std::wstring& asMessage, cChatEntry& aEntry)
{
	if (!InspectText(asAuthor, static_cast<size_t>(-1), true))
		return eChatEntryValidation_InvalidAuthor;
	const std::wstring author = Trim(asAuthor);
	if (author.empty() || !InspectText(author, 32, true))
		return eChatEntryValidation_InvalidAuthor;
	if (!InspectText(asMessage, static_cast<size_t>(-1), false))
		return eChatEntryValidation_InvalidMessage;
	const std::wstring message = Trim(asMessage);
	if (message.empty() || !InspectText(message, 256, false))
		return eChatEntryValidation_InvalidMessage;
	aEntry = cChatEntry(author, message);
	return eChatEntryValidation_Valid;
}

cChatModel::cChatModel()
	: mContext(eChatContext_StartupMenu), mbComposerOpen(false)
{
}

void cChatModel::cContextState::Clear()
{
	mvEntries.clear();
	msDraft.clear();
	mfElapsedSinceEntry = 0.0;
}

cChatModel::cContextState& cChatModel::State(eChatContext aContext)
{
	return aContext == eChatContext_Game ? mGameState : mStartupMenuState;
}

const cChatModel::cContextState& cChatModel::State(eChatContext aContext) const
{
	return aContext == eChatContext_Game ? mGameState : mStartupMenuState;
}

eChatEntryValidation cChatModel::AddEntry(eChatContext aContext,
	const std::wstring& asAuthor, const std::wstring& asMessage)
{
	cChatEntry entry;
	const eChatEntryValidation validation = TryCreateEntry(asAuthor, asMessage, entry);
	if (validation != eChatEntryValidation_Valid) return validation;
	cContextState& state = State(aContext);
	if (state.mvEntries.size() == 10) state.mvEntries.erase(state.mvEntries.begin());
	state.mvEntries.push_back(entry);
	state.mfElapsedSinceEntry = 0.0;
	return eChatEntryValidation_Valid;
}

const std::vector<cChatEntry>& cChatModel::GetEntries(eChatContext aContext) const
{
	return State(aContext).mvEntries;
}

void cChatModel::SetContext(eChatContext aContext)
{
	if (aContext == mContext) return;
	State(mContext).Clear();
	mbComposerOpen = false;
	mContext = aContext;
}

void cChatModel::BeginLoading()
{
	State(eChatContext_Game).Clear();
	if (mContext == eChatContext_Game) mbComposerOpen = false;
}

void cChatModel::Tick(double afElapsedSeconds)
{
	if (mbComposerOpen || afElapsedSeconds <= 0.0) return;
	mStartupMenuState.mfElapsedSinceEntry += afElapsedSeconds;
	mGameState.mfElapsedSinceEntry += afElapsedSeconds;
}

float cChatModel::GetOpacity() const
{
	if (State(mContext).mvEntries.empty()) return 0.0f;
	if (mbComposerOpen) return 1.0f;
	const double elapsed = State(mContext).mfElapsedSinceEntry;
	if (elapsed <= 10.0) return 1.0f;
	if (elapsed >= 12.0) return 0.0f;
	return static_cast<float>((12.0 - elapsed) / 2.0);
}

void cChatModel::OpenComposer()
{
	State(mContext).msDraft.clear();
	mbComposerOpen = true;
}

void cChatModel::CancelComposer()
{
	State(mContext).msDraft.clear();
	mbComposerOpen = false;
}

void cChatModel::SetDraft(const std::wstring& asDraft)
{
	State(mContext).msDraft = asDraft;
}

const std::wstring& cChatModel::GetDraft() const
{
	return State(mContext).msDraft;
}

eChatSubmitResult cChatModel::SubmitComposer()
{
	cContextState& state = State(mContext);
	if (Trim(state.msDraft).empty())
	{
		CancelComposer();
		return eChatSubmitResult_Empty;
	}
	const eChatEntryValidation validation = AddEntry(mContext, L"Daniel", state.msDraft);
	if (validation != eChatEntryValidation_Valid) return eChatSubmitResult_Invalid;
	CancelComposer();
	return eChatSubmitResult_Added;
}
