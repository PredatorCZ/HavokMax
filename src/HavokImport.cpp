/*  Havok Tool for 3ds Max
    Copyright(C) 2019-2020 Lukas Cone

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

    Havok Tool uses HavokLib 2016-2020 Lukas Cone
*/
#include "datas/except.hpp"
#include "datas/master_printer.hpp"
#include "havok_api.hpp"

#include "HavokMax.h"

#define HavokImport_CLASS_ID Class_ID(0xad115395, 0x924c02c0)
static const TCHAR _className[] = _T("HavokImport");

class HavokImport : public SceneImport, HavokMaxV2 {
public:
  // Constructor/Destructor
  HavokImport();

  int ExtCount() override;                  // Number of extensions supported
  const TCHAR *Ext(int n) override;         // Extension #n
  const TCHAR *LongDesc() override;         // Long ASCII description
  const TCHAR *ShortDesc() override;        // Short ASCII description
  const TCHAR *AuthorName() override;       // ASCII Author name
  const TCHAR *CopyrightMessage() override; // ASCII Copyright message
  const TCHAR *OtherMessage1() override;    // Other message #1
  const TCHAR *OtherMessage2() override;    // Other message #2
  unsigned int Version() override;    // Version number * 100 (i.e. v3.01 = 301)
  void ShowAbout(HWND hWnd) override; // Show DLL's "About..." box
  int DoImport(const TCHAR *name, ImpInterface *i, Interface *gi,
               BOOL suppressPrompts = FALSE) override;

  void DoImport(const std::string &fileName, bool suppressPrompts);

  void LoadSkeleton(const hkaSkeleton *skel);
  void LoadAnimation(const hkaAnimation *ani, const hkaAnimationBinding *bind);
  void LoadRootMotion(const hkaAnimatedReferenceFrame *ani,
                      const std::vector<float> &times);
};

class : public ClassDesc2 {
public:
  virtual int IsPublic() { return TRUE; }
  virtual void *Create(BOOL) { return new HavokImport(); }
  virtual const TCHAR *ClassName() { return _className; }
  virtual SClass_ID SuperClassID() { return SCENE_IMPORT_CLASS_ID; }
  virtual Class_ID ClassID() { return HavokImport_CLASS_ID; }
  virtual const TCHAR *Category() { return NULL; }
  virtual const TCHAR *InternalName() { return _className; }
  virtual HINSTANCE HInstance() { return hInstance; }
} HavokImportDesc;

ClassDesc2 *GetHavokImportDesc() { return &HavokImportDesc; }

//--- HavokImp -------------------------------------------------------
HavokImport::HavokImport() {}

int HavokImport::ExtCount() { return static_cast<int>(extensions.size()); }

const TCHAR *HavokImport::Ext(int n) {
  return std::next(extensions.begin(), n)->data();
}

const TCHAR *HavokImport::LongDesc() { return _T("Havok Import"); }

const TCHAR *HavokImport::ShortDesc() { return _T("Havok Import"); }

const TCHAR *HavokImport::AuthorName() { return _T("Lukas Cone"); }

const TCHAR *HavokImport::CopyrightMessage() {
  return _T(HavokMax_COPYRIGHT "Lukas Cone");
}

const TCHAR *HavokImport::OtherMessage1() { return _T(""); }

const TCHAR *HavokImport::OtherMessage2() { return _T(""); }

unsigned int HavokImport::Version() { return HAVOKMAX_VERSIONINT; }

void HavokImport::ShowAbout(HWND hWnd) { ShowAboutDLG(hWnd); }

static const MSTR skelNameHint = _T("hkaSkeleton");
static const MSTR boneNameHint = _T("hkaBone");

