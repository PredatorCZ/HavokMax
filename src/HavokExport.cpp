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
#include "datas/fileinfo.hpp"
#include <impapi.h>

#define HavokExport_CLASS_ID	Class_ID(0x2b020aa4, 0x5c7f7d58)
static const TCHAR _className[] = _T("HavokExport");

class HavokExport : public SceneExport, public HavokMaxV2
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

	void ProcessAnimation(xmlSkeleton *skel, xmlAnimationBinding *binds, xmlInterleavedAnimation *anim);
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
HavokExport::HavokExport() {}

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
	return HAVOKMAX_VERSIONINT;
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
		TimeValue captureFrame = ex->IDC_EDIT_CAPTUREFRAME_index * GetTicksPerFrame();
		xmlBoneMAX *currentNode = new xmlBoneMAX();
		currentNode->ID = static_cast<short>(skeleton->bones.size());
		currentNode->name = esString(nde->GetName());
		currentNode->ref = nde;

		INode *parentNode = nde->GetParentNode();
		Matrix3 nodeTM = nde->GetNodeTM(captureFrame);

		nodeTM.NoScale();

		if (parentNode && !parentNode->IsRootNode())
		{
			for (auto &b : skeleton->bones)
				if (static_cast<xmlBoneMAX*>(b)->ref == parentNode)
				{
					currentNode->parent = b;
					Matrix3 parentTM = static_cast<INode *>(static_cast<xmlBoneMAX *>(b)->ref)->GetNodeTM(captureFrame);
					parentTM.NoScale();
					parentTM.Invert();
					nodeTM *= parentTM;
					break;
				}
		}
		
		if (!currentNode->parent)
			nodeTM *= ex->inverseCorMat;	

		currentNode->transform.position = reinterpret_cast<const Vector4 &>(nodeTM.GetTrans() * ex->inverseScale);
		currentNode->transform.position.W = 1.0f;
		currentNode->transform.rotation = reinterpret_cast<Vector4 &>(static_cast<Quat>(nodeTM).Conjugate());

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


bool CanSkipBone(INode *cNode)
{
	Control *ctr = cNode->GetTMController();
	Control *posCtrl = ctr->GetPositionController();

	if (!posCtrl)
		return false;

	IKeyControl *posCtr = GetKeyControlInterface(posCtrl);

	int numKeys = 0;

	if (posCtr)
		numKeys |= posCtr->GetNumKeys();
	else
	{
		posCtr = GetKeyControlInterface(posCtrl->GetXController());
		numKeys |= posCtr->GetNumKeys();
		posCtr = GetKeyControlInterface(posCtrl->GetYController());
		numKeys |= posCtr->GetNumKeys();
		posCtr = GetKeyControlInterface(posCtrl->GetZController());
		numKeys |= posCtr->GetNumKeys();
	}

	Control *rotCtrl = ctr->GetRotationController();

	if (!rotCtrl)
		return false;

	IKeyControl *rotCtr = GetKeyControlInterface(rotCtrl);

	if (rotCtr)
		numKeys |= rotCtr->GetNumKeys();
	else
	{
		rotCtr = GetKeyControlInterface(rotCtrl->GetXController());
		numKeys |= rotCtr->GetNumKeys();
		rotCtr = GetKeyControlInterface(rotCtrl->GetYController());
		numKeys |= rotCtr->GetNumKeys();
		rotCtr = GetKeyControlInterface(rotCtrl->GetZController());
		numKeys |= rotCtr->GetNumKeys();
	}

	Control *scaleCtrl = ctr->GetScaleController();

	if (!scaleCtrl)
		return false;

	IKeyControl *scaleCtr = GetKeyControlInterface(scaleCtrl);

	if (scaleCtr)
		numKeys |= scaleCtr->GetNumKeys();
	else
	{
		scaleCtr = GetKeyControlInterface(scaleCtrl->GetXController());
		numKeys |= scaleCtr->GetNumKeys();
		scaleCtr = GetKeyControlInterface(scaleCtrl->GetYController());
		numKeys |= scaleCtr->GetNumKeys();
		scaleCtr = GetKeyControlInterface(scaleCtrl->GetZController());
		numKeys |= scaleCtr->GetNumKeys();
	}

	return numKeys < 2;
}

