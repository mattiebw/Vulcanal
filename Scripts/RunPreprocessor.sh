#!/bin/sh
echo Running preprocessor...
cd ../Tools/Preprocessor/bin/Release/net8.0/ubuntu.24.04-x64/
./Preprocessor "../../../../../../Content/" $1 Linux
