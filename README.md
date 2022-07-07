This repo has been archived, because it's no longer possible to do development due to licensing issues within 3ds max (among other problems).

For exhausting explanation, head to [my blog post about those issues](https://lukascone.wordpress.com/2021/06/07/da-futureh/).

Further development/releases will be available in [HavokToolset](https://github.com/PredatorCZ/HavokLib/tree/master/toolset) subproject for HavokLib.

# HavokMax

HavokMax is Havok importer/exporter for 3ds max.\
Buildable with VS2017.\
Supported 3ds max versions: **2010 - 2022**\
Tested on 3ds max versions: **2017**

## Building

Head to the [Building a 3ds max CMake projects](https://github.com/PredatorCZ/PreCore/wiki/Building-a-3ds-max-CMake-projects) wiki page.

## Installation

### [Latest Release](https://github.com/PredatorCZ/HavokMax/releases/)

Move corresponding .dlu located in correct version folder into ***%3ds max installation directory%/plugins***. \
Versions must match!\
Additionally plugin will require **Visual C++ Redistributable for Visual Studio 2017** to be installed in order to work.

## License

This plugin is available under GPL v3 license. (See LICENSE.md)

This plugin uses following libraries:

* HavokLib, Copyright (c) 2016-2020 Lukas Cone
