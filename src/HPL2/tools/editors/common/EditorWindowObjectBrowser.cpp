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

#include "EditorWindowObjectBrowser.h"
#include "EditorThumbnailBuilder.h"
#include "EditorGrid.h"

#include "EditorWorld.h"

#include "ObjectSearch.h"

#include <algorithm>

//-------------------------------------------------------------------



//-------------------------------------------------------------------
//-------------------------------------------------------------------

iEditorObjectIndexEntryMeshObject::iEditorObjectIndexEntryMeshObject(iEditorObjectIndexDir* apDir) : iEditorObjectIndexEntry(apDir)
{
}

//-------------------------------------------------------------------

tWString& iEditorObjectIndexEntryMeshObject::GetMeshFileName()
{
	if(msMeshFileName==_W(""))
		msMeshFileName = GetFileNameRelPath();
	
	return msMeshFileName;
}

//-------------------------------------------------------------------

bool iEditorObjectIndexEntryMeshObject::CreateFromFile(const tWString& asFilename)
{
	if(iEditorObjectIndexEntry::CreateFromFile(asFilename)==false)
		return false;

	iEditorObjectIndex* pIndex = mpParentDir->GetIndex();
	
	cMeshEntity* pMeshEntity = CreateTempEntity(pIndex->GetEditor()->GetTempWorld());

	mlTriangleCount = pMeshEntity->GetMesh()->GetTriangleCount();

	mvBVMin = 999999999.0f;
	mvBVMax = -999999999.0f;
	for(int i=0;i<pMeshEntity->GetSubMeshEntityNum();++i)
	{
		cSubMeshEntity* pSubMeshEntity = pMeshEntity->GetSubMeshEntity(i);

		cBoundingVolume* pBV = pSubMeshEntity->GetBoundingVolume();
		cMath::ExpandAABB(mvBVMin, mvBVMax, pBV->GetMin(), pBV->GetMax());
	}
	
	return true;
}

//-------------------------------------------------------------------

bool iEditorObjectIndexEntryMeshObject::CreateFromXmlElement(cXmlElement* apElement)
{
	if(iEditorObjectIndexEntry::CreateFromXmlElement(apElement)==false)
		return false;

	mlTriangleCount = apElement->GetAttributeInt("TriCount");
	mvBVMin = apElement->GetAttributeVector3f("BVMin");
	mvBVMax = apElement->GetAttributeVector3f("BVMax");

	return true;
}

//-------------------------------------------------------------------

void iEditorObjectIndexEntryMeshObject::Save(cXmlElement* apElement)
{
	iEditorObjectIndexEntry::Save(apElement);

	apElement->SetAttributeInt("TriCount", mlTriangleCount);
	apElement->SetAttributeVector3f("BVMin", mvBVMin);
	apElement->SetAttributeVector3f("BVMax", mvBVMax);
}

//-------------------------------------------------------------------

void iEditorObjectIndexEntryMeshObject::BuildThumbnail()
{
	iEditorObjectIndex* pIndex = mpParentDir->GetIndex();
	iEditorBase* pEditor = pIndex->GetEditor();
	cEditorThumbnailBuilder* pTmbBuilder = pEditor->GetThumbnailBuilder();
	
	////////////////////////////////////////////////////////////////
	// Create thumbnail if it doesn't exist or the object was updated
	bool bIsUpdated = IsUpdated();
	if(bIsUpdated || cPlatform::FileExists(GetThumbnailFilename())==false)
	{
		pEditor->ShowLoadingWindow(_W("Loading"), _W("Creating Thumbnail"));
		cMeshEntity* pEnt = CreateTempEntity(pEditor->GetTempWorld());

		//pEditor->GetThumbnailBuilder()->BuildThumbnailFromMesh(sMeshFilename,sThumbnailFilename);
		pTmbBuilder->BuildThumbnailFromMeshEntity(pEnt, _W(""));

		if(bIsUpdated==false)
			pIndex->SetRefreshThumbnails(true);
	}
}

//-------------------------------------------------------------------

