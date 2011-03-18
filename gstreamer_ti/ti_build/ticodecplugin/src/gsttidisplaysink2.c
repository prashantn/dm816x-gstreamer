/*
 * gsttidisplaysink2.c
 *
 * This file implements "tidisplaysink2" element.
 *
 * Example usage:
 *     gst-launch videotestsrc ! tidisplaysink2 -v
 *
 * Original Author:
 *     Brijesh Singh, Texas Instruments, Inc.
 *
 * Copyright (C) 2010-2011 Texas Instruments Incorporated - http://www.ti.com/
 *
 * This program is free software; you can redistribute it and/or modify 
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation version 2.1 of the License.
 *
 * This program is distributed #as is# WITHOUT ANY WARRANTY of any kind,
 * whether express or implied; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 */

#include <gst/gst.h>
#include <gst/base/gstbasesink.h>
#include <gst/video/gstvideosink.h>
#include <gst/video/video.h>
#include <string.h>
#include <gst/gstmarshal.h>
#include <linux/videodev.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>

#include "gsttidisplaysink2.h"
#include "gstticommonutils.h"

/* Define platform specific defaults */
#define DEFAULT_DISPLAY_OUTPUT    "system"
#define DEFAULT_VIDEO_STD         "auto"

#if defined(Platform_dm365) || defined(Platform_dm368)
  #define DEFAULT_NUM_BUFS          3
  #define MAX_NUM_BUFS              16
  #define MIN_NUM_BUFS              3
  #define DEFAULT_DEVICE            "/dev/video2"
  #define DEFAULT_MMAP_BUFFER       FALSE
  #define DEFAULT_DMA_COPY          TRUE
  #define SINK_CAPS	GST_VIDEO_CAPS_YUV("UYVY")";" GST_VIDEO_CAPS_YUV("NV12")
  #define REGISTER_BUFFER_ALLOC     TRUE
#elif defined(Platform_omapl138)
  #define DEFAULT_DEVICE            "/dev/fb0"
  #define DEFAULT_NUM_BUFS          1
  #define MAX_NUM_BUFS              2
  #define MIN_NUM_BUFS              1
  #define DEFAULT_MMAP_BUFFER       TRUE
  #define DEFAULT_DMA_COPY          TRUE
  #define SINK_CAPS	GST_VIDEO_CAPS_RGB_16
  #define REGISTER_BUFFER_ALLOC     FALSE
#else
  #define DEFAULT_NUM_BUFS          3
  #define MAX_NUM_BUFS              16
  #define MIN_NUM_BUFS              3
  #define DEFAULT_DEVICE            "/dev/video1"
  #define DEFAULT_MMAP_BUFFER       FALSE
  #define DEFAULT_DMA_COPY          TRUE
  #define SINK_CAPS	 GST_VIDEO_CAPS_YUV("UYVY")
  #define REGISTER_BUFFER_ALLOC     TRUE
#endif


static void *parent_class;

/* Define sink and src pad capabilities. */
static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE(
  "sink",
  GST_PAD_SINK,
  GST_PAD_ALWAYS,
  GST_STATIC_CAPS
  (
   SINK_CAPS 
  )
);

enum
{
  PROP_0,
  PROP_DEVICE,
  PROP_ROTATE,
  PROP_NUMBUFS,
  PROP_OVERLAY_TOP,
  PROP_OVERLAY_LEFT,
  PROP_OVERLAY_WIDTH,
  PROP_OVERLAY_HEIGHT,
  PROP_MMAP_BUFFER,
  PROP_DISPLAY_OUTPUT,
  PROP_DISPLAY_STD,
  PROP_DMA_COPY,
  PROP_DEVICE_FD
};

enum
{
  OVERLAY_TOP_SET = 0x01,
  OVERLAY_LEFT_SET = 0x02,
  OVERLAY_WIDTH_SET = 0x04,
  OVERLAY_HEIGHT_SET = 0x08
};

/* Declare variable used to categorize GST_LOG output */
GST_DEBUG_CATEGORY_STATIC (gst_tidisplaysink2_debug);
#define GST_CAT_DEFAULT gst_tidisplaysink2_debug

