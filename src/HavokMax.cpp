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
#include "resource.h"
#include <commctrl.h>
#include <IPathConfigMgr.h>
#include <3dsmaxport.h>
#include <iparamm2.h>
#include "datas/DirectoryScanner.hpp"
#include <array>

static const std::array<const TCHAR *, 9> toolsetNames =
{
	_T("5.5.0"),
	_T("6.6.0"),
	_T("7.1.0"),
	_T("2010.2"),
	_T("2011.1"),
	_T("2011.2"),
	_T("2012.2"),
	_T("2013.1"),
	_T("2014.1"),
};

struct PresetData
{
	TSTRING name;
	float scale;
	bool external;
	Matrix3 corMat;
	PresetData() : scale(1.0f), name(_T("Default")), external(false) { corMat.IdentityMatrix(); }
	bool operator==(const PresetData &input) const { return !name.compare(input.name); }
	bool operator==(const TSTRING &input) const { return !name.compare(input); }
};

extern HINSTANCE hInstance;
static const TCHAR *categoryName = _T("HK_PRESET");
static PresetData defaultPreset;
std::vector<TSTRING> extensions = { _T("hkx"), _T("hkt") };
static std::vector<PresetData*> presets = { &defaultPreset };
static HBITMAP bitmapGreen,
	bitmapRed,
	bitmapGray;

const TCHAR _name[] = _T("Havok Tool");
const TCHAR _info[] = _T("\nCopyright (C) 2016-2019 Lukas Cone\nVersion 1");
const TCHAR _license[] = _T("Havok Tool uses HavokLib, Copyright(C) 2016-2019 Lukas Cone.");
#pragma message("ADD LINK")
const TCHAR _homePage[] = _T("https://lukascone.wordpress.com/2019/02/22/apex-engine-plugin");

#include "MAXex/win/AboutDlg.h"

void AddExtension(TSTRING &ext)
{
	for (auto &e : extensions)
		if (e == ext)
			return;

	extensions.push_back(ext);
}

void Rescan()
{
	TSTRING CFGPath = IPathConfigMgr::GetPathConfigMgr()->GetDir(APP_PLUGCFG_DIR);

	DirectoryScanner scanner;
	scanner.AddFilter(_T(".ini"));
	scanner.Scan(CFGPath);

	TCHAR buffer[CFGBufferSize];

	for (auto &cfg : scanner)
	{
		TSTRING extBuff;

		GetText(categoryName, extBuff, cfg.c_str(), buffer, _T("Name"));

		if (!extBuff.size())
			continue;

		PresetData *data = nullptr;

		for (auto &p : presets)
			if (*p == extBuff)
			{
				data = p;
				break;
			}

		if (!data)
		{
			data = new PresetData();
			data->name = extBuff;
			data->external = true;
			presets.push_back(data);
		}

		GetValue(categoryName, data->scale, cfg.c_str(), buffer, _T("Scale"));
		GetCorrectionMatrix(data->corMat, cfg.c_str(), buffer);
		extBuff.clear();
		GetText(categoryName, extBuff, cfg.c_str(), buffer, _T("Extensions"));

		if (extBuff.size())
		{
			size_t foundSeparator = extBuff.find('|');
			size_t lastSeparator = 0;

			if (foundSeparator == TSTRING::npos)
			{
				AddExtension(extBuff);
			}
			else
			{
				do
				{
					TSTRING cExt = extBuff.substr(lastSeparator, foundSeparator - lastSeparator);
					AddExtension(cExt);
					lastSeparator = ++foundSeparator;
				} while ((foundSeparator = extBuff.find('|', foundSeparator)) != TSTRING::npos);

				TSTRING cExt = extBuff.substr(lastSeparator);
				AddExtension(cExt);
			}
		}
	}
}

