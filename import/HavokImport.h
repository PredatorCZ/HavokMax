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
#include "resource.h"
#include <tchar.h>
#include <string>
#include <windows.h>
#include <matrix3.h>
#include "MAXex/win/CFGMacros.h"
#include <vector>

struct PresetData;

class HavokImport
{	
public:
	TSTRING currentPresetName;
	HWND comboHandle;
	HWND hWnd;
	TSTRING cfgpath;
	t_Flags<char> sanityCheck;
	const TCHAR *CFGFile;
	void CollisionHandler();
	void LoadCFG();
	void BuildCFG();
	void SaveCFG();
	int SavePreset(const TCHAR* presetName);
	int SavePreset(PresetData* presetData);
	void UpdatePresetUI(PresetData* presetData);
	void UpdateData();
	int SanityBitcher();
	NewIDConfigValue(IDC_EDIT_SCALE);
	Matrix3 corMat;
	HavokImport();
	int SpawnDialog();
};

void BuildHavokImpResources();
void DestroyHavokImpResources();
void Rescan();
void ShowAboutDLG(HWND hWnd);
extern std::vector<TSTRING> extensions;