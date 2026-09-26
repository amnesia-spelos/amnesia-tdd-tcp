#include "gui/TextDocument.h"

#include <cstdlib>
#include <iostream>
#include <string>

using hpl::cTextDocument;

namespace
{
	void Expect(bool abCondition, const char* apDescription)
	{
		if (abCondition) return;
		std::cerr << "FAIL: " << apDescription << "\n";
		exit(1);
	}

	cTextDocument FromBytes(const std::string& asBytes)
	{
		cTextDocument document;
		document.SetBytes(asBytes.data(), asBytes.size());
		return document;
	}

	bool HasLines(const cTextDocument& aDocument, const wchar_t* const* apLines, size_t alCount)
	{
		if (aDocument.GetLineNum() != alCount) return false;
		for (size_t index = 0; index < alCount; ++index)
		{
			if (aDocument.GetLine(index) != apLines[index]) return false;
		}
		return true;
	}

	template <size_t tCount>
	bool HasLines(const cTextDocument& aDocument, const wchar_t* const (&apLines)[tCount])
	{
		return HasLines(aDocument, apLines, tCount);
	}
}

int main()
{
	// Empty input and trailing line breaks
	{
		const wchar_t* const expected[] = { L"" };
		Expect(HasLines(cTextDocument(), expected), "a new document has one empty line");
		Expect(HasLines(FromBytes(""), expected), "an empty buffer gives one empty line");
		Expect(HasLines(FromBytes("\xEF\xBB\xBF"), expected), "a lone BOM gives one empty line");
	}
	{
		const wchar_t* const expected[] = { L"void OnStart()", L"" };
		Expect(HasLines(FromBytes("void OnStart()\n"), expected),
			"a trailing line break produces an empty last line");
	}
	{
		const wchar_t* const expected[] = { L"", L"" };
		Expect(HasLines(FromBytes("\r\n"), expected), "a lone CRLF gives two empty lines");
	}
	{
		const wchar_t* const expected[] = { L"abc" };
		Expect(HasLines(FromBytes("abc"), expected), "text without a line break is one line");
	}

	// Line endings
	{
		const wchar_t* const expected[] = { L"a", L"b", L"c" };
		Expect(HasLines(FromBytes("a\nb\nc"), expected), "LF splits lines");
		Expect(HasLines(FromBytes("a\r\nb\r\nc"), expected), "CRLF splits lines once");
		Expect(HasLines(FromBytes("a\rb\rc"), expected), "lone CR splits lines");
		Expect(HasLines(FromBytes("a\r\nb\rc"), expected), "CRLF then lone CR splits lines");
	}
	{
		const wchar_t* const expected[] = { L"a", L"", L"b", L"", L"c", L"" };
		Expect(HasLines(FromBytes("a\n\nb\r\rc\r\n"), expected),
			"mixed line endings keep blank lines between them");
	}
	{
		const wchar_t* const expected[] = { L"a", L"", L"b" };
		Expect(HasLines(FromBytes("a\n\r\nb"), expected), "LF followed by CRLF is two breaks");
		Expect(HasLines(FromBytes("a\r\r\nb"), expected), "CR followed by CRLF is two breaks");
	}

	// UTF-8 decoding
	{
		const wchar_t* const expected[] = { L"caf\x00E9 \x20AC", L"\x0161\x0165" };
		Expect(HasLines(FromBytes("caf\xC3\xA9 \xE2\x82\xAC\n\xC5\xA1\xC5\xA5"), expected),
			"two- and three-byte UTF-8 sequences decode");
	}
	{
		const wchar_t* const expected[] = { L"x\xD83D\xDE00y" };
		Expect(HasLines(FromBytes("x\xF0\x9F\x98\x80y"), expected),
			"four-byte UTF-8 sequences decode to a surrogate pair");
	}
	{
		const wchar_t* const expected[] = { L"void OnStart()" };
		Expect(HasLines(FromBytes("\xEF\xBB\xBFvoid OnStart()"), expected), "a leading BOM is stripped");
	}
	{
		const wchar_t* const expected[] = { L"a\xFEFF" L"b" };
		Expect(HasLines(FromBytes("a\xEF\xBB\xBF" "b"), expected), "a BOM that isn't leading is kept");
	}

	// Latin-1 fallback
	{
		const wchar_t* const expected[] = { L"caf\x00E9", L"\x00E9t\x00E9" };
		Expect(HasLines(FromBytes("caf\xE9\r\n\xE9t\xE9"), expected),
			"invalid bytes decode as Latin-1");
	}
	{
		const wchar_t* const expected[] = { L"\x00E9\x00E9" };
		Expect(HasLines(FromBytes("\xC3\xA9\xE9"), expected),
			"a valid sequence before an invalid byte still decodes as UTF-8");
	}
	{
		const wchar_t* const expected[] = { L"\x00E2\x0082x" };
		Expect(HasLines(FromBytes("\xE2\x82x"), expected),
			"a truncated sequence decodes byte by byte as Latin-1");
	}
	{
		const wchar_t* const expected[] = { L"\x00E2\x0082" };
		Expect(HasLines(FromBytes("\xE2\x82"), expected),
			"a sequence truncated by the end of the buffer decodes as Latin-1");
	}
	{
		const wchar_t* const expected[] = { L"\x00C0\x00AF" };
		Expect(HasLines(FromBytes("\xC0\xAF"), expected), "an overlong sequence decodes as Latin-1");
	}
	{
		const wchar_t* const expected[] = { L"\x00ED\x00A0\x0080" };
		Expect(HasLines(FromBytes("\xED\xA0\x80"), expected),
			"an encoded surrogate decodes as Latin-1");
	}
	{
		const wchar_t* const expected[] = { L"\x00F4\x0090\x0080\x0080" };
		Expect(HasLines(FromBytes("\xF4\x90\x80\x80"), expected),
			"a code point above U+10FFFF decodes as Latin-1");
	}
	{
		const wchar_t* const expected[] = { L"\x0080\x00BF" };
		Expect(HasLines(FromBytes("\x80\xBF"), expected), "stray continuation bytes decode as Latin-1");
	}

	// Tab stops
	{
		const wchar_t* const expected[] = { L"    b" };
		Expect(HasLines(FromBytes("\tb"), expected), "a leading tab expands to four columns");
	}
	{
		const wchar_t* const expected[] = { L"a   b" };
		Expect(HasLines(FromBytes("a\tb"), expected), "a tab after one column expands to the next stop");
	}
	{
		const wchar_t* const expected[] = { L"abc b" };
		Expect(HasLines(FromBytes("abc\tb"), expected), "a tab after three columns expands to one space");
	}
	{
		const wchar_t* const expected[] = { L"abcd    b" };
		Expect(HasLines(FromBytes("abcd\tb"), expected), "a tab on a stop expands to a full stop");
	}
	{
		const wchar_t* const expected[] = { L"        x", L"ab  y" };
		Expect(HasLines(FromBytes("\t\tx\nab\ty"), expected), "tab columns restart on each line");
	}
	{
		const wchar_t* const expected[] = { L"\x00E9   b" };
		Expect(HasLines(FromBytes("\xC3\xA9\tb"), expected), "a multi-byte character is one column");
	}
	{
		const wchar_t* const expected[] = { L"\xD83D\xDE00   b" };
		Expect(HasLines(FromBytes("\xF0\x9F\x98\x80\tb"), expected), "a surrogate pair is one column");
	}

	std::cout << "Text document tests passed.\n";
	return 0;
}
