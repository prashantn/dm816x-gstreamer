#!/bin/sh
#
# process_avopts.sh
#
# Copyright (C) $year Texas Instruments Incorporated - http://www.ti.com/
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as
# published by the Free Software Foundation version 2.1 of the License.
#
# This program is distributed #as is# WITHOUT ANY WARRANTY of any kind,
# whether express or implied; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.

#------------------------------------------------------------------------------
# Print help
#------------------------------------------------------------------------------
help ()
{
    echo "Usage: $0 -f <file> [opts]"
    echo "Options:"
    echo " -f  |   Input file"
    echo " -h  |   Print this help"
    echo
    echo "Audio options:"
    echo " -a  |   Audio decoder plugin to use [Default:$audio_plugin]"
    echo " -n  |   Audio decoder engine name to use [Default:$audiocodecName]"
    echo " -s  |   Linux sound standard to use [Default:$soundStd]"
    echo
    echo "Video options:"
    echo " -v  |   Video  decoder plugin to use [Default:$video_plugin]"
    echo " -p  |   Video decoder engine name to use [Default:$videocodecName]"
    echo " -z  |   Linux display standard (fbdev, v4l2) [Default:$dispStd]"
    echo " -d  |   Display device to use [Default:$dispDevice]"
    echo " -y  |   Video standard to use [Default:$videoStd]"
    echo " -o  |   Video output to use [Default:$videoOut]"
    echo " -x  |   Enable resizer [Default:$resizer]"
    echo " -c  |   Disable accel frame copy [Default:$accelFrameCopy]"
    if [ "${PLATFORM}" = "omap3530" ]; then
        echo " -r  |   Rotation angle [Default:$rotation] "
    fi

    exit 1;
}

#------------------------------------------------------------------------------
# exectue command and exit on failure
#------------------------------------------------------------------------------
execute ()
{
    echo "$*"
    $* >/dev/null
    if [ $? -ne 0 ]; then
        echo "ERROR: Failed to execute '$*'"
        exit 1
    fi
}

args=`getopt f:a:s:v:z:d:y:o:p:n:r:xc $*`
if test $? != 0 ; then
    help;
fi

set -- $args
for i
do
    case "$i" in
        -f) shift; fileName=$1; shift;;
        -a) shift; audio_plugin=$1; shift;;
        -s) shift; soundStd=$1; shift;;
        -v) shift; video_plugin=$1; shift;;
        -z) shift; dispStd=$1; shift;;
        -d) shift; dispDevice=$1; shift;;
        -y) shift; videoStd=$1; shift;;
        -o) shift; videoOut=$1; shift;;
        -n) shift; audiocodecName=$1; shift;;
        -p) shift; videocodecName=$1; shift;;
        -x) shift; resizer=TRUE;;
        -r) shift; rotation=$1; shift;;
        -c) shift; accelFrameCopy=FALSE;;
        -h) help;;
    esac
done

if [ "$fileName" = "" ]; then
    help;
fi

if [ ! -f $fileName ]; then
    echo "ERROR: $fileName does not exist !"
    exit 1
fi

execute "$GSTINSPECT $video_plugin"
execute "$GSTINSPECT $audio_plugin"

case "$soundStd" in
    alsa)
        execute "$GSTINSPECT alsasink"
        if [ "${PLATFORM}" = "omap3530" ]; then
            # audio driver on OMAP does not support 32-bit format.
            audio_sink="audioconvert ! audio/x-raw-int, width=16, depth=16  ! alsasink"
        else
            audio_sink="alsasink"
        fi
        ;;
    oss)
        execute "$GSTINSPECT osssink"
        audio_sink="audioconvert ! osssink"
        ;;
    *)
        echo "ERROR: Invalid sound standard !"
        exit 1
        ;;
esac
if [ "${audiocodecName}" != "" ]; then
    audio_plugin_args="engineName=decode codecName=${audiocodecName}"
fi

case "$dispStd" in
    fbdev)
    if [ "$dispDevice" = "" ]; then
        dispDevice="/dev/fb/3"
    fi
        ;;
    v4l2)
    if [ "$dispDevice" = "" ]; then
        dispDevice="/dev/video2"
    fi
        ;;
    *)
        echo "ERROR: Invalid display standard !"
        exit 1
        ;;
esac
if [ "${videocodecName}" != "" ]; then
    video_plugin_args="engineName=decode codecName=${videocodecName}"
fi

video_sink="TIDmaiVideoSink displayStd=$dispStd displayDevice=$dispDevice videoStd=$videoStd videoOutput=$videoOut resizer=$resizer accelFrameCopy=$accelFrameCopy"

echo "*********** Pipeline Settings *************"
echo "platform               = ${PLATFORM}"
echo "audio_plugin           = $audio_plugin"
echo "audio_plugin_args      = $audio_plugin_args"
echo "soundStd               = $soundStd"
echo "video_plugin           = $video_plugin"
echo "video_plugin_args      = $video_plugin_args"
echo "dispStd                = $dispStd"
echo "dispDevice             = $dispDevice"
echo "videoStd               = $videoStd"
echo "videoOutput            = $videoOut"
echo "resizer                = $resizer"
echo "accelFrameCopy         = $accelFrameCopy"
if [ "${PLATFORM}" = "omap3530" ]; then
echo "rotation               = $rotation"
fi
echo ""

if [ "${PLATFORM}" = "omap3530" ]; then
            rotation_opts="rotation=$rotation"
fi
