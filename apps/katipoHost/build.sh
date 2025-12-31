#!/bin/bash
cmake -H. -Bbuild
sleep 2
if cmake --build build -- -j4; then
cp -f bin/katipoHost ../katipoHost
echo "Build complete. The katipoHost binary is now available in this directory. Run by typing: ./katipoHost"
else
exit 1;
fi