cMeshEntity* iEditorObjectIndexEntryMeshObject::CreateTempEntity(cWorld* apWorld)
{
	iEditorObjectIndex* pIndex = mpParentDir->GetIndex();
	iEditorBase* pEditor = pIndex->GetEditor();
	cResources* pResources = pEditor->GetEngine()->GetResources();

	tString sSourceFile = cString::To8Char(GetFileNameFullPath());

	cMesh* pMesh = pResources->GetMeshManager()->CreateMesh(sSourceFile);
    if(pMesh==NULL)
		return NULL;

	cMeshEntity* pMeshEntity = apWorld->CreateMeshEntity("TempObject_"+msFileName, pMesh);
	pMeshEntity->SetSourceFile(sSourceFile);
	pMeshEntity->SetActive(false);
	pMeshEntity->SetVisible(false);

	return pMeshEntity;
}

//-------------------------------------------------------------------

////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
////////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------

cEditorWindowObjectBrowser::cEditorWindowObjectBrowser(iEditorEditMode* apEditMode,
													   const tWStringVec& avBaseDirs,
													   bool abAddCategoryHeaders,
													   const tWString& asAssetFileMask) : iEditModeObjectCreatorWindow(apEditMode)

{
	mvBaseDirs = avBaseDirs;
	mbAddCategoryHeaders = abAddCategoryHeaders;
	msAssetFileMask = asAssetFileMask;
	mbAssetPathListBuilt = false;

	mpPreviewEntity = NULL;
	mpCurrentIndex = NULL;
}

cEditorWindowObjectBrowser::~cEditorWindowObjectBrowser()
{
	mpObjectList->ClearItems();
	mvCurrentListedEntries.clear();
	mvCurrentListedAssetPaths.clear();

	if(mpPreviewEntity)
		mpEditor->GetEditorWorld()->GetWorld()->DestroyMeshEntity(mpPreviewEntity);

	STLMapDeleteAll(mmapObjectIndices);
}

//-------------------------------------------------------------------

////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
////////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------

iEditorObjectIndexEntryMeshObject* cEditorWindowObjectBrowser::GetSelectedObject()
{
	int lIndex = mpObjectList->GetSelectedItem();

	if(lIndex==-1 || lIndex >=(int)mvCurrentListedEntries.size())
		return NULL;

	return mvCurrentListedEntries[lIndex];
}

//-------------------------------------------------------------------

//-------------------------------------------------------------------

void cEditorWindowObjectBrowser::Reset()
{
	mpObjectList->SetSelectedItem(-1);
	UpdateObjectInfo();
}

//-------------------------------------------------------------------

////////////////////////////////////////////////////////////////////
// PROTECTED METHODS
////////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------

//-------------------------------------------------------------------

bool cEditorWindowObjectBrowser::ObjectSets_OnChange(iWidget* apWidget, const cGuiMessageData& aData)
{
	BuildObjectList();
	return true;
}
kGuiCallbackDeclaredFuncEnd(cEditorWindowObjectBrowser,ObjectSets_OnChange);

//-------------------------------------------------------------------

bool cEditorWindowObjectBrowser::ObjectList_OnChangeSelection(iWidget* apWidget, const cGuiMessageData& aData)
{
	////////////////////////////////////////////////////////////
	// A search result's entry is looked up the first time it is selected
	int lIndex = mpObjectList->GetSelectedItem();
	if(lIndex>=0 && lIndex<(int)mvCurrentListedEntries.size() &&
		mvCurrentListedEntries[lIndex]==NULL && mvCurrentListedAssetPaths[lIndex]!=_W(""))
	{
		mvCurrentListedEntries[lIndex] = FindEntryByAssetPath(mvCurrentListedAssetPaths[lIndex]);
	}

	UpdateObjectInfo();
	return true;
}
kGuiCallbackDeclaredFuncEnd(cEditorWindowObjectBrowser, ObjectList_OnChangeSelection);

//-------------------------------------------------------------------

