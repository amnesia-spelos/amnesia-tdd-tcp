#include "gui/WidgetTextArea.h"

#include "system/String.h"

#include "math/Math.h"

#include "graphics/FontData.h"

#include "gui/Gui.h"
#include "gui/GuiSkin.h"
#include "gui/GuiSet.h"
#include "gui/GuiGfxElement.h"

#include "gui/WidgetSlider.h"

namespace hpl {

	static const float kfTextPadding = 4;
	static const float kfTextTopMargin = 2;
	static const int klWheelLines = 3;
	static const int klScrollColumns = 4;

	//-----------------------------------------------------------------------

	// Sets a slider to cover alMaxValue+1 positions, where the bar covers
	// afVisibleShare of them, and returns the value clamped to the new range.
	static int SetSliderRange(cWidgetSlider *apSlider, int alMaxValue, float afVisibleShare, int alStep, int alPageStep)
	{
		apSlider->SetMaxValue(alMaxValue);
		apSlider->SetBarValueSize(cMath::Max((int)(afVisibleShare * (alMaxValue+1)), 1));
		apSlider->SetButtonValueAdd(cMath::Max(alStep, 1));
		apSlider->SetBarClickValueAdd(cMath::Max(alPageStep, 1));
		apSlider->SetValue(apSlider->GetValue());
		return apSlider->GetValue();
	}

	//////////////////////////////////////////////////////////////////////////
	// CONSTRUCTORS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	cWidgetTextArea::cWidgetTextArea(cGuiSet *apSet, cGuiSkin *apSkin) : iWidget(eWidgetType_TextArea,apSet, apSkin)
	{
		mbClipsGraphics = true;

		mfBackgroundZ = -0.5;
		mfSliderWidth = mpSkin->GetAttribute(eGuiSkinAttribute_ListBoxSliderWidth).x;

		mfLongestLineWidth = 0;
		mfLineNumberWidth = 0;

		mlFirstLine = 0;
		mlVisibleLines = 1;
		mlScrollPixelsX = 0;

		mpVerticalSlider = NULL;
		mpHorizontalSlider = NULL;
	}

	//-----------------------------------------------------------------------

