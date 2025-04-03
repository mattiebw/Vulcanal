@echo off
echo Cloning submodules
git submodule update --init --recursive
echo Building preprocessor
cd ./Tools/Preprocessor/
SET DOTNET_CLI_WORKLOAD_UPDATE_NOTIFY_DISABLE=true
dotnet build --configuration Release
cd ../../Scripts/
./GenerateProjectsWindows.bat
