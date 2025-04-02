@echo off
echo Cloning submodules
git submodule update --init --recursive
@REM echo Building preprocessor
@REM cd ./Tools/Preprocessor/
@REM dotnet build --configuration Release
cd Scripts/
./GenerateProjectsWindows.bat