static gboolean
setcaps(GstBaseSink *base, GstCaps *caps)
{
    GstTIDisplaySink2 *sink = (GstTIDisplaySink2 *)base;
    GstStructure *structure;
    guint32             fourcc;
    int width, height;
    const gchar  *mime;

    GST_LOG_OBJECT(sink,"setcaps begin\n");
    structure = gst_caps_get_structure(caps, 0);
    mime      = gst_structure_get_name(structure);

    gst_structure_get_int(structure, "width", &width);
    gst_structure_get_int(structure, "height", &height);
    
    sink->dAttrs.width = width;
    sink->dAttrs.height = height;
    
    if (!strncmp(mime, "video/x-raw-yuv", 15)) {
        gst_structure_get_fourcc(structure, "format", &fourcc);

        switch (fourcc) {
            case GST_MAKE_FOURCC('U', 'Y', 'V', 'Y'):
                sink->dAttrs.colorSpace = ColorSpace_UYVY;
                break;
            case GST_MAKE_FOURCC('N', 'V', '1', '2'):
                sink->dAttrs.colorSpace = ColorSpace_YUV420PSEMI;
                break;
            default:
                GST_ERROR("unsupported fourcc\n");
                return FALSE;
        }
        sink->dAttrs.displayStd = Display_Std_V4L2;
    }

    if (!strncmp(mime, "video/x-raw-rgb", 15)) {
        sink->dAttrs.colorSpace = ColorSpace_RGB565;
        sink->dAttrs.displayStd = Display_Std_FBDEV;
    }

    GST_LOG_OBJECT(sink,"setcaps end\n");
    return TRUE;
}

static gboolean 
alloc_bufTab(GstBaseSink *base, guint size, GstCaps *caps)
{
    GstStructure *structure;
    BufferGfx_Attrs gfx  = BufferGfx_Attrs_DEFAULT;
    GstTIDisplaySink2 *sink = (GstTIDisplaySink2 *)base;
    guint32 fourcc;
    int width, height;
    const gchar  *mime;

    structure = gst_caps_get_structure(caps, 0);
    mime      = gst_structure_get_name(structure);

    gst_structure_get_int(structure, "width", &width);
    gst_structure_get_int(structure, "height", &height);
    gfx.dim.width = width;
    gfx.dim.height = height;
    
    if (!strncmp(mime, "video/x-raw-yuv", 15)) {
        /* Map input buffer fourcc to dmai color space  */
        gst_structure_get_fourcc(structure, "format", &fourcc);

        switch (fourcc) {
            case GST_MAKE_FOURCC('U', 'Y', 'V', 'Y'):
                gfx.colorSpace = ColorSpace_UYVY;
                break;
            case GST_MAKE_FOURCC('N', 'V', '1', '2'):
                gfx.colorSpace = ColorSpace_YUV420PSEMI;
                break;
            default:
                GST_ERROR("unsupported fourcc\n");
                return FALSE;
        }

    }

    gfx.dim.lineLength = BufferGfx_calcLineLength(width, gfx.colorSpace);

    #if defined(Platform_dm365) || defined(Platform_dm368)
        gfx.dim.lineLength = Dmai_roundUp(gfx.dim.lineLength, 32);
    #endif

    if (size <= 0) {
        size = gst_ti_calc_buffer_size(gfx.dim.width,
            gfx.dim.height, gfx.dim.lineLength, gfx.colorSpace);
    }

    gfx.bAttrs.useMask = gst_tidmaibuffer_VIDEOSINK_FREE;
    sink->dAttrs.numBufs = sink->numbufs;
    sink->hBufTab = gst_tidmaibuftab_new(sink->dAttrs.numBufs, size, 
        BufferGfx_getBufferAttrs(&gfx));
    gst_tidmaibuftab_set_blocking(sink->hBufTab, FALSE);

    return TRUE;
}

static GstFlowReturn
buffer_alloc(GstBaseSink *base, guint64 offset, guint size, GstCaps *caps, 
    GstBuffer **buf)
{
    GstTIDisplaySink2 *sink = (GstTIDisplaySink2 *)base;
    Buffer_Handle       hDispBuf    = NULL;

    GST_LOG_OBJECT(sink,"buffer alloc begin");

    if (sink->mmap_buffer) {
        sink->mmap_buffer = FALSE;
        GST_ELEMENT_ERROR(sink, RESOURCE, FAILED,
        ("can not use driver mmaped buffer for pad allocation"), (NULL));
        return GST_FLOW_UNEXPECTED;
    }

    if (!sink->hBufTab) {
        if (!alloc_bufTab(base, size, caps)) {
            GST_ELEMENT_ERROR(sink, RESOURCE, FAILED,
            ("Failed to allocate peer buffer"), (NULL));
            return GST_FLOW_UNEXPECTED;
        }

    }

    /* Get a buffer from the BufTab */
    if (!(hDispBuf = gst_tidmaibuftab_get_buf(sink->hBufTab))) {
        /* Wait for free buffer */
        gst_tidmaibuftab_set_blocking(sink->hBufTab, TRUE);
        hDispBuf = gst_tidmaibuftab_get_buf(sink->hBufTab);
        gst_tidmaibuftab_set_blocking(sink->hBufTab, FALSE);
    }

    if (!hDispBuf) {
        GST_ELEMENT_ERROR(sink, RESOURCE, FAILED,
        ("Failed to get free buffer for pad allocation"), (NULL));
        return GST_FLOW_UNEXPECTED;
    }

    /* Return the display buffer */
    BufferGfx_resetDimensions(hDispBuf);
    Buffer_freeUseMask(hDispBuf, gst_tidmaibuffer_DISPLAY_FREE);
    *buf = gst_tidmaibuffertransport_new(hDispBuf, NULL);
    gst_buffer_set_caps(*buf, caps);

    GST_LOG_OBJECT(sink, "allocated buffer %p (%ld)", Buffer_getUserPtr(hDispBuf), 
        Buffer_getSize(hDispBuf));
    GST_LOG_OBJECT(sink, "buffer alloc end");
    return GST_FLOW_OK;
}