	cWidgetTextArea::~cWidgetTextArea()
	{
		if(mpSet->IsDestroyingSet()==false)
		{
			mpSet->DestroyWidget(mpVerticalSlider);
			mpSet->DestroyWidget(mpHorizontalSlider);
		}
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PUBLIC METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void cWidgetTextArea::SetDocument(const cTextDocument& aDocument)
	{
		mDocument = aDocument;

		if(mpVerticalSlider && mpHorizontalSlider)
		{
			mpVerticalSlider->SetValue(0);
			mpHorizontalSlider->SetValue(0);
		}

		UpdateTextMetrics();
	}

	//-----------------------------------------------------------------------

	void cWidgetTextArea::SetDefaultFontType(iFontData *apFont)
	{
		iWidget::SetDefaultFontType(apFont);
		UpdateTextMetrics();
	}

	void cWidgetTextArea::SetDefaultFontSize(const cVector2f& avSize)
	{
		iWidget::SetDefaultFontSize(avSize);
		UpdateTextMetrics();
	}

	//-----------------------------------------------------------------------

	//////////////////////////////////////////////////////////////////////////
	// PROTECTED METHODS
	//////////////////////////////////////////////////////////////////////////

	//-----------------------------------------------------------------------

	void cWidgetTextArea::UpdateTextMetrics()
	{
		mfLongestLineWidth = 0;
		mfLineNumberWidth = 0;

		if(mpDefaultFontType)
		{
			for(size_t i=0; i<mDocument.GetLineNum(); ++i)
			{
				const std::wstring& sLine = mDocument.GetLine(i);
				if(sLine.empty()) continue;

				float fWidth = mpDefaultFontType->GetLength(mvDefaultFontSize, sLine.c_str());
				if(fWidth > mfLongestLineWidth) mfLongestLineWidth = fWidth;
			}

			tWString sLastLineNumber = cString::ToStringW((int)mDocument.GetLineNum());
			mfLineNumberWidth = mpDefaultFontType->GetLength(mvDefaultFontSize, sLastLineNumber.c_str()) +
								kfTextPadding*2;
		}

		UpdateScrollRanges();
	}

	//-----------------------------------------------------------------------

	void cWidgetTextArea::UpdateScrollRanges()
	{
		cVector2f vViewSize = GetTextViewSize();
		mlVisibleLines = (int)((vViewSize.y - kfTextTopMargin) / GetRowHeight());
		if(mlVisibleLines < 1) mlVisibleLines = 1;

		if(mpVerticalSlider==NULL || mpHorizontalSlider==NULL)
			return;

		////////////////////////////////
		// Vertical, in lines
		int lLineNum = (int)mDocument.GetLineNum();
		mlFirstLine = SetSliderRange(	mpVerticalSlider, cMath::Max(lLineNum - mlVisibleLines, 0),
										(float)mlVisibleLines / lLineNum, 1, mlVisibleLines-1);

		////////////////////////////////
		// Horizontal, in pixels
		float fContentWidth = mfLongestLineWidth + kfTextPadding*2;
		float fViewWidth = cMath::Max(vViewSize.x, 1.0f);
		mlScrollPixelsX = SetSliderRange(	mpHorizontalSlider, cMath::Max((int)ceilf(fContentWidth - fViewWidth), 0),
											fViewWidth / cMath::Max(fContentWidth, fViewWidth),
											(int)(mvDefaultFontSize.x * klScrollColumns), (int)fViewWidth);
	}

	//-----------------------------------------------------------------------

	float cWidgetTextArea::GetRowHeight()
	{
		return mvDefaultFontSize.y + 2;
	}

	//-----------------------------------------------------------------------

	cVector2f cWidgetTextArea::GetTextViewSize()
	{
		return cVector2f(	cMath::Max(mvSize.x - mfSliderWidth - mfLineNumberWidth, 0.0f),
							cMath::Max(mvSize.y - mfSliderWidth, 0.0f));
	}

	//-----------------------------------------------------------------------

	bool cWidgetTextArea::MoveVerticalSlider(iWidget* apWidget, const cGuiMessageData& aData)
	{
		mlFirstLine = aData.mlVal;

		return true;
	}
	kGuiCallbackDeclaredFuncEnd(cWidgetTextArea,MoveVerticalSlider)

	bool cWidgetTextArea::MoveHorizontalSlider(iWidget* apWidget, const cGuiMessageData& aData)
	{
		mlScrollPixelsX = aData.mlVal;

		return true;
	}
	kGuiCallbackDeclaredFuncEnd(cWidgetTextArea,MoveHorizontalSlider)

	//-----------------------------------------------------------------------

	void cWidgetTextArea::OnInit()
	{
		mpVerticalSlider = mpSet->CreateWidgetSlider(eWidgetSliderOrientation_Vertical,0,0,0,this);
		mpVerticalSlider->AddCallback(eGuiMessage_SliderMove,this,kGuiCallback(MoveVerticalSlider));

		mpHorizontalSlider = mpSet->CreateWidgetSlider(eWidgetSliderOrientation_Horizontal,0,0,0,this);
		mpHorizontalSlider->AddCallback(eGuiMessage_SliderMove,this,kGuiCallback(MoveHorizontalSlider));

		OnChangeSize();
	}

	//-----------------------------------------------------------------------

	void cWidgetTextArea::OnLoadGraphics()
	{
		mpGfxBackground = mpSkin->GetGfx(eGuiSkinGfx_ListBoxBackground);

		mvGfxBorders[0] = mpSkin->GetGfx(eGuiSkinGfx_FrameBorderRight);
		mvGfxBorders[1] = mpSkin->GetGfx(eGuiSkinGfx_FrameBorderLeft);
		mvGfxBorders[2] = mpSkin->GetGfx(eGuiSkinGfx_FrameBorderUp);
		mvGfxBorders[3] = mpSkin->GetGfx(eGuiSkinGfx_FrameBorderDown);

		mvGfxCorners[0] = mpSkin->GetGfx(eGuiSkinGfx_FrameCornerLU);
		mvGfxCorners[1] = mpSkin->GetGfx(eGuiSkinGfx_FrameCornerRU);
		mvGfxCorners[2] = mpSkin->GetGfx(eGuiSkinGfx_FrameCornerRD);
		mvGfxCorners[3] = mpSkin->GetGfx(eGuiSkinGfx_FrameCornerLD);

		// The default font is only known once graphics are loaded
		UpdateTextMetrics();
	}

	//-----------------------------------------------------------------------

	void cWidgetTextArea::OnChangeSize()
	{
		if(mpVerticalSlider && mpHorizontalSlider)
		{
			mpVerticalSlider->SetSize(cVector2f(mfSliderWidth, cMath::Max(mvSize.y - mfSliderWidth, 0.0f)));
			mpVerticalSlider->SetPosition(cVector3f(mvSize.x - mfSliderWidth, 0, 0.2f));

			mpHorizontalSlider->SetSize(cVector2f(cMath::Max(mvSize.x - mfSliderWidth, 0.0f), mfSliderWidth));
			mpHorizontalSlider->SetPosition(cVector3f(0, mvSize.y - mfSliderWidth, 0.2f));
		}

		UpdateScrollRanges();
	}

	//-----------------------------------------------------------------------

	void cWidgetTextArea::OnDraw(float afTimeStep, cGuiClipRegion *apClipRegion)
	{
		////////////////////////////////
		// Background
		mpSet->DrawGfx(	mpGfxBackground,GetGlobalPosition() +cVector3f(0,0,mfBackgroundZ),
						mvSize);

		////////////////////////////////
		// Line-number separator
		mpSet->DrawGfx(	cGui::mpGfxRect, GetGlobalPosition() + cVector3f(mfLineNumberWidth-1,0,mfBackgroundZ+0.05f),
						cVector2f(1, GetTextViewSize().y), cColor(0.3f, 0.4f));

		////////////////////////////////
		// Borders
		DrawBordersAndCorners(	NULL, mvGfxBorders, mvGfxCorners,
								GetGlobalPosition() -
									cVector3f(	mvGfxCorners[0]->GetActiveSize().x,
												mvGfxCorners[0]->GetActiveSize().y,0),
									mvSize +	mvGfxCorners[0]->GetActiveSize() +
												mvGfxCorners[2]->GetActiveSize());
	}

	//-----------------------------------------------------------------------

	void cWidgetTextArea::OnDrawAfterClip(float afTimeStep, cGuiClipRegion *apClipRegion)
	{
		const cVector2f vViewSize = GetTextViewSize();
		const float fRowHeight = GetRowHeight();

		const int lLineNum = (int)mDocument.GetLineNum();
		const int lEndLine = cMath::Min(mlFirstLine + mlVisibleLines + 1, lLineNum);

		const cVector3f vStart = GetGlobalPosition() + cVector3f(0, kfTextTopMargin, 0.01f + mfBackgroundZ + 0.1f);

		////////////////////////////////
		// Line numbers
		cGuiClipRegion* pRegion = apClipRegion->CreateChild(GetGlobalPosition(), cVector2f(mfLineNumberWidth, vViewSize.y));
		mpSet->SetCurrentClipRegion(pRegion);

		cVector3f vPosition = vStart + cVector3f(mfLineNumberWidth - kfTextPadding, 0, 0);
		for(int i=mlFirstLine; i<lEndLine; ++i)
		{
			DrawDefaultText(cString::ToStringW(i+1), vPosition, eFontAlign_Right, mDefaultFontColor * cColor(1, 0.5f));
			vPosition.y += fRowHeight;
		}

		////////////////////////////////
		// Text
		pRegion = apClipRegion->CreateChild(GetGlobalPosition() + cVector3f(mfLineNumberWidth,0,0), vViewSize);
		mpSet->SetCurrentClipRegion(pRegion);

		vPosition = vStart + cVector3f(mfLineNumberWidth + kfTextPadding - (float)mlScrollPixelsX, 0, 0);
		for(int i=mlFirstLine; i<lEndLine; ++i)
		{
			const std::wstring& sLine = mDocument.GetLine(i);
			if(sLine.empty()==false)
				DrawDefaultText(sLine, vPosition, eFontAlign_Left);
			vPosition.y += fRowHeight;
		}

		mpSet->SetCurrentClipRegion(apClipRegion);
	}

	//-----------------------------------------------------------------------

	bool cWidgetTextArea::OnMouseDown(const cGuiMessageData& aData)
	{
		const bool bWheelUp = (aData.mlVal & eGuiMouseButton_WheelUp)!=0;
		const bool bWheelDown = (aData.mlVal & eGuiMouseButton_WheelDown)!=0;
		if(bWheelUp==false && bWheelDown==false)
			return true;

		const int lDirection = bWheelUp ? -1 : 1;
		mpVerticalSlider->SetValue(mpVerticalSlider->GetValue() + lDirection*klWheelLines);

		return true;
	}

	//-----------------------------------------------------------------------

}
