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

#pragma once
#include "MAXex/3DSMaxSDKCompat.h"
#include "datas/reflector.hpp"
#include "hklib/hk_base.hpp"
#include "project.h"

#include <iparamb2.h>
#include <iparamm2.h>
#include <istdplug.h>
#include <maxtypes.h>
// SIMPLE TYPE

#include <commdlg.h>
#include <direct.h>
#include <impexp.h>
#undef min
#undef max
#include "datas/flags.hpp"
#include <set>
#include <vector>

static constexpr int HAVOKMAX_VERSIONINT =
    HavokMax_VERSION_MAJOR * 100 + HavokMax_VERSION_MINOR;

struct PresetData;
extern HINSTANCE hInstance;

REFLECTOR_CREATE(Checked, ENUM, 2, CLASS, 8, CH_ANIMATION, CH_ANISKELETON,
                 CH_ANIOPTIMIZE, CH_DISABLE_SCALE);
REFLECTOR_CREATE(Visible, ENUM, 2, CLASS, 8, CH_ANISKELETON, CH_ANIOPTIMIZE,
                 SP_ANIEND, SP_ANISTART);

class HavokMax : public ReflectorInterface<HavokMax> {
public:
  enum DLGTYPE_e { DLGTYPE_unknown, DLGTYPE_import, DLGTYPE_export };

  // config
  es::Flags<Checked> checked;
  es::Flags<Visible> visible;
  int32 motionIndex, additiveOverride;
  hkToolset toolset;
  TimeValue animationStart, animationEnd, captureFrame;
  std::string currentPresetName;

  // preset data
  float objectScale;
  Matrix3 corMat;

  DLGTYPE_e instanceDialogType;
  HWND comboHandle;
  HWND hWnd;
  es::Flags<uint8> sanityCheck;
  int32 numAnimations;

  void LoadCFG();
  void BuildCFG();
  void SaveCFG();
  void UpdateData();
  void SetupExportUI();

  int SavePreset(const std::string &presetName);
  virtual int SavePreset(PresetData &presetData) = 0;
  virtual void Setup(HWND hwnd);
  virtual void CollisionHandler() = 0;
  virtual void UpdatePresetUI(PresetData &presetData) = 0;
  virtual int SanityBitcher() = 0;
  virtual int SpawnImportDialog() = 0;
  virtual int SpawnExportDialog() = 0;
  virtual INT_PTR DlgCommandCallBack(WPARAM wParam, LPARAM lParam) = 0;

  HavokMax();
  virtual ~HavokMax() {}
};

class HavokMaxV1 : public HavokMax {
public:
  void CollisionHandler();
  void UpdatePresetUI(PresetData &presetData);
  int SanityBitcher();
  int SpawnImportDialog();
  int SpawnExportDialog();
  INT_PTR DlgCommandCallBack(WPARAM wParam, LPARAM lParam);
  int SavePreset(PresetData &presetData);
};

class HavokMaxV2 : public HavokMax {
public:
  HWND comboRight;
  HWND comboBack;
  HWND comboAddOvr;

  void CollisionHandler();
  void UpdatePresetUI(PresetData &presetData);
  int SanityBitcher();
  int SpawnImportDialog();
  int SpawnExportDialog();
  void Setup(HWND hwnd);
  INT_PTR DlgCommandCallBack(WPARAM wParam, LPARAM lParam);
  int SavePreset(PresetData &presetData);
};

void BuildHavokResources();
void DestroyHavokResources();
void ShowAboutDLG(HWND hWnd);
extern std::set<TSTRING> extensions;