static gboolean
start(GstBaseSink *base)
{
    GST_LOG("start begin");
    /* nothing to do here */
    GST_LOG("start end");

    return TRUE;
}

static gboolean
stop(GstBaseSink *base)
{
    GstTIDisplaySink2 *sink = (GstTIDisplaySink2 *)base;

    GST_LOG_OBJECT(sink,"stop begin");
    if (sink->hBufTab) {
           gst_tidmaibuftab_unref(sink->hBufTab);
    }

    if (sink->hFc) {
        Framecopy_delete(sink->hFc);
    }

    if (sink->hDisplay) {
        Display_delete(sink->hDisplay);
    }

    GST_LOG_OBJECT(sink,"stop end");
    return TRUE;
}

static int 
xcopy(GstBaseSink *base, GstBuffer *buf, Buffer_Handle hOutBuf)
{
    Framecopy_Attrs fcAttrs = Framecopy_Attrs_DEFAULT;
    GstTIDisplaySink2 *sink = (GstTIDisplaySink2 *)base;
    Buffer_Handle   hInBuf, tmpBuf = NULL;
    BufferGfx_Attrs gfxAttrs   = BufferGfx_Attrs_DEFAULT;

    GST_LOG_OBJECT(sink,"copy begin");
    /* Check if its dmai transport buffer */
    if (GST_IS_TIDMAIBUFFERTRANSPORT(buf)) {
        /* get the dmai buffer handle and configure framecopy to use HW accel */
        hInBuf = GST_TIDMAIBUFFERTRANSPORT_DMAIBUF(buf);
        fcAttrs.accel = sink->dma_copy;

        /* on omap3, use SDMA based copy */
        #if defined(Platform_omap3530) || defined(Platform_dm3730)
        fcAttrs.sdma  = TRUE;
        #endif
    }
    else {
        /* Create a reference dmai buffer */
        gfxAttrs.dim.width = sink->dAttrs.width;
        gfxAttrs.dim.height = sink->dAttrs.height;
        gfxAttrs.colorSpace = sink->dAttrs.colorSpace;
        gfxAttrs.dim.lineLength = BufferGfx_calcLineLength(sink->dAttrs.width, sink->dAttrs.colorSpace);
        #if defined(Platform_dm365) || defined(Platform_dm368)
            gfxAttrs.dim.lineLength = Dmai_roundUp(gfxAttrs.dim.lineLength, 32);
        #endif
        gfxAttrs.bAttrs.reference = TRUE;

        tmpBuf= Buffer_create(GST_BUFFER_SIZE(buf),
                BufferGfx_getBufferAttrs(&gfxAttrs));
        if (tmpBuf == NULL) {
            GST_ERROR("failed to create  buffer\n");
            return FALSE;
        }
        Buffer_setUserPtr(tmpBuf, (Int8*)GST_BUFFER_DATA(buf));
        Buffer_setNumBytesUsed(tmpBuf, GST_BUFFER_SIZE(buf));
        hInBuf = tmpBuf;

        /* use non accel framecopy */
        if (sink->dma_copy) {
            GST_WARNING_OBJECT(sink, "DMA copy is not possible on non contiguous buffer, defaulting to slow copy\n");
        }

        fcAttrs.accel = FALSE;
    }
    
    if (sink->hFc == NULL) {
        GST_INFO_OBJECT(sink,"Creating %s framecopy handle\n", fcAttrs.accel ? "accel":"non-accel");

        sink->hFc = Framecopy_create(&fcAttrs);
        if (sink->hFc == NULL) {
             GST_ELEMENT_ERROR(sink, RESOURCE, FAILED,
            ("Failed to create framecopy handle\n"), (NULL));
            return FALSE;
        }
        g_object_set(sink, "dma-copy", fcAttrs.accel ? TRUE : FALSE, NULL);
    }

    if (Framecopy_config(sink->hFc, hInBuf, hOutBuf) < 0) {
        GST_ELEMENT_ERROR(sink, RESOURCE, FAILED,
        ("Failed to config framecopy\n"), (NULL));
        return FALSE;
    }

    if (Framecopy_execute(sink->hFc, hInBuf, hOutBuf) < 0) {
        GST_ELEMENT_ERROR(sink, RESOURCE, FAILED,
        ("Failed to config framecopy\n"), (NULL));
        return FALSE;
    }

    if (tmpBuf) {
        Buffer_delete(tmpBuf);
    }

    GST_LOG_OBJECT(sink,"copy end");
    return TRUE;
}

