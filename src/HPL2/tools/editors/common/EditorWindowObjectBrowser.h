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

#ifndef HPLEDITOR_EDITOR_WINDOW_OBJECT_BROWSER_H
#define HPLEDITOR_EDITOR_WINDOW_OBJECT_BROWSER_H

#include "EditorWindow.h"
#include "EditorIndex.h"

#include <set>

//----------------------------------------------------------

class iEditorBase;

class iEditorObjectIndex;
class iEditorObjectIndexEntry;

class cEditorWindowObjectBrowser;

//----------------------------------------------------------

////////////////////////////////////////////////////////////
// BASE INDEX ENTRY FOR MESH OBJECTS
////////////////////////////////////////////////////////////

//----------------------------------------------------------

class iEditorObjectIndexEntryMeshObject : public iEditorObjectIndexEntry
{
public:
	iEditorObjectIndexEntryMeshObject(iEditorObjectIndexDir* apDir);

	bool CreateFromFile(const tWString& asFilename);
	bool CreateFromXmlElement(cXmlElement* apElement);

	const cVector3f& GetBVMin() { return mvBVMin; }
	const cVector3f& GetBVMax() { return mvBVMax; }
	int GetTriangleCount() { return mlTriangleCount; }

	virtual tWString& GetMeshFileName();

	void Save(cXmlElement* apElement);

	virtual cMeshEntity* CreateTempEntity(cWorld* apWorld);
protected:
	void BuildThumbnail();

	cVector3f mvBVMin;
	cVector3f mvBVMax;
	int mlTriangleCount;

	tString msTempFileName;

	tWString msMeshFileName;
};


//----------------------------------------------------------

////////////////////////////////////////////////////////////
// OBJECT BROWSER WINDOW
////////////////////////////////////////////////////////////

//----------------------------------------------------------

class cEditorWindowObjectBrowser : public iEditModeObjectCreatorWindow
{
public:
	cEditorWindowObjectBrowser(iEditorEditMode* apEditMode, const tWStringVec& avBaseDirs, bool abAddCategoryHeaders, const tWString& asAssetFileMask);
	~cEditorWindowObjectBrowser();

	virtual iEditorObjectIndexEntryMeshObject* GetSelectedObject();
	cMeshEntity* GetPreviewEntity() { return mpPreviewEntity; }

	void Reset();

	cMesh* GetSelectedMesh();
protected:
	void OnInitLayout();

	bool ObjectSets_OnChange(iWidget* apWidget, const cGuiMessageData& aData);
	kGuiCallbackDeclarationEnd(ObjectSets_OnChange);

	bool ObjectList_OnChangeSelection(iWidget* apWidget, const cGuiMessageData& aData);
	kGuiCallbackDeclarationEnd(ObjectList_OnChangeSelection);

	bool Search_OnTextChange(iWidget* apWidget, const cGuiMessageData& aData);
	kGuiCallbackDeclarationEnd(Search_OnTextChange);

	bool Refresh_OnPressed(iWidget* apWidget, const cGuiMessageData& aData);
	kGuiCallbackDeclarationEnd(Refresh_OnPressed);

	void OnSetActive(bool abX);

	void BuildObjectSetList();
	void BuildObjectSetListHelper(const tWString& asFolder, int alLevel);

	void BuildAssetPathListOnFirstOpen();
	void BuildAssetPathList();
	void BuildAssetPathListHelper(const tWString& asDiskFolder, const tWString& asAssetPathPrefix, std::set<tWString>& asetAssetPaths);

	void BuildObjectList();
	void BuildObjectListHelper(const tWString& asBaseFolder, const tWString& asCurrentFolder, tWStringList& alstInvalidFiles);
	void ClearObjectList();
	virtual void UpdateObjectList();

	tWString GetSearchCategory();
	bool IsSearchQueryBlank();
	bool IsListingSearchResults();
	void AddSearchResultsToList();
	void AddUnselectableItemToList(const tWString& asText);

	void WriteInvalidFileListToFile(tWString& asFolder, tWStringList& alstInvalidFiles);

	void UpdateObjectInfo();
	void AddEntriesInDirToList(iEditorObjectIndexDir* apDir,std::vector<iEditorObjectIndexEntryMeshObject*>& avEntries);

	iEditorObjectIndexEntryMeshObject* FindEntryByAssetPath(const tWString& asAssetPath);

	iEditorObjectIndex* CreateIndex(const tWString& asFolder);
	virtual iEditorObjectIndex* CreateSpecificIndex(iEditorBase* apEditorBase, const tWString& asFolder)=0;

	virtual cMeshEntity* CreatePreviewEntity(iEditorObjectIndexEntryMeshObject* apEntry)=0;

	////////////////////////////////////////////////////
	// Data
	tWStringVec mvBaseDirs;
	
	tWStringVec mvDirectories;

	// Layout stuff
	cWidgetGroup* mpSelectionGroup;
	cWidgetComboBox* mpObjectSets;
	cWidgetTextBox* mpSearchBox;
	cWidgetListBox* mpObjectList;
	cWidgetButton* mpButtonRefresh;

	cWidgetGroup* mpInfoGroup;
	cWidgetLabel* mvLabelBVSize[2];
	cWidgetLabel* mvLabelPolyCount[2];
	cWidgetLabel* mvLabelAssetPath[2];
	cWidgetLabel* mpLabelThumbnail;
	cWidgetImage* mpThumbnail;

	tWStringVec mvCategoryStrings;

	bool mbAddCategoryHeaders;

	cMeshEntity* mpPreviewEntity;

	std::map<tWString, iEditorObjectIndex*> mmapObjectIndices;
	iEditorObjectIndex* mpCurrentIndex;
	std::vector<iEditorObjectIndexEntryMeshObject*> mvCurrentListedEntries;
	// The Asset Path of each listed item, empty for headers and hints. A search result's entry
	// is only looked up, building its Category index if needed, once the result is selected.
	tWStringVec mvCurrentListedAssetPaths;

	// Every Asset Path under the base dirs, sorted, walked without building any index
	tWString msAssetFileMask;
	tWStringVec mvAssetPaths;
	bool mbAssetPathListBuilt;

	tIntVec mvDirIndices;
};

//----------------------------------------------------------

#endif // HPLEDITOR_EDITOR_WINDOW_OBJECT_BROWSER_H
