#!/bin/bash
cmake -H. -Bbuild
sleep 2
if cmake --build build -- -j4; then
cp -f bin/katipoClient ../katipoClient
echo "Build complete. The katipoClient binary is now available in this directory. Run by typing: ./katipoClient"
else
exit 1;
fi