void HavokImport::LoadSkeleton(const hkaSkeleton *skel) {
  std::vector<INode *> nodes;
  int currentBone = 0;

  for (auto b : *skel->Bones()) {
    TSTRING boneName = ToTSTRING(b->Name());
    INode *node = GetCOREInterface()->GetINodeByName(boneName.c_str());

    if (!node) {
      Object *obj = static_cast<Object *>(
          CreateInstance(HELPER_CLASS_ID, Class_ID(DUMMY_CLASS_ID, 0)));
      node = GetCOREInterface()->CreateObjectNode(obj);
      node->ShowBone(2);
      node->SetWireColor(0x80ff);
    }

    Matrix3 nodeTM = {};
    uni::RTSValue bneTM;
    b->GetTM(bneTM);
    nodeTM.SetRotate(
        reinterpret_cast<const Quat &>(bneTM.rotation.QConjugate()));
    nodeTM.SetTrans(
        reinterpret_cast<const Point3 &>(bneTM.translation * objectScale));
    const auto parentNode = b->Parent();
    if (parentNode) {
      const auto bIndex = parentNode->Index();
      nodes[bIndex]->AttachChild(node);
      nodeTM *= nodes[bIndex]->GetNodeTM(0);
    } else
      nodeTM *= corMat;

    node->SetNodeTM(0, nodeTM);

    node->SetName(ToBoneName(boneName));
    nodes.push_back(node);
    node->SetUserPropString(skelNameHint, ToTSTRING(skel->Name()).data());
    node->SetUserPropString(boneNameHint, ToTSTRING(currentBone).c_str());

    currentBone++;
  }
}

static class : public ITreeEnumProc {
  const MSTR skelNameHint = _T("hkaSkeleton");
  const MSTR boneNameHint = _T("hkaBone");
  const MSTR skelNameExclude = _T("ragdoll");

public:
  std::vector<INode *> bones;

  void RescanBones() {
    bones.clear();
    GetCOREInterface7()->GetScene()->EnumTree(this);
  }

  INode *LookupNode(int ID) {
    for (auto &b : bones)
      if (b->UserPropExists(boneNameHint)) {
        int _ID;
        b->GetUserPropInt(boneNameHint, _ID);

        if (_ID == ID)
          return b;
      }

    return nullptr;
  }

  int callback(INode *node) override {
    Object *refobj = node->EvalWorldState(0).obj;

    if (node->UserPropExists(skelNameHint)) {
      MSTR skelName;
      node->GetUserPropString(skelNameHint, skelName);
      skelName.toLower();

      const int skelNameExcludeSize = skelNameExclude.Length();
      bool found = true;

      for (int s = 0; s < skelNameExcludeSize; s++)
        if (skelName[s] != skelNameExclude[s]) {
          found = false;
          break;
        }

      if (found)
        return TREE_CONTINUE;

      bones.push_back(node);
    }

    return TREE_CONTINUE;
  }
} iBoneScanner;

void HavokImport::LoadRootMotion(const hkaAnimatedReferenceFrame *ani,
                                 const std::vector<float> &times) {
  if (!ani) {
    return;
  }

  std::vector<INode *> rootNodes;
  std::copy_if(iBoneScanner.bones.begin(), iBoneScanner.bones.end(),
               std::back_inserter(rootNodes), [](INode *item) {
                 auto pNode = item->GetParentNode();

                 if (!pNode || pNode->IsRootNode()) {
                   return true;
                 }

                 return false;
               });

  for (auto r : rootNodes) {
    std::vector<Matrix3> cMats;
    cMats.reserve(times.size());

    for (auto t : times) {
      cMats.emplace_back(r->GetNodeTM(SecToTicks(t)));
    }

    size_t cTime = 0;
    Control *cnt = r->GetTMController();
    AnimateOn();

    for (auto t : times) {
      hkQTransform trans;
      ani->GetValue(trans, t);

      Matrix3 cMat(true);
      Quat &cRotation = reinterpret_cast<Quat &>(trans.rotation.QConjugate());
      auto cTrans = reinterpret_cast<Point3 &>(trans.translation * objectScale);
      cMat.SetRotate(cRotation);
      cMat.SetTrans(cTrans * corMat);
      //cMat *= corMat;

      SetXFormPacket packet(cMats[cTime++] * cMat);

      cnt->SetValue(SecToTicks(t), &packet);
    }

    AnimateOff();
  }
}

