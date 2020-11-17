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
#include "datas/directory_scanner.hpp"
#include "datas/pugiex.hpp"
#include "datas/reflector_xml.hpp"
#include <map>

#include "HavokMax.h"
#include "MAXex/hk_preset.hpp"
#define max(a, b) (((a) > (b)) ? (a) : (b))
#define min(a, b) (((a) < (b)) ? (a) : (b))
#include "resource.h"
#include <3dsmaxport.h>
#include <IPathConfigMgr.h>
#include <array>
#include <commctrl.h>
#include <iparamm2.h>

const TCHAR _name[] = _T("Havok Tool");
const TCHAR _info[] =
    _T("\n" HavokMax_COPYRIGHT "Lukas Cone\nVersion " HavokMax_VERSION);
const TCHAR _license[] =
    _T("Havok Tool uses HavokLib, Copyright(C) 2016-2020 Lukas Cone.");
const TCHAR _homePage[] =
    _T("https://lukascone.wordpress.com/2019/03/21/havok-3ds-max-plugin");

#include "MAXex/win/AboutDlg.h"

REFLECTOR_CREATE(HavokMax, 1, VARNAMES, checked, visible, motionIndex, toolset,
                 animationStart, animationEnd, captureFrame, currentPresetName);

struct PresetData : ReflectorInterface<PresetData> {
  float scale;
  bool external;
  Matrix3 corMat;
  PresetData() : scale(1.0f), external(false) { corMat.IdentityMatrix(); }
};

REFLECTOR_CREATE(PresetData, 1, VARNAMES, scale);

std::set<TSTRING> extensions = {_T("hkx"), _T("hkt"), _T("hka")};
static std::map<std::string, PresetData> presets{
    {"Default", PresetData{}},
};

void SavePresets(pugi::xml_node node) {
  auto rootNode = node.append_child("presets");

  for (auto &f : presets) {
    if (!f.second.external) {
      auto presetNode = rootNode.append_child("preset");
      presetNode.append_attribute("name").set_value(f.first.data());
      ReflectorWrapConst<PresetData> rWrap(f.second);
      ReflectorXMLUtil::Save(rWrap, presetNode);
      auto corMatNode = presetNode.append_child("matrix");
      auto corMatData = GetCorrectionMatrix(f.second.corMat);
      corMatNode.append_buffer(corMatData.data(), corMatData.size());
    }
  }
}

static void GetCorrectionMatrix(Matrix3 &value, es::string_view text) {
  float sign = 1.0f;
  uint32 curRow = 0;

  for (auto t : text) {
    if (curRow > 2) {
      return;
    }

    switch (t) {
    case '-':
      sign = -1.f;
      continue;
    case 'X':
      value.SetRow(curRow, Point3(sign, 0.0f, 0.0f));
      break;
    case 'Y':
      value.SetRow(curRow, Point3(0.0f, sign, 0.0f));
      break;
    case 'Z':
      value.SetRow(curRow, Point3(0.0f, 0.0f, sign));
      break;
    }

    curRow++;
    sign = 1.f;
  }
}

static auto &LoadPreset(pugi::xml_node node) {
  auto prName = node.attribute("name").value();
  auto &prData = presets[prName];
  ReflectorWrap<PresetData> rWrap(prData);
  ReflectorXMLUtil::Load(rWrap, node);
  auto corMatNode = node.child("matrix");

  if (corMatNode.empty()) {
    return prData;
  }

  GetCorrectionMatrix(prData.corMat, corMatNode.text().get());
  return prData;
}

static void LoadPresets(pugi::xml_node node) {
  auto rootNode = node.child("presets");

  if (rootNode.empty()) {
    return;
  }

  for (auto ch : rootNode) {
    LoadPreset(ch);
  }
}

static void LoadExternalPresetLegacy(const TCHAR *filename) {
  TSTRING prName_;
  prName_.resize(0x102);
  TCHAR legacyGroup[] = _T("HK_PRESET");
  auto numReadChars =
      GetPrivateProfileString(legacyGroup, _T("Name"), _T(""), &prName_[0],
                              (DWORD)prName_.size(), filename);
  prName_.resize(numReadChars);

  if (prName_.empty()) {
    return;
  }

  decltype(auto) prName = std::to_string(prName_);
  prName.erase(0, 2);
  auto &prData = presets[prName];
  prData.external = true;
  prName_.resize(16);

  GetPrivateProfileString(legacyGroup, _T("Scale"), _T(""), &prName_[0],
                          (DWORD)prName_.size(), filename);

  prData.scale = std::stof(prName_);

  GetPrivateProfileString(legacyGroup, _T("Matrix"), _T(""), &prName_[0],
                          (DWORD)prName_.size(), filename);

  GetCorrectionMatrix(prData.corMat, std::to_string(prName_));

  prName_.resize(0x102);
  numReadChars =
      GetPrivateProfileString(legacyGroup, _T("Extensions"), _T(""),
                              &prName_[0], (DWORD)prName_.size(), filename);
  prName_.resize(numReadChars);

  if (prName_.empty()) {
    return;
  }

  es::basic_string_view<TCHAR> extRef(prName_.data());
  extRef.remove_prefix(2);

  while (!extRef.empty()) {
    const size_t found = extRef.find('|');

    if (!found) {
      extRef.remove_prefix(1);
    }

    if (found == extRef.npos) {
      extensions.emplace(extRef);
      break;
    } else {
      es::basic_string_view<TCHAR> subitem(extRef.begin(), found);
      extensions.emplace(subitem);
      extRef.remove_prefix(found + 1);
    }
  }
}