bool cEditorWindowObjectBrowser::Search_OnTextChange(iWidget* apWidget, const cGuiMessageData& aData)
{
	UpdateObjectList();
	return true;
}
kGuiCallbackDeclaredFuncEnd(cEditorWindowObjectBrowser, Search_OnTextChange);

//-------------------------------------------------------------------

bool cEditorWindowObjectBrowser::Refresh_OnPressed(iWidget* apWidget, const cGuiMessageData& aData)
{
	BuildAssetPathList();

	if(mpCurrentIndex)
		mpCurrentIndex->Refresh();

	UpdateObjectList();

	return true;
}
kGuiCallbackDeclaredFuncEnd(cEditorWindowObjectBrowser, Refresh_OnPressed);

//-------------------------------------------------------------------

void cEditorWindowObjectBrowser::OnSetActive(bool abX)
{
	iEditModeObjectCreatorWindow::OnSetActive(abX);

	if(abX)
		BuildAssetPathListOnFirstOpen();
}

//-------------------------------------------------------------------

void cEditorWindowObjectBrowser::OnInitLayout()
{
	//////////////////////
	// Set up layout
    mpBGFrame->SetSize(cVector2f(200,870));

	cVector3f vPos;
	cVector2f vSize;

	///////////////////////////////////////////////////
	// Object Selection (search scope, search box, list..)
	mpSelectionGroup = mpSet->CreateWidgetGroup(cVector3f(5,8,0.1f), cVector2f(190,355) , _W(""), mpBGFrame);

	mpObjectSets = mpSet->CreateWidgetComboBox(cVector3f(10,10,0.1f), cVector2f(170,25),_W(""), mpSelectionGroup);
	mpObjectSets->AddCallback(eGuiMessage_SelectionChange,this, kGuiCallback(ObjectSets_OnChange));
	mpObjectSets->SetCanEdit(false);

	mpSearchBox = mpSet->CreateWidgetTextBox(cVector3f(10,42,0.1f), cVector2f(170,0), _W(""), mpSelectionGroup, eWidgetTextBoxInputType_Normal);
	mpSearchBox->AddCallback(eGuiMessage_TextChange, this, kGuiCallback(Search_OnTextChange));

	mpObjectList = mpSet->CreateWidgetListBox(cVector3f(10,75,0.1f), cVector2f(170,240),mpSelectionGroup);
	mpObjectList->SetDefaultFontSize(11);
	mpObjectList->SetBackgroundZ(0.001f);
	mpObjectList->AddCallback(eGuiMessage_SelectionChange, this, kGuiCallback(ObjectList_OnChangeSelection));

	mpButtonRefresh = mpSet->CreateWidgetButton(cVector3f(10, 320, 0.1f), cVector2f(170, 25), _W("Refresh"), mpSelectionGroup);
	mpButtonRefresh->AddCallback(eGuiMessage_ButtonPressed, this, kGuiCallback(Refresh_OnPressed));

    ///////////////////////////////////////////////
	// Object info: BV Size, poly count, Asset Path, thumbnail
	mpInfoGroup = mpSet->CreateWidgetGroup(cVector3f(5,370,0.1f), cVector2f(190,300),_W("Object info"), mpBGFrame);

	vPos = cVector3f(5,170,0.1f);
	vSize = cVector2f(50,25);

	for(int i=0;i<2;++i)
	{
		mvLabelBVSize[i] = mpSet->CreateWidgetLabel(vPos,vSize,_W(""), mpInfoGroup);
		mvLabelBVSize[i]->SetDefaultFontSize(12.5f);

		mvLabelPolyCount[i] = mpSet->CreateWidgetLabel(vPos + cVector3f(0,40,0),vSize,_W(""), mpInfoGroup);		
		mvLabelPolyCount[i]->SetDefaultFontSize(12.5f);

		mvLabelAssetPath[i] = mpSet->CreateWidgetLabel(vPos + cVector3f(0,80,0),vSize,_W(""), mpInfoGroup);
		mvLabelAssetPath[i]->SetDefaultFontSize(12.5f);

		vPos.y += 20;


	}

	mvLabelBVSize[0]->SetText(_W("BB Size:"));
	mvLabelPolyCount[0]->SetText(_W("Polygon Count:"));
	mvLabelAssetPath[0]->SetText(_W("Asset Path:"));
	mvLabelAssetPath[1]->SetSize(cVector2f(180,25));
	mvLabelAssetPath[1]->SetDefaultFontSize(11);

	mpLabelThumbnail = mpSet->CreateWidgetLabel(cVector3f(30,15,0.1f), cVector2f(50,25), _W("Thumbnail"), mpInfoGroup);
	mpLabelThumbnail->SetDefaultFontSize(12.5f);

	cWidgetFrame* pThumbFrame = mpSet->CreateWidgetFrame(cVector3f(30,35,0.5f),128,true,mpInfoGroup);
	pThumbFrame->SetBackGroundColor(cColor(0.4f,1));
	pThumbFrame->SetBackgroundZ(0);
	pThumbFrame->SetDrawBackground(true);

	mpThumbnail = mpSet->CreateWidgetImage("",cVector3f(0,0,1.1f),128,eGuiMaterial_Diffuse, false, pThumbFrame);

	BuildObjectSetList();
}

