# HavokMax
HavokMax is Havok importer/exporter for 3ds max.\
Builded with VS2015.\
Supported 3ds max versions: **2010 - 2019**\
Tested on 3ds max versions: **2017**

## Building
### Editing .vcxproj
All essential configurations are within **PropertyGroup Label="MAXConfigurations"** field.
- **MaxSDK**: changes path where is your MAX SDK installation.
If your MAX SDK installation is somewhere else than default path stated in this field, you can edit it here.
- **MaxDebugConfiguration**: changes 3ds max version and platform, so all necessary files are copied into plugin directory, this will enable post-build event. You must have set ***Working Directory*** under ***Debugging*** in ***Project Properties*** to location, where 3ds max is installed (where 3dsmax.exe is).

## Installation
### [Latest Release](https://github.com/PredatorCZ/HavokMax/releases/)
Move corresponding .dlu located in correct version folder into ***%3ds max installation directory%/plugins***. \
Versions must match!\
Additionally plugin will require **Visual C++ Redistributable for Visual Studio 2015** to be installed in order to work.

## License
This plugin is available under GPL v3 license. (See LICENSE.md)

This plugin uses following libraries:

* HavokLib, Copyright (c) 2016-2019 Lukas Cone
