#!/bin/sh

if [ $# -ne 1 ]
then
   echo 
   echo "Usage:  $0 <release number>"
   echo 
   echo "Example:  $0 0.99.00"
   echo 
   exit 0
fi

REL=$1
shift

REL_US=`echo $REL | sed s#[.]#_#g`

echo "Creating release ${REL} from TAG_RELEASE_${REL_US}"

FULL_NAME="gst-ti-plugin-full-${REL}"
MIN_NAME="gst-ti-plugin-minimal-${REL}"

umask 0002
svn export https://omapzoom.org/svn/gstreamer_ti/tags/TAG_RELEASE_${REL_US}/gstreamer_ti

ln -s gstreamer_ti ${FULL_NAME}
ln -s gstreamer_ti/ti_build/ticodecplugin ${MIN_NAME}
tar chf - ${FULL_NAME} | gzip --best -c > ${FULL_NAME}.tar.gz
tar chf - ${MIN_NAME}  | gzip --best -c > ${MIN_NAME}.tar.gz