void BuildHavokResources()
{
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

void DestroyHavokResources()
{
	for (auto &f : presets)
		if (f != &defaultPreset)
			delete f;

	DeleteObject(bitmapGreen);
	DeleteObject(bitmapRed);
	DeleteObject(bitmapGray);
}

HavokMax::HavokMax() :
	CFGFile(nullptr), hWnd(nullptr), comboHandle(nullptr), currentPresetName(_T("Default")),
	IDConfigValue(IDC_EDIT_SCALE)(1.0f), instanceDialogType(DLGTYPE_unknown), IDConfigIndex(IDC_CB_TOOLSET)(0) { corMat.IdentityMatrix(); }

void HavokMax::BuildCFG()
{
	cfgpath = IPathConfigMgr::GetPathConfigMgr()->GetDir(APP_PLUGCFG_DIR);
	cfgpath.append(_T("\\HavokImpSettings.ini"));
	CFGFile = cfgpath.c_str();
}
static const TCHAR *hkPresetHeader = _T("HK_PRESETS_HEADER");

void HavokMax::LoadCFG()
{
	BuildCFG();
	TCHAR buffer[CFGBufferSize];
	int numUserPresets = 0;

	GetText(hkPresetHeader, currentPresetName, CFGFile, buffer, _T("currentPresetName"));
	GetIndex(hkPresetHeader, numUserPresets, CFGFile, buffer, _T("numUserPresets"));

	for (int p = 0; p < numUserPresets; p++)
	{
		TSTRING prStr = _T("PresetName");
		prStr.append(ToTSTRING(p));
		
		PresetData *data = nullptr;
		TSTRING presetName;
		bool pushNew = false;

		GetText(hkPresetHeader, presetName, CFGFile, buffer, prStr.c_str());
		
		for (auto &p : presets)
			if (!p->name.compare(presetName))
				data = p;
				
		if (!data)
		{
			data = new PresetData();
			data->name = presetName;
			pushNew = true;
		}
		
		GetValue(data->name.c_str(), data->scale, CFGFile, buffer, _T("Scale"));
		GetCorrectionMatrix(data->corMat, CFGFile, buffer, data->name.c_str());
		
		if (pushNew)
			presets.push_back(data);
	}

	GetCFGIndex(IDC_CB_TOOLSET);
}

void HavokMax::SaveCFG()
{
	BuildCFG();
	TCHAR buffer[CFGBufferSize];
	const LRESULT curSel = SendMessage(comboHandle, CB_GETCURSEL, 0, 0);
	int numUserPresets = 0;

	currentPresetName = presets[curSel]->name.c_str();
	WriteText(hkPresetHeader, currentPresetName, CFGFile, _T("currentPresetName"));

	for (auto &p : presets)
		if (!p->external)
		{
			if (p != &defaultPreset)
			{
				TSTRING prStr = _T("PresetName");
				prStr.append(ToTSTRING(numUserPresets));
				numUserPresets++;
				WriteText(hkPresetHeader, p->name, CFGFile, prStr.c_str());
			}

			WriteValue(p->name.c_str(), p->scale, CFGFile, buffer, _T("Scale"));
			WriteCorrectionMatrix(p->corMat, CFGFile, buffer, p->name.c_str());
		}

	WriteIndex(hkPresetHeader, numUserPresets, CFGFile, buffer, _T("numUserPresets"));

	SetCFGIndex(IDC_CB_TOOLSET);
}

void HavokMax::CollisionHandler()
{
	if (
		(IsDlgButtonChecked(hWnd, IDC_RB_XX) && (IsDlgButtonChecked(hWnd, IDC_RB_YX) || IsDlgButtonChecked(hWnd, IDC_RB_ZX))) ||
		(IsDlgButtonChecked(hWnd, IDC_RB_XY) && (IsDlgButtonChecked(hWnd, IDC_RB_YY) || IsDlgButtonChecked(hWnd, IDC_RB_ZY))) ||
		(IsDlgButtonChecked(hWnd, IDC_RB_XZ) && (IsDlgButtonChecked(hWnd, IDC_RB_YZ) || IsDlgButtonChecked(hWnd, IDC_RB_ZZ)))
		)
	{
		SendDlgItemMessage(hWnd, IDC_PC_ROWX, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)bitmapRed);
		sanityCheck(1, true);
	}
	else
	{
		SendDlgItemMessage(hWnd, IDC_PC_ROWX, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)bitmapGreen);
		sanityCheck(1, false);
	}

	if (
		(IsDlgButtonChecked(hWnd, IDC_RB_YX) && (IsDlgButtonChecked(hWnd, IDC_RB_XX) || IsDlgButtonChecked(hWnd, IDC_RB_ZX))) ||
		(IsDlgButtonChecked(hWnd, IDC_RB_YY) && (IsDlgButtonChecked(hWnd, IDC_RB_XY) || IsDlgButtonChecked(hWnd, IDC_RB_ZY))) ||
		(IsDlgButtonChecked(hWnd, IDC_RB_YZ) && (IsDlgButtonChecked(hWnd, IDC_RB_XZ) || IsDlgButtonChecked(hWnd, IDC_RB_ZZ)))
		)
	{
		SendDlgItemMessage(hWnd, IDC_PC_ROWY, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)bitmapRed);
		sanityCheck(2, true);
	}
	else
	{
		SendDlgItemMessage(hWnd, IDC_PC_ROWY, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)bitmapGreen);
		sanityCheck(2, false);
	}

	if (
		(IsDlgButtonChecked(hWnd, IDC_RB_ZX) && (IsDlgButtonChecked(hWnd, IDC_RB_YX) || IsDlgButtonChecked(hWnd, IDC_RB_XX))) ||
		(IsDlgButtonChecked(hWnd, IDC_RB_ZY) && (IsDlgButtonChecked(hWnd, IDC_RB_YY) || IsDlgButtonChecked(hWnd, IDC_RB_XY))) ||
		(IsDlgButtonChecked(hWnd, IDC_RB_ZZ) && (IsDlgButtonChecked(hWnd, IDC_RB_YZ) || IsDlgButtonChecked(hWnd, IDC_RB_XZ)))
		)
	{
		SendDlgItemMessage(hWnd, IDC_PC_ROWZ, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)bitmapRed);
		sanityCheck(3, true);
	}
	else
	{
		SendDlgItemMessage(hWnd, IDC_PC_ROWZ, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)bitmapGreen);
		sanityCheck(3, false);
	}

	const bool checkedStatus = IsDlgButtonChecked(hWnd, IDC_CH_ROWX) && IsDlgButtonChecked(hWnd, IDC_CH_ROWY) && IsDlgButtonChecked(hWnd, IDC_CH_ROWZ);
	sanityCheck(0, checkedStatus);
	SendDlgItemMessage(hWnd, IDC_PC_INVERT, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)(checkedStatus ? bitmapRed : bitmapGreen));
}

