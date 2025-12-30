#!/bin/bash
cmake -H. -Bbuild
sleep 2
if cmake --build build -- -j4; then
cp -f bin/katipoTracker katipoTracker
echo "\nBuild complete. The katipoTracker binary is now available in this directory.\n\n"
else
exit 1;
fi