void HavokImport::LoadAnimation(const hkaAnimation *ani,
                                const hkaAnimationBinding *bind) {
  if (!ani) {
    printerror("[Havok] Unregistered animation format.");
    return;
  }

  /*if (!ani->IsDecoderSupported()) {
    printerror("[Havok] Usupported animation type: ",
               << ani->GetAnimationTypeName());
    return;
  }*/

  iBoneScanner.RescanBones();

  TimeValue numTicks = SecToTicks(ani->Duration());
  TimeValue ticksPerFrame = GetTicksPerFrame();
  TimeValue overlappingTicks = numTicks % ticksPerFrame;

  if (overlappingTicks > (ticksPerFrame / 2)) {
    numTicks += ticksPerFrame - overlappingTicks;
  } else {
    numTicks -= overlappingTicks;
  }
  Interval aniRange(0, numTicks);
  GetCOREInterface()->SetAnimRange(aniRange);

  std::vector<float> frameTimes;

  for (TimeValue v = 0; v <= aniRange.End(); v += GetTicksPerFrame()) {
    frameTimes.push_back(TicksToSec(v));
  }

  const auto numBones = ani->GetNumOfTransformTracks();
  const BlendHint blendType = bind ? bind->GetBlendHint() : BlendHint::NORMAL;

  std::vector<Matrix3> addTMs(numBones, true);

  if (blendType != BlendHint::NORMAL) {
    for (int curBone = 0; curBone < numBones; curBone++) {
      INode *node = nullptr;
      TSTRING boneName;

      if (ani->GetNumAnnotations()) {
        hkaAnnotationTrackPtr annot = ani->GetAnnotation(curBone);
        boneName = ToTSTRING(annot.get()->GetName());
        node = GetCOREInterface()->GetINodeByName(boneName.c_str());
      }

      if (!node && bind) {
        node = iBoneScanner.LookupNode(
            bind->GetTransformTrackToBoneIndex(curBone));
      }

      if (!node) {
        continue;
      }

      Matrix3 inPacket = node->GetNodeTM(0);

      if (!node->GetParentNode()->IsRootNode()) {
        Matrix3 parentTM = node->GetParentTM(0);
        parentTM.Invert();
        inPacket *= parentTM;
      }

      addTMs[curBone] = inPacket;
    }
  }

  const auto tracks = ani->Tracks();

  for (int curBone = 0; curBone < numBones; curBone++) {
    INode *node = nullptr;
    es::string_view bneNameRaw;

    if (ani->GetNumAnnotations()) {
      hkaAnnotationTrackPtr annot = ani->GetAnnotation(curBone);
      bneNameRaw = annot.get()->GetName();
      TSTRING boneName = ToTSTRING(bneNameRaw);
      node = GetCOREInterface()->GetINodeByName(boneName.c_str());
    }

    if (!node) {
      if (bind && bind->GetNumTransformTrackToBoneIndices()) {
        node = iBoneScanner.LookupNode(
            bind->GetTransformTrackToBoneIndex(curBone));
      } else {
        node = iBoneScanner.LookupNode(curBone);
      }
    }

    if (!node) {
      if (bneNameRaw.length()) {
        printwarning("[Havok] Couldn't find bone: " << bneNameRaw);
      } else if (bind && bind->GetNumTransformTrackToBoneIndices()) {
        printwarning("[Havok] Couldn't find hkaBone: "
                     << bind->GetTransformTrackToBoneIndex(curBone));
      } else {
        printwarning("[Havok] Couldn't find hkaBone: " << curBone);
      }

      continue;
    }

    Control *cnt = node->GetTMController();

    if (cnt->GetPositionController()->ClassID() !=
        Class_ID(LININTERP_POSITION_CLASS_ID, 0)) {
      cnt->SetPositionController((Control *)CreateInstance(
          CTRL_POSITION_CLASS_ID, Class_ID(LININTERP_POSITION_CLASS_ID, 0)));
    }

    if (cnt->GetRotationController()->ClassID() !=
        Class_ID(LININTERP_ROTATION_CLASS_ID, 0)) {
      cnt->SetRotationController((Control *)CreateInstance(
          CTRL_ROTATION_CLASS_ID, Class_ID(LININTERP_ROTATION_CLASS_ID, 0)));
    }

    if (cnt->GetScaleController()->ClassID() !=
        Class_ID(LININTERP_SCALE_CLASS_ID, 0)) {
      cnt->SetScaleController((Control *)CreateInstance(
          CTRL_SCALE_CLASS_ID, Class_ID(LININTERP_SCALE_CLASS_ID, 0)));
    }

    SuspendAnimate();
    AnimateOn();

    const auto track = tracks->At(curBone);

    for (auto &t : frameTimes) {
      hkQTransform trans;
      track->GetValue(trans, t);

      Matrix3 cMat;
      Quat &cRotation = reinterpret_cast<Quat &>(trans.rotation.QConjugate());
      auto cTrans = reinterpret_cast<Point3 &>(trans.translation * objectScale);

      if (blendType == BlendHint::ADDITIVE_DEPRECATED) {
        cMat.SetRotate(Quat(addTMs[curBone]) + cRotation);
        cMat.SetTrans(cTrans + addTMs[curBone].GetTrans());
      } else if (blendType == BlendHint::ADDITIVE) {
        cMat.SetRotate(cRotation + Quat(addTMs[curBone]));
        cMat.SetTrans(cTrans + addTMs[curBone].GetTrans());
      } else {
        cMat.SetRotate(cRotation);
        cMat.SetTrans(cTrans);
      }

      if (!checked[Checked::CH_DISABLE_SCALE]) {
        cMat.Scale(reinterpret_cast<Point3 &>(trans.scale));
      }

      if (node->GetParentNode()->IsRootNode()) {
        cMat *= corMat;
      } else if (!checked[Checked::CH_DISABLE_SCALE]) {
        Matrix3 pAbsMat = node->GetParentTM(SecToTicks(t));
        Point3 nScale = {pAbsMat.GetRow(0).Length(), pAbsMat.GetRow(1).Length(),
                         pAbsMat.GetRow(2).Length()};

        for (int s = 0; s < 3; s++) {
          if (!nScale[s]) {
            nScale[s] = FLT_EPSILON;
          }
        }
        Point3 fracPos = cMat.GetTrans() / nScale;
        nScale = 1.f - nScale;
        cMat.Translate(fracPos * nScale);
      }

      SetXFormPacket packet(cMat);

      cnt->SetValue(SecToTicks(t), &packet);
    }

    AnimateOff();

    Control *rotControl = (Control *)CreateInstance(
        CTRL_ROTATION_CLASS_ID, Class_ID(HYBRIDINTERP_ROTATION_CLASS_ID, 0));
    rotControl->Copy(cnt->GetRotationController());
    cnt->GetRotationController()->Copy(rotControl);
  }

  LoadRootMotion(ani->GetExtractedMotion(), frameTimes);
}

