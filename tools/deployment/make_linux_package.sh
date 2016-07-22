#!/usr/bin/env bash

#  Copyright (c) 2014-present, Facebook, Inc.
#  All rights reserved.
#
#  This source code is licensed under the BSD-style license found in the
#  LICENSE file in the root directory of this source tree. An additional grant
#  of patent rights can be found in the PATENTS file in the same directory.

set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
SOURCE_DIR="$SCRIPT_DIR/../.."
BUILD_DIR="$SOURCE_DIR/build/linux"

export PATH="$PATH:/usr/local/bin"
source "$SOURCE_DIR/tools/lib.sh"

PACKAGE_VERSION=`git describe --tags HEAD || echo 'unknown-version'`
PACKAGE_ARCH=`uname -m`
PACKAGE_ITERATION=""
PACKAGE_TYPE=""
DESCRIPTION="osquery is an operating system instrumentation toolchain."
PACKAGE_NAME="osquery"
if [[ $PACKAGE_VERSION == *"-"* ]]; then
  DESCRIPTION="$DESCRIPTION (unstable/latest version)"
fi

OUTPUT_PKG_PATH="$BUILD_DIR/${PACKAGE_NAME}-${PACKAGE_VERSION}."

# Config files
INITD_SRC="$SCRIPT_DIR/osqueryd.initd"
INITD_DST="/etc/init.d/osqueryd"
SYSTEMD_SERVICE_SRC="$SCRIPT_DIR/osqueryd.service"
SYSTEMD_SERVICE_DST="/usr/lib/systemd/system/osqueryd.service"
SYSTEMD_SYSCONFIG_SRC="$SCRIPT_DIR/osqueryd.sysconfig"
SYSTEMD_SYSCONFIG_DST="/etc/sysconfig/osqueryd"
SYSTEMD_SYSCONFIG_DST_DEBIAN="/etc/default/osqueryd"
CTL_SRC="$SCRIPT_DIR/osqueryctl"
PACKS_SRC="$SOURCE_DIR/packs"
PACKS_DST="/usr/share/osquery/packs/"
OSQUERY_POSTINSTALL=${OSQUERY_POSTINSTALL:-""}
OSQUERY_CONFIG_SRC=${OSQUERY_CONFIG_SRC:-""}
OSQUERY_EXAMPLE_CONFIG_SRC="$SCRIPT_DIR/osquery.example.conf"
OSQUERY_EXAMPLE_CONFIG_DST="/usr/share/osquery/osquery.example.conf"
OSQUERY_LOG_DIR="/var/log/osquery/"
OSQUERY_VAR_DIR="/var/osquery"
OSQUERY_ETC_DIR="/etc/osquery"

WORKING_DIR=/tmp/osquery_packaging
INSTALL_PREFIX=$WORKING_DIR/prefix
DEBUG_PREFIX=$WORKING_DIR/debug

function usage() {
  fatal "Usage: $0 -t deb|rpm -i REVISION -d DEPENDENCY_LIST

  This will generate an Linux package with:
  (1) An example config /var/osquery/osquery.example.config
  (2) An init.d script /etc/init.d/osqueryd
  (3) The osquery toolset /usr/bin/osquery*"
}

function parse_args() {
  while [ "$1" != "" ]; do
    case $1 in
      -t | --type )           shift
                              PACKAGE_TYPE=$1
                              ;;
      -c | --config )         shift
                              OSQUERY_CONFIG_SRC=$1
                              ;;
      -p | --postinst )       shift
                              OSQUERY_POSTINSTALL=$1
                              ;;
      -i | --iteration )      shift
                              PACKAGE_ITERATION=$1
                              ;;
      -d | --dependencies )   shift
                              PACKAGE_DEPENDENCIES="${@}"
                              ;;
      -h | --help )           usage
                              ;;
    esac
    shift
  done
}

function check_parsed_args() {
  if [[ $PACKAGE_TYPE = "" ]] || [[ $PACKAGE_ITERATION = "" ]]; then
    usage
  fi

  OUTPUT_PKG_PATH=$OUTPUT_PKG_PATH$PACKAGE_TYPE
}

