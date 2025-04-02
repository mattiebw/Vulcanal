#!/bin/sh
echo "Generating projects for linux..."
cd ..
./Scripts/Binaries/premake5 gmake2 --os=linux --dotnet=msnet
