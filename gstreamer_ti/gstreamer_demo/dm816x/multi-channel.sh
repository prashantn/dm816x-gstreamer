
###### Example 1 #############
# The pipeline decodes 2 channel H.264 video and discards the decoded data
src1="filesrc location=sample1_720p.264 ! typefind ! h264parse access-unit=true "
src2="filesrc location=sample2_720p.264 ! typefind ! h264parse access-unit=true "
gst-launch -v ${src1} ! queue2 ! omx_h264dec ! fakesink silent=true . ${src2} ! queue2 ! omx_h264dec ! fakesink silent=true

###### Example 2 #############
# The pipeline decodes 2 channel H.264 video, color convert and discards the data
src1="filesrc location=sample1_720p.264 ! typefind ! h264parse access-unit=true "
src2="filesrc location=sample2_720p.264 ! typefind ! h264parse access-unit=true "
gst-launch -v ${src1} ! queue2 ! omx_h264dec ! queue2 ! omx_scaler ! fakesink silent=true . ${src2} ! queue2 ! omx_h264dec ! queue2 ! omx_scaler ! fakesink silent=true

###### Example 3 ############
# The pipeline decodes 2 channel H.264 video, scale to QVGA, compsosite buffers using ARM videomixer2 element and display
# using OMX video sink
src1="filesrc location=sample1_720p.264 ! typefind ! h264parse access-unit=true ! queue2 ! omx_h264dec ! queue2 ! omx_scale "
src2="filesrc location=sample2_720p.264 ! typefind ! h264parse access-unit=true ! queue2 ! omx_h264dec ! queue2 ! omx_scale "
gst-launch -v videomixer2 name=mix ! queue2 ! omx_ctrl ! queue2 ! omx_videosink \
 ${src1} ! video/x-raw-yuv,width=320,height=240 ! queue2 ! videobox top=0 left=0 ! mix. \
 ${src2} ! video/x-raw-yuv,width=320,height=240 ! queue2 ! videobox top=240 left=240 ! mix.


