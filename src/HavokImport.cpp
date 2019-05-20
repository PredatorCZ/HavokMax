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
#include "datas/masterprinter.hpp"

#define HavokImport_CLASS_ID	Class_ID(0xad115395, 0x924c02c0)
static const TCHAR _className[] = _T("HavokImport");

class HavokImport : public SceneImport, HavokMaxV2
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
	void LoadAnimation(const hkaAnimation *ani);
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
	return HAVOKMAX_VERSIONINT;
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

void HavokImport::LoadAnimation(const hkaAnimation *ani)
{
	if (!ani)
	{
		printerror("[Havok] Unregistered animation format.");
		return;
	}

	if (!ani->IsDecoderSupported())
	{
		printerror("[Havok] Usupported animation type: ", << ani->GetAnimationTypeName());
		return;
	}

	TimeValue numTicks = SecToTicks(ani->GetDuration());
	TimeValue ticksPerFrame = GetTicksPerFrame();
	TimeValue overlappingTicks = numTicks % ticksPerFrame;

	if (overlappingTicks > (ticksPerFrame / 2))
		numTicks += ticksPerFrame - overlappingTicks;
	else
		numTicks -= overlappingTicks;

	Interval aniRange(0, numTicks);
	GetCOREInterface()->SetAnimRange(aniRange);

	std::vector<float> frameTimes;

	for (TimeValue v = 0; v <= aniRange.End(); v += GetTicksPerFrame())
		frameTimes.push_back(TicksToSec(v));

	int curBone = 0;
	float frameRate = ani->ComputeFrameRate();

	for (auto &a : ani->Annotations())
	{
		TSTRING boneName = esStringConvert<TCHAR>(a.get()->GetName());
		INode *node = GetCOREInterface()->GetINodeByName(boneName.c_str());

		if (!node)
		{
			printwarning("[Havok] Couldn't find bone: ", << boneName);
			curBone++;
			continue;
		}

		Control *cnt = node->GetTMController();

		if (cnt->GetPositionController()->ClassID() != Class_ID(LININTERP_POSITION_CLASS_ID, 0))
			cnt->SetPositionController((Control *)CreateInstance(CTRL_POSITION_CLASS_ID, Class_ID(LININTERP_POSITION_CLASS_ID, 0)));

		if (cnt->GetRotationController()->ClassID() != Class_ID(LININTERP_ROTATION_CLASS_ID, 0))
			cnt->SetRotationController((Control *)CreateInstance(CTRL_ROTATION_CLASS_ID, Class_ID(LININTERP_ROTATION_CLASS_ID, 0)));

		if (cnt->GetScaleController()->ClassID() != Class_ID(LININTERP_SCALE_CLASS_ID, 0))
			cnt->SetScaleController((Control *)CreateInstance(CTRL_SCALE_CLASS_ID, Class_ID(LININTERP_SCALE_CLASS_ID, 0)));

		SuspendAnimate();
		AnimateOn();

		for (auto &t : frameTimes)
		{
			hkQTransform trans;
			ani->GetTransform(curBone, t, frameRate, trans);

			Matrix3 cMat;
			Quat &rots = reinterpret_cast<Quat &>(trans.rotation);
			cMat.SetRotate(rots.Conjugate());
			cMat.SetTrans(reinterpret_cast<Point3 &>(trans.position) * IDC_EDIT_SCALE_value);
			cMat.Scale(reinterpret_cast<Point3 &>(trans.scale));

			if (node->GetParentNode()->IsRootNode())
				cMat *= corMat;
			else
			{
				Matrix3 pAbsMat = node->GetParentTM(SecToTicks(t));
				Point3 nScale = { pAbsMat.GetRow(0).Length(), pAbsMat.GetRow(1).Length(), pAbsMat.GetRow(2).Length() };
				Point3 fracPos = cMat.GetTrans() / nScale;
				nScale = 1.f - nScale;
				cMat.Translate(fracPos * nScale);
			}

			SetXFormPacket packet(cMat);

			cnt->SetValue(SecToTicks(t), &packet);	
		}

		AnimateOff();

		Control *rotControl = (Control *)CreateInstance(CTRL_ROTATION_CLASS_ID, Class_ID(HYBRIDINTERP_ROTATION_CLASS_ID, 0));
		rotControl->Copy(cnt->GetRotationController());
		cnt->GetRotationController()->Copy(rotControl);
		
		curBone++;
	}
}



int HavokImport::DoImport(const TCHAR*fileName, ImpInterface* /*importerInt*/, Interface* /*ip*/, BOOL suppressPrompts)
{
	char *oldLocale = setlocale(LC_NUMERIC, NULL);
	setlocale(LC_NUMERIC, "en-US");

	if (!suppressPrompts)
		if (!SpawnImportDialog())
			return TRUE;

	std::wstring _filename = esString(fileName);
	IhkPackFile *pFile = IhkPackFile::Create(_filename.c_str());

	if (!pFile)
		return FALSE;

	const hkRootLevelContainer *rootCont = pFile->GetRootLevelContainer();

	for (auto &v : *rootCont)
		if (v == hkaAnimationContainer::HASH)
		{
			const hkaAnimationContainer *_cont = v;

			for (auto &s : _cont->Skeletons())
				LoadSkeleton(s);

			if (_cont->GetNumAnimations())
				LoadAnimation(_cont->GetAnimation(0));
		}
	
	delete pFile;

	setlocale(LC_NUMERIC, oldLocale);

	return TRUE;
}
	