//-------------------------------------------------------------------

void cEditorWindowObjectBrowser::BuildObjectSetList()
{
	mvDirectories.clear();
	mpObjectSets->ClearItems();

	////////////////////////////////////////////////////////////
	// The first item, and the default search scope, is All Categories
	mvDirectories.push_back(_W(""));
	mpObjectSets->AddItem(_W("All"));

	for(int i=0;i<(int)mvBaseDirs.size();++i)
        BuildObjectSetListHelper(mvBaseDirs[i],0);

	mpObjectSets->SetSelectedItem(0, false, false);
	mpCurrentIndex = NULL;
	UpdateObjectList();
}

//-------------------------------------------------------------------

void cEditorWindowObjectBrowser::BuildAssetPathListOnFirstOpen()
{
	if(mbAssetPathListBuilt)
		return;

	BuildAssetPathList();
	UpdateObjectList();
}

//-------------------------------------------------------------------

void cEditorWindowObjectBrowser::BuildAssetPathList()
{
	std::set<tWString> setAssetPaths;

	for(int i=0;i<(int)mvBaseDirs.size();++i)
	{
		tWStringList lstCategories;
		cPlatform::FindFoldersInDir(lstCategories, mvBaseDirs[i], false);

		tWStringListIt it = lstCategories.begin();
		for(;it!=lstCategories.end();++it)
			BuildAssetPathListHelper(cString::AddSlashAtEndW(mvBaseDirs[i]) + *it, *it, setAssetPaths);
	}

	mvAssetPaths.assign(setAssetPaths.begin(), setAssetPaths.end());
	mbAssetPathListBuilt = true;
}

//-------------------------------------------------------------------

void cEditorWindowObjectBrowser::BuildAssetPathListHelper(const tWString& asDiskFolder, const tWString& asAssetPathPrefix, std::set<tWString>& asetAssetPaths)
{
	////////////////////////////////////////////////////////////
	// Skip the folders the indices skip
	if(cString::GetFirstStringPosW(cString::GetFileNameW(asAssetPathPrefix), _W("."))==0)
		return;

	tWStringList lstFiles;
	cPlatform::FindFilesInDir(lstFiles, asDiskFolder, msAssetFileMask);

	tWStringListIt itFiles = lstFiles.begin();
	for(;itFiles!=lstFiles.end();++itFiles)
		asetAssetPaths.insert(asAssetPathPrefix + _W("/") + cString::SetFileExtW(*itFiles, _W("")));

	tWStringList lstDirs;
	cPlatform::FindFoldersInDir(lstDirs, asDiskFolder, false, false);

	tWStringListIt itDirs = lstDirs.begin();
	for(;itDirs!=lstDirs.end();++itDirs)
		BuildAssetPathListHelper(cString::AddSlashAtEndW(asDiskFolder) + *itDirs, asAssetPathPrefix + _W("/") + *itDirs, asetAssetPaths);
}

