#!/bin/bash

git clone git@github.com:Neargye/magic_enum.git
git clone git@github.com:yhirose/cpp-httplib.git
git clone git@github.com:nlohmann/json.git

g++ main.cpp -o run -std=c++20 -O3 -Ijson/include/ -Imagic_enum/include