void HavokImport::DoImport(const std::string &fileName, bool suppressPrompts) {
  auto pFile = IhkPackFile::Create(std::to_string(fileName));
  const hkRootLevelContainer *rootCont = pFile->GetRootLevelContainer();

  for (auto &v : *rootCont) {
    if (v == hkaAnimationContainer::GetHash()) {
      const hkaAnimationContainer *aniCont = v;

      numAnimations = aniCont->GetNumAnimations();

      if (!suppressPrompts) {
        if (!SpawnImportDialog()) {
          return;
        }
      }

      for (auto s : aniCont->Skeletons()) {
        LoadSkeleton(s);
      }

      if (numAnimations) {
        LoadAnimation(aniCont->GetAnimation(motionIndex),
                      aniCont->GetNumBindings()
                          ? aniCont->GetBinding(motionIndex)
                          : nullptr);
      }
    }
  }
}

void SwapLocale() {
  static std::string oldLocale;

  if (!oldLocale.empty()) {
    setlocale(LC_NUMERIC, oldLocale.data());
    oldLocale.clear();
  } else {
    oldLocale = setlocale(LC_NUMERIC, nullptr);
    setlocale(LC_NUMERIC, "en-US");
  }
}

int HavokImport::DoImport(const TCHAR *fileName, ImpInterface * /*importerInt*/,
                          Interface * /*ip*/, BOOL suppressPrompts) {
  SwapLocale();
  TSTRING filename_ = fileName;

  try {
    DoImport(std::to_string(filename_), suppressPrompts);
  } catch (const es::InvalidHeaderError &) {
    SwapLocale();
    return FALSE;
  } catch (const std::exception &e) {
    if (suppressPrompts) {
      printerror(e.what());
    } else {
      auto msg = ToTSTRING(e.what());
      MessageBox(GetActiveWindow(), msg.data(), _T("Exception thrown!"),
                 MB_ICONERROR | MB_OK);
    }
  } catch (...) {
    if (suppressPrompts) {
      printerror("Unhandled exception has been thrown!");
    } else {
      MessageBox(GetActiveWindow(), _T("Unhandled exception has been thrown!"),
                 _T("Exception thrown!"), MB_ICONERROR | MB_OK);
    }
  }

  SwapLocale();

  return TRUE;
}
