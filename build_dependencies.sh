#!/bin/bash
#
# If not stated otherwise in this file or this component's LICENSE
# file the following copyright and licenses apply:
#
# Copyright 2026 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set -x
set -e
##############################
GITHUB_WORKSPACE="${PWD}"
ls -la ${GITHUB_WORKSPACE}
cd ${GITHUB_WORKSPACE}

# # ############################# 
#1. Install Dependencies and packages

apt update
apt install -y libcurl4-openssl-dev valgrind lcov clang libsystemd-dev libboost-all-dev meson curl libunwind-dev libdrm-dev
pip install jsonref

# Clone the required repositories


git clone --branch  R4.4.3 https://github.com/rdkcentral/ThunderTools.git

git clone --branch R4.4.1 https://github.com/rdkcentral/Thunder.git

git clone --branch RDKEMW-19266 https://github.com/rdkcentral/entservices-apis.git

cd ..
git clone --branch develop https://github.com/rdkcentral/entservices-helpers.git
cd "$GITHUB_WORKSPACE"

git clone --branch 1.0.14 https://github.com/rdkcentral/entservices-testframework.git

rm -rf rdk-cert-config
git clone -b 1.0.6 https://github.com/rdkcentral/rdk-cert-config.git && \
    cd rdk-cert-config && autoreconf --install && ./configure --prefix=/usr/local --enable-testrdkcerts && \
    make && make install
cp RdkConfigApi/src/librdkconfig.a /usr/local/lib/librdkconfig.a

rm -rf rbus 

git clone https://github.com/rdkcentral/rbus
cmake -Hrbus -Bbuild/rbus -DBUILD_FOR_DESKTOP=ON -DCMAKE_BUILD_TYPE=Debug
make -C build/rbus && make -C build/rbus install

