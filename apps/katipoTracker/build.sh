#!/bin/bash
cmake -H. -Bbuild
sleep 2
if cmake --build build -- -j4; then
cp -f bin/katipoTracker ../katipoTracker
echo "Build complete. The katipoTracker binary is now available in ../katipoTracker."
else
exit 1;
fi
