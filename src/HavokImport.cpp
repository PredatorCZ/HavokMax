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

#define HavokImport_CLASS_ID	Class_ID(0xad115395, 0x924c02c0)
static const TCHAR _className[] = _T("HavokImport");

class HavokImport : public SceneImport, HavokMax
{
public:
	//Constructor/Destructor
	HavokImport();
	virtual ~HavokImport() {}

	virtual int				ExtCount();					// Number of extensions supported
	virtual const TCHAR *	Ext(int n);					// Extension #n (i.e. "3DS")
	virtual const TCHAR *	LongDesc();					// Long ASCII description (i.e. "Autodesk 3D Studio File")
	virtual const TCHAR *	ShortDesc();				// Short ASCII description (i.e. "3D Studio")
	virtual const TCHAR *	AuthorName();				// ASCII Author name
	virtual const TCHAR *	CopyrightMessage();			// ASCII Copyright message
	virtual const TCHAR *	OtherMessage1();			// Other message #1
	virtual const TCHAR *	OtherMessage2();			// Other message #2
	virtual unsigned int	Version();					// Version number * 100 (i.e. v3.01 = 301)
	virtual void			ShowAbout(HWND hWnd);		// Show DLL's "About..." box
	virtual int				DoImport(const TCHAR *name,ImpInterface *i,Interface *gi, BOOL suppressPrompts=FALSE);	// Import file

	void LoadSkeleton(const hkaSkeleton &skel);
};

class : public ClassDesc2 
{
public:
	virtual int				IsPublic() 		{ return TRUE; }
	virtual void *			Create(BOOL) 	{ return new HavokImport(); }
	virtual const TCHAR *	ClassName() 	{ return _className; }
	virtual SClass_ID		SuperClassID() 	{ return SCENE_IMPORT_CLASS_ID; }
	virtual Class_ID		ClassID() 		{ return HavokImport_CLASS_ID; }
	virtual const TCHAR *	Category() 		{ return NULL; }
	virtual const TCHAR *	InternalName() 	{ return _className; }
	virtual HINSTANCE		HInstance() 	{ return hInstance; }
}HavokImportDesc;

ClassDesc2* GetHavokImportDesc() { return &HavokImportDesc; }

//--- HavokImp -------------------------------------------------------
HavokImport::HavokImport()
{
	Rescan();
}

int HavokImport::ExtCount()
{
	return static_cast<int>(extensions.size());
}

const TCHAR *HavokImport::Ext(int n)
{
	return extensions[n].c_str();
}

const TCHAR *HavokImport::LongDesc()
{
	return _T("Havok Import");
}

const TCHAR *HavokImport::ShortDesc()
{
	return _T("Havok Import");
}

const TCHAR *HavokImport::AuthorName()
{
	return _T("Lukas Cone");
}

const TCHAR *HavokImport::CopyrightMessage()
{
	return _T("Copyright (C) 2016-2019 Lukas Cone");
}

const TCHAR *HavokImport::OtherMessage1() 
{		
	return _T("");
}

const TCHAR *HavokImport::OtherMessage2() 
{		
	return _T("");
}

unsigned int HavokImport::Version()
{				
	return 100;
}

void HavokImport::ShowAbout(HWND hWnd)
{
	ShowAboutDLG(hWnd);
}

static const MSTR skelNameHint = _T("hkaSkeleton");
static const MSTR boneNameHint = _T("hkaBone");

void HavokImport::LoadSkeleton(const hkaSkeleton &skel)
{
	std::vector<INode*> nodes;
	int currentBone = 0;

	for (auto b : skel.FullBones())
	{
		TSTRING boneName = esString(b);
		INode *node = GetCOREInterface()->GetINodeByName(boneName.c_str());

		if (!node)
		{
			Object *obj = static_cast<Object*>(CreateInstance(HELPER_CLASS_ID, Class_ID(DUMMY_CLASS_ID, 0)));
			node = GetCOREInterface()->CreateObjectNode(obj);
			node->ShowBone(2);
			node->SetWireColor(0x80ff);
		}

		Matrix3 nodeTM = {};
		nodeTM.SetRotate(reinterpret_cast<const Quat&>(b.transform->rotation).Conjugate());
		nodeTM.SetTrans(reinterpret_cast<const Point3&>(b.transform->position) * IDC_EDIT_SCALE_value);
		
		if (b.parentID > -1)
		{
			nodes[b.parentID]->AttachChild(node);
			nodeTM *= nodes[b.parentID]->GetNodeTM(0);
		}
		else
			nodeTM *= corMat;

		node->SetNodeTM(0, nodeTM);
		node->SetName(ToBoneName(boneName));
		nodes.push_back(node);
		node->SetUserPropString(skelNameHint, static_cast<TSTRING>(esString(skel)).c_str());
		node->SetUserPropString(boneNameHint, ToTSTRING(currentBone).c_str());

		currentBone++;
	}
}

int HavokImport::DoImport(const TCHAR* filename, ImpInterface* /*importerInt*/, Interface* /*ip*/, BOOL suppressPrompts)
{
	if (!suppressPrompts)
		if (!SpawnImportDialog())
			return TRUE;

	IhkPackFile *pFile = IhkPackFile::Create(filename);

	if (!pFile)
		return FALSE;

	const hkRootLevelContainer *rootCont = pFile->GetRootLevelContainer();

	for (auto &v : *rootCont)
		if (v == hkaAnimationContainer::HASH)
		{
			const hkaAnimationContainer *_cont = v;

			for (auto &s : _cont->Skeletons())
				LoadSkeleton(s);
		}
	
	delete pFile;

	return TRUE;
}
	
