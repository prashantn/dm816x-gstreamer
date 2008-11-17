export LD_LIBRARY_PATH=.:/opt/system_files_gstreamer:/opt/gstreamer/lib
export GST_PLUGIN_PATH=/opt/gstreamer/lib/gstreamer-0.10/
export PATH=$PATH:/opt/gstreamer/bin

DEBUG="--gst-debug-no-color --gst-debug=TIImgenc1:4"
#DEBUG="--gst-debug-no-color --gst-debug=*:5"
#DEBUG="--gst-debug-no-color --gst-debug=TIDmaiBufTrans:5"

gst-launch ${DEBUG} filesrc location=$1 ! TIImgenc1 engineName=encode codecName=jpegenc resolution=720x480 iColorSpace=UYVY oColorSpace=422P ! filesink location="./test.jpeg"
