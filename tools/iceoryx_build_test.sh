#!/bin/bash

# Copyright (c) 2019-2020 by Robert Bosch GmbH. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# This script builds iceoryx_utils und iceoryx_posh and executes all tests

set -e

#====================================================================================================
#==== Step 0 : Setup ================================================================================
#====================================================================================================

# the absolute path of the directory assigned to the build
WORKSPACE=$(git rev-parse --show-toplevel)
BUILD_DIR=$WORKSPACE/build
NUM_JOBS=""

CLEAN_BUILD=false
BUILD_TYPE=""
STRICT_FLAG="off"
TEST_FLAG="off"
RUN_TEST=false
INTROSPECTION_FLAG="on"
DDS_GATEWAY_FLAG="off"
CYCLONEDDS_FLAG="$DDS_GATEWAY_FLAG"

while (( "$#" )); do
  case "$1" in
    -b|--builddir)
        BUILD_DIR=$(realpath $2)
        shift 2
        ;;
    -j|--jobs)
        NUM_JOBS=$2
        shift 2
        ;;
    "clean")
        CLEAN_BUILD=true
        shift 1
        ;;
    "release")
        BUILD_TYPE="Release"
        shift 1
        ;;
    "debug")
        BUILD_TYPE="Debug"
        shift 1
        ;;
    "strict")
        STRICT_FLAG="on"
        shift 1
        ;;
    "test")
        RUN_TEST=true
        TEST_FLAG="on"
        shift 1
        ;;
    "with-dds-gateway")
        echo " [i] Including DDS gateway in build."
        DDS_GATEWAY_FLAG="on"
        CYCLONEDDS_FLAG="$DDS_GATEWAY_FLAG"
        shift 1
        ;;
    "with-tests")
        echo " [i] Including tests."
        TEST_FLAG="on"
        shift 1
        ;;
    "skip-introspection")
        echo " [i] Not including introspection client in build."
        INTROSPECTION_FLAG="off"
        shift 1
        ;;
    "help")
        echo "Build script for iceoryx."
        echo "By default, iceoryx, the dds gateway and the examples are built."
        echo ""
        echo "Usage:"
        echo "    iceoryx_build_test.sh [--builddir <dir>] [<args>]"
        echo "Options:"
        echo "    -b --builddir         Specify a non-default build directory"
        echo "    -j --jobs             Specify the number of jobs to run simultaneously"
        echo "Args:"
        echo "    clean                 Cleans the build directory"
        echo "    release               Build release configuration"
        echo "    debug                 Build debug configuration"
        echo "    strict                Build is performed with '-Werror'"
        echo "    test                  Builds and runs the tests"
        echo "    with-dds-gateway      Builds the iceoryx dds gateway"
        echo "    with-tests            Builds the tests (doesn't run)"
        echo "    skip-introspection    Skips building iceoryx introspection"
        echo "    help                  Prints this help"
        echo ""
        echo "e.g. iceoryx_build_test.sh -b ./build-scripted clean test release"
        exit 0
        ;;
    *)
        echo "Invalid argument '$1'. Try 'help' for options."
        exit -1
        ;;
  esac
done

# define directories dependent on the build directory
ICEORYX_INSTALL_PREFIX=$BUILD_DIR/install/prefix/

echo " [i] Building in $BUILD_DIR"

#====================================================================================================
#==== Step 1 : Build  ===============================================================================
#====================================================================================================

# run number of jobs equal to number of available cores unless manually specified
if [ -z $NUM_JOBS ]
then
    NUM_JOBS=1
fi
echo " [i] Building with $NUM_JOBS jobs"

# clean build folder
if [ $CLEAN_BUILD == true ]
then
    echo " [i] Cleaning build directory"
    cd $WORKSPACE
    rm -rf $BUILD_DIR/*
fi

# create a new build directory and change the current working directory
echo " [i] Preparing build directory"
cd $WORKSPACE
mkdir -p $BUILD_DIR
cd $BUILD_DIR
echo " [i] Current working directory: $(pwd)"

echo ">>>>>> Start building iceoryx package <<<<<<"
cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DBUILD_STRICT=$STRICT_FLAG -DCMAKE_INSTALL_PREFIX=$ICEORYX_INSTALL_PREFIX -DTOML_CONFIG=on -Dtest=$TEST_FLAG -Droudi_environment=on -Dexamples=OFF -Dintrospection=$INTROSPECTION_FLAG -Ddds_gateway=$DDS_GATEWAY_FLAG -Dcyclonedds=$CYCLONEDDS_FLAG $WORKSPACE/iceoryx_meta
cmake --build . --target install -- -j$NUM_JOBS
echo ">>>>>> Finished building iceoryx package <<<<<<"

echo ">>>>>> Start building iceoryx examples <<<<<<"
cd $BUILD_DIR
mkdir -p iceoryx_examples
echo ">>>>>>>> icedelivery"
cd $BUILD_DIR/iceoryx_examples
mkdir -p icedelivery
cd icedelivery
cmake -DCMAKE_PREFIX_PATH=$ICEORYX_INSTALL_PREFIX $WORKSPACE/iceoryx_examples/icedelivery
cmake --build . -- -j$NUM_JOBS
echo ">>>>>>>> iceperf"
cd $BUILD_DIR/iceoryx_examples
mkdir -p iceperf
cd iceperf
cmake -DCMAKE_PREFIX_PATH=$ICEORYX_INSTALL_PREFIX $WORKSPACE/iceoryx_examples/iceperf
cmake --build . -- -j$NUM_JOBS
echo ">>>>>> Finished building iceoryx examples <<<<<<"

#====================================================================================================
#==== Step 2 : Run all Tests  =======================================================================
#====================================================================================================

if [ $RUN_TEST == true ]
then

# the absolute path of the directory assigned to the build
cd $BUILD_DIR
mkdir -p tools
cp $WORKSPACE/tools/run_all_tests.sh $BUILD_DIR/tools/run_all_tests.sh

echo " [i] Running all tests"
$BUILD_DIR/tools/run_all_tests.sh

for COMPONENT in $COMPONENTS; do

    if [ ! -f testresults/"$COMPONENT"_ModuleTestResults.xml ]; then
        echo "xml:"$COMPONENT"_ModuletestTestResults.xml not found!"
        exit 1
    fi

    if [ ! -f testresults/"$COMPONENT"_ComponenttestTestResults.xml ]; then
        echo "xml:"$COMPONENT"_ComponenttestTestResults.xml not found!"
        exit 1
    fi

    if [ ! -f testresults/"$COMPONENT"_IntegrationTestResults.xml ]; then
        echo "xml:"$COMPONENT"_IntegrationTestResults.xml not found!"
        exit 1
    fi

done

fi
