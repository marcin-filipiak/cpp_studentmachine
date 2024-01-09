#!/bin/sh

echo "----Kompilacja programu----"
g++ -o ../build/studentmachine studentmachine.cpp
echo "---Kompilacja ukonczona----"
echo "---Uruchamianie programu---"
cp ../build/studentmachine ../../server/src/files/studentmachine
chmod +x ../build/studentmachine
../build/studentmachine