//-------------------------------------------------------------------

void cEditorWindowObjectBrowser::BuildObjectSetListHelper(const tWString& asFolder, int alLevel)
{
	tWStringList lstObjectDirs;

	cPlatform::FindFoldersInDir(lstObjectDirs, asFolder, false);

	tWStringListIt it = lstObjectDirs.begin();

	for(;it!=lstObjectDirs.end();++it)
	{
		tWString sDir = cString::AddSlashAtEndW(asFolder) + *it;

		tWString sItem;
		sItem.append(alLevel,_W('-'));
		sItem += *it;

		mvDirectories.push_back(sDir);
		mpObjectSets->AddItem(sItem);
	}
}

//-------------------------------------------------------------------

void cEditorWindowObjectBrowser::WriteInvalidFileListToFile(tWString& asFolder, tWStringList& alstInvalidFiles)
{
	/*
	if(alstInvalidFiles.empty())
		return;

	tString sInvalidFileList = cString::To8Char(mpEditor->GetMainLookUpDir(eDir_Home)) + mpEditor->GetInvalidFilesListFilename();
	tString sFolder = cString::ReplaceCharTo(cString::To8Char(asFolder), "/", "\\");

	FILE* pFile = fopen(sInvalidFileList.c_str(), "a");
	tWStringListIt it = alstInvalidFiles.begin();

	fprintf(pFile, "Found %d invalid entries in folder %s :\n", alstInvalidFiles.size(), sFolder.c_str());
	for(;it!=alstInvalidFiles.end();++it)
	{
		tWString& sFilename = *it;

		fprintf(pFile, "\t-%s\n", cString::To8Char(sFilename).c_str());
	}
	fprintf(pFile,"\n");

	fclose(pFile);*/
}

//-------------------------------------------------------------------

void cEditorWindowObjectBrowser::BuildObjectList()
{
	tWString sCategory = GetSearchCategory();
	mpCurrentIndex = sCategory==_W("")? NULL : CreateIndex(sCategory);

	UpdateObjectList();
}

//-------------------------------------------------------------------

void cEditorWindowObjectBrowser::ClearObjectList()
{
	mvCurrentListedEntries.clear();
	mvCurrentListedAssetPaths.clear();
	mpObjectList->ClearItems();
	UpdateObjectInfo();
}

//-------------------------------------------------------------------

void cEditorWindowObjectBrowser::UpdateObjectList()
{
	ClearObjectList();

	if(IsListingSearchResults())
		AddSearchResultsToList();
	else if(mpCurrentIndex)
		AddEntriesInDirToList(mpCurrentIndex->GetRootDir(), mvCurrentListedEntries);
}

//-------------------------------------------------------------------

tWString cEditorWindowObjectBrowser::GetSearchCategory()
{
	if(mpObjectSets->GetSelectedItem()<=0)
		return _W("");

	return mpObjectSets->GetText();
}

//-------------------------------------------------------------------

bool cEditorWindowObjectBrowser::IsSearchQueryBlank()
{
	const tWString& sQuery = mpSearchBox->GetText();
	for(size_t i=0;i<sQuery.size();++i)
	{
		if(iswspace(sQuery[i])==0)
			return false;
	}

	return true;
}

//-------------------------------------------------------------------

bool cEditorWindowObjectBrowser::IsListingSearchResults()
{
	////////////////////////////////////////////////////////////
	// A Category with an empty query lists its index, as before searching existed
	return GetSearchCategory()==_W("") || IsSearchQueryBlank()==false;
}

//-------------------------------------------------------------------