int HavokMax::SanityBitcher()
{
	const int insane = reinterpret_cast<decltype(sanityCheck)::ValueType&>(sanityCheck);

	if (!insane)
		return insane;

	TSTRING reslt = _T("Invalid matrix parameters.\n");

	if (sanityCheck[1] || sanityCheck[2] || sanityCheck[3])
	{
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

int HavokMax::SavePreset(const TCHAR* presetName)
{
	PresetData *data = nullptr;

	for (auto &p : presets)
		if (*p == presetName)
		{
			data = p;
			break;
		}

	if (data)
	{
		MessageBox(hWnd, _T("Creation of duplicate presets is not allowed.\nPlease choose a diferent name."), _T("Cannot save preset"), MB_ICONSTOP);
		return 1;
	}

	if (!data)
	{
		data = new PresetData();
		data->name = presetName;
		presets.push_back(data);
	}

	return SavePreset(data);
}

int HavokMax::SavePreset(PresetData* data)
{
	if (data->external)
	{
		MessageBox(hWnd, _T("Cannot save into external preset!"), _T("Cannot save preset"), MB_ICONSTOP);
		return 1;
	}

	data->scale = IDC_EDIT_SCALE_value;

	float sign = IsDlgButtonChecked(hWnd, IDC_CH_ROWX) ? -1.0f : 1.0f;

	data->corMat.SetRow(0, Point3(
		IsDlgButtonChecked(hWnd, IDC_RB_XX) ? sign : 0.0f,
		IsDlgButtonChecked(hWnd, IDC_RB_XY) ? sign : 0.0f,
		IsDlgButtonChecked(hWnd, IDC_RB_XZ) ? sign : 0.0f
	));

	sign = IsDlgButtonChecked(hWnd, IDC_CH_ROWY) ? -1.0f : 1.0f;

	data->corMat.SetRow(1, Point3(
		IsDlgButtonChecked(hWnd, IDC_RB_YX) ? sign : 0.0f,
		IsDlgButtonChecked(hWnd, IDC_RB_YY) ? sign : 0.0f,
		IsDlgButtonChecked(hWnd, IDC_RB_YZ) ? sign : 0.0f
	));

	sign = IsDlgButtonChecked(hWnd, IDC_CH_ROWZ) ? -1.0f : 1.0f;

	data->corMat.SetRow(2, Point3(
		IsDlgButtonChecked(hWnd, IDC_RB_ZX) ? sign : 0.0f,
		IsDlgButtonChecked(hWnd, IDC_RB_ZY) ? sign : 0.0f,
		IsDlgButtonChecked(hWnd, IDC_RB_ZZ) ? sign : 0.0f
	));

	return 0;
}

static const int toEnableItems[] = {
	IDC_RB_XX, IDC_RB_XY, IDC_RB_XZ, IDC_RB_YX, IDC_RB_YY, IDC_RB_YZ, IDC_RB_ZX, IDC_RB_ZY,
	IDC_RB_ZZ, IDC_CH_ROWX, IDC_CH_ROWY, IDC_CH_ROWZ, IDC_SPIN, IDC_SPIN_SCALE };

void HavokMax::UpdatePresetUI(PresetData* data)
{
	corMat = data->corMat;
	IDC_EDIT_SCALE_value = data->scale;

	SetupFloatSpinner(hWnd, IDC_SPIN_SCALE, IDC_EDIT_SCALE, 0, 5000, IDC_EDIT_SCALE_value);

	if (data->corMat.GetRow(0)[0])
		CheckRadioButton(hWnd, IDC_RB_XX, IDC_RB_XZ, IDC_RB_XX);
	else if (data->corMat.GetRow(0)[1])
		CheckRadioButton(hWnd, IDC_RB_XX, IDC_RB_XZ, IDC_RB_XY);
	else if (data->corMat.GetRow(0)[2])
		CheckRadioButton(hWnd, IDC_RB_XX, IDC_RB_XZ, IDC_RB_XZ);

	if (data->corMat.GetRow(1)[0])
		CheckRadioButton(hWnd, IDC_RB_YZ, IDC_RB_YX, IDC_RB_YX);
	else if (data->corMat.GetRow(1)[1])
		CheckRadioButton(hWnd, IDC_RB_YZ, IDC_RB_YX, IDC_RB_YY);
	else if (data->corMat.GetRow(1)[2])
		CheckRadioButton(hWnd, IDC_RB_YZ, IDC_RB_YX, IDC_RB_YZ);

	if (data->corMat.GetRow(2)[0])
		CheckRadioButton(hWnd, IDC_RB_ZX, IDC_RB_ZZ, IDC_RB_ZX);
	else if (data->corMat.GetRow(2)[1])
		CheckRadioButton(hWnd, IDC_RB_ZX, IDC_RB_ZZ, IDC_RB_ZY);
	else if (data->corMat.GetRow(2)[2])
		CheckRadioButton(hWnd, IDC_RB_ZX, IDC_RB_ZZ, IDC_RB_ZZ);

	CheckDlgButton(hWnd, IDC_CH_ROWX, (data->corMat.GetRow(0)[0] + data->corMat.GetRow(0)[1] + data->corMat.GetRow(0)[2]) < 0);
	CheckDlgButton(hWnd, IDC_CH_ROWY, (data->corMat.GetRow(1)[0] + data->corMat.GetRow(1)[1] + data->corMat.GetRow(1)[2]) < 0);
	CheckDlgButton(hWnd, IDC_CH_ROWZ, (data->corMat.GetRow(2)[0] + data->corMat.GetRow(2)[1] + data->corMat.GetRow(2)[2]) < 0);

	CollisionHandler();

	const int bruh = sizeof(toEnableItems) / 4;

	for (int it = 0; it < bruh; it++)
		EnableWindow(GetDlgItem(hWnd, toEnableItems[it]), !data->external);

	SendDlgItemMessage(hWnd, IDC_PC_INVERT, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)(!data->external ? (sanityCheck[0] ? bitmapRed : bitmapGreen) : bitmapGray));
	SendDlgItemMessage(hWnd, IDC_PC_ROWX, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)(!data->external ? (sanityCheck[1] ? bitmapRed : bitmapGreen) : bitmapGray));
	SendDlgItemMessage(hWnd, IDC_PC_ROWY, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)(!data->external ? (sanityCheck[2] ? bitmapRed : bitmapGreen) : bitmapGray));
	SendDlgItemMessage(hWnd, IDC_PC_ROWZ, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)(!data->external ? (sanityCheck[3] ? bitmapRed : bitmapGreen) : bitmapGray));
}

