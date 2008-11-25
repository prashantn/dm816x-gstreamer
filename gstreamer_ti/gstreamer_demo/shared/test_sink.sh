#!/bin/sh
#
# test_sink.sh
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
# Defaults used if not overridden by target
#------------------------------------------------------------------------------
if [ "${inputWidth}" = "" ]; then
inputWidth=720
fi
if [ "${inputHeight}" = "" ]; then
inputHeight=480
fi

#------------------------------------------------------------------------------
# Print help
#------------------------------------------------------------------------------
help ()
{
    echo "Usage: $0 [opts] "
    echo "Options:"
    echo " -v   | Test video display (see below)"
    echo " -a   | Test audio output (see below)"
    echo 
    echo "Audio stream options:"
    echo " -s   | Linux sound standand to use [Default:$soundStd]"
    echo
    echo "Video stream options:"
    echo " -s   | Linux display standard to use [Default:$dispStd]"
    echo " -d   | Display device to use [Default:$dispDevice]"
    echo " -y   | Video standard to use [Default:$videoStd]"
    echo " -o   | Video display device to use [Default:$videoOut]"
    echo " -x   | Use resizer [Default:$resizer]"
    if [ "${PLATFORM}" = "omap3530" ]; then
        echo " -r   | Rotation angle [Default:$rotation]"
    fi              
    echo 
    echo "Video input options:"
    echo " -w   | Input image width to use [Default:$inputWidth]"
    echo " -t   | Input image height to use [Default:$inputHeight]"
    echo
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

args=`getopt s:d:y:o:r:avxw:t:h $*`

if test $? != 0 ; then
    help
fi

for i
do
   case "$i" in
    -v) shift; streamType="video" ;;
    -a) shift; streamType="audio" ;;
    -s) shift; std=$1; shift;;
    -d) shift; dispDevice=$1; shift;;
    -y) shift; videoStd=$1; shift;;
    -o) shift; videoOut=$1; shift;;
    -r) shift; rotation=$1; shift;;
    -x) shift; resizer=TRUE;;
    -w) shift; inputWidth=$1; shift;;
    -t) shift; inputHeight=$1; shift;;
    -h) shift; help ;;
    esac
done

# main loop
case "$streamType" in
    audio)
        test -z $std || soundStd=$std
        case "$soundStd" in
            oss)    
                execute "$GSTINSPECT osssink"
                dstsink="audioconvert ! volume volume=5 ! osssink"
                ;;
            alsa) 
                execute "$GSTINSPECT alsasink"
                if [ "${PLATFORM}" = "omap3530" ]; then
                    dstsink="audioconvert ! audio/x-raw-int, width=16, depth=16  ! alsasink"
                else
                    dstsink="alsasink"
                fi
                ;;
            *)    
                echo "ERROR: Invalid sound standard"
                exit 1
                ;;
        esac        
        # generate sine wave
        srcsink="audiotestsrc wave=3"
        echo "***** Testing audio ****"
        echo "soundStd = $soundStd"
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
        echo "***** Testing video ****"
        echo "displayStd  = $dispStd"
        echo "displDevice = $dispDevice"
        echo "videoStd    = $videoStd"
        echo "videoOut    = $videoOut"
        echo "resizer     = $resizer"
        echo "inputWidth  = $inputWidth"
        echo "inputHeight = $inputHeight"
        if [ "${PLATFORM}" = "omap3530" ]; then
            echo "Rotation  = $rotation"
            rotation_args="rotation=$rotation"
        fi
        dstsink="TIDmaiVideoSink displayStd=$dispStd displayDevice=$dispDevice videoStd=$videoStd videoOutput=$videoOut resizer=$resizer ${rotation_args}"  
        srcsink="videotestsrc ! video/x-raw-yuv, format=(fourcc)UYVY, width=${inputWidth}, height=${inputHeight}"
        ;;
    *)
        help
        ;;
esac

#------------------------------------------------------------------------------
# Enable debugging flags (optional)
#------------------------------------------------------------------------------
#DEBUG="--gst-debug-no-color --gst-debug=GST_PLUGIN_LOADING:*"
#DEBUG="--gst-debug-no-color --gst-debug=TIDmaiVideoSink:5"
#DEBUG=-v

#------------------------------------------------------------------------------
# Execute the pipeline
#------------------------------------------------------------------------------
. ../shared/run_pipe.sh
run_pipe "${GSTLAUNCH} ${DEBUG} ${srcsink} ! ${dstsink}"
exit $?
