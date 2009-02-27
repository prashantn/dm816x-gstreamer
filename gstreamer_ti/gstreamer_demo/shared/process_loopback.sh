#!/bin/sh
#
# process_loopback.sh
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
    echo "Usage: $0 [opts]"
    echo "Options:"
    echo " -h  |   Print this help"
    echo " -a  |   Audio loopback"
    echo " -v  |   Video loopback" 
    echo
    echo "Video loopback options:"
    echo " -s | Live source element to use[Default:$video_live_source]"
    echo " -z | Linux display standard (fbdev, v4l2) [Default:$dispStd]"
    echo " -d | Display device to use [Default:$dispDevice]"
    echo " -y | Video standard to use [Default:$videoStd]"
    echo " -o | Video display device to use [Default:$videoOut]"
    echo " -x | Use resizer [Default:$resizer]"
    echo " -c | Disable accel frame copy [Default:$accelFrameCopy]"
    if [ "${PLATFORM}" = "omap3530" ]; then
        echo " -r | Rotation angle [Default:$rotation]"
    fi              
    echo "Audio Encode options:"
    echo " -s | Live source element to use [Default:$audio_live_source] "
    echo " -z | Linux audio standard (oss,alsa) [Default:$soundStd]"
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

args=`getopt s:z:d:y:o:x:r:havxc $*`
if test $? != 0 ; then
    help;
fi

set -- $args
for i
do
    case "$i" in
        -a) shift; streamType="audio";;
        -v) shift; streamType="video";;
        -s) shift; src_elem=$1; shift;;
        -z) shift; std=$1; shift;;
        -d) shift; dispDevice=$1; shift;;
        -y) shift; videoStd=$1; shift;;
        -o) shift; videoOut=$1; shift;;
        -x) shift; resizer=TRUE;;
        -r) shift; rotation=$1; shift;;
        -c) shift; accelFrameCopy=FALSE;;
        -h) help;;
    esac
done

if [ "$streamType" = "" ]; then
    help;
fi

# main switch     
case "$streamType" in

    # parse audio encoder information 
    audio)
        # If live source is selected then check if user has passed source 
        # element value. If not, then use default.
        test -z $src_elem || src=${src_elem}
        ! test -z $src || src=${audio_live_source}
        execute "$GSTINSPECT $src"
        src_args=$audio_src_args;
  
        # set the sink 
        ! test -z $std || std=${soundStd}

        case "$std" in
            alsa)
                sink="alsasink" ;;
            oss)
                sink="osssink" ;;
            *)
                echo "invalid sound standard"
                help;;
        esac
        ;;

    video)
        # If live source is selected then check if user has passed source 
        # element value. If not, then use default.
        test -z $src_elem || src=${src_elem}
        ! test -z $src || src=${video_live_source}
        execute "$GSTINSPECT $src"
        src_args=$video_src_args

        test -z $std || dispStd=${std}
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

        sink="TIDmaiVideoSink displayStd=$dispStd displayDevice=$dispDevice videoStd=$videoStd videoOutput=$videoOut resizer=$resizer accelFrameCopy=$accelFrameCopy $video_sink_args"

        ;;
    *)
        help
        ;;
esac
 
echo "*********** Pipeline Settings *************"
echo "platform               = ${PLATFORM}"
echo "source                 = $src"
echo "source_args            = $src_args"
echo ""