void LoadExternalPreset(pugi::xml_node node) {
  auto &prData = LoadPreset(node);
  prData.external = true;
  auto extNode = node.child("extensions");

  if (extNode.empty()) {
    return;
  }

  for (auto c : extNode) {
    extensions.emplace(ToTSTRING(c.name()));
  }
}

void ScanPresets() {
  TSTRING cfgPath = IPathConfigMgr::GetPathConfigMgr()->GetDir(APP_PLUGCFG_DIR);
  auto cfgPathS = std::to_string(cfgPath);
  DirectoryScanner legacyScan;
  legacyScan.AddFilter(".ini");
  legacyScan.Scan(cfgPathS);

  for (auto &s : legacyScan) {
    auto cName = ToTSTRING(s);
    LoadExternalPresetLegacy(cName.data());
  }

  DirectoryScanner scanner;
  scanner.AddFilter(".xml");
  scanner.Scan(cfgPathS);

  for (auto &s : scanner) {
    try {
      pugi::xml_document doc = XMLFromFile(s);
      auto prNode = doc.child("HavokPreset");

      if (prNode.empty()) {
        continue;
      }

      LoadExternalPreset(prNode);
    } catch (...) {
      continue;
    }
  }
}

static HBITMAP bitmapGreen, bitmapRed, bitmapGray;

void BuildHavokResources() {
  HBITMAP bitmapBulbs = LoadBitmap(hInstance, MAKEINTRESOURCE(IDB_BITMAP1));
  HIMAGELIST images = ImageList_Create(8, 8, 0, 0, 1);
  ImageList_Add(images, bitmapBulbs, NULL);
  DeleteObject(bitmapBulbs);

  HICON hIcon = ImageList_GetIcon(images, 0, 0);
  ICONINFO iconInfo;
  GetIconInfo(hIcon, &iconInfo);
  bitmapGreen = iconInfo.hbmColor;

  hIcon = ImageList_GetIcon(images, 1, 0);
  GetIconInfo(hIcon, &iconInfo);
  bitmapRed = iconInfo.hbmColor;

  hIcon = ImageList_GetIcon(images, 2, 0);
  GetIconInfo(hIcon, &iconInfo);
  bitmapGray = iconInfo.hbmColor;

  ImageList_Destroy(images);
}

void DestroyHavokResources() {
  DeleteObject(bitmapGreen);
  DeleteObject(bitmapRed);
  DeleteObject(bitmapGray);
}

HavokMax::HavokMax()
    : hWnd(), comboHandle(), currentPresetName("Default"), objectScale(1.0f),
      instanceDialogType(DLGTYPE_unknown), toolset(HK500), captureFrame(),
      motionIndex() {
  corMat.IdentityMatrix();

  Interval aniRange = GetCOREInterface()->GetAnimRange();

  animationStart = aniRange.Start() / GetTicksPerFrame();
  animationEnd = aniRange.End() / GetTicksPerFrame();

  RegisterReflectedTypes<Visible, Checked, hkToolset>();

  ScanPresets();
  LoadCFG();

  auto fndPres = presets.find(currentPresetName);

  if (!es::IsEnd(presets, fndPres)) {
    corMat = fndPres->second.corMat;
    objectScale = fndPres->second.scale;
  }
}

static auto GetConfig() {
  TSTRING cfgpath = IPathConfigMgr::GetPathConfigMgr()->GetDir(APP_PLUGCFG_DIR);
  return cfgpath + _T("/HavokMaxSettings.xml");
}

static void LoadLegacyConfig() {
  TSTRING cfgpath = IPathConfigMgr::GetPathConfigMgr()->GetDir(APP_PLUGCFG_DIR);
  cfgpath += _T("/HavokImpSettings.ini");
  TCHAR legacyGroup[] = _T("HK_PRESETS_HEADER");
  UINT numPresets = GetPrivateProfileInt(legacyGroup, _T("numUserPresets"), -1,
                                         cfgpath.data());

  auto Migrate = [&](auto &&prName_) {
    TSTRING group;
    group.resize(0x102);

    GetPrivateProfileString(prName_.data(), _T("Scale"), _T(""), &group[0],
                            (DWORD)group.size(), cfgpath.data());

    if (!group[0]) {
      return;
    }

    auto &prData = presets[to_string(prName_)];
    prData.scale = std::stof(group);

    GetPrivateProfileString(prName_.data(), _T("Matrix"), _T(""), &group[0],
                            (DWORD)group.size(), cfgpath.data());

    GetCorrectionMatrix(prData.corMat, std::to_string(group));
  };

  Migrate(TSTRING(_T("Default")));

  if (numPresets == -1) {
    return;
  }

  for (size_t p = 0; p < numPresets; p++) {
    TSTRING genName = _T("PresetName");
    genName += ToTSTRING(std::to_string(p));
    TSTRING prName_;
    prName_.resize(0x102);
    auto numReadChars =
        GetPrivateProfileString(legacyGroup, genName.data(), _T(""), &prName_[0],
                                (DWORD)prName_.size(), cfgpath.data());

    prName_.resize(numReadChars);

    if (prName_.empty()) {
      return;
    }

    prName_.erase(0, 2);

    Migrate(prName_);
  }
}