function main() {
  parse_args $@
  check_parsed_args

  platform OS
  distro $OS DISTRO

  rm -rf $WORKING_DIR
  rm -f $OUTPUT_PKG_PATH
  mkdir -p $INSTALL_PREFIX

  log "copying osquery binaries"
  BINARY_INSTALL_DIR="$INSTALL_PREFIX/usr/bin/"
  mkdir -p $BINARY_INSTALL_DIR
  cp "$BUILD_DIR/osquery/osqueryi" $BINARY_INSTALL_DIR
  cp "$BUILD_DIR/osquery/osqueryd" $BINARY_INSTALL_DIR
  strip $BINARY_INSTALL_DIR/*
  cp "$CTL_SRC" $BINARY_INSTALL_DIR

  # Create the prefix log dir and copy source configs
  log "copying osquery configurations"
  mkdir -p $INSTALL_PREFIX/$OSQUERY_VAR_DIR
  mkdir -p $INSTALL_PREFIX/$OSQUERY_LOG_DIR
  mkdir -p $INSTALL_PREFIX/$OSQUERY_ETC_DIR
  mkdir -p $INSTALL_PREFIX/$PACKS_DST
  mkdir -p `dirname $INSTALL_PREFIX$OSQUERY_EXAMPLE_CONFIG_DST`
  cp $OSQUERY_EXAMPLE_CONFIG_SRC $INSTALL_PREFIX$OSQUERY_EXAMPLE_CONFIG_DST
  cp $PACKS_SRC/* $INSTALL_PREFIX/$PACKS_DST

  if [[ $OSQUERY_CONFIG_SRC != "" ]] && [[ -f $OSQUERY_CONFIG_SRC ]]; then
    log "config setup"
    cp $OSQUERY_CONFIG_SRC $INSTALL_PREFIX/$OSQUERY_ETC_DIR/osquery.conf
  fi

  if [[ $DISTRO = "xenial" ]]; then
    #Change config path to Ubuntu/Xenial default
    SYSTEMD_SYSCONFIG_DST=$SYSTEMD_SYSCONFIG_DST_DEBIAN
  fi

  if [[ $DISTRO = "centos7" || $DISTRO = "rhel7" || $DISTRO = "xenial" ]]; then
    # Install the systemd service and sysconfig
    mkdir -p `dirname $INSTALL_PREFIX$SYSTEMD_SERVICE_DST`
    mkdir -p `dirname $INSTALL_PREFIX$SYSTEMD_SYSCONFIG_DST`
    cp $SYSTEMD_SERVICE_SRC $INSTALL_PREFIX$SYSTEMD_SERVICE_DST
    cp $SYSTEMD_SYSCONFIG_SRC $INSTALL_PREFIX$SYSTEMD_SYSCONFIG_DST
  else
    mkdir -p `dirname $INSTALL_PREFIX$INITD_DST`
    cp $INITD_SRC $INSTALL_PREFIX$INITD_DST
  fi

  if [[ $DISTRO = "xenial" ]]; then
    #Change config path in service unit
    sed -i 's/sysconfig/default/g' $INSTALL_PREFIX$SYSTEMD_SERVICE_DST
  fi

  log "creating package"
  IFS=',' read -a deps <<< "$PACKAGE_DEPENDENCIES"
  PACKAGE_DEPENDENCIES=
  for element in "${deps[@]}"
  do
    element=`echo $element | sed 's/ *$//'`
    PACKAGE_DEPENDENCIES="$PACKAGE_DEPENDENCIES -d \"$element\""
  done

  platform OS
  distro $OS DISTRO
  FPM="fpm"
  if [[ $DISTRO == "lucid" ]]; then
    FPM="/var/lib/gems/1.8/bin/fpm"
  fi

# some tune to stay compliant with Debian package naming convention
  if [[ $PACKAGE_TYPE == "deb" ]]; then
    DEB_PACKAGE_ARCH=`dpkg --print-architecture`
    OUTPUT_PKG_PATH="$BUILD_DIR/${PACKAGE_NAME}_${PACKAGE_VERSION}_${DEB_PACKAGE_ARCH}.deb"
  fi

  POSTINST_CMD=""
  if [[ $OSQUERY_POSTINSTALL != "" ]] && [[ -f $OSQUERY_POSTINSTALL ]]; then
    POSTINST_CMD="--after-install $OSQUERY_POSTINSTALL"
  fi

  EPILOG="--url https://osquery.io \
    -m osquery@osquery.io          \
    --vendor Facebook              \
    --license BSD                  \
    --description \"$DESCRIPTION\""

  CMD="$FPM -s dir -t $PACKAGE_TYPE \
    -n $PACKAGE_NAME -v $PACKAGE_VERSION \
    --iteration $PACKAGE_ITERATION \
    -a $PACKAGE_ARCH               \
    $POSTINST_CMD                  \
    $PACKAGE_DEPENDENCIES          \
    -p $OUTPUT_PKG_PATH            \
    $EPILOG \"$INSTALL_PREFIX/=/\""
  eval "$CMD"
  log "package created at $OUTPUT_PKG_PATH"

  # Generate debug packages for Linux or CentOS
  BUILD_DEBUG_PKG=false
  if [[ $OS = "ubuntu" || $OS = "debian" ]]; then
    BUILD_DEBUG_PKG=true
    PACKAGE_DEBUG_NAME="$PACKAGE_NAME-dbg"
    PACKAGE_DEBUG_DEPENDENCIES="osquery (= $PACKAGE_VERSION-$PACKAGE_ITERATION)"

    # Debian only needs the non-stripped binaries.
    BINARY_DEBUG_DIR=$DEBUG_PREFIX/usr/lib/debug/usr/bin
    mkdir -p $BINARY_DEBUG_DIR
    cp "$BUILD_DIR/osquery/osqueryi" $BINARY_DEBUG_DIR
    cp "$BUILD_DIR/osquery/osqueryd" $BINARY_DEBUG_DIR
  elif [[ $OS = "rhel" || $OS = "centos" || $OS = "fedora" ]]; then
    BUILD_DEBUG_PKG=true
    PACKAGE_DEBUG_NAME="$PACKAGE_NAME-debuginfo"
    PACKAGE_DEBUG_DEPENDENCIES="osquery = $PACKAGE_VERSION"

    # Create Build-ID links for executables and Dwarfs.
    BUILD_ID_SHELL=`eu-readelf -n "$BUILD_DIR/osquery/osqueryi" | grep "Build ID" | awk '{print $3}'`
    BUILD_ID_DAEMON=`eu-readelf -n "$BUILD_DIR/osquery/osqueryd" | grep "Build ID" | awk '{print $3}'`
    BUILDLINK_DEBUG_DIR=$DEBUG_PREFIX/usr/lib/debug/.build-id/64
    if [[ ! "$BUILD_ID_SHELL" = "" ]]; then
      mkdir -p $BUILDLINK_DEBUG_DIR
      ln -s ../../../../bin/osqueryi $BUILDLINK_DEBUG_DIR/$BUILD_ID_SHELL
      ln -s ../../bin/osqueryi.debug $BUILDLINK_DEBUG_DIR/$BUILD_ID_SHELL.debug
      ln -s ../../../../bin/osqueryd $BUILDLINK_DEBUG_DIR/$BUILD_ID_DAEMON
      ln -s ../../bin/osqueryd.debug $BUILDLINK_DEBUG_DIR/$BUILD_ID_DAEMON.debug
    fi

    # Install the non-stripped binaries.
    BINARY_DEBUG_DIR=$DEBUG_PREFIX/usr/lib/debug/usr/bin/
    mkdir -p $BINARY_DEBUG_DIR
    cp "$BUILD_DIR/osquery/osqueryi" "$BINARY_DEBUG_DIR/osqueryi.debug"
    cp "$BUILD_DIR/osquery/osqueryd" "$BINARY_DEBUG_DIR/osqueryd.debug"

    # Finally install the source.
    SOURCE_DEBUG_DIR=$DEBUG_PREFIX/usr/src/debug/osquery-$PACKAGE_VERSION
    BUILD_DIR=`readlink --canonicalize "$BUILD_DIR"`
    SOURCE_DIR=`readlink --canonicalize "$SOURCE_DIR"`
    for file in `"$SCRIPT_DIR/getfiles.py" --build "$BUILD_DIR/" --base "$SOURCE_DIR/"`
    do
      mkdir -p `dirname "$SOURCE_DEBUG_DIR/$file"`
      cp "$SOURCE_DIR/$file" "$SOURCE_DEBUG_DIR/$file"
    done
  fi

  PACKAGE_DEBUG_DEPENDENCIES=`echo "$PACKAGE_DEBUG_DEPENDENCIES"|tr '-' '_'`
  OUTPUT_DEBUG_PKG_PATH="$BUILD_DIR/$PACKAGE_DEBUG_NAME-$PACKAGE_VERSION.$PACKAGE_TYPE"
  if [[ $BUILD_DEBUG_PKG ]]; then
    rm -f $OUTPUT_DEBUG_PKG_PATH
    CMD="$FPM -s dir -t $PACKAGE_TYPE \
      -n $PACKAGE_DEBUG_NAME -v $PACKAGE_VERSION \
      --iteration $PACKAGE_ITERATION \
      -a $PACKAGE_ARCH \
      -d \"$PACKAGE_DEBUG_DEPENDENCIES\" \
      -p $OUTPUT_DEBUG_PKG_PATH \
      $EPILOG \"$DEBUG_PREFIX/=/\""
    eval "$CMD"
    log "debug created at $OUTPUT_DEBUG_PKG_PATH"
  fi
}

main $@