void HavokExport::ProcessAnimation(xmlSkeleton *skel, xmlAnimationBinding *binds, xmlInterleavedAnimation *anim)
{
	anim->animType = HK_INTERLEAVED_ANIMATION;

	Interval captureIterval(IDC_EDIT_ANISTART_index * GetTicksPerFrame(), IDC_EDIT_ANIEND_index * GetTicksPerFrame());

	anim->duration = TicksToSec(captureIterval.End() - captureIterval.Start());
	anim->annotations.reserve(skel->GetNumBones());

	for (auto &b : skel->bones)
	{
		xmlBoneMAX *cBone = static_cast<xmlBoneMAX *>(b);
		INode *cNode = static_cast<INode *>(cBone->ref);

		if (flags[IDC_CH_ANIOPTIMIZE_checked] && flags[IDC_CH_ANIOPTIMIZE_enabled] && CanSkipBone(cNode))
			continue;

		binds->transformTrackToBoneIndices.push_back(cBone->ID);

		xmlInterleavedAnimation::Transform_Container *aCont = new xmlInterleavedAnimation::Transform_Container;
		aCont->reserve(captureIterval.Duration() / GetTicksPerFrame());

		Control *rotateControl = (Control *)CreateInstance(CTRL_ROTATION_CLASS_ID, Class_ID(HYBRIDINTERP_ROTATION_CLASS_ID, 0));
		IKeyControl *kCon = GetKeyControlInterface(rotateControl);

		for (TimeValue t = captureIterval.Start(); t <= captureIterval.End(); t += GetTicksPerFrame())
		{
			Matrix3 lMat = cNode->GetNodeTM(t);
			Matrix3 pMat;
			INode *parentNode = cNode->GetParentNode();

			if (parentNode && !parentNode->IsRootNode())
			{
				pMat = cNode->GetParentTM(t);
				pMat.Invert();
			}
			else
				pMat = inverseCorMat;
			
			lMat *= pMat;
			
			hkQTransform cTransform;
			
			cTransform.scale = { lMat.GetRow(0).Length(), lMat.GetRow(1).Length(), lMat.GetRow(2).Length(), 0.0f };	

			if (parentNode && !parentNode->IsRootNode())
			{
				pMat = cNode->GetParentTM(t);
				pMat.NoScale();
				pMat.Invert();
			}

			lMat = cNode->GetNodeTM(t);
			lMat.NoScale();
			lMat *= pMat;

			ILinRotKey cKey;
			cKey.time = t;
			cKey.val = Quat(lMat).Conjugate();
			kCon->AppendKey(&cKey);		

			cTransform.position = reinterpret_cast<const Vector4 &>(lMat.GetTrans());
			cTransform.position.W = 1.0f;
			aCont->push_back(cTransform);
			//TODO check scale
		}

		Control *rotateControl2 = (Control *)CreateInstance(CTRL_ROTATION_CLASS_ID, Class_ID(LININTERP_ROTATION_CLASS_ID , 0));
		rotateControl2->Copy(rotateControl);
		IKeyControl *kCon2 = GetKeyControlInterface(rotateControl2);

		for (int k = 0; k < kCon2->GetNumKeys(); k++)
		{
			ILinRotKey cKey;
			kCon2->GetKey(k, &cKey);

			aCont->at(k).rotation = reinterpret_cast<Vector4 &>(cKey.val);
		}

		if (aCont->size() == 1)
			for (auto &t : *aCont)
				aCont->push_back(aCont->at(0));

		anim->transforms.push_back(aCont);

		xmlAnnotationTrack annot;
		annot.name = cBone->name;
		anim->annotations.push_back(annot);	
	}
}

void SaveEnvData(xmlEnvironment *env, const TCHAR *fileName)
{
	TFileInfo fleInfo(fileName);
	MSTR *curMaxFile = &GetCOREInterface()->GetCurFilePath();
	
	if (curMaxFile->length())
	{
		TFileInfo maxInfo(curMaxFile->data());

		xmlEnvironmentVariable asset;
		asset.name = "asset";
		asset.value = esString(maxInfo.GetFileName());
		env->variables.push_back(asset);

		xmlEnvironmentVariable assetFolder;
		assetFolder.name = "assetFolder";
		assetFolder.value = esString(maxInfo.GetPath());
		env->variables.push_back(assetFolder);

		xmlEnvironmentVariable assetPath;
		assetPath.name = "assetPath";
		assetPath.value = esString(maxInfo.GetFullPath());
		env->variables.push_back(assetPath);
	}

	xmlEnvironmentVariable out;
	out.name = "out";
	out.value = esString(fleInfo.GetFileName());
	env->variables.push_back(out);

	xmlEnvironmentVariable outFolder;
	outFolder.name = "outFolder";
	outFolder.value = esString(fleInfo.GetPath());
	env->variables.push_back(outFolder);

	xmlEnvironmentVariable outPath;
	outPath.name = "outPath";
	outPath.value = esString(fleInfo.GetFullPath());
	env->variables.push_back(outPath);
}

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
	xmlEnvironment *envData = hkFile.NewClass<xmlEnvironment>();
	const bool useSkeleton = (flags[IDC_CH_ANIMATION_checked] && flags[IDC_CH_ANISKELETON_checked]) || !flags[IDC_CH_ANIMATION_checked];
	xmlSkeleton *skel = useSkeleton ? hkFile.NewClass<xmlSkeleton>() : new xmlSkeleton;

	skel->name = "Reference";
	iSceneScanner.skeleton = skel;
	iSceneScanner.ex = this;
	ei->theScene->EnumTree(&iSceneScanner);
	cont->AddVariant(aniCont);
	cont->AddVariant(envData);
	SaveEnvData(envData, fileName);

	if (useSkeleton)
		aniCont->skeletons.push_back(skel);

	if (flags[IDC_CH_ANIMATION_checked])
	{
		xmlAnimationBinding *binding = hkFile.NewClass<xmlAnimationBinding>();
		xmlInterleavedAnimation *anim = hkFile.NewClass<xmlInterleavedAnimation>();
		binding->animation = dynamic_cast<xmlAnimation*>(anim);
		aniCont->animations.push_back(binding->animation);
		aniCont->bindings.push_back(binding);

		if (flags[IDC_CH_ANISKELETON_checked])
			binding->skeletonName = skel->name;

		ProcessAnimation(skel, binding, anim);
	}

	hkFile.ExportXML(fileName, static_cast<hkXMLToolsets>(IDC_CB_TOOLSET_index + 1));

	if (!useSkeleton)
		delete skel;

	setlocale(LC_NUMERIC, oldLocale);

	return TRUE;
}