void HavokMax::LoadCFG() {
  pugi::xml_document doc;
  auto conf = GetConfig();

  if (doc.load_file(conf.data())) {
    ReflectorWrap<HavokMax> rWrap(this);
    ReflectorXMLUtil::Load(rWrap, doc);
    LoadPresets(doc);
  } else {
    LoadLegacyConfig();
  }

  // clang-format off
  CheckDlgButton(hWnd, IDC_CH_ANIMATION, checked[Checked::CH_ANIMATION]);
  CheckDlgButton(hWnd, IDC_CH_ANIOPTIMIZE, checked[Checked::CH_ANIOPTIMIZE]);
  CheckDlgButton(hWnd, IDC_CH_ANISKELETON, checked[Checked::CH_ANISKELETON]);
  CheckDlgButton(hWnd, IDC_CH_DISABLE_SCALE, checked[Checked::CH_DISABLE_SCALE]);
  EnableWindow(GetDlgItem(hWnd, IDC_CH_ANIOPTIMIZE), visible[Visible::CH_ANIOPTIMIZE]);
  EnableWindow(GetDlgItem(hWnd, IDC_CH_ANISKELETON), visible[Visible::CH_ANISKELETON]);
  EnableWindow(GetDlgItem(hWnd, IDC_EDIT_ANIEND), visible[Visible::SP_ANIEND]);
  EnableWindow(GetDlgItem(hWnd, IDC_SPIN_ANIEND), visible[Visible::SP_ANIEND]);
  EnableWindow(GetDlgItem(hWnd, IDC_EDIT_ANISTART), visible[Visible::SP_ANISTART]);
  EnableWindow(GetDlgItem(hWnd, IDC_SPIN_ANISTART), visible[Visible::SP_ANISTART]);
  // clang-format on
}

void HavokMax::SaveCFG() {
  pugi::xml_document doc;
  ReflectorWrapConst<HavokMax> rWrap(this);
  ReflectorXMLUtil::Save(rWrap, doc);
  SavePresets(doc);
  auto conf = GetConfig();
  doc.save_file(conf.data());
}

int HavokMax::SavePreset(const std::string &presetName) {
  auto fnd = presets.find(presetName);

  if (!es::IsEnd(presets, fnd)) {
    MessageBox(hWnd,
               _T("Creation of duplicate presets is not allowed.\nPlease ")
               _T("choose a diferent name."),
               _T("Cannot save preset"), MB_ICONSTOP);
    return 1;
  }

  return SavePreset(presets[presetName]);
}

auto GetPreset(HWND hWnd) {
  const int textLen = GetWindowTextLength(hWnd);
  TSTRING wndText;
  wndText.resize(textLen);
  GetWindowText(hWnd, &wndText[0], textLen + 1);
  decltype(auto) wndTextS = std::to_string(wndText);
  return presets.find(wndTextS);
}

void HavokMax::UpdateData() {
  auto cPres = GetPreset(comboHandle);
  SavePreset(cPres->second);
  corMat = cPres->second.corMat;
}

void HavokMax::SetupExportUI() {
  SetWindowText(hWnd, _T("Havok Export V" HavokMax_VERSION));

  HWND comboItem = GetDlgItem(hWnd, IDC_CB_TOOLSET);
  auto ren = GetReflectedEnum<hkToolset>();

  for (auto t : ren) {
    if (t == "HKUNKVER") {
      continue;
    }

    if (t == "HK2015") {
      break;
    }

    t.remove_prefix(2);

    if (t.size() > 3) {
      const size_t fndUnder = t.find('_');
      auto cName = ToTSTRING(t.to_string());

      if (fndUnder != t.npos) {
        cName.replace(fndUnder, 1, 1, '.');
      }

      SendMessage(comboItem, CB_ADDSTRING, 0, (LPARAM)cName.data());
    } else {
      const TCHAR verOld[] = {t[0], '.', t[1], '.', t[2], 0};
      SendMessage(comboItem, CB_ADDSTRING, 0, (LPARAM)verOld);
    }
  }

  SendMessage(comboItem, CB_SETCURSEL, toolset - 1, 0);

  SetupIntSpinner(hWnd, IDC_SPIN_ANIEND, IDC_EDIT_ANIEND, -10000, 10000,
                  animationEnd);
  SetupIntSpinner(hWnd, IDC_SPIN_ANISTART, IDC_EDIT_ANISTART, -10000, 10000,
                  animationStart);
  SetupIntSpinner(hWnd, IDC_SPIN_CAPTUREFRAME, IDC_EDIT_CAPTUREFRAME, -10000,
                  10000, captureFrame);
}

void HavokMax::Setup(HWND hwnd) {
  hWnd = hwnd;
  comboHandle = GetDlgItem(hwnd, IDC_CB_PRESET);
}

INT_PTR CALLBACK NewPresetDLG(HWND hWnd, UINT message, WPARAM wParam, LPARAM) {
  switch (message) {
  case WM_COMMAND:
    switch (LOWORD(wParam)) {
    case IDOK: {
      HWND comboHandle = GetDlgItem(hWnd, IDC_CB_NEWPRESET);
      const int textLen = GetWindowTextLength(comboHandle) + 1;

      if (textLen == 1) {
        EndDialog(hWnd, NULL);
        return TRUE;
      }

      TCHAR *textData = static_cast<TCHAR *>(malloc(sizeof(TCHAR) * textLen));
      GetWindowText(comboHandle, textData, textLen);
      EndDialog(hWnd, reinterpret_cast<INT_PTR>(textData));
      return TRUE;
    }
    case IDCANCEL:
      EndDialog(hWnd, NULL);
      return TRUE;
    }
  }
  return FALSE;
}