static char*
dmai_colorspace_string(ColorSpace_Type colorspace)
{
    switch(colorspace) {
        case ColorSpace_YUV420PSEMI:
            return "YUV420PSEMI";
        case ColorSpace_YUV422PSEMI:
            return "YUV422PSEMI";
        case ColorSpace_UYVY:
            return "UYVY";
        case ColorSpace_RGB888:
            return "RGB888";
        case ColorSpace_RGB565:
            return "RGB565";
        case ColorSpace_2BIT:
            return "2-bit";
        case ColorSpace_YUV420P:
            return "YUV420P";
        case ColorSpace_YUV422P:
            return "YUV422P";
        case ColorSpace_YUV444P:
            return "YUV444P";
        case ColorSpace_GRAY:
            return "Gray";
        case ColorSpace_COUNT:
        case ColorSpace_NOTSET:
            return "unknown";
    }
    return "unknown";
}

static gchar*
dmai_display_output_str (int id)
{
    if (id == Display_Output_SVIDEO)
        return "svideo";

    if (id == Display_Output_COMPOSITE)
        return "composite";

    if (id == Display_Output_COMPONENT)
        return "component";

    if (id == Display_Output_LCD)
        return "lcd";

    if (id == Display_Output_DVI)
        return "dvi";

    return "system";
}

static int 
dmai_display_output (const gchar* str)
{
    if (!strcmp(str, "svideo"))
        return Display_Output_SVIDEO;

    if (!strcmp(str, "composite"))
        return Display_Output_COMPOSITE;
    
    if (!strcmp(str, "component"))
        return Display_Output_COMPONENT;

    if (!strcmp(str, "lcd"))
        return Display_Output_LCD;

    if (!strcmp(str, "dvi"))
        return Display_Output_DVI;

    return Display_Output_SYSTEM;
}

static int 
dmai_video_std (const gchar* str)
{
    if (!strcmp(str, "ntsc"))
        return VideoStd_D1_NTSC;

    if (!strcmp(str, "pal"))
        return VideoStd_D1_PAL;

    if (!strcmp(str, "480p"))
        return VideoStd_480P;

    if (!strcmp(str, "720p"))
        return VideoStd_720P_60;

    if (!strcmp(str, "1080i"))
        return VideoStd_1080I_30;

    return VideoStd_AUTO;
}

static gchar*
dmai_video_std_str (int id)
{
    if (id == VideoStd_D1_NTSC)
        return "ntsc";

    if (id == VideoStd_D1_PAL)
        return "pal";

    if (id == VideoStd_480P)
        return "480p";

    if (id == VideoStd_720P_60)
        return "720p";

    if (id == VideoStd_1080I_30)
        return "1080i";

    return "auto";
}

static GstFlowReturn
preroll(GstBaseSink *base, GstBuffer *buffer)
{
    GST_LOG("preroll begin");
    /* nothing to do */
    GST_LOG("preroll end");

    return GST_FLOW_OK;
}

