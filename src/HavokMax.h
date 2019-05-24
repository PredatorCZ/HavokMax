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

#pragma once
#include "MAXex/3DSMaxSDKCompat.h"
#include <istdplug.h>
#include <iparamb2.h>
#include <iparamm2.h>
#include <maxtypes.h>
//SIMPLE TYPE

#include <impexp.h>
#include <direct.h>
#include <commdlg.h>

#include "MAXex/win/CFGMacros.h"
#include <vector>
#include "HavokXMLApi.hpp"

#define HAVOKMAX_VERSION 1.4
#define HAVOKMAX_VERSIONINT 140

extern TCHAR *GetString(int id);
extern HINSTANCE hInstance;

struct PresetData;

class HavokMax
{
public:

	enum DLGTYPE_e
	{
		DLGTYPE_unknown,
		DLGTYPE_import,
		DLGTYPE_export
	};

	enum ConfigBoolean
	{
		IDConfigBool(IDC_CH_ANIMATION),
		IDConfigBool(IDC_CH_ANISKELETON),
		IDConfigBool(IDC_CH_ANIOPTIMIZE),
		IDConfigVisible(IDC_CH_ANISKELETON),
		IDConfigVisible(IDC_CH_ANIOPTIMIZE),
		IDConfigVisible(IDC_EDIT_ANIEND),
		IDConfigVisible(IDC_EDIT_ANISTART),
		IDConfigVisible(IDC_SPIN_ANIEND),
		IDConfigVisible(IDC_SPIN_ANISTART),
	};

	DLGTYPE_e instanceDialogType;
	TSTRING currentPresetName;
	HWND comboHandle;
	HWND hWnd;
	TSTRING cfgpath;
	esFlags<char> sanityCheck;
	const TCHAR *CFGFile;
	NewIDConfigValue(IDC_EDIT_SCALE);
	NewIDConfigIndex(IDC_CB_TOOLSET);
	NewIDConfigIndex(IDC_EDIT_ANIEND);
	NewIDConfigIndex(IDC_EDIT_ANISTART);
	NewIDConfigIndex(IDC_EDIT_CAPTUREFRAME);

	esFlags<ushort, ConfigBoolean> flags;
	Matrix3 corMat;

	void LoadCFG();
	void BuildCFG();
	void SaveCFG();
	void UpdateData();
	void SetupExportUI();

	int SavePreset(const TCHAR *presetName);
	virtual int SavePreset(PresetData *presetData) = 0;
	virtual void Setup(HWND hwnd);
	virtual void CollisionHandler() = 0;
	virtual void UpdatePresetUI(PresetData *presetData) = 0;
	virtual int SanityBitcher() = 0;
	virtual int SpawnImportDialog() = 0;
	virtual int SpawnExportDialog() = 0;
	virtual INT_PTR DlgCommandCallBack(WPARAM wParam, LPARAM lParam) = 0;

	HavokMax();	
	virtual ~HavokMax() {}
};

class HavokMaxV1 : public HavokMax
{
public:
	void CollisionHandler();
	void UpdatePresetUI(PresetData *presetData);
	int SanityBitcher();
	int SpawnImportDialog();
	int SpawnExportDialog();
	INT_PTR DlgCommandCallBack(WPARAM wParam, LPARAM lParam);
	int SavePreset(PresetData *presetData);
};

class HavokMaxV2 : public HavokMax
{
public:
	HWND comboRight;
	HWND comboBack;

	void CollisionHandler();
	void UpdatePresetUI(PresetData *presetData);
	int SanityBitcher();
	int SpawnImportDialog();
	int SpawnExportDialog();
	void Setup(HWND hwnd);
	INT_PTR DlgCommandCallBack(WPARAM wParam, LPARAM lParam);
	int SavePreset(PresetData *presetData);
};

void BuildHavokResources();
void DestroyHavokResources();
void Rescan();
void ShowAboutDLG(HWND hWnd);
extern std::vector<TSTRING> extensions;
