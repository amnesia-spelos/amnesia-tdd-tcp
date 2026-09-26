/*
 * Copyright © 2009-2020 Frictional Games
 *
 * This file is part of Amnesia: The Dark Descent.
 *
 * Amnesia: The Dark Descent is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * Amnesia: The Dark Descent is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Amnesia: The Dark Descent.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "LevelEditorWindowMapScript.h"

//------------------------------------------------------------------

cLevelEditorWindowMapScript::cLevelEditorWindowMapScript(iEditorBase* apEditor, const cVector2f& avSize) : iEditorWindowPopUp(apEditor,
																														   "MapScript",
																														   true,
																														   false,
																														   false,
																														   avSize)
{
	mpTextArea = NULL;
}

cLevelEditorWindowMapScript::~cLevelEditorWindowMapScript()
{
}

//------------------------------------------------------------------

void cLevelEditorWindowMapScript::SetScript(const tWString& asFilename, const cTextDocument& aDocument)
{
	mpWindow->SetText(_W("Map Script \x2013 ") + cString::GetFileNameW(asFilename));
	mpTextArea->SetDocument(aDocument);
}

//------------------------------------------------------------------

void cLevelEditorWindowMapScript::OnInitLayout()
{
	iEditorWindowPopUp::OnInitLayout();
	mpWindow->SetText(_W("Map Script"));

	mpTextArea = mpSet->CreateWidgetTextArea(cVector3f(10,35,0.1f), mvSize-cVector2f(20,45), mpWindow);
}

//------------------------------------------------------------------