static gboolean
display_create(GstBaseSink *base, GstBuffer *buffer)
{
    GstTIDisplaySink2 *sink = (GstTIDisplaySink2 *)base;
    int width;

    GST_LOG_OBJECT(sink,"display_create begin");
    if (sink->hDisplay == NULL) {

        sink->dAttrs.rotation = sink->rotate;
        sink->dAttrs.numBufs = sink->numbufs;
        sink->dAttrs.displayDevice = sink->device;
        sink->dAttrs.videoOutput = dmai_display_output(sink->display_output);
        sink->dAttrs.videoStd = dmai_video_std(sink->video_std);

        if (sink->dAttrs.colorSpace == ColorSpace_YUV420PSEMI && sink->dAttrs.delayStreamon) {
            width = sink->dAttrs.width;
            sink->dAttrs.width = width - (Dmai_roundUp(width,32) - width);
            GST_DEBUG("Rounding width from %d to %d\n", width, sink->dAttrs.width);
	    }

        if (sink->hBufTab == NULL && !sink->mmap_buffer) {
            if (!alloc_bufTab(base, 0, gst_buffer_get_caps(buffer))) {        
                GST_ELEMENT_ERROR(sink, RESOURCE, FAILED,
                ("Failed to allocate buffer\n"), (NULL));
                return FALSE;
            }
        }

        GST_INFO_OBJECT(sink, "Creating display device: \n \
          displayStd    = %s \n \
          rotation      = %d \n \
          numBufs       = %d \n \
          device        = %s \n \
          width         = %d \n \
          height        = %d \n \
          delayStreamon = %d \n \
          userAllocBuf  = %s \n \
          colorSpace    = %s \n \
          videoStd      = %s \n \
          videoOutput   = %s",
          sink->dAttrs.displayStd == Display_Std_V4L2 ? "V4L2" : "FBDEV",
          sink->dAttrs.rotation, sink->dAttrs.numBufs,
          sink->dAttrs.displayDevice, sink->dAttrs.width, sink->dAttrs.height,
          sink->dAttrs.delayStreamon, GST_TIDMAIBUFTAB_BUFTAB(sink->hBufTab) ? 
          "TRUE" : "FALSE",
          dmai_colorspace_string(sink->dAttrs.colorSpace), 
          dmai_video_std_str(sink->dAttrs.videoStd), 
          dmai_display_output_str(sink->dAttrs.videoOutput));

        sink->hDisplay = Display_create(GST_TIDMAIBUFTAB_BUFTAB(sink->hBufTab),
                             &sink->dAttrs);

        if (sink->hDisplay == NULL) {
            GST_ELEMENT_ERROR(sink, RESOURCE, FAILED,
            ("Failed to create display handle\n"), (NULL));
            return FALSE;
        }
        sink->fd = Display_getHandle(sink->hDisplay);
    }

    g_object_set(sink, "device", sink->dAttrs.displayDevice, NULL);
    g_object_set(sink, "video-standard", dmai_video_std_str(sink->dAttrs.videoStd), NULL);
    g_object_set(sink, "display-output", dmai_display_output_str(sink->dAttrs.videoOutput), NULL);
    g_object_set(sink, "queue-size", sink->dAttrs.numBufs, NULL);
    g_object_set(sink, "mmap-buffer", GST_TIDMAIBUFTAB_BUFTAB(sink->hBufTab) ? FALSE : TRUE, NULL);

    GST_LOG_OBJECT(sink,"display_create end");
    return TRUE;
}

static gboolean
update_overlay_fields(GstTIDisplaySink2 *sink)
{
    struct v4l2_format format;

    GST_LOG_OBJECT(sink,"update_overlay_fields begin");
    if (sink->hDisplay == NULL)
        return FALSE;
    
    if (sink->overlay_set) {

        memset (&format, 0x00, sizeof (struct v4l2_format));
        format.type = V4L2_BUF_TYPE_VIDEO_OVERLAY;

        if (ioctl(sink->fd, VIDIOC_G_FMT, &format) < 0) {
            GST_ERROR("Failed to execute VIDIOC_G_FMT\n");
            return FALSE;
        }

        if (sink->overlay_set & OVERLAY_TOP_SET)  
            format.fmt.win.w.top = sink->overlay_top;
        if (sink->overlay_set & OVERLAY_LEFT_SET)  
            format.fmt.win.w.left = sink->overlay_left;
        if (sink->overlay_set & OVERLAY_WIDTH_SET) 
            format.fmt.win.w.width = sink->overlay_width;
        if (sink->overlay_set & OVERLAY_HEIGHT_SET)  
            format.fmt.win.w.height = sink->overlay_height;

        GST_INFO_OBJECT(sink, "Setting overlay top=%d, left=%d, width=%d, height=%d",
            format.fmt.win.w.top, format.fmt.win.w.left,
            format.fmt.win.w.width, format.fmt.win.w.height);

        if (ioctl(sink->fd, VIDIOC_S_FMT, &format) < 0) {
            GST_ERROR("Failed to execute VIDIOC_G_FMT\n");
            return FALSE;
        }
        sink->overlay_set = 0;
	
    }

    GST_LOG_OBJECT(sink,"update_overlay_fields end");
    return TRUE;
}

