# skyrim-classic-fixes
A DLL that fixes engine-level bugs in Skyrim Classic.

This DLL depends on [the Shared Items Project I use for my reverse-engineered findings](https://github.com/DavidJCobb/skyrim-classic-re). In order to get the solution and project to load properly, you will need to manually edit the files (current.sln and CobbBugFixes.vcxproj) and fix up the file paths for the shared project. Visual Studio 2015 will not allow you to do this through the GUI; it refuses to load anything that has incorrect paths, but also refuses to allow you to tell it where the missing files are. I apologize for the inconvenience; doing things this way will allow me to consolidate all of my reverse-engineered code so that I don't end up with multiple copies that have fallen out of synch, and it would be a perfectly viable approach were Visual Studio even a little less brittle.

SKSE source code and project files are not included.