rm -rf WebconfigFramework
git clone https://github.com/rdkcentral/WebconfigFramework.git
cd WebconfigFramework && export INSTALL_DIR='/usr/local'&& \
export CFLAGS="-I${INSTALL_DIR}/include/rtmessage -I${INSTALL_DIR}/include/msgpack -I${INSTALL_DIR}/include/rbus -I${INSTALL_DIR}/include" && \
export LDFLAGS="-L${INSTALL_DIR}/lib" && \
autoreconf --install && \
./configure --prefix=/usr/local && make && make install && cp -r include/* /usr/local/include/

rm -rf rdk_logger

git clone https://github.com/rdkcentral/rdk_logger.git
cd rdk_logger && autoreconf --install && ./configure && make && make install

cd ${GITHUB_WORKSPACE}
rm -rf iarmmgrs
git clone https://github.com/rdkcentral/iarmmgrs.git
cp iarmmgrs/sysmgr/include/sysMgr.h /usr/local/include
cp iarmmgrs/maintenance/include/maintenanceMGR.h /usr/local/include

cd ${GITHUB_WORKSPACE}
rm -rf telemetry
git clone https://github.com/rdkcentral/telemetry.git
cd telemetry
cp include/*.h /usr/local/include
sh  build_inside_container.sh

cd ${GITHUB_WORKSPACE}
git clone https://github.com/rdkcentral/dcm-agent.git
cd dcm-agent
git checkout feature/RDKEMW-22385
autoreconf -i
./configure
cp uploadstblogs/include/*.h /usr/local/include
cd uploadstblogs/src
make
make install
cd ${GITHUB_WORKSPACE}

############################
# Build Thunder-Tools
echo "======================================================================================"
echo "buliding thunderTools"
cd ThunderTools
patch -p1 < $GITHUB_WORKSPACE/entservices-testframework/patches/00010-R4.4-Add-support-for-project-dir.patch
cd -


cmake -G Ninja -S ThunderTools -B build/ThunderTools \
    -DEXCEPTIONS_ENABLE=ON \
    -DCMAKE_INSTALL_PREFIX="$GITHUB_WORKSPACE/install/usr" \
    -DCMAKE_MODULE_PATH="$GITHUB_WORKSPACE/install/tools/cmake" \
    -DGENERIC_CMAKE_MODULE_PATH="$GITHUB_WORKSPACE/install/tools/cmake" \

cmake --build build/ThunderTools --target install


############################
# Build Thunder
echo "======================================================================================"
echo "buliding thunder"

cd Thunder
patch -p1 < $GITHUB_WORKSPACE/entservices-testframework/patches/Use_Legact_Alt_Based_On_ThunderTools_R4.4.3.patch
patch -p1 < $GITHUB_WORKSPACE/entservices-testframework/patches/error_code_R4_4.patch
patch -p1 < $GITHUB_WORKSPACE/entservices-testframework/patches/1004-Add-support-for-project-dir.patch
patch -p1 < $GITHUB_WORKSPACE/entservices-testframework/patches/RDKEMW-733-Add-ENTOS-IDS.patch
patch -p1 < $GITHUB_WORKSPACE/entservices-testframework/patches/Jsonrpc_dynamic_error_handling.patch
cd -

cmake -G Ninja -S Thunder -B build/Thunder \
    -DMESSAGING=ON \
    -DCMAKE_INSTALL_PREFIX="$GITHUB_WORKSPACE/install/usr" \
    -DCMAKE_MODULE_PATH="$GITHUB_WORKSPACE/install/tools/cmake" \
    -DGENERIC_CMAKE_MODULE_PATH="$GITHUB_WORKSPACE/install/tools/cmake" \
    -DBUILD_TYPE=Debug \
    -DBINDING=127.0.0.1 \
    -DPORT=55555 \
    -DEXCEPTIONS_ENABLE=ON \

cmake --build build/Thunder --target install


############################
# Build entservices-apis
echo "======================================================================================"
echo "buliding entservices-apis"
cd entservices-apis
rm -rf jsonrpc/DTV.json
patch -p1 < $GITHUB_WORKSPACE/entservices-testframework/patches/RDKEMW-1007.patch
cd ..

cmake -G Ninja -S entservices-apis  -B build/entservices-apis \
    -DEXCEPTIONS_ENABLE=ON \
    -DCMAKE_INSTALL_PREFIX="$GITHUB_WORKSPACE/install/usr" \
    -DCMAKE_MODULE_PATH="$GITHUB_WORKSPACE/install/tools/cmake" \

cmake --build build/entservices-apis --target install


############################
# generating minimal mock headers
cd $GITHUB_WORKSPACE/entservices-testframework/Tests
mkdir -p headers
cd headers
touch secure_wrapper.h
touch wpa_ctrl.h
touch rdk_logger_milestone.h
mkdir -p rdk/iarmbus
touch rdk/iarmbus/libIARM.h
touch rdk/iarmbus/libIBus.h
touch iarm.h
cd $GITHUB_WORKSPACE
############################
# Build entservices-helpers
echo "======================================================================================"
echo "building entservices-helpers"
cmake -G Ninja -S ../entservices-helpers -B build/entservices-helpers \
    -DEXCEPTIONS_ENABLE=ON \
    -DCMAKE_INSTALL_PREFIX="$GITHUB_WORKSPACE/install/usr" \
    -DCMAKE_MODULE_PATH="$GITHUB_WORKSPACE/install/tools/cmake" \
    "-DCMAKE_CXX_FLAGS=-I$GITHUB_WORKSPACE/entservices-testframework/Tests/mocks -I$GITHUB_WORKSPACE/entservices-testframework/Tests/headers -I$GITHUB_WORKSPACE/entservices-testframework/Tests/headers/rdk/iarmbus -include $GITHUB_WORKSPACE/entservices-testframework/Tests/mocks/Iarm.h "

cmake --build build/entservices-helpers --target install


############################
# generating extrnal headers
cd $GITHUB_WORKSPACE
cd entservices-testframework/Tests
echo " Empty mocks creation to avoid compilation errors"
echo "======================================================================================"
mkdir -p headers
mkdir -p headers/rdk/ds
mkdir -p headers/rdk/iarmbus
mkdir -p headers/rdk/iarmmgrs-hal
mkdir -p headers/rdk/halif/deepsleep-manager
mkdir -p headers/proc
echo "dir created successfully"
echo "======================================================================================"

echo "======================================================================================"
echo "empty headers creation"
cd headers
echo "current working dir: "${PWD}
touch rdk/ds/audioOutputPort.hpp
touch rdk/ds/exception.hpp
touch rdk/ds/host.hpp
touch rdk/ds/sleepMode.hpp
touch rdk/ds/videoOutputPort.hpp
touch rdk/iarmbus/libIARM.h
touch rdk/iarmbus/libIBus.h
touch rdk/iarmbus/libIBusDaemon.h
touch rdk/halif/deepsleep-manager/deepSleepMgr.h
touch rdk/iarmmgrs-hal/mfrMgr.h
touch rdk/iarmmgrs-hal/sysMgr.h
touch rfcapi.h
touch telemetry_busmessage_sender.h
touch secure_wrapper.h
touch proc/readproc.h
echo "files created successfully"
echo "======================================================================================"

cd ../../

ls -la ${GITHUB_WORKSPACE}