void cEditorWindowObjectBrowser::AddSearchResultsToList()
{
	tWString sCategory = GetSearchCategory();
	const tWString& sQuery = mpSearchBox->GetText();

	if(sCategory==_W("") && IsSearchQueryBlank())
	{
		AddUnselectableItemToList(_W("Type to search or pick a Category"));
		return;
	}

	tWString sHeaderCategory;
	for(size_t i=0;i<mvAssetPaths.size();++i)
	{
		const tWString& sAssetPath = mvAssetPaths[i];
		if(MatchesObjectSearch(sAssetPath, sCategory, sQuery)==false)
			continue;

		tWString sAssetCategory = GetAssetPathCategory(sAssetPath);
		if(sCategory==_W("") && sAssetCategory!=sHeaderCategory)
		{
			AddUnselectableItemToList(sAssetCategory);
			sHeaderCategory = sAssetCategory;
		}

		mpObjectList->AddItem(cString::SubW(sAssetPath, (int)sAssetCategory.size()+1));
		mvCurrentListedEntries.push_back(NULL);
		mvCurrentListedAssetPaths.push_back(sAssetPath);
	}
}

//-------------------------------------------------------------------

void cEditorWindowObjectBrowser::AddUnselectableItemToList(const tWString& asText)
{
	cWidgetItem* pItem = mpObjectList->AddItem(asText);
	pItem->SetSelectable(false);
	mvCurrentListedEntries.push_back(NULL);
	mvCurrentListedAssetPaths.push_back(_W(""));
}

//-------------------------------------------------------------------

void cEditorWindowObjectBrowser::UpdateObjectInfo()
{
	iEditorObjectIndexEntryMeshObject* pObj = GetSelectedObject();
	cGui* pGui = mpEditor->GetEngine()->GetGui();

	tWString sBVSize;
	tWString sTriCount;

	cGuiGfxElement* pImg = mpThumbnail->GetImage();
	if(pImg!=NULL)
		pGui->DestroyGfx(pImg);

	cWorld* pWorld = mpEditor->GetEditorWorld()->GetWorld();

	if(mpPreviewEntity)
		pWorld->DestroyMeshEntity(mpPreviewEntity);
	mpPreviewEntity = pObj?pObj->CreateTempEntity(pWorld):NULL;

	if(pObj==NULL)
	{
		sBVSize = _W("");
		sTriCount = _W("");

		pImg = NULL;
	}
	else
	{
		cVector3f vBVSize = pObj->GetBVMax() - pObj->GetBVMin();
		
		sBVSize = _W("(");
		for(int i=0;i<3;++i)
		{
			sBVSize += cString::ToStringW(vBVSize.v[i],3,true);
			if(i!=2) sBVSize += _W(",");
		}
		sBVSize+=_W(")");

		sTriCount = cString::ToStringW(pObj->GetTriangleCount());
		tString sThumbFile = cString::To8Char(pObj->GetThumbnailFilename());
		pImg = pGui->CreateGfxImage(sThumbFile,eGuiMaterial_Diffuse);
	}

	int lIndex = mpObjectList->GetSelectedItem();
	tWString sAssetPath = (pObj && lIndex>=0 && lIndex<(int)mvCurrentListedAssetPaths.size())? mvCurrentListedAssetPaths[lIndex] : _W("");

	mvLabelBVSize[1]->SetText(sBVSize);
	mvLabelPolyCount[1]->SetText(sTriCount);
	mvLabelAssetPath[1]->SetText(sAssetPath);

	mpThumbnail->SetImage(pImg);
}

//-------------------------------------------------------------------

iEditorObjectIndex* cEditorWindowObjectBrowser::CreateIndex(const tWString& asFolder)
{
	///////////////////////////////////////////////////////////
	// First try to return an already created index
	std::map<tWString, iEditorObjectIndex*>::iterator it = mmapObjectIndices.find(asFolder);
    if(it!=mmapObjectIndices.end())
	{
		iEditorObjectIndex* pIndex = it->second;
		// Should the index be refreshed here?
		pIndex->Refresh();
		return pIndex;
	}

	mpEditor->ShowLoadingWindow(_W("Loading"), _W("Loading"));

	iEditorObjectIndex* pIndex = NULL;
	for(int i=0;i<(int)mvBaseDirs.size();++i)
	{
		tWString sFolder = cString::AddSlashAtEndW(mvBaseDirs[i]) + asFolder;
		if(cPlatform::FolderExists(sFolder)==false)
			continue;

		pIndex = CreateSpecificIndex(mpEditor, sFolder);
		if(pIndex)
		{
			mmapObjectIndices[asFolder] = pIndex;
			pIndex->Create();

			break;
		}
	}


	return pIndex;
}

