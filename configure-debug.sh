#!/bin/bash

top_dir=$(pwd)

DEV_HOME=$HOME/dev

export CXX=g++
#export CXX=clang++

CXXFLAGS="$CXXFLAGS -D DEBUG"
CXXFLAGS="$CXXFLAGS -fno-inline"
CXXFLAGS="$CXXFLAGS -fno-eliminate-unused-debug-types"
CXXFLAGS="$CXXFLAGS -g3"
CXXFLAGS="$CXXFLAGS -O0"
export CXXFLAGS

#rm -fr $top_dir/install
rm -fr $top_dir/build-debug
mkdir -p $top_dir/build-debug
cd $top_dir/build-debug

$top_dir/configure --prefix=$DEV_HOME