INT_PTR CALLBACK DialogCallbacksMain(HWND hWnd, UINT message, WPARAM wParam,
                                     LPARAM lParam) {
  HavokMax *imp =
      reinterpret_cast<HavokMax *>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

  switch (message) {
  case WM_INITDIALOG: {
    CenterWindow(hWnd, GetParent(hWnd));
    SetWindowLongPtr(hWnd, GWLP_USERDATA, lParam);
    imp = reinterpret_cast<HavokMax *>(lParam);
    imp->Setup(hWnd);
    imp->LoadCFG();

    for (auto &p : presets) {
      auto cName = ToTSTRING(p.first);
      SendMessage(imp->comboHandle, CB_ADDSTRING, 0, (LPARAM)cName.data());
    }

    auto cName = ToTSTRING(imp->currentPresetName);
    const std::string defString = "Default";
    LRESULT reslt =
        SendMessage(imp->comboHandle, CB_SELECTSTRING, 0, (LPARAM)cName.data());
    if (reslt < 0) {
      cName = ToTSTRING(defString);
      SendMessage(imp->comboHandle, CB_SELECTSTRING, 0, (LPARAM)cName.c_str());
    }

    decltype(auto) cNameS = reslt < 0 ? defString : imp->currentPresetName;

    imp->UpdatePresetUI(presets.at(cNameS));

    if (imp->instanceDialogType == HavokMax::DLGTYPE_import) {
      SetWindowText(hWnd, _T("Havok Import V" HavokMax_VERSION));
    } else if (imp->instanceDialogType == HavokMax::DLGTYPE_export) {
      imp->SetupExportUI();
    }

    return TRUE;
  }
  case WM_CLOSE:
    imp->SaveCFG();
    EndDialog(hWnd, 0);
    return TRUE;
  case WM_COMMAND:
    switch (LOWORD(wParam)) {
    case IDC_BT_DONE:
      if (!imp->SanityBitcher()) {
        imp->SaveCFG();
        EndDialog(hWnd, 1);
      }
      return TRUE;
    case IDC_BT_ABOUT:
      ShowAboutDLG(hWnd);
      return TRUE;
    case IDC_BT_CANCEL:
      EndDialog(hWnd, 0);
      return TRUE;

    case IDC_BT_SAVEPRESET:
      imp->SaveCFG();
      return TRUE;

    case IDC_BT_ADDPRESET: {
      if (!imp->SanityBitcher()) {
        TCHAR *textData = reinterpret_cast<TCHAR *>(DialogBox(
            hInstance, MAKEINTRESOURCE(IDD_NEWPRESET), hWnd, NewPresetDLG));

        if (textData) {
          std::string presetName = std::to_string(textData);
          if (!imp->SavePreset(presetName)) {
            const LRESULT relt = SendMessage(imp->comboHandle, CB_ADDSTRING, 0,
                                             (LPARAM)textData);
            SendMessage(imp->comboHandle, CB_SETCURSEL, relt, 0);
            auto cPres = GetPreset(imp->comboHandle);
            imp->UpdatePresetUI(cPres->second);
            imp->currentPresetName = cPres->first;
          }

          free(textData);
        }
        imp->SaveCFG();
      }
      return TRUE;
    }
    case IDC_BT_DELETEPRESET: {
      auto cPres = GetPreset(imp->comboHandle);

      if (cPres->first != "Default" && !cPres->second.external) {
        const LRESULT curSel =
            SendMessage(imp->comboHandle, CB_GETCURSEL, 0, 0);
        SendMessage(imp->comboHandle, CB_SETCURSEL, curSel - 1, 0);
        SendMessage(imp->comboHandle, CB_DELETESTRING, curSel, 0);
        auto nPres = GetPreset(imp->comboHandle);
        imp->UpdatePresetUI(nPres->second);
        imp->currentPresetName = nPres->first;
        presets.erase(cPres);
      }
      imp->SaveCFG();
      return TRUE;
    }

    case IDC_CB_PRESET: {
      switch (HIWORD(wParam)) {
      case CBN_SELCHANGE: {
        auto cPres = GetPreset(imp->comboHandle);
        imp->UpdatePresetUI(cPres->second);
        imp->currentPresetName = cPres->first;
        return TRUE;
      } break;
      }
      break;
    }

    case IDC_CB_TOOLSET: {
      switch (HIWORD(wParam)) {
      case CBN_SELCHANGE: {
        const LRESULT curSel = SendMessage((HWND)lParam, CB_GETCURSEL, 0, 0);
        imp->toolset = static_cast<hkToolset>(curSel + 1);
        return TRUE;
      } break;
      }
      break;
    }

    case IDC_CH_ANIMATION: {
      const bool isChecked = IsDlgButtonChecked(hWnd, IDC_CH_ANIMATION) != 0;
      imp->checked.Set(Checked::CH_ANIMATION, isChecked);
      imp->visible.Set(Visible::CH_ANISKELETON, isChecked);
      imp->visible.Set(Visible::SP_ANIEND, isChecked);
      imp->visible.Set(Visible::SP_ANISTART, isChecked);

      EnableWindow(GetDlgItem(hWnd, IDC_CH_ANISKELETON), isChecked);
      EnableWindow(GetDlgItem(hWnd, IDC_EDIT_ANIEND), isChecked);
      EnableWindow(GetDlgItem(hWnd, IDC_EDIT_ANISTART), isChecked);
      EnableWindow(GetDlgItem(hWnd, IDC_SPIN_ANIEND), isChecked);
      EnableWindow(GetDlgItem(hWnd, IDC_SPIN_ANISTART), isChecked);
    }

    case IDC_CH_ANISKELETON: {
      const bool isChecked = IsDlgButtonChecked(hWnd, IDC_CH_ANISKELETON) != 0;
      imp->checked.Set(Checked::CH_ANISKELETON, isChecked);
      imp->visible.Set(Visible::CH_ANIOPTIMIZE,
                       isChecked && imp->visible[Visible::CH_ANISKELETON]);
      EnableWindow(GetDlgItem(hWnd, IDC_CH_ANIOPTIMIZE),
                   imp->visible[Visible::CH_ANIOPTIMIZE]);
      break;
    }

    case IDC_CH_ANIOPTIMIZE:
      imp->checked.Set(Checked::CH_ANIOPTIMIZE,
                       IsDlgButtonChecked(hWnd, IDC_CH_ANIOPTIMIZE) != 0);
      break;

    case IDC_CH_DISABLE_SCALE:
      imp->checked.Set(Checked::CH_DISABLE_SCALE,
                       IsDlgButtonChecked(hWnd, IDC_CH_DISABLE_SCALE) != 0);
      break;

    default:
      return imp ? imp->DlgCommandCallBack(wParam, lParam) : FALSE;
    }

  case CC_SPINNER_CHANGE:
    switch (LOWORD(wParam)) {
    case IDC_SPIN_SCALE:
      imp->objectScale = reinterpret_cast<ISpinnerControl *>(lParam)->GetFVal();
      imp->UpdateData();
      break;
    case IDC_SPIN_CAPTUREFRAME:
      imp->captureFrame =
          reinterpret_cast<ISpinnerControl *>(lParam)->GetIVal();
      break;
    case IDC_SPIN_ANISTART:
      imp->animationStart =
          reinterpret_cast<ISpinnerControl *>(lParam)->GetIVal();
      break;
    case IDC_SPIN_ANIEND:
      imp->animationEnd =
          reinterpret_cast<ISpinnerControl *>(lParam)->GetIVal();
      break;
    case IDC_SPIN_MOTIONID:
      imp->motionIndex = reinterpret_cast<ISpinnerControl *>(lParam)->GetIVal();
      break;
    }
  }
  return (INT_PTR)FALSE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// HavokMaxV1

static const int toEnableItemsV1[] = {
    IDC_RB_XX,   IDC_RB_XY,   IDC_RB_XZ, IDC_RB_YX,     IDC_RB_YY,
    IDC_RB_YZ,   IDC_RB_ZX,   IDC_RB_ZY, IDC_RB_ZZ,     IDC_CH_ROWX,
    IDC_CH_ROWY, IDC_CH_ROWZ, IDC_SPIN,  IDC_SPIN_SCALE};

void HavokMaxV1::UpdatePresetUI(PresetData &data) {
  corMat = data.corMat;
  objectScale = data.scale;

  SetupFloatSpinner(hWnd, IDC_SPIN_SCALE, IDC_EDIT_SCALE, 0, 5000, objectScale);

  if (data.corMat.GetRow(0)[0])
    CheckRadioButton(hWnd, IDC_RB_XX, IDC_RB_XZ, IDC_RB_XX);
  else if (data.corMat.GetRow(0)[1])
    CheckRadioButton(hWnd, IDC_RB_XX, IDC_RB_XZ, IDC_RB_XY);
  else if (data.corMat.GetRow(0)[2])
    CheckRadioButton(hWnd, IDC_RB_XX, IDC_RB_XZ, IDC_RB_XZ);

  if (data.corMat.GetRow(1)[0])
    CheckRadioButton(hWnd, IDC_RB_YZ, IDC_RB_YX, IDC_RB_YX);
  else if (data.corMat.GetRow(1)[1])
    CheckRadioButton(hWnd, IDC_RB_YZ, IDC_RB_YX, IDC_RB_YY);
  else if (data.corMat.GetRow(1)[2])
    CheckRadioButton(hWnd, IDC_RB_YZ, IDC_RB_YX, IDC_RB_YZ);

  if (data.corMat.GetRow(2)[0])
    CheckRadioButton(hWnd, IDC_RB_ZX, IDC_RB_ZZ, IDC_RB_ZX);
  else if (data.corMat.GetRow(2)[1])
    CheckRadioButton(hWnd, IDC_RB_ZX, IDC_RB_ZZ, IDC_RB_ZY);
  else if (data.corMat.GetRow(2)[2])
    CheckRadioButton(hWnd, IDC_RB_ZX, IDC_RB_ZZ, IDC_RB_ZZ);

  CheckDlgButton(hWnd, IDC_CH_ROWX,
                 (data.corMat.GetRow(0)[0] + data.corMat.GetRow(0)[1] +
                  data.corMat.GetRow(0)[2]) < 0);
  CheckDlgButton(hWnd, IDC_CH_ROWY,
                 (data.corMat.GetRow(1)[0] + data.corMat.GetRow(1)[1] +
                  data.corMat.GetRow(1)[2]) < 0);
  CheckDlgButton(hWnd, IDC_CH_ROWZ,
                 (data.corMat.GetRow(2)[0] + data.corMat.GetRow(2)[1] +
                  data.corMat.GetRow(2)[2]) < 0);

  CollisionHandler();

  const int bruh = sizeof(toEnableItemsV1) / 4;

  for (int it = 0; it < bruh; it++)
    EnableWindow(GetDlgItem(hWnd, toEnableItemsV1[it]), !data.external);

  SendDlgItemMessage(hWnd, IDC_PC_INVERT, STM_SETIMAGE, IMAGE_BITMAP,
                     (LPARAM)(!data.external
                                  ? (sanityCheck[0] ? bitmapRed : bitmapGreen)
                                  : bitmapGray));
  SendDlgItemMessage(hWnd, IDC_PC_ROWX, STM_SETIMAGE, IMAGE_BITMAP,
                     (LPARAM)(!data.external
                                  ? (sanityCheck[1] ? bitmapRed : bitmapGreen)
                                  : bitmapGray));
  SendDlgItemMessage(hWnd, IDC_PC_ROWY, STM_SETIMAGE, IMAGE_BITMAP,
                     (LPARAM)(!data.external
                                  ? (sanityCheck[2] ? bitmapRed : bitmapGreen)
                                  : bitmapGray));
  SendDlgItemMessage(hWnd, IDC_PC_ROWZ, STM_SETIMAGE, IMAGE_BITMAP,
                     (LPARAM)(!data.external
                                  ? (sanityCheck[3] ? bitmapRed : bitmapGreen)
                                  : bitmapGray));
}

int HavokMaxV1::SpawnImportDialog() {
  instanceDialogType = DLGTYPE_import;
  return DialogBoxParam(hInstance, MAKEINTRESOURCE(IDD_IMPORT),
                        GetActiveWindow(), DialogCallbacksMain,
                        reinterpret_cast<LPARAM>(this));
}

int HavokMaxV1::SpawnExportDialog() {
  instanceDialogType = DLGTYPE_export;
  return DialogBoxParam(hInstance, MAKEINTRESOURCE(IDD_EXPORT),
                        GetActiveWindow(), DialogCallbacksMain,
                        reinterpret_cast<LPARAM>(this));
}

void HavokMaxV1::CollisionHandler() {
  if ((IsDlgButtonChecked(hWnd, IDC_RB_XX) &&
       (IsDlgButtonChecked(hWnd, IDC_RB_YX) ||
        IsDlgButtonChecked(hWnd, IDC_RB_ZX))) ||
      (IsDlgButtonChecked(hWnd, IDC_RB_XY) &&
       (IsDlgButtonChecked(hWnd, IDC_RB_YY) ||
        IsDlgButtonChecked(hWnd, IDC_RB_ZY))) ||
      (IsDlgButtonChecked(hWnd, IDC_RB_XZ) &&
       (IsDlgButtonChecked(hWnd, IDC_RB_YZ) ||
        IsDlgButtonChecked(hWnd, IDC_RB_ZZ)))) {
    SendDlgItemMessage(hWnd, IDC_PC_ROWX, STM_SETIMAGE, IMAGE_BITMAP,
                       (LPARAM)bitmapRed);
    sanityCheck += 1;
  } else {
    SendDlgItemMessage(hWnd, IDC_PC_ROWX, STM_SETIMAGE, IMAGE_BITMAP,
                       (LPARAM)bitmapGreen);
    sanityCheck -= 1;
  }

  if ((IsDlgButtonChecked(hWnd, IDC_RB_YX) &&
       (IsDlgButtonChecked(hWnd, IDC_RB_XX) ||
        IsDlgButtonChecked(hWnd, IDC_RB_ZX))) ||
      (IsDlgButtonChecked(hWnd, IDC_RB_YY) &&
       (IsDlgButtonChecked(hWnd, IDC_RB_XY) ||
        IsDlgButtonChecked(hWnd, IDC_RB_ZY))) ||
      (IsDlgButtonChecked(hWnd, IDC_RB_YZ) &&
       (IsDlgButtonChecked(hWnd, IDC_RB_XZ) ||
        IsDlgButtonChecked(hWnd, IDC_RB_ZZ)))) {
    SendDlgItemMessage(hWnd, IDC_PC_ROWY, STM_SETIMAGE, IMAGE_BITMAP,
                       (LPARAM)bitmapRed);
    sanityCheck += 2;
  } else {
    SendDlgItemMessage(hWnd, IDC_PC_ROWY, STM_SETIMAGE, IMAGE_BITMAP,
                       (LPARAM)bitmapGreen);
    sanityCheck -= 2;
  }

  if ((IsDlgButtonChecked(hWnd, IDC_RB_ZX) &&
       (IsDlgButtonChecked(hWnd, IDC_RB_YX) ||
        IsDlgButtonChecked(hWnd, IDC_RB_XX))) ||
      (IsDlgButtonChecked(hWnd, IDC_RB_ZY) &&
       (IsDlgButtonChecked(hWnd, IDC_RB_YY) ||
        IsDlgButtonChecked(hWnd, IDC_RB_XY))) ||
      (IsDlgButtonChecked(hWnd, IDC_RB_ZZ) &&
       (IsDlgButtonChecked(hWnd, IDC_RB_YZ) ||
        IsDlgButtonChecked(hWnd, IDC_RB_XZ)))) {
    SendDlgItemMessage(hWnd, IDC_PC_ROWZ, STM_SETIMAGE, IMAGE_BITMAP,
                       (LPARAM)bitmapRed);
    sanityCheck += 3;
  } else {
    SendDlgItemMessage(hWnd, IDC_PC_ROWZ, STM_SETIMAGE, IMAGE_BITMAP,
                       (LPARAM)bitmapGreen);
    sanityCheck -= 3;
  }

  const bool checkedStatus = IsDlgButtonChecked(hWnd, IDC_CH_ROWX) &&
                             IsDlgButtonChecked(hWnd, IDC_CH_ROWY) &&
                             IsDlgButtonChecked(hWnd, IDC_CH_ROWZ);
  sanityCheck.Set(0, checkedStatus);
  SendDlgItemMessage(hWnd, IDC_PC_INVERT, STM_SETIMAGE, IMAGE_BITMAP,
                     (LPARAM)(checkedStatus ? bitmapRed : bitmapGreen));
}

int HavokMaxV1::SanityBitcher() {
  const int insane =
      reinterpret_cast<decltype(sanityCheck)::ValueType &>(sanityCheck);

  if (!insane)
    return insane;

  TSTRING reslt = _T("Invalid matrix parameters.\n");

  if (sanityCheck[1] || sanityCheck[2] || sanityCheck[3]) {
    reslt.append(_T("Following rows are colliding: "));

    if (sanityCheck[1])
      reslt.append(_T("X, "));

    if (sanityCheck[2])
      reslt.append(_T("Y, "));

    if (sanityCheck[3])
      reslt.append(_T("Z, "));

    reslt.pop_back();
    reslt.pop_back();
    reslt.push_back('\n');
  }

  if (sanityCheck[0])
    reslt.append(_T("All rows are inverted!\n"));

  MessageBox(hWnd, reslt.c_str(), _T("Invalid matrix settings!"), MB_ICONSTOP);

  return insane;
}

INT_PTR HavokMaxV1::DlgCommandCallBack(WPARAM wParam, LPARAM lParam) {
  switch (LOWORD(wParam)) {
  case IDC_RB_XX:
  case IDC_RB_YX:
  case IDC_RB_ZX:
  case IDC_RB_XY:
  case IDC_RB_YY:
  case IDC_RB_ZY:
  case IDC_RB_XZ:
  case IDC_RB_YZ:
  case IDC_RB_ZZ:
  case IDC_CH_ROWX:
  case IDC_CH_ROWY:
  case IDC_CH_ROWZ: {
    CollisionHandler();
    UpdateData();
    return TRUE;
  }
  }
  return (INT_PTR)FALSE;
}

int HavokMaxV1::SavePreset(PresetData &data) {
  if (data.external) {
    MessageBox(hWnd, _T("Cannot save into external preset!"),
               _T("Cannot save preset"), MB_ICONSTOP);
    return 1;
  }

  data.scale = objectScale;

  float sign = IsDlgButtonChecked(hWnd, IDC_CH_ROWX) ? -1.0f : 1.0f;

  data.corMat.SetRow(0,
                     Point3(IsDlgButtonChecked(hWnd, IDC_RB_XX) ? sign : 0.0f,
                            IsDlgButtonChecked(hWnd, IDC_RB_XY) ? sign : 0.0f,
                            IsDlgButtonChecked(hWnd, IDC_RB_XZ) ? sign : 0.0f));

  sign = IsDlgButtonChecked(hWnd, IDC_CH_ROWY) ? -1.0f : 1.0f;

  data.corMat.SetRow(1,
                     Point3(IsDlgButtonChecked(hWnd, IDC_RB_YX) ? sign : 0.0f,
                            IsDlgButtonChecked(hWnd, IDC_RB_YY) ? sign : 0.0f,
                            IsDlgButtonChecked(hWnd, IDC_RB_YZ) ? sign : 0.0f));

  sign = IsDlgButtonChecked(hWnd, IDC_CH_ROWZ) ? -1.0f : 1.0f;

  data.corMat.SetRow(2,
                     Point3(IsDlgButtonChecked(hWnd, IDC_RB_ZX) ? sign : 0.0f,
                            IsDlgButtonChecked(hWnd, IDC_RB_ZY) ? sign : 0.0f,
                            IsDlgButtonChecked(hWnd, IDC_RB_ZZ) ? sign : 0.0f));

  return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// HavokMaxV2

int HavokMaxV2::SanityBitcher() {
  const int insane =
      reinterpret_cast<decltype(sanityCheck)::ValueType &>(sanityCheck);

  if (!insane)
    return insane;

  TSTRING reslt;

  if (sanityCheck[1])
    reslt.append(_T("Back and Right axes cannot have same values!\n"));

  if (sanityCheck[0])
    reslt.append(_T("All axes are inverted!\n"));

  MessageBox(hWnd, reslt.c_str(), _T("Invalid coordsys settings!"),
             MB_ICONSTOP);

  return insane;
}

void HavokMaxV2::CollisionHandler() {
  const LRESULT backSel = SendMessage(comboBack, CB_GETCURSEL, 0, 0);
  const LRESULT rightSel = SendMessage(comboRight, CB_GETCURSEL, 0, 0);

  const bool checkedStatus = IsDlgButtonChecked(hWnd, IDC_CH_INVERT_BACK) &&
                             IsDlgButtonChecked(hWnd, IDC_CH_INVERT_RIGHT) &&
                             IsDlgButtonChecked(hWnd, IDC_CH_INVERT_TOP);
  sanityCheck.Set(0, checkedStatus);

  ShowWindow(GetDlgItem(hWnd, IDC_PC_INVERT_ERROR),
             checkedStatus ? SW_SHOW : SW_HIDE);

  const bool colliding = backSel == rightSel;
  sanityCheck.Set(1, colliding);

  ShowWindow(GetDlgItem(hWnd, IDC_PC_REMAP_ERROR1),
             colliding ? SW_SHOW : SW_HIDE);
  ShowWindow(GetDlgItem(hWnd, IDC_PC_REMAP_ERROR2),
             colliding ? SW_SHOW : SW_HIDE);
}

INT_PTR HavokMaxV2::DlgCommandCallBack(WPARAM wParam, LPARAM lParam) {
  switch (LOWORD(wParam)) {
  case IDC_CB_BACK:
  case IDC_CB_RIGHT:
  case IDC_CH_INVERT_TOP:
  case IDC_CH_INVERT_RIGHT:
  case IDC_CH_INVERT_BACK: {
    CollisionHandler();
    UpdateData();
    return TRUE;
  }
  }
  return (INT_PTR)FALSE;
}

int HavokMaxV2::SpawnImportDialog() {
  instanceDialogType = DLGTYPE_import;
  return DialogBoxParam(hInstance, MAKEINTRESOURCE(IDD_IMPORT_NEW),
                        GetActiveWindow(), DialogCallbacksMain,
                        reinterpret_cast<LPARAM>(this));
}

int HavokMaxV2::SpawnExportDialog() {
  instanceDialogType = DLGTYPE_export;
  return DialogBoxParam(hInstance, MAKEINTRESOURCE(IDD_EXPORT_NEW),
                        GetActiveWindow(), DialogCallbacksMain,
                        reinterpret_cast<LPARAM>(this));
}

void HavokMaxV2::Setup(HWND hwnd) {
  HavokMax::Setup(hwnd);
  comboBack = GetDlgItem(hWnd, IDC_CB_BACK);
  comboRight = GetDlgItem(hWnd, IDC_CB_RIGHT);

  static const TCHAR *_items[] = {_T("Right"), _T("Back"), _T("Top")};

  for (int c = 0; c < 3; c++) {
    SendMessage(comboBack, CB_ADDSTRING, 0, (LPARAM)_items[c]);
    SendMessage(comboRight, CB_ADDSTRING, 0, (LPARAM)_items[c]);
  }
}

static const int toEnableItemsV2[] = {
    IDC_CB_BACK,        IDC_CB_RIGHT, IDC_CH_INVERT_TOP, IDC_CH_INVERT_RIGHT,
    IDC_CH_INVERT_BACK, IDC_SPIN,     IDC_SPIN_SCALE};

void HavokMaxV2::UpdatePresetUI(PresetData &data) {
  corMat = data.corMat;
  objectScale = data.scale;

  if (numAnimations && motionIndex < 0) {
    motionIndex = 0;
  }

  SetupFloatSpinner(hWnd, IDC_SPIN_SCALE, IDC_EDIT_SCALE, 0, 5000, objectScale);
  SetupIntSpinner(hWnd, IDC_SPIN_MOTIONID, IDC_EDIT_MOTIONID, 0,
                  numAnimations - 1, motionIndex);

  int curIndex = 0;

  if (data.corMat.GetRow(0)[1])
    curIndex = 1;
  else if (data.corMat.GetRow(0)[2])
    curIndex = 2;

  SendMessage(comboRight, CB_SETCURSEL, curIndex, 0);
  curIndex = 0;

  if (data.corMat.GetRow(1)[1])
    curIndex = 1;
  else if (data.corMat.GetRow(1)[2])
    curIndex = 2;

  SendMessage(comboBack, CB_SETCURSEL, curIndex, 0);

  CheckDlgButton(hWnd, IDC_CH_INVERT_RIGHT,
                 (data.corMat.GetRow(0)[0] + data.corMat.GetRow(0)[1] +
                  data.corMat.GetRow(0)[2]) < 0);
  CheckDlgButton(hWnd, IDC_CH_INVERT_BACK,
                 (data.corMat.GetRow(1)[0] + data.corMat.GetRow(1)[1] +
                  data.corMat.GetRow(1)[2]) < 0);
  CheckDlgButton(hWnd, IDC_CH_INVERT_TOP,
                 (data.corMat.GetRow(2)[0] + data.corMat.GetRow(2)[1] +
                  data.corMat.GetRow(2)[2]) < 0);

  CollisionHandler();

  const int bruh = sizeof(toEnableItemsV2) / 4;

  for (int it = 0; it < bruh; it++)
    EnableWindow(GetDlgItem(hWnd, toEnableItemsV2[it]), !data.external);
}

int HavokMaxV2::SavePreset(PresetData &data) {
  if (data.external) {
    MessageBox(hWnd, _T("Cannot save into external preset!"),
               _T("Cannot save preset"), MB_ICONSTOP);
    return 1;
  }

  data.scale = objectScale;

  float sign = IsDlgButtonChecked(hWnd, IDC_CH_INVERT_RIGHT) ? -1.0f : 1.0f;
  Point3 value = {};
  int XAxisSelection =
      static_cast<int>(SendMessage(comboRight, CB_GETCURSEL, 0, 0));

  value[XAxisSelection] = sign;
  data.corMat.SetRow(0, value);
  value[XAxisSelection] = 0.0f;

  sign = IsDlgButtonChecked(hWnd, IDC_CH_INVERT_BACK) ? -1.0f : 1.0f;

  int YAxisSelection =
      static_cast<int>(SendMessage(comboBack, CB_GETCURSEL, 0, 0));

  value[YAxisSelection] = sign;
  data.corMat.SetRow(1, value);
  value[YAxisSelection] = 0.0f;

  sign = IsDlgButtonChecked(hWnd, IDC_CH_INVERT_TOP) ? -1.0f : 1.0f;

  int ZAxisSelection = 3 - (YAxisSelection + XAxisSelection);

  value[ZAxisSelection] = sign;
  data.corMat.SetRow(2, value);

  return 0;
}