static GstFlowReturn
render(GstBaseSink *base, GstBuffer *buffer)
{
    GstTIDisplaySink2 *sink = (GstTIDisplaySink2 *)base;
    Buffer_Handle         inBuf        = NULL;
    Buffer_Handle         outBuf        = NULL;
    gboolean  pad_alloced_buffer = FALSE;

    GST_LOG_OBJECT(sink,"render begin");

    /* Check if its dmai transport buffer */
    if (GST_IS_TIDMAIBUFFERTRANSPORT(buffer)) {
        inBuf = GST_TIDMAIBUFFERTRANSPORT_DMAIBUF(buffer);

        /* Check if buffer belongs to our buffer tab */
        if (GST_TIDMAIBUFTAB_BUFTAB(sink->hBufTab) == Buffer_getBufTab(inBuf)) {
            pad_alloced_buffer = TRUE;
    
            /* Its pad allocated buffer, delay the streaming  */
            if (sink->hDisplay == NULL) {
                sink->dAttrs.delayStreamon = TRUE;
            }
        }
    }

    /* create the display */
    if (sink->hDisplay == NULL) {

        if (!display_create(base, buffer)) {
            GST_ELEMENT_ERROR(sink, RESOURCE, FAILED,
            ("Failed to create display handle\n"), (NULL));
            return GST_FLOW_UNEXPECTED;
        }
    }

    /* If the streaming was delayed then simply give it back to the display
     * and continue. 
     */
    if (pad_alloced_buffer) {
        sink->framecounts++;

        /* Mark buffer as in-use by the display so it can't be re-used
         * until it comes back from Display_get.
         */
        Buffer_setUseMask(inBuf, Buffer_getUseMask(inBuf) | 
                            gst_tidmaibuffer_DISPLAY_FREE);
        if (Display_put(sink->hDisplay, inBuf) < 0) {
            GST_ELEMENT_ERROR(sink, RESOURCE, FAILED,
            ("Failed to put display buffer\n"), (NULL));
            return GST_FLOW_UNEXPECTED;
        }

        /* If overlay is set then enable it */
        if (sink->overlay_set) {
            if (!update_overlay_fields(sink)) {
                GST_ELEMENT_ERROR(sink, RESOURCE, FAILED,
                ("Failed to update overlay fields"), (NULL));
                return GST_FLOW_UNEXPECTED;
            }
        }

        /* Make sure we have en-queued at least MIN_NUM_BUFS buffers before calling first
         * de-queue otherwise de-queue ioctl will block forever.
         */
        if (sink->framecounts >= MIN_NUM_BUFS) {
            if (Display_get(sink->hDisplay, &outBuf) < 0) {
                GST_ELEMENT_ERROR(sink, RESOURCE, FAILED,
                ("Failed to put display buffer\n"), (NULL));
                return GST_FLOW_UNEXPECTED;
            }
            Buffer_freeUseMask(outBuf, gst_tidmaibuffer_VIDEOSINK_FREE);
            Buffer_freeUseMask(outBuf, gst_tidmaibuffer_DISPLAY_FREE);

            /* ublock gst_tidmaibuftab_get_buf */
            Rendezvous_force(GST_TIDMAIBUFTAB_BUFAVAIL_RV(sink->hBufTab));
        }
        goto finish;
    }

    /* If overlay is set then enable it */
    if (sink->overlay_set) {
        if (!update_overlay_fields(sink)) {
            GST_ELEMENT_ERROR(sink, RESOURCE, FAILED,
            ("Failed to update overlay fields"), (NULL));
            return GST_FLOW_UNEXPECTED;
        }
    }

    /* Get free buffer from driver */
    if (Display_get(sink->hDisplay, &outBuf) < 0) {
        GST_ELEMENT_ERROR(sink, RESOURCE, FAILED,
        ("Failed to get display buffer\n"), (NULL));
        return GST_FLOW_UNEXPECTED;
    }

    /* Copy input buffer into driver buffer */
    if (xcopy(base, buffer, outBuf) < 0) {
        GST_ELEMENT_ERROR(sink, RESOURCE, FAILED,
        ("Failed to copy input buffer"), (NULL));
        return GST_FLOW_UNEXPECTED;
    }

    /* Give buffer back to driver */
    if (Display_put(sink->hDisplay, outBuf) < 0) {
        GST_ELEMENT_ERROR(sink, RESOURCE, FAILED,
        ("Failed to put display buffer"), (NULL));
        return GST_FLOW_UNEXPECTED;
    }

finish:    
    GST_LOG_OBJECT(sink,"render end");
    return GST_FLOW_OK;
}

