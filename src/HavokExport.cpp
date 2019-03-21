/*	Havok Tool for 3ds Max
	Copyright(C) 2019 Lukas Cone

	This program is free software : you can redistribute it and / or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.If not, see <https://www.gnu.org/licenses/>.
	
	Havok Tool uses HavokLib 2016-2019 Lukas Cone
*/

#include "HavokMax.h"
#include "datas/esstring.h"
#include <impapi.h>

#define HavokExport_CLASS_ID	Class_ID(0x2b020aa4, 0x5c7f7d58)
static const TCHAR _className[] = _T("HavokExport");

class HavokExport : public SceneExport, HavokMaxV2
{
public:
	//Constructor/Destructor
	HavokExport();
	virtual ~HavokExport() {}

	int				ExtCount();					// Number of extensions supported
	const TCHAR *	Ext(int n);					// Extension #n (i.e. "3DS")
	const TCHAR *	LongDesc();					// Long ASCII description (i.e. "Autodesk 3D Studio File")
	const TCHAR *	ShortDesc();				// Short ASCII description (i.e. "3D Studio")
	const TCHAR *	AuthorName();				// ASCII Author name
	const TCHAR *	CopyrightMessage();			// ASCII Copyright message
	const TCHAR *	OtherMessage1();			// Other message #1
	const TCHAR *	OtherMessage2();			// Other message #2
	unsigned int	Version();					// Version number * 100 (i.e. v3.01 = 301)
	void			ShowAbout(HWND hWnd);		// Show DLL's "About..." box
	
	BOOL			SupportsOptions(int ext, DWORD options);
	int				DoExport(const TCHAR *name, ExpInterface *ei, Interface *i, BOOL suppressPrompts = FALSE, DWORD options = 0);

	float inverseScale = 1.0f;
	Matrix3 inverseCorMat = 1.0f;
};

class : public ClassDesc2 
{
public:
	int				IsPublic() 		{ return TRUE; }
	void *			Create(BOOL) 	{ return new HavokExport(); }
	const TCHAR *	ClassName() 	{ return _className; }
	SClass_ID		SuperClassID() 	{ return SCENE_EXPORT_CLASS_ID; }
	Class_ID		ClassID() 		{ return HavokExport_CLASS_ID; }
	const TCHAR *	Category() 		{ return NULL; }
	const TCHAR *	InternalName() 	{ return _className; }
	HINSTANCE		HInstance() 	{ return hInstance; }
}HavokExportDesc;

ClassDesc2* GetHavokExportDesc() { return &HavokExportDesc; }

//--- HavokImp -------------------------------------------------------
HavokExport::HavokExport()
{
	Rescan();
}

int HavokExport::ExtCount()
{
	return static_cast<int>(extensions.size());
}

const TCHAR *HavokExport::Ext(int n)
{
	return extensions[n].c_str();
}

const TCHAR *HavokExport::LongDesc()
{
	return _T("Havok Export");
}

const TCHAR *HavokExport::ShortDesc()
{
	return _T("Havok Export");
}

const TCHAR *HavokExport::AuthorName()
{
	return _T("Lukas Cone");
}

const TCHAR *HavokExport::CopyrightMessage()
{
	return _T("Copyright (C) 2016-2019 Lukas Cone");
}

const TCHAR *HavokExport::OtherMessage1()
{		
	return _T("");
}

const TCHAR *HavokExport::OtherMessage2()
{		
	return _T("");
}

unsigned int HavokExport::Version()
{				
	return 100;
}

void HavokExport::ShowAbout(HWND hWnd)
{
	ShowAboutDLG(hWnd);
}

BOOL HavokExport::SupportsOptions(int /*ext*/, DWORD /*options*/)
{
	return TRUE;
}

struct xmlBoneMAX : xmlBone
{
	void *ref;
};

thread_local static class : public ITreeEnumProc
{
	void AddBone(INode *nde)
	{
		xmlBoneMAX *currentNode = new xmlBoneMAX();
		currentNode->ID = static_cast<short>(skeleton->bones.size());
		currentNode->name = esString(nde->GetName());
		currentNode->ref = nde;

		INode *parentNode = nde->GetParentNode();
		Matrix3 nodeTM = nde->GetNodeTM(0);

		nodeTM.NoScale();

		if (parentNode && !parentNode->IsRootNode())
		{
			for (auto &b : skeleton->bones)
				if (static_cast<xmlBoneMAX*>(b)->ref == parentNode)
				{
					currentNode->parent = b;
					Matrix3 parentTM = static_cast<INode *>(static_cast<xmlBoneMAX *>(b)->ref)->GetNodeTM(0);
					parentTM.NoScale();
					parentTM.Invert();
					nodeTM *= parentTM;
					break;
				}
		}
		
		if (!currentNode->parent)
			nodeTM *= ex->inverseCorMat;	

		currentNode->transform.position = reinterpret_cast<const Vector &>(nodeTM.GetTrans() * ex->inverseScale);
		currentNode->transform.rotation = reinterpret_cast<Vector4 &>(static_cast<Quat>(nodeTM).Invert());

		skeleton->bones.push_back(currentNode);
	}
public:
	xmlSkeleton *skeleton;
	HavokExport *ex;
	int callback(INode * node)
	{
		AddBone(node);

		return TREE_CONTINUE;
	}
}iSceneScanner;

int HavokExport::DoExport(const TCHAR *fileName, ExpInterface *ei, Interface * /*i*/, BOOL suppressPrompts, DWORD /*options*/)
{
	char *oldLocale = setlocale(LC_NUMERIC, NULL);
	setlocale(LC_NUMERIC, "en-US");

	if (!suppressPrompts)
		if (!SpawnExportDialog())
			return TRUE;

	inverseScale = 1.0f / IDC_EDIT_SCALE_value;
	inverseCorMat = corMat;
	inverseCorMat.Invert();

	xmlHavokFile hkFile = {};
	xmlRootLevelContainer *cont = hkFile.NewClass<xmlRootLevelContainer>();
	xmlAnimationContainer *aniCont = hkFile.NewClass<xmlAnimationContainer>();
	xmlSkeleton *skel = hkFile.NewClass<xmlSkeleton>();
	skel->name = "Reference";
	cont->AddVariant(aniCont);
	aniCont->skeletons.push_back(skel);
	iSceneScanner.skeleton = skel;
	iSceneScanner.ex = this;
	ei->theScene->EnumTree(&iSceneScanner);

	hkFile.ExportXML(fileName, static_cast<hkXMLToolsets>(IDC_CB_TOOLSET_index + 1));

	setlocale(LC_NUMERIC, oldLocale);

	return TRUE;
}
	
