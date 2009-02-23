#!/bin/sh
#
# process_elemencopts.sh
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
    echo "Usage: $0 -f <input file> -o <output file> [opts]"
    echo "Options:"
    echo " -f  |   Input file"
    echo " -o  |   Output file"
    echo " -l  |   Use live source"
    echo " -h  |   Print this help"
    echo " -a  |   Use audio encoder"
    echo " -v  |   Use video encoder" 
    echo
    echo "Video Encode options:"
    echo " -s | Live source element to use [Default:$video_live_source]"
    echo " -p | Video encoder plugin to use [Default:$video_encoder]"
    echo " -n | Video encoder codec to use [Default:$video_encoder_codec_name]"
    echo " -r | Image resolution  [Default:$video_encoder_resolution](if input file is used)"
    echo " -i | Color space (UYVY,Y8C8) [Default:$video_encoder_color_space] (if input file is used)"
    echo 
    echo "Audio Encode options:"
    echo " -s | Live source element to use [Default:$audio_live_source] "
    echo " -p | Audio encoder plugin to use [Default:$audio_encoder]"
    echo " -n | Audio encoder codec to use [Default:$audio_encoder_codec_name]"
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

args=`getopt f:p:o:s:r:i:n:havl $*`
if test $? != 0 ; then
    help;
fi

set -- $args
for i
do
    case "$i" in
        -a) shift; streamType="audio";;
        -v) shift; streamType="video";;
        -l) shift; live_source="live";;
        -f) shift; inputFile=$1; shift;;
        -o) shift; outputFile=$1; shift;;
        -s) shift; src_elem=$1; shift;;
        -p) shift; plugin=$1; shift;;
        -r) shift; video_encoder_resolution=$1; shift;;
        -i) shift; video_encoder_color_space=$1; shift;;
        -n) shift; codecname=$1; shift;;
        -h) help;;
    esac
done

if [ "$outputFile" = "" ]; then
    echo "No output file specified"
    help;
fi

# If live source is not selected then fall back to input file method
if [ "$live_source" = "" ]; then
    
    if [ "$inputFile" = "" ]; then
        echo "No input file specified"
        help;
    fi

    if [ ! -f $inputFile ]; then
        echo "ERROR: $inputFile does not exist !"
        exit 1
    fi
    
    # set source information 
    src="filesrc"
    src_args="location=$inputFile"
fi


# main switch     
case "$streamType" in

    # parse audio encoder information 
    audio)
        # If live source is selected then check if user has passed source 
        # element value. If not, then use default.
        test -z $live_source || test -z $src_elem || src=${src_elem}
        ! test -z $src || src=${audio_live_source}
        execute "$GSTINSPECT $src"
        
        # if live source is seleted then get source property (if any)
        test -z $live_source || src_args=$audio_src_args
        
        # If plugin value is not passed then use default.
        test -z $plugin || encoder_plugin=$plugin
        ! test -z $plugin || encoder_plugin=$audio_encoder
        execute "$GSTINSPECT $encoder_plugin"

        # if codecname is passed then we assume its TI encoder and set the
        # codecname and other informations
        test -z $codecname || encoder_plugin_args="codecName=$codecname engineName=encode $video_encoder_args"

        ;;

    video)
        # If live source is selected then check if user has passed source 
        # element value. If not, then use default.
        test -z $live_source || test -z $src_elem || src=${src_elem}
        ! test -z $src || src=${video_live_source}
        execute "$GSTINSPECT $src"

        # if live source is seleted then get source property (if any)
        test -z $live_source || src_args=$video_src_args

        # If plugin value is not passed then use default.
        test -z $plugin || encoder_plugin=$plugin
        ! test -z $plugin || encoder_plugin=$video_encoder
        execute "$GSTINSPECT $encoder_plugin"

        # if codecname is passed then we assume its TI encoder and set the
        # codecname and other informations
        test -z $codecname || video_encoder_codec_name=$codecname
        encoder_plugin_args="codecName=$video_encoder_codec_name engineName=encode $video_encoder_args" 
    
        # if live source is not selected then ad colorspace and resolution 
        # properties.
        ! test -z $live_source || encoder_plugin_args="$encoder_plugin_args iColorSpace=$video_encoder_color_space resolution=$video_encoder_resolution $video_encoder_args"
        ;;
    *)
        help
        ;;
esac
 
echo "*********** Pipeline Settings *************"
echo "platform               = ${PLATFORM}"
echo "source                 = $src"
echo "source_args            = $src_args"
echo "encoder_plugin         = $encoder_plugin"
echo "encoder_plugin_args    = $encoder_plugin_args"
echo ""