static void 
set_property(GObject *object, guint prop_id, const GValue *value, 
    GParamSpec *pspec)
{
    GstTIDisplaySink2 *sink = GST_TIDISPLAYSINK2(object);

    GST_LOG_OBJECT(sink,"set property begin");
    switch (prop_id) {
        case PROP_DEVICE:
            if (sink->device) {
                g_free((gpointer)sink->device);
            }
            sink->device = (gchar*)g_malloc(strlen(g_value_get_string(value)) 
                + 1);
            strcpy((gchar*)sink->device, g_value_get_string(value));
            break;
        case PROP_ROTATE:
            sink->rotate = g_value_get_int(value);
            break;
        case PROP_NUMBUFS:
            sink->numbufs = g_value_get_int(value);
            break;
        case PROP_DISPLAY_OUTPUT:
            if (sink->display_output) {
                g_free((gpointer)sink->display_output);
            }
            sink->display_output = (gchar*)g_malloc(strlen(g_value_get_string(value)) 
                + 1);
            strcpy((gchar*)sink->display_output, g_value_get_string(value));
            break;
        case PROP_DISPLAY_STD:
            if (sink->video_std) {
                g_free((gpointer)sink->video_std);
            }
            sink->video_std = (gchar*)g_malloc(strlen(g_value_get_string(value)) 
                + 1);
            strcpy((gchar*)sink->video_std, g_value_get_string(value));
            break;
        case PROP_OVERLAY_LEFT:
            sink->overlay_left = g_value_get_int(value);
            sink->overlay_set |= OVERLAY_LEFT_SET;
            break;
        case PROP_OVERLAY_TOP:
            sink->overlay_top = g_value_get_int(value);
            sink->overlay_set |= OVERLAY_TOP_SET;
            break;
        case PROP_OVERLAY_WIDTH:
            sink->overlay_width = g_value_get_uint(value);
            sink->overlay_set |= OVERLAY_WIDTH_SET;
            break;
        case PROP_OVERLAY_HEIGHT:
            sink->overlay_height = g_value_get_uint(value);
            sink->overlay_set |= OVERLAY_HEIGHT_SET;
            break;
        case PROP_MMAP_BUFFER:
            sink->mmap_buffer = g_value_get_boolean(value);
            break;
        case PROP_DMA_COPY:
            sink->dma_copy = g_value_get_boolean(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }

    GST_LOG_OBJECT(sink,"set property end");
}

static void 
get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
    GstTIDisplaySink2 *sink = GST_TIDISPLAYSINK2(object);

    GST_LOG_OBJECT(sink,"get property begin");

    switch (prop_id) {
        case PROP_DEVICE:
            g_value_set_string(value, sink->device);
            break;
        case PROP_ROTATE:
            g_value_set_int(value, sink->rotate);
            break;
        case PROP_DEVICE_FD:
            g_value_set_int(value, sink->fd);
            break;
        case PROP_NUMBUFS:
            g_value_set_int(value, sink->numbufs);
            break;
        case PROP_DISPLAY_OUTPUT:
            g_value_set_string(value, sink->display_output);
            break;
        case PROP_DISPLAY_STD:
            g_value_set_string(value, sink->video_std);
            break;
        case PROP_OVERLAY_LEFT:
            g_value_set_int(value, sink->overlay_left);
            break;
        case PROP_OVERLAY_TOP:
            g_value_set_int(value, sink->overlay_top);
            break;
        case PROP_OVERLAY_WIDTH:
            g_value_set_uint(value, sink->overlay_width);
            break;
        case PROP_OVERLAY_HEIGHT:
            g_value_set_uint(value, sink->overlay_height);
            break;
        case PROP_MMAP_BUFFER:
            g_value_set_boolean(value, sink->mmap_buffer);
            break;
        case PROP_DMA_COPY:
            g_value_set_boolean(value, sink->dma_copy);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }

    GST_LOG_OBJECT(sink,"get property end");
}