void HavokMax::UpdateData()
{
	const LRESULT curSel = SendMessage(comboHandle, CB_GETCURSEL, 0, 0);

	SavePreset(presets[curSel]);

	corMat = presets[curSel]->corMat;
}

INT_PTR CALLBACK NewPresetDLG(HWND hWnd, UINT message, WPARAM wParam, LPARAM)
{
	switch (message)
	{
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDOK:
		{
			HWND comboHandle = GetDlgItem(hWnd, IDC_CB_NEWPRESET);
			const int textLen = GetWindowTextLength(comboHandle) + 1;

			if (textLen == 1)
			{
				EndDialog(hWnd, NULL);
				return TRUE;
			}

			TCHAR *textData = static_cast<TCHAR*>(malloc(sizeof(TCHAR) * textLen));
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

INT_PTR CALLBACK DialogCallbacks(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	HavokMax *imp = reinterpret_cast<HavokMax*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

	switch (message)
	{
	case WM_INITDIALOG:
	{
		CenterWindow(hWnd, GetParent(hWnd));
		SetWindowLongPtr(hWnd, GWLP_USERDATA, lParam);
		imp = reinterpret_cast<HavokMax*>(lParam);
		imp->hWnd = hWnd;
		imp->comboHandle = GetDlgItem(hWnd, IDC_CB_PRESET);
		imp->LoadCFG();
		SetupFloatSpinner(hWnd, IDC_SPIN_SCALE, IDC_EDIT_SCALE, 0, 5000, imp->IDC_EDIT_SCALE_value);

		for (auto &p : presets)
			SendMessage(imp->comboHandle, CB_ADDSTRING, 0, (LPARAM)p->name.c_str());

		LRESULT reslt = SendMessage(imp->comboHandle, CB_SELECTSTRING, 0, (LPARAM)imp->currentPresetName.c_str());
		SendMessage(imp->comboHandle, CB_SETCURSEL, reslt > 0 ? reslt : 0, 0);

		imp->UpdatePresetUI(presets[reslt]);

		if (imp->instanceDialogType == HavokMax::DLGTYPE_import)
			SetWindowText(hWnd, _T("Havok Import v1"));
		else if (imp->instanceDialogType == HavokMax::DLGTYPE_export)
		{
			SetWindowText(hWnd, _T("Havok Export v1"));

			HWND comboItem = GetDlgItem(hWnd, IDC_CB_TOOLSET);

			for (auto &t : toolsetNames)
				SendMessage(comboItem, CB_ADDSTRING, 0, (LPARAM)t);

			SendMessage(comboItem, CB_SETCURSEL, imp->IDC_CB_TOOLSET_index, 0);

		}

		return TRUE;
	}
	case WM_CLOSE:
		imp->SaveCFG();
		EndDialog(hWnd, 0);
		return TRUE;
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDC_BT_DONE:
			if (!imp->SanityBitcher())
			{
				imp->SaveCFG();
				EndDialog(hWnd, 1);
				return TRUE;
			}
		case IDC_BT_ABOUT:
			ShowAboutDLG(hWnd);
			return TRUE;
		case IDC_BT_CANCEL:
			EndDialog(hWnd, 0);
			return TRUE;
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
		case IDC_CH_ROWZ:
		{
			imp->CollisionHandler();
			imp->UpdateData();
			return TRUE;
		}

		case IDC_BT_SAVEPRESET:
			imp->SaveCFG();
			return TRUE;

		case IDC_BT_ADDPRESET:
		{
			if (!imp->SanityBitcher())
			{
				TCHAR *textData = reinterpret_cast<TCHAR*>(DialogBox(hInstance, MAKEINTRESOURCE(IDD_NEWPRESET), hWnd, NewPresetDLG));

				if (textData)
				{
					if (!imp->SavePreset(textData))
					{
						const LRESULT relt = SendMessage(imp->comboHandle, CB_ADDSTRING, 0, (LPARAM)textData);
						SendMessage(imp->comboHandle, CB_SETCURSEL, relt, 0);
						imp->UpdatePresetUI(presets[relt]);
					}

					free(textData);
				}
				imp->SaveCFG();
			}
			return TRUE;
		}
		case IDC_BT_DELETEPRESET:
		{
			const LRESULT curSel = SendMessage(imp->comboHandle, CB_GETCURSEL, 0, 0);

			if (curSel > 0 && !presets[curSel]->external)
			{
				SendMessage(imp->comboHandle, CB_DELETESTRING, curSel, 0);
				SendMessage(imp->comboHandle, CB_SETCURSEL, curSel - 1, 0);
				imp->UpdatePresetUI(presets[curSel - 1]);

				WritePrivateProfileString(presets[curSel]->name.c_str(), NULL, NULL, imp->CFGFile);
				delete presets[curSel];
				presets.erase(presets.begin() + curSel);
			}
			imp->SaveCFG();
			return TRUE;
		}

		case IDC_CB_PRESET:
		{
			switch (HIWORD(wParam))
			{
			case CBN_SELCHANGE:
			{
				const LRESULT curSel = SendMessage((HWND)lParam, CB_GETCURSEL, 0, 0);
				imp->UpdatePresetUI(presets[curSel]);
				return TRUE;
			}
			break;
			}
		break;
		}

		case IDC_CB_TOOLSET:
		{
			switch (HIWORD(wParam))
			{
			case CBN_SELCHANGE:
			{
				const LRESULT curSel = SendMessage((HWND)lParam, CB_GETCURSEL, 0, 0);
				imp->IDC_CB_TOOLSET_index = curSel;
				return TRUE;
			}
			break;
			}
		break;
		}
		}

		case CC_SPINNER_CHANGE:
			switch (LOWORD(wParam))
			{
			case IDC_SPIN_SCALE:
				imp->IDC_EDIT_SCALE_value = reinterpret_cast<ISpinnerControl *>(lParam)->GetFVal();
				imp->UpdateData();
				break;
			}
	}
	return (INT_PTR)FALSE;
}

int HavokMax::SpawnImportDialog()
{
	instanceDialogType = DLGTYPE_import;
	return DialogBoxParam(hInstance, MAKEINTRESOURCE(IDD_IMPORT), GetActiveWindow(), DialogCallbacks, reinterpret_cast<LPARAM>(this));
}

int HavokMax::SpawnExportDialog()
{
	instanceDialogType = DLGTYPE_export;
	return DialogBoxParam(hInstance, MAKEINTRESOURCE(IDD_EXPORT), GetActiveWindow(), DialogCallbacks, reinterpret_cast<LPARAM>(this));
}
