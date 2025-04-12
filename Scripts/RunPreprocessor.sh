#!/bin/sh
echo Running preprocessor...
cd ../Tools/Preprocessor/bin/Release/net8.0/
./Preprocessor "../../../../../Content/" $1 Linux $2