static void 
tidisplaysink2_class_init(GstTIDisplaySink2Class *klass)
{
    GObjectClass     *gobject_class;
    GstElementClass  *gstelement_class;
    GstBaseSinkClass *gstbase_sink_class;

    GST_LOG("class_init  begin");

    gobject_class      = G_OBJECT_CLASS(klass);
    gstelement_class   = GST_ELEMENT_CLASS(klass);
    gstbase_sink_class = GST_BASE_SINK_CLASS(klass);

    gobject_class->set_property = set_property;
    gobject_class->get_property = get_property;

    parent_class = g_type_class_peek_parent (klass);

    g_object_class_install_property(gobject_class, PROP_DEVICE,
           g_param_spec_string("device", "Device",    "Device location", 
    DEFAULT_DEVICE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class, PROP_NUMBUFS,
        g_param_spec_int("queue-size", "Video driver queue size",
        "Number of buffers to be enqueud in the driver in streaming mod", 
        MIN_NUM_BUFS, MAX_NUM_BUFS, DEFAULT_NUM_BUFS, G_PARAM_READWRITE ));

    g_object_class_install_property(gobject_class, PROP_DISPLAY_OUTPUT,
        g_param_spec_string("display-output", "Display Output", 
        "Available Display Output \n"
        "\t\t\tsvideo\n"
        "\t\t\tcomposite\n"
        "\t\t\tcomponent\n"
        "\t\t\tlcd\n"
        "\t\t\tdvi\n"
        "\t\t\tsystem", 
        DEFAULT_DISPLAY_OUTPUT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class, PROP_MMAP_BUFFER,
        g_param_spec_boolean("mmap-buffer", "Driver buffer",
        "Use driver mapped buffers (i.e mmap) for display",
        DEFAULT_MMAP_BUFFER, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_DMA_COPY,
        g_param_spec_boolean("dma-copy", "Use DMA for copy",
        "Use DMA to copy upstream data",
        DEFAULT_DMA_COPY, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_DEVICE_FD,
        g_param_spec_int("device-fd", "File descriptor of the device",
        "File descriptor of the device"
        , -1, G_MAXINT, -1, G_PARAM_READABLE ));
    
    #if defined(Platform_omap3530) || defined(Platform_dm3730)
    /* these properties are supported only on omap platform */
    g_object_class_install_property(gobject_class, PROP_OVERLAY_TOP,
        g_param_spec_int("overlay-top", "Overlay top",
        "The topmost (y) coordinate of the video overlay; top left corner of"
        " screen is 0,0", G_MININT, G_MAXINT, 0, G_PARAM_READWRITE ));

    g_object_class_install_property(gobject_class, PROP_OVERLAY_LEFT,
        g_param_spec_int("overlay-left", "Overlay left",
        "The leftmost (x) coordinate of the video overlay; top left corner of"
        " screen is 0,0",  G_MININT, G_MAXINT, 0, G_PARAM_READWRITE ));

    g_object_class_install_property(gobject_class, PROP_OVERLAY_WIDTH,
        g_param_spec_uint("overlay-width", "Overlay Width",
        "The width of the video overlay; default is equal to negotiated image"
        " width", 0, G_MAXUINT, 0, G_PARAM_READWRITE ));

    g_object_class_install_property(gobject_class, PROP_OVERLAY_HEIGHT,
        g_param_spec_uint("overlay-height", "Overlay height",
        "The height of the video overlay; default is equal to negotiated"
        " image width", 0, G_MAXUINT, 0, G_PARAM_READWRITE ));

    g_object_class_install_property(gobject_class, PROP_ROTATE,
        g_param_spec_int("rotate", "rotate", "Rotate video at the specified "
        " angle (e.g 90, 180, 270)", 0, 270, 0, G_PARAM_READWRITE ));

    #endif

    g_object_class_install_property(gobject_class, PROP_DISPLAY_STD,
        g_param_spec_string("video-standard", "Display Standard", 
        "Available Display Standard \n"
        "\t\t\tntsc\n"
        "\t\t\tpal\n"
        "\t\t\t480p\n"
        "\t\t\t720p\n"
        "\t\t\t1080\n"
        "\t\t\tauto", 
        DEFAULT_VIDEO_STD, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    gstbase_sink_class->set_caps = setcaps;
    gstbase_sink_class->buffer_alloc = REGISTER_BUFFER_ALLOC ? buffer_alloc : NULL;
    gstbase_sink_class->start = start;
    gstbase_sink_class->stop = stop;
    gstbase_sink_class->render = render;
    gstbase_sink_class->preroll = preroll;

    GST_LOG("class init end");
}

static void
tidisplaysink2_base_init(void *gclass)
{
    static GstElementDetails element_details = {
        "Dmai based sink",
        "Sink/Video",
        "Renders video on TI OMAP and Davinci platform",
        "Brijesh Singh; Texas Instruments, Inc."
    };

    GstElementClass *element_class = GST_ELEMENT_CLASS(gclass);

    GST_LOG("base init begin");

    gst_element_class_add_pad_template(element_class,
    gst_static_pad_template_get (&sink_factory));
    gst_element_class_set_details(element_class, &element_details);

    GST_LOG("base init end");
}

static void
init (GstTIDisplaySink2 *sink, gpointer *data)
{
    GST_LOG_OBJECT(sink,"init start");

    #if defined(Platform_omap3530) || defined(Platform_dm3730)
    sink->dAttrs = Display_Attrs_O3530_VID_DEFAULT;
    #elif defined(Platform_omapl138)
    sink->dAttrs = Display_Attrs_OMAPL138_OSD_DEFAULT;
    #elif defined(Platform_dm365) || defined(Platform_dm368)
    sink->dAttrs = Display_Attrs_DM365_VID_DEFAULT;
    #endif

    sink->numbufs = DEFAULT_NUM_BUFS;
    sink->mmap_buffer = DEFAULT_MMAP_BUFFER;
    sink->dma_copy = DEFAULT_DMA_COPY;
    sink->fd = -1;

    g_object_set(sink, "device", DEFAULT_DEVICE, NULL);
    g_object_set(sink, "video-standard", DEFAULT_VIDEO_STD, NULL);
    g_object_set(sink, "display-output", DEFAULT_DISPLAY_OUTPUT, NULL);

    GST_LOG_OBJECT(sink,"init end");
}

GType
gst_tidisplaysink2_get_type(void)
{
    static GType type;

    if (G_UNLIKELY(type == 0)) {
        GTypeInfo type_info = {
            sizeof(struct gst_tidisplay_sink_class),
            tidisplaysink2_base_init,
            NULL,
            (GClassInitFunc) tidisplaysink2_class_init,
            NULL,
            NULL,
            sizeof(GstTIDisplaySink2),
            0,
            (GInstanceInitFunc)init
        };

        type = g_type_register_static(GST_TYPE_BASE_SINK, "GstTIDisplaySink2", 
                &type_info, 0);

        /* Initialize GST_LOG for this object */
        GST_DEBUG_CATEGORY_INIT(gst_tidisplaysink2_debug, "tidisplaysink2", 0, 
            "Video Display sink");
    }

    return type;
}

