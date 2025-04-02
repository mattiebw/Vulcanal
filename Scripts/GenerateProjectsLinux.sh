#!/bin/sh
echo "Generating projects for linux..."
cd ..
./Scripts/Bin/premake5 gmake2 --os=linux --dotnet=msnet
