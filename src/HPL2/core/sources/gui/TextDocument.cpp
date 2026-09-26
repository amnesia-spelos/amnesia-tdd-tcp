#include "gui/TextDocument.h"

#include <cwchar>

namespace hpl {

	//////////////////////////////////////////////////////////////////////////
	// HELPERS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	namespace
	{
		bool IsContinuationByte(unsigned char alByte)
		{
			return (alByte & 0xC0) == 0x80;
		}

		// Decodes one valid UTF-8 sequence at apData. Returns the number of bytes
		// used, or 0 if the bytes there aren't valid UTF-8.
		size_t DecodeUtf8(const unsigned char* apData, size_t alSize, unsigned int& alCodePoint)
		{
			const unsigned char lLead = apData[0];
			size_t lLength;
			unsigned int lMinimum;
			if (lLead < 0x80)		{ alCodePoint = lLead; return 1; }
			else if (lLead >= 0xC2 && lLead <= 0xDF) { lLength = 2; lMinimum = 0x80;		alCodePoint = lLead & 0x1F; }
			else if (lLead >= 0xE0 && lLead <= 0xEF) { lLength = 3; lMinimum = 0x800;		alCodePoint = lLead & 0x0F; }
			else if (lLead >= 0xF0 && lLead <= 0xF4) { lLength = 4; lMinimum = 0x10000;	alCodePoint = lLead & 0x07; }
			else return 0;

			if (alSize < lLength) return 0;
			for (size_t i = 1; i < lLength; ++i)
			{
				if (IsContinuationByte(apData[i]) == false) return 0;
				alCodePoint = (alCodePoint << 6) | (apData[i] & 0x3F);
			}

			if (alCodePoint < lMinimum) return 0;
			if (alCodePoint >= 0xD800 && alCodePoint <= 0xDFFF) return 0;
			if (alCodePoint > 0x10FFFF) return 0;
			return lLength;
		}

		void AppendCodePoint(std::wstring& asLine, unsigned int alCodePoint)
		{
#if WCHAR_MAX > 0xFFFF
			asLine += static_cast<wchar_t>(alCodePoint);
#else
			if (alCodePoint > 0xFFFF)
			{
				alCodePoint -= 0x10000;
				asLine += static_cast<wchar_t>(0xD800 + (alCodePoint >> 10));
				asLine += static_cast<wchar_t>(0xDC00 + (alCodePoint & 0x3FF));
			}
			else
			{
				asLine += static_cast<wchar_t>(alCodePoint);
			}
#endif
		}
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cTextDocument::cTextDocument()
		: mvLines(1)
	{
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void cTextDocument::SetBytes(const char* apData, size_t alSize)
	{
		const unsigned char* pData = reinterpret_cast<const unsigned char*>(apData);
		size_t lPos = 0;
		if (alSize >= 3 && pData[0] == 0xEF && pData[1] == 0xBB && pData[2] == 0xBF)
			lPos = 3;

		mvLines.assign(1, std::wstring());
		size_t lColumn = 0;

		while (lPos < alSize)
		{
			const unsigned char lByte = pData[lPos];

			if (lByte == '\r' || lByte == '\n')
			{
				++lPos;
				if (lByte == '\r' && lPos < alSize && pData[lPos] == '\n')
					++lPos;

				mvLines.push_back(std::wstring());
				lColumn = 0;
				continue;
			}

			if (lByte == '\t')
			{
				const size_t lSpaces = kTabSize - lColumn % kTabSize;
				mvLines.back().append(lSpaces, L' ');
				lColumn += lSpaces;
				++lPos;
				continue;
			}

			unsigned int lCodePoint;
			size_t lLength = DecodeUtf8(pData + lPos, alSize - lPos, lCodePoint);
			if (lLength == 0)
			{
				// Not UTF-8, so take the byte as Latin-1
				lCodePoint = lByte;
				lLength = 1;
			}

			AppendCodePoint(mvLines.back(), lCodePoint);
			++lColumn;
			lPos += lLength;
		}
	}

	//-----------------------------------------------------------------------

}
