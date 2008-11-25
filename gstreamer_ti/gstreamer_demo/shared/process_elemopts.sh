#!/bin/sh
#
# process_elemopts.sh
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
    echo "Usage: $0 -a|-v -f <file name> [opts]"
    echo "Options:"
    echo " -a  |   File contains an audio stream"
    echo " -v  |   File contains a video stream"
    echo " -f  |   Input filename"
    echo
    echo "Audio stream options:"
    echo " -p  |   Audio plugin to use [Default:$audio_plugin]"
    echo " -n  |   Audio codec name to use [Default:$audiocodecName]"
    echo " -s  |   Linux sound standard to use [Default:$soundStd]"
    echo
    echo "Video stream options:"
    echo " -p  |   Video plugin to use [Default:$video_plugin]"
    echo " -n  |   Video codec name [Default:$videocodecName]"
    echo " -s  |   Linux display standard to use [Default:$dispStd]"
    echo " -d  |   Display device [Default:$dispDevice]"
    echo " -y  |   Video standard [Default:$videoStd]"
    echo " -o  |   Display output [Default:$videoOut]"
    echo " -x  |   Use resizer to scale image[Default:$resizer] "
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
    $* >/dev/null
    if [ $? -ne 0 ]; then
        echo "ERROR: Failed to execute '$*'"
        exit 1
    fi
}

if [ "${PLATFORM}" = "omap3530" ]; then
    args=`getopt f:p:n:s:d:y:o:r:xav $*`
else
    args=`getopt f:p:n:s:d:y:o:xav $*`
fi

if test $? != 0 ; then
    help
fi

set -- $args

for i
do
    case "$i" in
        -f) shift; fileName=$1; shift;;
        -a) shift; streamType="audio" ;;
        -v) shift; streamType="video" ;;
        -p) shift; plugin=$1; shift ;;
        -n) shift; codecname=$1; shift ;;
        -s) shift; std=$1; shift ;;
        -d) shift; dispDevice=$1; shift ;;
        -y) shift; videoStd=$1; shift ;;
        -o) shift; videoOut=$1; shift ;;
        -x) shift; resizer=TRUE ;;
        -r) shift; rotation=$1; shift;;
        -h) shift; help ;;
    esac
done

if [ "$fileName" = "" ]; then
    help
fi

if [ ! -f $fileName ]; then
    echo "ERROR: $fileName does not exist !"
    exit 1
fi


# main loop
case "$streamType" in
    audio)
        test -z $std || soundStd=$std
        case "$soundStd" in
            oss)    
                execute "$GSTINSPECT osssink"
                sink="audioconvert ! osssink"
                ;;
            alsa) 
                execute "$GSTINSPECT alsasink"
                if [ "${PLATFORM}" = "omap3530" ]; then
                    sink="audioconvert ! audio/x-raw-int, width=16, depth=16  ! alsasink"
                else
                    sink="alsasink"
                fi
                ;;
            *)    
                echo "ERROR: Invalid sound standard"
                exit 1
                ;;
        esac 
        test -z $plugin || audio_plugin=$plugin
        plugin=$audio_plugin
        test -z $codecname || audiocodecName=$codecname
        test -z $audiocodecName || plugin_option="genTimeStamps=FALSE engineName=decode codecName=$audiocodecName"
        echo "******* Audio stream *********"
        echo "soundStd       = $soundStd"
        echo "plugin         = $plugin"       
        echo "plugin_option  = $plugin_option"
        ;;
    video)
        test -z $std || dispStd=$std
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
                echo "ERROR: Invalid display standard"
                exit 1
        esac
        test -z $plugin || video_plugin=$plugin
        plugin="$video_plugin"
        test -z $codecname ||videocodecName=$codecname
        test -z $videocodecName ||plugin_option="engineName=decode codecName=$videocodecName"
        echo "*********** Video stream ************"
        echo "plugin        = $plugin"
        echo "plugin_option = $plugin_option"
        echo "dispStd       = $dispStd"
        echo "dispDevice    = $dispDevice"
        echo "videoStd      = $videoStd"
        echo "videoOut      = $videoOut"
        echo "resizer       = $resizer"
        if [ "${PLATFORM}" = "omap3530" ]; then
            rotation_opts="rotation=$rotation"
        fi

        sink="TIDmaiVideoSink displayStd=$dispStd displayDevice=$dispDevice videoStd=$videoStd videoOutput=$videoOut resizer=$resizer ${rotation_opts}"  
        ;;
    *)
        help;
        ;;
esac

execute "$GSTINSPECT $plugin"
