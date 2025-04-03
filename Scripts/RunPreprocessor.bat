@echo off
echo Running preprocessor...
cd ../Tools/Preprocessor/bin/Release/net8.0/
Preprocessor.exe "../../../../../Content/" %1 Windows
