#ifndef HPL_TEXT_DOCUMENT_H
#define HPL_TEXT_DOCUMENT_H

#include <cstddef>
#include <string>
#include <vector>

namespace hpl {

	//-----------------------------------------------------------------------

	/**
	 * The display lines of a text file. Built from the raw file bytes without
	 * any engine or locale dependencies:
	 * - Decodes UTF-8 and strips a leading byte-order mark. Bytes that aren't
	 *   valid UTF-8 decode one by one as Latin-1.
	 * - Splits lines on CRLF, LF and lone CR. A trailing line break gives an
	 *   empty last line, and there is always at least one line.
	 * - Expands tabs to spaces, with a stop every kTabSize columns.
	 */
	class cTextDocument
	{
	public:
		static const size_t kTabSize = 4;

		cTextDocument();

		void SetBytes(const char* apData, size_t alSize);

		size_t GetLineNum() const { return mvLines.size(); }
		const std::wstring& GetLine(size_t alIdx) const { return mvLines[alIdx]; }

	private:
		std::vector<std::wstring> mvLines;
	};

	//-----------------------------------------------------------------------

};
#endif // HPL_TEXT_DOCUMENT_H
