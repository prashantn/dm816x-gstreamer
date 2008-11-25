#!/bin/sh
#
# process_imgopts.sh
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
    echo "Usage: $0 -f <input file> -o <output file> -r <resolution> [opts]"
    echo "Options:"
    echo " -f  |   Input file"
    echo " -o  |   Output file"
    echo " -h  |   Print this help"
    echo
    echo "Image options:"
    echo " -p  |   Image encoder plugin to use [Default:$image_plugin]"
    echo " -i  |   Color space of input file (UYVY, YUV420P, YUV422P) [Default:$inputcolorSpace]"
    echo " -c  |   Color space of outpt file (UYVY, YUV420P, YUV422P) [Default:$outputcolorSpace]"
    echo " -r  |   Resolution of input and output image [Default:$resolution]"
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

args=`getopt f:p:o:r:i:c:h $*`
if test $? != 0 ; then
    help;
fi

set -- $args
for i
do
    case "$i" in
        -f) shift; inputFile=$1; shift;;
        -p) shift; image_plugin=$1; shift;;
        -o) shift; outputFile=$1; shift;;
        -r) shift; resolution=$1; shift;;
	-i) shift; inputcolorSpace=$1; shift;;
	-c) shift; outputcolorSpace=$1; shift;;
        -h) help;;
    esac
done

if [ "$inputFile" = "" ]; then
    echo "No input file specified"
    help;
fi

if [ ! -f $inputFile ]; then
    echo "ERROR: $inputFile does not exist !"
    exit 1
fi

if [ "$outputFile" = "" ]; then
    echo "No output file specified"
    help;
fi

if [ "$resolution" = "" ]; then
    echo "No resolution specified"
    help;
fi

execute "$GSTINSPECT $image_plugin"

image_plugin_args="resolution=$resolution iColorSpace=$inputcolorSpace oColorSpace=$outputcolorSpace"

echo "*********** Pipeline Settings *************"
echo "platform               = ${PLATFORM}"
echo "image_plugin           = $image_plugin"
echo "image_plugin_args      = $image_plugin_args"
echo ""
