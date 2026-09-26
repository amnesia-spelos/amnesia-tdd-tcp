#ifndef HPL_WIDGET_TEXT_AREA_H
#define HPL_WIDGET_TEXT_AREA_H

#include "gui/Widget.h"
#include "gui/TextDocument.h"

namespace hpl {

	class cWidgetSlider;

	//-----------------------------------------------------------------------

	/**
	 * Read-only view of a cTextDocument with a line-number column. Lines don't
	 * wrap; the view scrolls with a vertical and a horizontal slider and the
	 * vertical mouse wheel. Only visible lines are drawn.
	 */
	class cWidgetTextArea : public iWidget
	{
	public:
		cWidgetTextArea(cGuiSet *apSet, cGuiSkin *apSkin);
		virtual ~cWidgetTextArea();

		void SetDocument(const cTextDocument& aDocument);

		void SetDefaultFontType(iFontData *apFont);
		void SetDefaultFontSize(const cVector2f& avSize);

	protected:
		/////////////////////////
		// Own functions
		void UpdateTextMetrics();
		void UpdateScrollRanges();

		float GetRowHeight();
		cVector2f GetTextViewSize();

		bool MoveVerticalSlider(iWidget* apWidget, const cGuiMessageData& aData);
		kGuiCallbackDeclarationEnd(MoveVerticalSlider);
		bool MoveHorizontalSlider(iWidget* apWidget, const cGuiMessageData& aData);
		kGuiCallbackDeclarationEnd(MoveHorizontalSlider);

		/////////////////////////
		// Implemented functions
		void OnInit();
		void OnLoadGraphics();
		void OnChangeSize();

		void OnDraw(float afTimeStep, cGuiClipRegion *apClipRegion);
		void OnDrawAfterClip(float afTimeStep, cGuiClipRegion *apClipRegion);

		bool OnMouseDown(const cGuiMessageData& aData);

		/////////////////////////
		// Data
		cTextDocument mDocument;

		float mfBackgroundZ;
		float mfSliderWidth;

		float mfLongestLineWidth;
		float mfLineNumberWidth;

		int mlFirstLine;
		int mlVisibleLines;
		int mlScrollPixelsX;

		cGuiGfxElement *mpGfxBackground;
		cGuiGfxElement *mvGfxBorders[4];
		cGuiGfxElement *mvGfxCorners[4];

		cWidgetSlider *mpVerticalSlider;
		cWidgetSlider *mpHorizontalSlider;
	};

	//-----------------------------------------------------------------------

};
#endif // HPL_WIDGET_TEXT_AREA_H