//-------------------------------------------------------------------

void cEditorWindowObjectBrowser::AddEntriesInDirToList(iEditorObjectIndexDir* apDir, std::vector<iEditorObjectIndexEntryMeshObject*>& avEntries)
{
	tWString sAssetFolder = apDir->GetIndex()->GetBaseFolder();
	if(apDir->GetRelPath()!=_W(""))
		sAssetFolder += _W("/") + apDir->GetRelPath();

	const tIndexEntryMap& mapEntries = apDir->GetEntries();
	tIndexEntryMapConstIt itEntries = mapEntries.begin();
	for(;itEntries!=mapEntries.end();++itEntries)
	{
		iEditorObjectIndexEntry* pEntry = itEntries->second;

		mpObjectList->AddItem(pEntry->GetEntryName());
		avEntries.push_back((iEditorObjectIndexEntryMeshObject*)pEntry);
		mvCurrentListedAssetPaths.push_back(sAssetFolder + _W("/") + cString::To16Char(pEntry->GetEntryName()));
	}

	tIndexDirMap& mapSubDirs = (tIndexDirMap&)apDir->GetSubDirs();
	tIndexDirMapIt itSubDirs = mapSubDirs.begin();
	for(;itSubDirs!=mapSubDirs.end();++itSubDirs)
	{
		iEditorObjectIndexDir* pSubDir = itSubDirs->second;

		if(mbAddCategoryHeaders && apDir->GetParentDir()==NULL)
		{
			cWidgetItem* pDirHeader = mpObjectList->AddItem((pSubDir->IsExpanded()?_W("- "):_W("+ ")) + pSubDir->GetDirName());
			pDirHeader->SetSelectable(false);
			avEntries.push_back(NULL);
			mvCurrentListedAssetPaths.push_back(_W(""));
		}

		AddEntriesInDirToList(pSubDir, avEntries);
	}
}

//-------------------------------------------------------------------

iEditorObjectIndexEntryMeshObject* cEditorWindowObjectBrowser::FindEntryByAssetPath(const tWString& asAssetPath)
{
	////////////////////////////////////////////////////////////
	// Use the Category's index, building it the first time. An existing index is not
	// refreshed here, since that would free the entries the list still points to
	tWString sCategory = GetAssetPathCategory(asAssetPath);
	iEditorObjectIndex* pIndex = NULL;

	std::map<tWString, iEditorObjectIndex*>::iterator it = mmapObjectIndices.find(sCategory);
	if(it!=mmapObjectIndices.end())
		pIndex = it->second;
	else
		pIndex = CreateIndex(sCategory);

	if(pIndex==NULL)
		return NULL;

	////////////////////////////////////////////////////////////
	// Walk the folders after the Category down to the entry
	iEditorObjectIndexDir* pDir = pIndex->GetRootDir();
	tWString sRest = cString::SubW(asAssetPath, (int)sCategory.size()+1);
	int lSlashPos = cString::GetFirstStringPosW(sRest, _W("/"));
	while(pDir && lSlashPos!=-1)
	{
		const tIndexDirMap& mapSubDirs = pDir->GetSubDirs();
		tIndexDirMap::const_iterator itDir = mapSubDirs.find(cString::SubW(sRest, 0, lSlashPos));
		pDir = itDir!=mapSubDirs.end()? itDir->second : NULL;

		sRest = cString::SubW(sRest, lSlashPos+1);
		lSlashPos = cString::GetFirstStringPosW(sRest, _W("/"));
	}

	if(pDir==NULL)
		return NULL;

	return (iEditorObjectIndexEntryMeshObject*)pDir->GetEntry(cString::To8Char(sRest));
}

//-------------------------------------------------------------------


