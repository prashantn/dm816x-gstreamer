/*
 * gsttidmaivideosink.c:
 *
 * Original Author:
 *     Chase Maupin, Texas Instruments, Inc.
 *     derived from fakesink
 *
 * Copyright (C) 2008-2010 Texas Instruments Incorporated - http://www.ti.com/
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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "gsttidmaivideosink.h"
#include "gstticommonutils.h"

#include <gst/gstmarshal.h>

#include <unistd.h>

/* Define sink (input) pad capabilities.
 *
 * UYVY - YUV 422 interleaved corresponding to V4L2_PIX_FMT_UYVY in v4l2
 * NV16 - YUV 422 semi planar. The dm6467 VDCE outputs this format after a
 *        color conversion.The format consists of two planes: one with the
 *        Y component and one with the CbCr components interleaved (hence semi)  *
 *        See the LSP VDCE documentation for a thorough description of this
 *        format.
 * Y8C8 - Same as NV16.  Y8C8 was used in MVL-based LSPs.
 * NV12 - YUV 420 semi planar corresponding to V4L2_PIX_FMT_NV12 in v4l2.
 *        The format consists of two planes: one with the
 *        Y component and one with the CbCr components interleaved with 
 *        2x2 subsampling. See the LSP documentation for a thorough  
 *        description of this format. 
 *
 * NOTE:  This pad must be named "sink" in order to be used with the
 * Base Sink class.
 */
static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE (
    "sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS
    ("video/x-raw-yuv, "
         "format=(fourcc)UYVY, "
         "framerate=(fraction)[ 0, MAX ], "
         "width=(int)[ 1, MAX ], "
         "height=(int)[ 1, MAX ];"
    "video/x-raw-yuv, "
         "format=(fourcc)Y8C8, "
         "framerate=(fraction)[ 0, MAX ], "
         "width=(int)[ 1, MAX ], "
         "height=(int)[ 1, MAX ];"
    "video/x-raw-yuv, "
         "format=(fourcc)NV16, "
         "framerate=(fraction)[ 0, MAX ], "
         "width=(int)[ 1, MAX ], "
         "height=(int)[ 1, MAX ];"
    "video/x-raw-yuv, "
         "format=(fourcc)NV12, "
         "framerate=(fraction)[ 0, MAX ], "
         "width=(int)[ 1, MAX ], "
         "height=(int)[ 1, MAX ]"
    )
);

GST_DEBUG_CATEGORY_STATIC (gst_tidmaivideosink_debug);
#define GST_CAT_DEFAULT gst_tidmaivideosink_debug

static const GstElementDetails gst_tidmaivideosink_details =
GST_ELEMENT_DETAILS ("TI DMAI Video Sink",
    "Sink/Video",
    "Displays video using the TI DMAI interface",
    "Chase Maupin; Texas Instruments, Inc.");


/* TIDmaiVideoSink signals and args */
enum
{
  SIGNAL_HANDOFF,
  SIGNAL_PREROLL_HANDOFF,
  LAST_SIGNAL
};

/* This value must be set to TRUE in order for the GstBaseSink class to
 * use the max-lateness value to drop frames.
 */
#define DEFAULT_SYNC TRUE

#define DEFAULT_SIGNAL_HANDOFFS FALSE
#define DEFAULT_CAN_ACTIVATE_PUSH TRUE
#define DEFAULT_CAN_ACTIVATE_PULL FALSE

enum
{
  PROP_0,
  PROP_DISPLAYSTD,
  PROP_DISPLAYDEVICE,
  PROP_VIDEOSTD,
  PROP_VIDEOOUTPUT,
  PROP_ROTATION,
  PROP_NUMBUFS,
  PROP_RESIZER,
  PROP_AUTOSELECT,
  PROP_FRAMERATE,
  PROP_ACCEL_FRAME_COPY,
  PROP_SYNC,
  PROP_SIGNAL_HANDOFFS,
  PROP_CAN_ACTIVATE_PUSH,
  PROP_CAN_ACTIVATE_PULL,
  PROP_CONTIG_INPUT_BUF,
  PROP_USERPTR_BUFS,
  PROP_HIDE_OSD
};

enum
{
  VAR_DISPLAYSTD,
  VAR_VIDEOSTD,
  VAR_VIDEOOUTPUT
};

#define _do_init(bla) \
    GST_DEBUG_CATEGORY_INIT (gst_tidmaivideosink_debug, "TIDmaiVideoSink", 0, "TIDmaiVideoSink Element");

GST_BOILERPLATE_FULL (GstTIDmaiVideoSink, gst_tidmaivideosink, GstVideoSink,
    GST_TYPE_VIDEO_SINK, _do_init);

static void
 gst_tidmaivideosink_set_property(GObject * object, guint prop_id,
     const GValue * value, GParamSpec * pspec);
static void
 gst_tidmaivideosink_get_property(GObject * object, guint prop_id,
     GValue * value, GParamSpec * pspec);
static GstCaps *
 gst_tidmaivideosink_get_caps(GstBaseSink * bsink);
static gboolean
 gst_tidmaivideosink_set_caps(GstBaseSink * bsink, GstCaps * caps);
static GstStateChangeReturn
 gst_tidmaivideosink_change_state(GstElement * element,
     GstStateChange transition);
static GstFlowReturn
 gst_tidmaivideosink_buffer_alloc(GstBaseSink * bsink, guint64 offset,
     guint size, GstCaps * caps, GstBuffer ** buf);
static GstFlowReturn
 gst_tidmaivideosink_preroll(GstBaseSink * bsink, GstBuffer * buffer);
static int
 gst_tidmaivideosink_videostd_get_attrs(VideoStd_Type videoStd,
     VideoStd_Attrs * attrs);
static gboolean
 gst_tidmaivideosink_init_display(GstTIDmaiVideoSink * sink);
static gboolean
 gst_tidmaivideosink_exit_display(GstTIDmaiVideoSink * sink);
static gboolean
 gst_tidmaivideosink_set_display_attrs(GstTIDmaiVideoSink * sink, 
    ColorSpace_Type colorSpace);
static gboolean
 gst_tidmaivideosink_process_caps(GstBaseSink * bsink, GstCaps * caps);
static GstFlowReturn
 gst_tidmaivideosink_render(GstBaseSink * bsink, GstBuffer * buffer);
static gboolean
 gst_tidmaivideosink_event(GstBaseSink * bsink, GstEvent * event);
static void 
    gst_tidmaivideosink_init_env(GstTIDmaiVideoSink *sink);
static gboolean
    gst_tidmaivideosink_alloc_display_buffers(GstTIDmaiVideoSink * sink,
        Int32 bufSize);
static gboolean
    gst_tidmaivideosink_open_osd(GstTIDmaiVideoSink * sink);
static gboolean
    gst_tidmaivideosink_close_osd(GstTIDmaiVideoSink * sink);
static gboolean
    gst_tidmaivideosink_fraction_is_multiple(GValue *, GValue *);
static gboolean
    gst_tidmaivideosink_fraction_divide(GValue *dividend, GValue *frac1,
        GValue *frac2);

static guint gst_tidmaivideosink_signals[LAST_SIGNAL] = { 0 };


/******************************************************************************
 * gst_tidmaivideosink_base_init
 ******************************************************************************/
static void gst_tidmaivideosink_base_init(gpointer g_class)
{
    GstElementClass *gstelement_class = GST_ELEMENT_CLASS (g_class);

    gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sink_factory));
    gst_element_class_set_details (gstelement_class, 
      &gst_tidmaivideosink_details);
}


/******************************************************************************
 * gst_tidmaivideosink_class_init
 ******************************************************************************/
static void gst_tidmaivideosink_class_init(GstTIDmaiVideoSinkClass * klass)
{
    GObjectClass     *gobject_class;
    GstElementClass  *gstelement_class;
    GstBaseSinkClass *gstbase_sink_class;

    gobject_class      = G_OBJECT_CLASS(klass);
    gstelement_class   = GST_ELEMENT_CLASS(klass);
    gstbase_sink_class = GST_BASE_SINK_CLASS(klass);

    parent_class = g_type_class_peek_parent (klass);

    gobject_class->set_property = GST_DEBUG_FUNCPTR
        (gst_tidmaivideosink_set_property);
    gobject_class->get_property = GST_DEBUG_FUNCPTR
        (gst_tidmaivideosink_get_property);

    g_object_class_install_property(gobject_class, PROP_SIGNAL_HANDOFFS,
        g_param_spec_boolean("signal-handoffs", "Signal handoffs",
            "Send a signal before unreffing the buffer",
            DEFAULT_SIGNAL_HANDOFFS, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_DISPLAYSTD,
        g_param_spec_string("displayStd", "Display Standard",
            "Use V4L2 or FBDev for Video Display", NULL, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_DISPLAYDEVICE,
        g_param_spec_string("displayDevice", "Display Device",
            "Video device to use (usually /dev/video0", NULL,
            G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_VIDEOSTD,
        g_param_spec_string("videoStd", "Video Standard",
            "Video Standard used\n"
            "\tAUTO (if supported), CIF, SIF_NTSC, SIF_PAL, VGA, D1_NTSC\n"
            "\tD1_PAL, 480P, 576P, 720P_60, 720P_50, 1080I_30, 1080I_25\n"
            "\t1080P_30, 1080P_60, 1080P_25, 1080P_24\n",
            NULL, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_VIDEOOUTPUT,
        g_param_spec_string("videoOutput", "Video Output",
            "Output used to display video (i.e. Composite, Component, "
            "LCD, DVI)", NULL, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_RESIZER,
        g_param_spec_boolean("resizer", "Enable resizer",
            "Enable hardware resizer to resize the image", FALSE,
            G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_AUTOSELECT,
        g_param_spec_boolean("autoselect", "Auto Select the VideoStd",
            "Automatically select the Video Standard to use based on "
            "the video input.  This only works when the upstream element "
            "sets the video size attributes in the buffer", FALSE,
            G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_NUMBUFS,
        g_param_spec_int("numBufs", "Number of Video Buffers",
            "Number of video buffers allocated by the driver",
            -1, G_MAXINT, -1, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_ROTATION,
        g_param_spec_int("rotation", "Rotation angle", "Rotation angle "
            "(OMAP3530/DM3730 only)", -1, G_MAXINT, -1, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_FRAMERATE,
        gst_param_spec_fraction("framerate", "frame rate of video",
            "Frame rate of the video expressed as a fraction.  A value "
            "of 0/1 indicates the framerate is not specified", 0, 1,
            G_MAXINT, 1, 0, 1, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_ACCEL_FRAME_COPY,
        g_param_spec_boolean("accelFrameCopy", "Accel frame copy",
             "Use hardware accelerated framecopy", TRUE, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_SYNC,
        g_param_spec_boolean("sync", "Sync audio video",
            "This determined whether A/V data is synced and video "
            "frames are dropped or not.", DEFAULT_SYNC, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_CAN_ACTIVATE_PUSH,
        g_param_spec_boolean("can-activate-push", "Can activate push",
            "Can activate in push mode", DEFAULT_CAN_ACTIVATE_PUSH,
            G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_CAN_ACTIVATE_PULL,
        g_param_spec_boolean("can-activate-pull", "Can activate pull",
            "Can activate in pull mode", DEFAULT_CAN_ACTIVATE_PULL,
            G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_CONTIG_INPUT_BUF,
        g_param_spec_boolean("contiguousInputFrame", "Contiguous Input frame",
            "Set this if elemenet recieves contiguous input frame",
            FALSE, G_PARAM_WRITABLE));

    g_object_class_install_property(gobject_class, PROP_USERPTR_BUFS,
        g_param_spec_boolean("useUserptrBufs", "Use USERPTR display buffers",
            "Allocate our own V4L2 display buffers.  The number of buffers"
            "allocated is specified by the numBufs property",
            FALSE, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_HIDE_OSD,
        g_param_spec_boolean("hideOSD", "Hide the OSD during video playback",
            "Initialize and hide the OSD during video playback",
            FALSE, G_PARAM_READWRITE));

    /**
    * GstTIDmaiVideoSink::handoff:
    * @dmaisink: the dmaisink instance
    * @buffer: the buffer that just has been received
    * @pad: the pad that received it
    *
    * This signal gets emitted before unreffing the buffer.
    */
    gst_tidmaivideosink_signals[SIGNAL_HANDOFF] =
        g_signal_new("handoff", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET(GstTIDmaiVideoSinkClass, handoff), NULL, NULL,
            gst_marshal_VOID__OBJECT_OBJECT, G_TYPE_NONE, 2, GST_TYPE_BUFFER,
            GST_TYPE_PAD);

   /**
    * GstTIDmaiVideoSink::preroll-handoff:
    * @dmaisink: the dmaisink instance
    * @buffer: the buffer that just has been received
    * @pad: the pad that received it
    *
    * This signal gets emitted before unreffing the buffer.
    *
    * Since: 0.10.7
    */
    gst_tidmaivideosink_signals[SIGNAL_PREROLL_HANDOFF] =
        g_signal_new("preroll-handoff", G_TYPE_FROM_CLASS(klass),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET(GstTIDmaiVideoSinkClass, preroll_handoff),
            NULL, NULL, gst_marshal_VOID__OBJECT_OBJECT, G_TYPE_NONE, 2,
            GST_TYPE_BUFFER, GST_TYPE_PAD);

    gstelement_class->change_state =
        GST_DEBUG_FUNCPTR(gst_tidmaivideosink_change_state);

    gstbase_sink_class->get_caps = 
        GST_DEBUG_FUNCPTR(gst_tidmaivideosink_get_caps);
    gstbase_sink_class->set_caps = 
        GST_DEBUG_FUNCPTR(gst_tidmaivideosink_set_caps);
    gstbase_sink_class->event    =
        GST_DEBUG_FUNCPTR(gst_tidmaivideosink_event);
    gstbase_sink_class->preroll  =
        GST_DEBUG_FUNCPTR(gst_tidmaivideosink_preroll);
    gstbase_sink_class->render   =
        GST_DEBUG_FUNCPTR(gst_tidmaivideosink_render);
    gstbase_sink_class->buffer_alloc =
        GST_DEBUG_FUNCPTR(gst_tidmaivideosink_buffer_alloc);

    /* Pad-buffer allocation is currently supported on DM365, OMAP3530 and DM3730 */
    #if !defined(Platform_dm365) && !defined(Platform_omap3530) && \
    !defined(Platform_dm3730) && !defined(Platform_dm368)
    gstbase_sink_class->buffer_alloc = NULL;
    #endif
}


/******************************************************************************
 * gst_tidmaivideosink_init_env
 *  Initialize element property default by reading environment variables.
 *****************************************************************************/
static void gst_tidmaivideosink_init_env(GstTIDmaiVideoSink *sink)
{
    GST_LOG("gst_tidmaivideosink_init_env - begin\n");

    if (gst_ti_env_is_defined("GST_TI_TIDmaiVideoSink_displayStd")) {
        sink->displayStd = 
            gst_ti_env_get_string("GST_TI_TIDmaiVideoSink_displayStd");
        GST_LOG("Setting displayStd=%s\n", sink->displayStd);
    }

    if (gst_ti_env_is_defined("GST_TI_TIDmaiVideoSink_displayDevice")) {
        sink->displayDevice = 
            gst_ti_env_get_string("GST_TI_TIDmaiVideoSink_displayDevice");
        GST_LOG("Setting displayDevice=%s\n", sink->displayDevice);
    }
    
    if (gst_ti_env_is_defined("GST_TI_TIDmaiVideoSink_videoStd")) {
        sink->videoStd = gst_ti_env_get_string("GST_TI_TIDmaiVideoSink_videoStd");
        GST_LOG("Setting videoStd=%s\n", sink->videoStd);
    }

    if (gst_ti_env_is_defined("GST_TI_TIDmaiVideoSink_videoOutput")) {
        sink->videoOutput =  
                gst_ti_env_get_string("GST_TI_TIDmaiVideoSink_videoOutput");
        GST_LOG("Setting displayBuffer=%s\n", sink->videoOutput);
    }
 
    if (gst_ti_env_is_defined("GST_TI_TIDmaiVideoSink_rotation")) {
        sink->rotation = gst_ti_env_get_int("GST_TI_TIDmaiVideoSink_rotation");
        GST_LOG("Setting rotation =%d\n", sink->rotation);
    }

    if (gst_ti_env_is_defined("GST_TI_TIDmaiVideoSink_numBufs")) {
        sink->numBufs = gst_ti_env_get_int("GST_TI_TIDmaiVideoSink_numBufs");
        GST_LOG("Setting numBufs=%d\n",sink->numBufs);
    }

    if (gst_ti_env_is_defined("GST_TI_TIDmaiVideoSink_resizer")) {
        sink->resizer = gst_ti_env_get_boolean("GST_TI_TIDmaiVideoSink_resizer");
        GST_LOG("Setting resizer=%s\n",sink->resizer ? "TRUE" : "FALSE");
    }

    if (gst_ti_env_is_defined("GST_TI_TIDmaiVideoSink_autoselect")) {
        sink->autoselect = 
                gst_ti_env_get_boolean("GST_TI_TIDmaiVideoSink_autoselect");
        GST_LOG("Setting autoselect=%s\n",sink->autoselect ? "TRUE" : "FALSE");
    }

    if (gst_ti_env_is_defined("GST_TI_TIDmaiVideoSink_accelFrameCopy")) {
        sink->accelFrameCopy = 
                gst_ti_env_get_boolean("GST_TI_TIDmaiVideoSink_accelFrameCopy");
        GST_LOG("Setting accelFrameCopy=%s\n",
                sink->accelFrameCopy ? "TRUE" : "FALSE");
    }
    
    GST_LOG("gst_tidmaivideosink_init_env - end\n");
}


/******************************************************************************
 * gst_tidmaivideosink_init
 ******************************************************************************/
static void gst_tidmaivideosink_init(GstTIDmaiVideoSink * dmaisink,
                GstTIDmaiVideoSinkClass * g_class)
{
    BufferGfx_Attrs defaultGfxAttrs  = BufferGfx_Attrs_DEFAULT;

    /* Set the default values to NULL or -1.  If the user specifies a value 
     * then the element will be non-null when the display is created.  
     * Anything that has a NULL value will be initialized with DMAI defaults 
     * in the gst_tidmaivideosink_init_display function.
     */
    dmaisink->displayStd          = NULL;
    dmaisink->displayDevice       = NULL;
    dmaisink->dGfxAttrs           = defaultGfxAttrs;
    dmaisink->videoStd            = NULL;
    dmaisink->videoOutput         = NULL;
    dmaisink->numBufs             = -1;
    dmaisink->can_set_display_framerate = FALSE;
    dmaisink->rotation            = -1;
    dmaisink->tempDmaiBuf         = NULL;
    dmaisink->accelFrameCopy      = TRUE;
    dmaisink->autoselect          = FALSE;
    dmaisink->prevVideoStd        = 0;
    dmaisink->useUserptrBufs      = FALSE;
    dmaisink->hideOSD             = FALSE;
    dmaisink->hDispBufTab         = NULL;

    dmaisink->signal_handoffs = DEFAULT_SIGNAL_HANDOFFS;

    /* Initialize GValue members */
    memset(&dmaisink->framerate, 0, sizeof(GValue));
    g_value_init(&dmaisink->framerate, GST_TYPE_FRACTION);
    g_assert(GST_VALUE_HOLDS_FRACTION(&dmaisink->framerate));
    gst_value_set_fraction(&dmaisink->framerate, 0, 1);

    memset(&dmaisink->dCapsFramerate, 0, sizeof(GValue));
    g_value_init(&dmaisink->dCapsFramerate, GST_TYPE_FRACTION);
    g_assert(GST_VALUE_HOLDS_FRACTION(&dmaisink->dCapsFramerate));
    gst_value_set_fraction(&dmaisink->dCapsFramerate, 0, 1);

    memset(&dmaisink->iattrs.framerate, 0, sizeof(GValue));
    g_value_init(&dmaisink->iattrs.framerate, GST_TYPE_FRACTION);
    g_assert(GST_VALUE_HOLDS_FRACTION(&dmaisink->iattrs.framerate));
    gst_value_set_fraction(&dmaisink->iattrs.framerate, 0, 1);

    memset(&dmaisink->oattrs.framerate, 0, sizeof(GValue));
    g_value_init(&dmaisink->oattrs.framerate, GST_TYPE_FRACTION);
    g_assert(GST_VALUE_HOLDS_FRACTION(&dmaisink->oattrs.framerate));
    gst_value_set_fraction(&dmaisink->oattrs.framerate, 0, 1);

    /* DM365 supports setting the display framerate */
    #if defined(Platform_dm365) || defined(Platform_dm368)
    dmaisink->can_set_display_framerate = TRUE;
    #endif

    gst_tidmaivideosink_init_env(dmaisink);
}

/*******************************************************************************
 * gst_tidmaivideosink_string_cap
 *    This function will capitalize the given string.  This makes it easier
 *    to compare the strings later.
*******************************************************************************/
static void gst_tidmaivideosink_string_cap(gchar * str)
{
    int len = strlen(str);
    int i;

    for (i = 0; i < len; i++) {
        str[i] = (char)toupper(str[i]);
    }
    return;
}


/******************************************************************************
 * gst_tidmaivideosink_set_property
 ******************************************************************************/
static void gst_tidmaivideosink_set_property(GObject * object, guint prop_id,
                const GValue * value, GParamSpec * pspec)
{
    GstTIDmaiVideoSink *sink;

    sink = GST_TIDMAIVIDEOSINK(object);

    switch (prop_id) {
        case PROP_DISPLAYSTD:
            sink->displayStd = g_strdup(g_value_get_string(value));
            gst_tidmaivideosink_string_cap(sink->displayStd);
            break;
        case PROP_DISPLAYDEVICE:
            sink->displayDevice = g_strdup(g_value_get_string(value));
            break;
        case PROP_VIDEOSTD:
            sink->videoStd = g_strdup(g_value_get_string(value));
            gst_tidmaivideosink_string_cap(sink->videoStd);
            break;
        case PROP_VIDEOOUTPUT:
            sink->videoOutput = g_strdup(g_value_get_string(value));
            gst_tidmaivideosink_string_cap(sink->videoOutput);
            break;
        case PROP_RESIZER:
            sink->resizer = g_value_get_boolean(value);
            break;
        case PROP_AUTOSELECT:
            sink->autoselect = g_value_get_boolean(value);
            break;
        case PROP_NUMBUFS:
            sink->numBufs = g_value_get_int(value);
            break;
        case PROP_ROTATION:
            sink->rotation = g_value_get_int(value);
            break;
        case PROP_FRAMERATE:
            g_value_copy(value, &sink->framerate);
            break;
        case PROP_ACCEL_FRAME_COPY:
            sink->accelFrameCopy = g_value_get_boolean(value);
            break;
        case PROP_SYNC:
            GST_BASE_SINK(sink)->sync = g_value_get_boolean(value);
            break;
        case PROP_SIGNAL_HANDOFFS:
            sink->signal_handoffs = g_value_get_boolean(value);
            break;
        case PROP_CAN_ACTIVATE_PUSH:
            GST_BASE_SINK(sink)->can_activate_push =
                g_value_get_boolean(value);
            break;
        case PROP_CAN_ACTIVATE_PULL:
            GST_BASE_SINK(sink)->can_activate_pull =
                g_value_get_boolean(value);
            break;
        case PROP_CONTIG_INPUT_BUF:
            sink->contiguousInputFrame = g_value_get_boolean(value);
            break;
        case PROP_USERPTR_BUFS:
            sink->useUserptrBufs = g_value_get_boolean(value);
            break;
        case PROP_HIDE_OSD:
            sink->hideOSD = g_value_get_boolean(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}


/******************************************************************************
 * gst_tidmaivideosink_get_property
 ******************************************************************************/
static void gst_tidmaivideosink_get_property(GObject * object, guint prop_id,
                GValue * value, GParamSpec * pspec)
{
    GstTIDmaiVideoSink *sink;

    sink = GST_TIDMAIVIDEOSINK(object);

    switch (prop_id) {
        case PROP_DISPLAYSTD:
            g_value_set_string(value, sink->displayStd);
            break;
        case PROP_DISPLAYDEVICE:
            g_value_set_string(value, sink->displayDevice);
            break;
        case PROP_VIDEOSTD:
            g_value_set_string(value, sink->videoStd);
            break;
        case PROP_VIDEOOUTPUT:
            g_value_set_string(value, sink->videoOutput);
            break;
        case PROP_RESIZER:
            g_value_set_boolean(value, sink->resizer);
            break;
        case PROP_AUTOSELECT:
            g_value_set_boolean(value, sink->autoselect);
            break;
        case PROP_ROTATION:
            g_value_set_int(value, sink->rotation);
            break;
        case PROP_NUMBUFS:
            g_value_set_int(value, sink->numBufs);
            break;
        case PROP_FRAMERATE:
            g_value_copy(&sink->framerate, value);
            break;
        case PROP_SYNC:
            g_value_set_boolean(value, GST_BASE_SINK(sink)->sync);
            break;
        case PROP_SIGNAL_HANDOFFS:
            g_value_set_boolean(value, sink->signal_handoffs);
            break;
        case PROP_ACCEL_FRAME_COPY:
            g_value_set_boolean(value, sink->accelFrameCopy);
            break;
        case PROP_CAN_ACTIVATE_PUSH:
            g_value_set_boolean(value, GST_BASE_SINK(sink)->can_activate_push);
            break;
        case PROP_CAN_ACTIVATE_PULL:
            g_value_set_boolean(value, GST_BASE_SINK(sink)->can_activate_pull);
            break;
        case PROP_CONTIG_INPUT_BUF:
            g_value_set_boolean(value, sink->contiguousInputFrame);
            break;
        case PROP_USERPTR_BUFS:
            g_value_set_boolean(value, sink->useUserptrBufs);
            break;
        case PROP_HIDE_OSD:
            g_value_set_boolean(value, sink->hideOSD);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}


/******************************************************************************
 * gst_tidmaivideosink_event
 ******************************************************************************/
static gboolean gst_tidmaivideosink_event(GstBaseSink * bsink,
                    GstEvent * event)
{
    const GstStructure *s;
    gchar              *sstr;

    if ((s = gst_event_get_structure(event)))
        sstr = gst_structure_to_string(s);
    else
        sstr = g_strdup("");


    GST_LOG("event  ******* E (type: %d, %s)\n", GST_EVENT_TYPE(event), sstr);
    g_free(sstr);

    return TRUE;
}


/******************************************************************************
 * gst_tidmaivideosink_buffer_alloc
 ******************************************************************************/
static GstFlowReturn gst_tidmaivideosink_buffer_alloc(GstBaseSink * bsink,
                         guint64 offset, guint size, GstCaps * caps,
                         GstBuffer ** buf)
{
    GstTIDmaiVideoSink *dmaisink    = GST_TIDMAIVIDEOSINK(bsink);
    BufferGfx_Attrs     gfxAttrs    = BufferGfx_Attrs_DEFAULT;
    gboolean            alloc_unref = FALSE;
    Buffer_Handle       hDispBuf    = NULL;
    GstCaps            *alloc_caps;

    *buf = NULL;

    GST_LOG_OBJECT(dmaisink,
        "a buffer of %d bytes was requested with caps %" GST_PTR_FORMAT
        " and offset %" G_GUINT64_FORMAT, size, caps, offset);

    /* assume we're going to alloc what was requested, keep track of wheter we
     * need to unref or not. When we suggest a new format upstream we will
     * create a new caps that we need to unref. */
    alloc_caps = caps;

    /* Process the buffer caps */
    if (!gst_tidmaivideosink_process_caps(bsink, alloc_caps)) {
        return GST_FLOW_UNEXPECTED;
    }

    /* Pad buffer allocation requires that we use user-allocated display
     * buffers.
     */
    if (!dmaisink->useUserptrBufs && dmaisink->hDisplay) {
        GST_ELEMENT_ERROR(dmaisink, RESOURCE, FAILED,
            ("Cannot use pad buffer allocation after mmap buffers already "
             "in use\n"), (NULL));
        return GST_FLOW_UNEXPECTED;
    }
    else {
        dmaisink->useUserptrBufs = TRUE;
    }

    /* Allocate the display buffers */
    if (!dmaisink->hDispBufTab && dmaisink->useUserptrBufs) {

        /* Set the display attributes now so we can allocate display buffers */
        if (!gst_tidmaivideosink_set_display_attrs(dmaisink,
            dmaisink->dGfxAttrs.colorSpace)) {
            GST_ERROR("Error while trying to set the display attributes\n");
            return GST_FLOW_UNEXPECTED;
        }

        if (!gst_tidmaivideosink_alloc_display_buffers(dmaisink, size)) {
            GST_ERROR("Failed to allocate display buffers");
            return GST_FLOW_UNEXPECTED;
        }

        /* This element cannot perform resizing when doing pad allocation */
        if (dmaisink->resizer) {
            GST_WARNING("resizer not supported for pad allocation; "
                "disabling\n");
            dmaisink->resizer = FALSE;
        }

        /* This element cannot perform rotation when doing pad allocation */
        if (dmaisink->rotation) {
            GST_WARNING("rotation not supported for pad allocation; "
                "disabling\n");
            dmaisink->rotation = FALSE;
        }
    }

    /* Get a buffer from the BufTab or display driver */
    if (!(hDispBuf = gst_tidmaibuftab_get_buf(dmaisink->hDispBufTab))) {
        if (dmaisink->hDisplay &&
            Display_get(dmaisink->hDisplay, &hDispBuf) < 0) {
            GST_ELEMENT_ERROR(dmaisink, RESOURCE, FAILED,
                ("Failed to get display buffer\n"), (NULL));
            return GST_FLOW_UNEXPECTED;
        }
    }

    /* If the geometry doesn't match, generate a new caps for it */
    Buffer_getAttrs(hDispBuf, BufferGfx_getBufferAttrs(&gfxAttrs));

    if (gfxAttrs.dim.width  != dmaisink->dGfxAttrs.dim.width  ||
        gfxAttrs.dim.height != dmaisink->dGfxAttrs.dim.height ||
        gfxAttrs.colorSpace != dmaisink->dGfxAttrs.colorSpace) {

        GstCaps *desired_caps;
        GstStructure *desired_struct;

        /* make a copy of the incomming caps to create the new suggestion. We
         * can't use make_writable because we might then destroy the original
         * caps which we still need when the peer does not accept the
         * suggestion.
         */
        desired_caps = gst_caps_copy (caps);
        desired_struct = gst_caps_get_structure (desired_caps, 0);

        GST_DEBUG ("we prefer to receive a %ldx%ld video; %ldx%ld was "
            "requested", gfxAttrs.dim.width, gfxAttrs.dim.height,
            dmaisink->dGfxAttrs.dim.width, dmaisink->dGfxAttrs.dim.height);
        gst_structure_set (desired_struct, "width", G_TYPE_INT,
            gfxAttrs.dim.width, NULL);
        gst_structure_set (desired_struct, "height", G_TYPE_INT,
            gfxAttrs.dim.height, NULL);

        if (gst_pad_peer_accept_caps (GST_VIDEO_SINK_PAD (dmaisink),
                desired_caps)) {
            alloc_caps  = desired_caps;
            alloc_unref = TRUE;

            if (!gst_tidmaivideosink_process_caps(bsink, alloc_caps)) {
                return GST_FLOW_UNEXPECTED;
            }
            GST_DEBUG ("peer pad accepts our desired caps %" GST_PTR_FORMAT,
                desired_caps);
        }
        else {
            GST_DEBUG ("peer pad does not accept our desired caps %" 
                GST_PTR_FORMAT, desired_caps);
        }
    }

    /* Return the display buffer */
    BufferGfx_resetDimensions(hDispBuf);
    Buffer_freeUseMask(hDispBuf, gst_tidmaibuffer_DISPLAY_FREE);
    *buf = gst_tidmaibuffertransport_new(hDispBuf, NULL);
    gst_buffer_set_caps(*buf, alloc_caps);

    /* If we allocated new caps, unref them now */
    if (alloc_unref) {
        gst_caps_unref (alloc_caps);
    }

    return GST_FLOW_OK;
}


/******************************************************************************
 * gst_tidmaivideosink_preroll
 ******************************************************************************/
static GstFlowReturn gst_tidmaivideosink_preroll(GstBaseSink * bsink,
                         GstBuffer * buffer)
{
    GstTIDmaiVideoSink *sink = GST_TIDMAIVIDEOSINK(bsink);

    GST_DEBUG("Begin\n");

    /* If someone is looking for this signal go ahead and throw it.  We aren't
     * doing anything with the buffer here anyway.
     */
    if (sink->signal_handoffs) {
        g_signal_emit(sink,
            gst_tidmaivideosink_signals[SIGNAL_PREROLL_HANDOFF], 0, buffer,
            bsink->sinkpad);
    }
    GST_DEBUG("Finish\n");

    return GST_FLOW_OK;
}


/*******************************************************************************
 * gst_tidmaivideosink_videostd_get_attrs
 *
 *    This function will take in a video standard enumeration and 
 *    videostd_attrs structure and fill in the width, height, and frame rate 
 *    of the standard.  The function returns a negative value on failure.
 *
 *    videoStd - The video standard to get the attributes of
 *    vattrs   - The video standard attributes structure to fill out
 *
*******************************************************************************/
static int gst_tidmaivideosink_videostd_get_attrs(VideoStd_Type videoStd,
               VideoStd_Attrs * vattrs)
{
    GST_DEBUG("Begin\n");

    /* Initialize the framerate GValue */
    memset(&vattrs->framerate, 0, sizeof(GValue));
    g_value_init(&vattrs->framerate, GST_TYPE_FRACTION);
    g_assert(GST_VALUE_HOLDS_FRACTION(&vattrs->framerate));

    switch (videoStd) {
        case VideoStd_1080P_24:
            gst_value_set_fraction(&vattrs->framerate, 24, 1);
            break;
        case VideoStd_SIF_PAL:
        case VideoStd_D1_PAL:
        case VideoStd_1080P_25:
        case VideoStd_1080I_25:
            gst_value_set_fraction(&vattrs->framerate, 25, 1);
            break;
        case VideoStd_CIF:
        case VideoStd_SIF_NTSC:
        case VideoStd_D1_NTSC:
            gst_value_set_fraction(&vattrs->framerate, 30000, 1001);
            break;
        case VideoStd_1080I_30:
        case VideoStd_1080P_30:
            gst_value_set_fraction(&vattrs->framerate, 30, 1);
            break;
        case VideoStd_576P:
        case VideoStd_720P_50:
            gst_value_set_fraction(&vattrs->framerate, 50, 1);
            break;
        case VideoStd_480P:
        case VideoStd_720P_60:
        #if defined(Platform_dm6467t)
        case VideoStd_1080P_60:
        #endif
        case VideoStd_VGA:
            gst_value_set_fraction(&vattrs->framerate, 60, 1);
            break;

        default:
            GST_ERROR("Unknown videoStd entered (VideoStd = %d)\n", videoStd);
            return -1;
    }
    vattrs->videostd = videoStd;

    GST_DEBUG("Finish\n");
    return (VideoStd_getResolution(videoStd, (Int32 *) & vattrs->width,
                (Int32 *) & vattrs->height));
}


/*******************************************************************************
 * gst_tidmaivideosink_find_videostd
 *
 *    This function will take in a VideoStd_Attrs structure and find the
 *    smallest video standard large enough to fit the input standard.  It
 *    also checks for the closest frame rate match.  The selected video
 *    standard is returned.  If no videostd is found that can match the size
 *    of the input requested and be at least a multiple of the frame rate
 *    then a negative value is returned.
 *
 *    The function begins searching the video standards at the value of
 *    prevVideoStd which is initialized to 1.  In this was if a video
 *    standard is found but the display cannot be created using that standard
 *    we can resume the search from the last standard used.
 * 
*******************************************************************************/
static VideoStd_Type gst_tidmaivideosink_find_videostd(
                         GstTIDmaiVideoSink * sink)
{
    VideoStd_Attrs  tattrs;
    int             bestmatch = -1;
    int             okmatch   = -1;
    int             dwidth;
    int             dheight;
    int             ret;
    int             i;

    GST_DEBUG("Begin\n");

    /* Initialize the width and height deltas to a large value.
     * If the videoStd we are checking has smaller deltas than it
     * is a better fit.
     */
    dwidth = dheight = 999999;

    /* Start from prevVideoStd + 1 and check for which window size fits best. */
    for (i = sink->prevVideoStd + 1; i < VideoStd_COUNT; i++) {
        ret = gst_tidmaivideosink_videostd_get_attrs(i, &tattrs);
        if (ret < 0) {
            GST_ERROR("Failed to get videostd attrs for videostd %d\n", i);
            return -1;
        }

        /* Check if width will fit */
        if (sink->iattrs.width > tattrs.width)
            continue;
        /* Check if height will fit */
        if (sink->iattrs.height > tattrs.height)
            continue;

        /* Check if the width and height are a better fit than the last
         * resolution.  If so we will look at the frame rate to help decide
         * if it is a compatible videostd.
         */
        GST_DEBUG("\nInput Attributes:\n"
                  "\tsink input attrs width = %ld\n"
                  "\tsink input attrs height = %ld\n"
                  "\tsink input attrs framerate = %d/%d\n\n"
                  "Display Attributes:\n"
                  "\tdisplay width = %d\n"
                  "\tdisplay height = %d\n\n"
                  "Proposed Standard (%d) Attributes:\n"
                  "\tstandard width = %ld\n"
                  "\tstandard height = %ld\n"
                  "\tstandard framerate = %d/%d\n",
                  sink->iattrs.width, sink->iattrs.height,
                  gst_value_get_fraction_numerator(&sink->iattrs.framerate),
                  gst_value_get_fraction_denominator(&sink->iattrs.framerate),
                  dwidth, dheight, i, tattrs.width, tattrs.height,
                  gst_value_get_fraction_numerator(&tattrs.framerate),
                  gst_value_get_fraction_denominator(&tattrs.framerate));

        if (((tattrs.width - sink->iattrs.width) <= dwidth) &&
            ((tattrs.height - sink->iattrs.height) <= dheight)) {
            /* Check if the frame rate is an exact match, if not check if
             * the input frame rate is a multiple of the display frame rate.
             * If neither case is true then this is not a compatable videostd.
             */
            if (gst_value_compare(&sink->iattrs.framerate, &tattrs.framerate)
                   == GST_VALUE_EQUAL) {
                bestmatch = i;
            }
            else if (!gst_tidmaivideosink_fraction_is_multiple(
                         &sink->iattrs.framerate, &tattrs.framerate)) {
                okmatch = i;
            } else {
                continue;
            }

            /* Set new width and height deltas.  These are set after we
             * check if the framerates match so that a standard with an
             * incompatible frame rate does not get selected and prevent
             * a standard with a compatible frame rate from being selected
             */
            dwidth = tattrs.width - sink->iattrs.width;
            dheight = tattrs.height - sink->iattrs.height;
        }
    }
    /* If we didn't find a best match check if we found one that was ok */
    if ((bestmatch == -1) && (okmatch != -1))
        bestmatch = okmatch;

    GST_DEBUG("Finish\n");
    return bestmatch;
}


/******************************************************************************
 * gst_tidmaivideosink_check_set_framerate
 *
 *    This function will first check if the user set a frame rate on the
 *    command line.  If not then it will check if the frame rate of the
 *    input stream is known.  Lastly as a final resort it will default to
 *    an input frame rate of 30fps.
 *
 ******************************************************************************/
static void gst_tidmaivideosink_check_set_framerate(GstTIDmaiVideoSink * sink)
{
    GST_DEBUG("Begin\n");

    /* Check if the framerate was set either by a property of by
     * caps information */
    if (gst_value_get_fraction_numerator(&sink->framerate) > 0) {
        g_value_copy(&sink->framerate, &sink->iattrs.framerate);
        return;
    }

    /* Default to 29.97fps if there is no framerate information available */
    if (gst_value_get_fraction_numerator(&sink->iattrs.framerate) == 0) {
        gst_value_set_fraction(&sink->iattrs.framerate, 30000, 1001);
    }

    GST_DEBUG("Finish\n");
    return;
}


/******************************************************************************
 * gst_tidmaivideosink_convert_attrs
 *    This function will convert the human readable strings for the
 *    attributes into the proper integer values for the enumerations.  
*******************************************************************************/
static int gst_tidmaivideosink_convert_attrs(int attr,
               GstTIDmaiVideoSink * sink)
{
    int ret;

    GST_DEBUG("Begin\n");

    switch (attr) {
        case VAR_DISPLAYSTD:
            /* Convert the strings V4L2 or FBDEV into their integer enumeration
             */
            if (!strcmp(sink->displayStd, "V4L2"))
                return Display_Std_V4L2;
            else if (!strcmp(sink->displayStd, "FBDEV"))
                return Display_Std_FBDEV;
            else {
                GST_ERROR("Invalid displayStd entered (%s)"
                          "Please choose from:\n \tV4L2, FBDEV\n",
                          sink->displayStd);
                return -1;
            }
            break;
        case VAR_VIDEOSTD:
            /* Convert the video standard strings into their interger
             * enumeration
             */
            if (!strcmp(sink->videoStd, "AUTO"))
                return VideoStd_AUTO;
            else if (!strcmp(sink->videoStd, "CIF"))
                return VideoStd_CIF;
            else if (!strcmp(sink->videoStd, "SIF_NTSC"))
                return VideoStd_SIF_NTSC;
            else if (!strcmp(sink->videoStd, "SIF_PAL"))
                return VideoStd_SIF_PAL;
            else if (!strcmp(sink->videoStd, "D1_NTSC"))
                return VideoStd_D1_NTSC;
            else if (!strcmp(sink->videoStd, "D1_PAL"))
                return VideoStd_D1_PAL;
            else if (!strcmp(sink->videoStd, "480P"))
                return VideoStd_480P;
            else if (!strcmp(sink->videoStd, "576P"))
                return VideoStd_576P;
            else if (!strcmp(sink->videoStd, "720P_60"))
                return VideoStd_720P_60;
            else if (!strcmp(sink->videoStd, "720P_50"))
                return VideoStd_720P_50;
            else if (!strcmp(sink->videoStd, "1080I_30"))
                return VideoStd_1080I_30;
            else if (!strcmp(sink->videoStd, "1080I_25"))
                return VideoStd_1080I_25;
            else if (!strcmp(sink->videoStd, "1080P_30"))
                return VideoStd_1080P_30;
            else if (!strcmp(sink->videoStd, "1080P_25"))
                return VideoStd_1080P_25;
            else if (!strcmp(sink->videoStd, "1080P_24"))
                return VideoStd_1080P_24;
            #if defined(Platform_dm6467t)
            else if (!strcmp(sink->videoStd, "1080P_60"))
                return VideoStd_1080P_60;
            #endif
            else if (!strcmp(sink->videoStd, "VGA"))
                return VideoStd_VGA;
            else {
                GST_ERROR("Invalid videoStd entered (%s).  "
                "Please choose from:\n"
                "\tAUTO (if supported), CIF, SIF_NTSC, SIF_PAL, VGA, D1_NTSC\n"
                "\tD1_PAL, 480P, 576P, 720P_60, 720P_50, 1080I_30, 1080I_25\n"
                "\t1080P_30, 1080P_60, 1080P_25, 1080P_24\n", sink->videoStd);
                return -1;
            }
            break;
        case VAR_VIDEOOUTPUT:
            /* Convert the strings SVIDEO, COMPONENT, or COMPOSITE into their
             * integer enumerations.
             */
            if (!strcmp(sink->videoOutput, "SVIDEO"))
                return Display_Output_SVIDEO;
            else if (!strcmp(sink->videoOutput, "COMPOSITE"))
                return Display_Output_COMPOSITE;
            else if (!strcmp(sink->videoOutput, "COMPONENT"))
                return Display_Output_COMPONENT;
            else if (!strcmp(sink->videoOutput, "DVI"))
                return Display_Output_DVI;
            else if (!strcmp(sink->videoOutput, "LCD"))
                return Display_Output_LCD;
            else if (!strcmp(sink->videoOutput, "AUTO"))
                return Display_Output_SYSTEM;
            else {
                GST_ERROR("Invalid videoOutput entered (%s)."
                    "Please choose from:\n"
                    "\tSVIDEO, COMPOSITE, COMPONENT, LCD, DVI, AUTO\n",
                    sink->videoOutput);
                return -1;
            }
            break;
        default:
            GST_ERROR("Unknown Attribute\n");
            ret = -1;
            break;
    }

    GST_DEBUG("Finish\n");
    return ret;
}

/******************************************************************************
 * gst_tidmaivideosink_set_display_attrs
 *    this function sets the display attributes to the DMAI defaults
 *    and overrides those default with user input if entered.
*******************************************************************************/
static gboolean gst_tidmaivideosink_set_display_attrs(GstTIDmaiVideoSink *sink,
    ColorSpace_Type colorSpace)
{
    int ret;

    GST_DEBUG("Begin\n");

    /* Determine which device this element is running on */
    if (Cpu_getDevice(NULL, &sink->cpu_dev) < 0) {
        GST_ERROR("Failed to determine target board\n");
        return FALSE;
    }

    /* Set the frame rate based on if the user gave a rate on the command line
     * or if the stream has the frame rate information.
     */
    gst_tidmaivideosink_check_set_framerate(sink);

    /* Set the display attrs to the defaults for this device */
    switch (sink->cpu_dev) {
        case Cpu_Device_DM6467:
            sink->dAttrs = Display_Attrs_DM6467_VID_DEFAULT;
            break;
        case Cpu_Device_OMAP3530:
            sink->dAttrs = Display_Attrs_O3530_VID_DEFAULT;
            break;
        #if defined(Platform_dm3730)
        case Cpu_Device_DM3730:
            sink->dAttrs = Display_Attrs_O3530_VID_DEFAULT;
            break;
        #endif
        #if defined(Platform_dm365)
        case Cpu_Device_DM365:
            sink->dAttrs = Display_Attrs_DM365_VID_DEFAULT;
            sink->dAttrs.colorSpace = colorSpace;
            break;
        #endif
        #if defined(Platform_dm368)
        case Cpu_Device_DM368:
            sink->dAttrs = Display_Attrs_DM365_VID_DEFAULT;
            sink->dAttrs.colorSpace = colorSpace;
            break;
        #endif
        default:
            sink->dAttrs = Display_Attrs_DM6446_DM355_VID_DEFAULT;
            break;
    }

    /* Override the default attributes if they were set by the user */
    sink->dAttrs.numBufs = sink->numBufs == -1 ? sink->dAttrs.numBufs :
        sink->numBufs;
    sink->dAttrs.displayStd = sink->displayStd == NULL ?
        sink->dAttrs.displayStd :
        gst_tidmaivideosink_convert_attrs(VAR_DISPLAYSTD, sink);

    /* If the user set a videoStd on the command line use that, else
     * if they set the autoselect option then detect the proper
     * video standard.  If neither value was set use the default value.
     */
    sink->dAttrs.videoStd = sink->videoStd == NULL ?
        (sink->autoselect == TRUE ? gst_tidmaivideosink_find_videostd(sink) :
        sink->dAttrs.videoStd) :
        gst_tidmaivideosink_convert_attrs(VAR_VIDEOSTD, sink);

    sink->dAttrs.videoOutput = sink->videoOutput == NULL ?
        sink->dAttrs.videoOutput :
        gst_tidmaivideosink_convert_attrs(VAR_VIDEOOUTPUT, sink);
    sink->dAttrs.displayDevice = sink->displayDevice == NULL ?
        sink->dAttrs.displayDevice : sink->displayDevice;

    /* Specify framerate if supported by DMAI and the display driver */
    if (sink->can_set_display_framerate) {
        #if defined (Platform_dm365) || defined(Platform_dm368)
        sink->dAttrs.forceFrameRateNum =
            gst_value_get_fraction_numerator(&sink->iattrs.framerate);
        sink->dAttrs.forceFrameRateDen =
            gst_value_get_fraction_denominator(&sink->iattrs.framerate);
        #else
        GST_ERROR("setting driver framerate is not supported\n");
        return FALSE;
        #endif
    }

    /* Set rotation on OMAP35xx/DM3730 */
    #if defined(Platform_omap3530) || defined(Platform_dm3730)
    if (sink->cpu_dev == Cpu_Device_OMAP3530 || sink->cpu_dev == Cpu_Device_DM3730) {
        sink->dAttrs.rotation = sink->rotation == -1 ?
            sink->dAttrs.rotation : sink->rotation;
    }
    #endif

    /* Validate that the inputs the user gave are correct. */
    if (sink->dAttrs.displayStd == -1) {
        GST_ERROR("displayStd is not valid\n");
        return FALSE;
    }

    if (sink->dAttrs.videoStd == -1) {
        GST_ERROR("videoStd is not valid\n");
        return FALSE;
    }

    if (sink->dAttrs.videoOutput == -1) {
        GST_ERROR("videoOutput is not valid\n");
        return FALSE;
    }

    if (sink->dAttrs.numBufs <= 0) {
        GST_ERROR("You must have at least 1 buffer to display with.  "
                  "Current value for numBufs = %d", sink->dAttrs.numBufs);
        return FALSE;
    }

    GST_DEBUG("Display Attributes:\n"
              "\tnumBufs = %d\n"
              "\tdisplayStd = %d\n"
              "\tvideoStd = %d\n"
              "\tvideoOutput = %d\n"
              "\tdisplayDevice = %s\n",
              sink->dAttrs.numBufs, sink->dAttrs.displayStd,
              sink->dAttrs.videoStd, sink->dAttrs.videoOutput,
              sink->dAttrs.displayDevice);

    ret = gst_tidmaivideosink_videostd_get_attrs(sink->dAttrs.videoStd,
                                                 &sink->oattrs);
    if (ret < 0) {
        GST_ERROR("Error getting videostd attrs ret = %d\n", ret);
        return FALSE;
    }

    GST_DEBUG("VideoStd_Attrs:\n"
              "\tvideostd = %d\n"
              "\twidth = %ld\n"
              "\theight = %ld\n"
              "\tframerate = %d/%d\n",
              sink->oattrs.videostd, sink->oattrs.width,
              sink->oattrs.height,
              gst_value_get_fraction_numerator(&sink->oattrs.framerate),
              gst_value_get_fraction_denominator(&sink->oattrs.framerate));

    GST_DEBUG("Finish\n");

    return TRUE;
}


/******************************************************************************
 * gst_tidmaivideosink_exit_display
 *    Delete any display or Framecopy objects that were created.
 *
 * NOTE:  For now this is left as a gboolean return in case there is a reason
 *        in the future that this function will return FALSE.
*******************************************************************************/
static gboolean gst_tidmaivideosink_exit_display(GstTIDmaiVideoSink * sink)
{
    GST_DEBUG("Begin\n");

    if (sink->hResize) {
        GST_DEBUG("closing resizer\n");
        Resize_delete(sink->hResize);
        sink->hResize = NULL;
    }

    if (sink->hFc) {
        GST_DEBUG("closing Framecopy\n");
        Framecopy_delete(sink->hFc);
        sink->hFc = NULL;
    }

    if (sink->hCcv) {
        GST_DEBUG("closing ccv\n");
        Ccv_delete(sink->hCcv);
        sink->hCcv = NULL;
    }

    if (sink->hDisplay) {
        GST_DEBUG("closing display\n");
        Display_delete(sink->hDisplay);
        sink->hDisplay = NULL;
    }

    if (sink->hDispBufTab) {
        GST_DEBUG("freeing display buffers\n");
        gst_tidmaibuftab_unref(sink->hDispBufTab);
        sink->hDispBufTab = NULL;
    }

    if (sink->tempDmaiBuf) {
        GST_DEBUG("Freeing temporary DMAI buffer\n");
        Buffer_delete(sink->tempDmaiBuf);
        sink->tempDmaiBuf = NULL;
    }

    gst_tidmaivideosink_close_osd(sink);

    GST_DEBUG("Finish\n");

    return TRUE;
}


/*******************************************************************************
 * gst_tidmaivideosink_init_display
 *
 * This function intializes the display.
 ******************************************************************************/
static gboolean gst_tidmaivideosink_init_display(GstTIDmaiVideoSink * sink)
{
    Resize_Attrs rAttrs = Resize_Attrs_DEFAULT;
    Ccv_Attrs ccvAttrs = Ccv_Attrs_DEFAULT;
    Framecopy_Attrs fcAttrs = Framecopy_Attrs_DEFAULT;
    ColorSpace_Type colorSpace = sink->dGfxAttrs.colorSpace;
    
    GST_DEBUG("Begin\n");

    /* Make sure this function is not called if the display has already been
     * created.
     */
    if (sink->hDisplay != NULL) {
        GST_ERROR("display has already been created");
        return FALSE;
    }

    GST_DEBUG("display handle is NULL; creating a display");

    /* Set the input videostd attrs so that when the display is created we can
     * auto-detect what size it needs to be if requested.
     */
    sink->iattrs.width  = sink->dGfxAttrs.dim.width;
    sink->iattrs.height = sink->dGfxAttrs.dim.height;

    /* If the framerate property wasn't specified, set the frame rate to what
     * we found in the caps information.
     */
    if (gst_value_get_fraction_numerator(&sink->framerate) == 0) {
        g_value_copy(&sink->dCapsFramerate, &sink->framerate);
    }

    GST_DEBUG("Frame rate = %d/%d\n",
        gst_value_get_fraction_numerator(&sink->framerate),
        gst_value_get_fraction_denominator(&sink->framerate));

    /* This loop will exit if one of the following conditions occurs:
     * 1.  The display was created
     * 2.  The display standard specified by the user was invalid
     * 3.  autoselect was enabled and no working standard could be found
     */
    while (TRUE) {
        if (!gst_tidmaivideosink_set_display_attrs(sink, colorSpace)) {
            GST_ERROR("Error while trying to set the display attributes\n");
            return FALSE;
        }

        /* If we own the display buffers, tell DMAI to delay starting the
         * display until we call Display_put for the first time.
         */
        if (sink->hDispBufTab) {
            #if defined(Platform_dm365) || defined(Platform_omap3530) || \
              defined(Platform_dm3730) || defined(Platform_dm368)
            sink->dAttrs.delayStreamon = TRUE;
            printf("delay stream on ....\n");
            #else
            GST_ERROR("delayed V4L2 streamon not supported\n");
            return FALSE;
            #endif
        }

        /* Allocate user-allocated display buffers, if requested */
        if (!sink->hDispBufTab && sink->useUserptrBufs) {
            if (!gst_tidmaivideosink_alloc_display_buffers(sink, 0)) {
                GST_ERROR("Failed to allocate display buffers");
                return FALSE;
            }
        }

        /* Open the OSD.  We don't use it, but opening it clears the contents
         * and makes it transparent so it doesn't interfere with the display.
         */
        if (sink->hideOSD && !gst_tidmaivideosink_open_osd(sink)) {
            GST_ERROR("Failed to hide the OSD");
            return FALSE;
        }

        /* Create the display device using the attributes set above */
        sink->hDisplay = Display_create(GST_TIDMAIBUFTAB_BUFTAB(
            sink->hDispBufTab), &sink->dAttrs);

        if ((sink->hDisplay == NULL) && (sink->autoselect == TRUE)) {
            GST_DEBUG("Could not create display with videoStd %d.  Searching for next valid standard.\n", 
            sink->dAttrs.videoStd);
            sink->prevVideoStd = sink->dAttrs.videoStd;
            continue;
        } else {
            /* If the display was created, or failed to be created and
             * autoselect was not set break out of the loop.
             */
            break;
        }
    } 

    if (sink->hDisplay == NULL) {
        GST_ERROR("Failed to open display device\n");
        return FALSE;
    }

    GST_DEBUG("Display Device Created\n");

    /* For DM6467 devices the frame copy is done by the color conversion engine
     */
    if (sink->cpu_dev == Cpu_Device_DM6467 && 
            colorSpace != ColorSpace_YUV422PSEMI) {
        /* Create the VDCE accelerated color conversion job from 420 to 422 */
        ccvAttrs.accel = TRUE;
        sink->hCcv = Ccv_create(&ccvAttrs);

        if (sink->hCcv == NULL) {
            GST_ERROR("Failed to create CCV job\n");
            return FALSE;
        }
        GST_DEBUG("CCV Device Created\n");
    }

    /* If user want to use the resizer to resize the image */
    else if (sink->resizer) {
        sink->hResize = Resize_create(&rAttrs);

        if (sink->hResize == NULL) {
            GST_ERROR("Failed to create resizer\n");
            return FALSE;
        }
        GST_DEBUG("Resizer Device Created\n");
    }

    /* Otherwise, use an accelerated frame copy */
    else {
        fcAttrs.accel = sink->accelFrameCopy;
        sink->hFc = Framecopy_create(&fcAttrs);

        if (sink->hFc == NULL) {
            GST_ERROR("Failed to create framcopy\n");
            return FALSE;
        }
        GST_DEBUG("Frame Copy Device Created\n");
    }

    GST_DEBUG("Finish\n");
    return TRUE;
}


/******************************************************************************
 * gst_tidmaivideosink_process_caps
 ******************************************************************************/
static gboolean gst_tidmaivideosink_process_caps(GstBaseSink * bsink,
                    GstCaps * caps)
{
    GstTIDmaiVideoSink *dmaisink  = GST_TIDMAIVIDEOSINK(bsink);
    GstStructure       *structure = NULL;
    gint                height;
    gint                width;
    guint32             fourcc;
    ColorSpace_Type     inBufColorSpace;
    gint                framerateDen;
    gint                framerateNum;

    structure = gst_caps_get_structure(caps, 0);

    /* The width and height of the input buffer are collected here so that it
     * can be checked against the width and height of the display buffer.
     */
    gst_structure_get_int(structure, "width", &width);
    gst_structure_get_int(structure, "height", &height);

    /* Map input buffer fourcc to dmai color space  */
    gst_structure_get_fourcc(structure, "format", &fourcc);

    switch (fourcc) {
        case GST_MAKE_FOURCC('U', 'Y', 'V', 'Y'):
            inBufColorSpace = ColorSpace_UYVY;
            break;
        case GST_MAKE_FOURCC('N', 'V', '1', '6'):
        case GST_MAKE_FOURCC('Y', '8', 'C', '8'):
            inBufColorSpace = ColorSpace_YUV422PSEMI;
            break;
        case GST_MAKE_FOURCC('N', 'V', '1', '2'):
            inBufColorSpace = ColorSpace_YUV420PSEMI;
            break;
        default:
            GST_ERROR("unsupported fourcc\n");
            return FALSE;
    }

    /* Read the frame rate */
    gst_structure_get_fraction(structure, "framerate", &framerateNum,
        &framerateDen);

    /* Populate the display graphics attributes */
    dmaisink->dGfxAttrs.bAttrs.reference = dmaisink->contiguousInputFrame;
    dmaisink->dGfxAttrs.dim.width        = width;
    dmaisink->dGfxAttrs.dim.height       = height;
    dmaisink->dGfxAttrs.dim.lineLength   = BufferGfx_calcLineLength(width, 
                                               inBufColorSpace);
    dmaisink->dGfxAttrs.colorSpace       = inBufColorSpace;

    gst_value_set_fraction(&dmaisink->dCapsFramerate, framerateNum,
        framerateDen);
        
    return TRUE;
}


/*******************************************************************************
 * gst_tidmaivideosink_render
 *   This is the main processing routine.  This function will receive a 
 *   buffer containing video data to be displayed on the screen.  If the
 *   display has not been initialized it will be configured and the input
 *   video frame will be copied to the display driver using hardware
 *   acceleration if possible.
*******************************************************************************/
static GstFlowReturn gst_tidmaivideosink_render(GstBaseSink * bsink,
                         GstBuffer * buf)
{
    Buffer_Handle         hDispBuf     = NULL;
    Buffer_Handle         inBuf        = NULL;
    gboolean              inBufIsOurs  = FALSE;
    GstTIDmaiVideoSink   *sink         = GST_TIDMAIVIDEOSINK(bsink);
    BufferGfx_Dimensions  dim;
    gchar                 dur_str[64];
    gchar                 ts_str[64];
    gfloat                heightper;
    gfloat                widthper;
    gint                  origHeight;
    gint                  origWidth;

    GST_DEBUG("\n\n\nBegin\n");

    /* Process the buffer caps */
    if (!gst_tidmaivideosink_process_caps(bsink, GST_BUFFER_CAPS(buf))) {
        goto cleanup;
    }

    /* If the input buffer is non dmai buffer, then allocate dmai buffer and 
     *  copy input buffer in dmai buffer using memcpy routine. 
     *  This logic will help to display non-dmai buffers. (e.g the video
     *  generated via videotestsrc plugin.
     */
    if (GST_IS_TIDMAIBUFFERTRANSPORT(buf)) {
        inBuf       = GST_TIDMAIBUFFERTRANSPORT_DMAIBUF(buf);
        inBufIsOurs = (sink->hDispBufTab &&
                          GST_TIDMAIBUFTAB_BUFTAB(sink->hDispBufTab) ==
                              Buffer_getBufTab(inBuf));
    } else {
        /* allocate DMAI buffer */
        if (sink->tempDmaiBuf == NULL) {

            GST_DEBUG("\nInput buffer is non-dmai, allocating new buffer");
            sink->tempDmaiBuf = Buffer_create(
                buf->size, BufferGfx_getBufferAttrs(&sink->dGfxAttrs));

            if (sink->tempDmaiBuf == NULL) {
                GST_ELEMENT_ERROR(sink, RESOURCE, FAILED, 
                ("Failed to allocate memory for input buffer\n"), (NULL));
                goto cleanup;
            }
        }
        inBuf = sink->tempDmaiBuf;
        
        #if defined(Platform_dm365) || defined(Platform_dm368)
        /* DM365: TO componsate resizer 32-byte alignment, we need to set
         * lineLength to roundup on 32-byte boundry.
         */
        if (sink->dGfxAttrs.colorSpace == ColorSpace_YUV420PSEMI) {
            BufferGfx_getDimensions(inBuf, &dim);

            if (GST_BUFFER_SIZE(buf) > gst_ti_calc_buffer_size(dim.width,
                dim.height, 0, sink->dGfxAttrs.colorSpace)) {
                dim.lineLength = Dmai_roundUp(dim.lineLength, 32);
                BufferGfx_setDimensions(inBuf, &dim);
            }
        }
        #endif

        /* If contiguous input frame is not set then use memcpy to copy the 
         * input buffer in contiguous dmai buffer.
         */
        if (sink->contiguousInputFrame) {
            Buffer_setUserPtr(inBuf, (Int8*)buf->data);
        }
        else {
            memcpy(Buffer_getUserPtr(inBuf), buf->data, buf->size);
        }
    }

    /* If the Display_Handle element is NULL, then either this is our first
     * buffer or the upstream element has re-nogatiated our capabilities which
     * resulted in our display handle being closed.  In either case we need to
     * initialize (or re-initialize) our display to handle the new stream.
     */
    if (sink->hDisplay == NULL) {
        if (!gst_tidmaivideosink_init_display(sink)) {
            GST_ELEMENT_ERROR(sink, RESOURCE, FAILED,
             ("Unable to initialize display\n"), (NULL));
            goto cleanup;
        }
    }

    /* If the input buffer originated from this element via pad allocation,
     * simply give it back to the display and continue.
     */
    if (inBufIsOurs) {

        /* Mark buffer as in-use by the display so it can't be re-used
         * until it comes back from Display_get */
        Buffer_setUseMask(inBuf, Buffer_getUseMask(inBuf) |
            gst_tidmaibuffer_DISPLAY_FREE);

        if (Display_put(sink->hDisplay, inBuf) < 0) {
            GST_ELEMENT_ERROR(sink, RESOURCE, FAILED,
            ("Failed to put display buffer\n"), (NULL));
            goto cleanup;
        }
        goto finish;
    }

    /* Otherwise, our input buffer originated from up-stream.  Retrieve a
     * display buffer to copy the contents into.
     */
    else {
        if (Display_get(sink->hDisplay, &hDispBuf) < 0) {
            GST_ELEMENT_ERROR(sink, RESOURCE, FAILED,
            ("Failed to get display buffer\n"), (NULL));
            goto cleanup;
        }
    }

    /* Retrieve the dimensions of the display buffer */
    BufferGfx_getDimensions(hDispBuf, &dim);
    GST_LOG("Display size %dx%d pitch %d\n",
            (Int) dim.width, (Int) dim.height, (Int) dim.lineLength);


    if (sink->resizer) {

        /* resize video image while maintaining the aspect ratio */
        BufferGfx_getDimensions(hDispBuf, &dim);
        if (sink->dGfxAttrs.dim.width > sink->dGfxAttrs.dim.height) {

            origHeight = dim.height;
            widthper   = (float)dim.width / sink->dGfxAttrs.dim.width;
            dim.height = sink->dGfxAttrs.dim.height * widthper;

            if (dim.height > origHeight)
                dim.height = origHeight;
            else
                dim.y = (origHeight - dim.height) / 2;

        } else {

            origWidth = dim.width;
            heightper = (float)dim.height / sink->dGfxAttrs.dim.height;
            dim.width = sink->dGfxAttrs.dim.width * heightper;

            if (dim.width > origWidth)
                dim.width = origWidth;
            else
                dim.x = (origWidth - dim.width) / 2;
        }
        BufferGfx_setDimensions(hDispBuf, &dim);

        /* Configure resizer */
        if (Resize_config(sink->hResize, inBuf, hDispBuf) < 0) {
            GST_ELEMENT_ERROR(sink, RESOURCE, FAILED,
            ("Failed to configure resizer\n"), (NULL));
            goto cleanup;
        }

        if (Resize_execute(sink->hResize, inBuf, hDispBuf) < 0) {
            GST_ELEMENT_ERROR(sink, RESOURCE, FAILED,
            ("Failed to execute resizer\n"), (NULL));
            goto cleanup;
        }

    } else {
        /* Check that the input buffer sizer are not too large for the 
         * display buffers.  This is because the display buffers are 
         * pre-allocated by the driver and are not increased at run time 
         * (for fbdev devices, for v4l2  devices these can grow at run time)
         * .However, since we don't expect the size of the clip to increase
         * in the middle of playing we will not resize the window, even for
         * v4l2 after the initial setup.  Instead we will only display the
         * portion of the video that fits on the screen.  If the video is 
         * smaller than the display center it in the screen.
         * TODO: later add an option to resize the video.
         */
        if (sink->dGfxAttrs.dim.width > dim.width) {
            GST_INFO("Input image width (%ld) greater than display width"
                     " (%ld)\n Image cropped to fit screen\n",
                     sink->dGfxAttrs.dim.width, dim.width);
            dim.x = 0;
        } else {
            dim.x     = ((dim.width - sink->dGfxAttrs.dim.width) / 2) & ~1;
            dim.width = sink->dGfxAttrs.dim.width;
        }

        if (sink->dGfxAttrs.dim.height > dim.height) {
            GST_INFO("Input image height (%ld) greater than display height"
                     " (%ld)\n Image cropped to fit screen\n",
                     sink->dGfxAttrs.dim.height, dim.height);
            dim.y = 0;
        } else {
            dim.y      = (dim.height - sink->dGfxAttrs.dim.height) / 2;
            dim.height = sink->dGfxAttrs.dim.height;
        }
        
        #if defined(Platform_dm365) || defined(Platform_dm368)
        /* Y must be even otherwise DMAI will report asseration */
        dim.y = dim.y & 0xFFFE;
        #endif

        #if defined(Platform_omap3530) || defined(Platform_dm3730)
        /* X must be even otherwise DMAI will report asseration */
        dim.x = dim.x & 0xFFFE;
        #endif

        BufferGfx_setDimensions(hDispBuf, &dim);

        /* DM6467 Only: Color convert the 420Psemi decoded buffer from
         * the video thread to the 422Psemi display.
         */
        if (sink->cpu_dev == Cpu_Device_DM6467 && 
                sink->dGfxAttrs.colorSpace != ColorSpace_YUV422PSEMI) {

            /* Configure the 420->422 color conversion job */
            if (Ccv_config(sink->hCcv, inBuf, hDispBuf) < 0) {
                GST_ELEMENT_ERROR(sink, RESOURCE, FAILED,
                ("Failed to configure CCV job\n"), (NULL));
                goto cleanup;
            }

            if (Ccv_execute(sink->hCcv, inBuf, hDispBuf) < 0) {
                GST_ELEMENT_ERROR(sink, RESOURCE, FAILED,
                ("Failed to execute CCV job\n"), (NULL));
                goto cleanup;
            }
        } else {
            if (Framecopy_config(sink->hFc, inBuf, hDispBuf) < 0) {
                GST_ELEMENT_ERROR(sink, RESOURCE, FAILED,
                ("Failed to configure framecopy\n"), (NULL));
                goto cleanup;
            }

            if (Framecopy_execute(sink->hFc, inBuf, hDispBuf) < 0) {
                GST_ELEMENT_ERROR(sink, RESOURCE, FAILED,
                ("Failed to execute framecopy\n"), (NULL));
                goto cleanup;
            }
        }
    }

    BufferGfx_resetDimensions(hDispBuf);

    /* Send filled buffer to display device driver to be displayed */
    if (Display_put(sink->hDisplay, hDispBuf) < 0) {
        GST_ELEMENT_ERROR(sink, RESOURCE, FAILED,
        ("Failed to put display buffer\n"), (NULL));
        goto cleanup;
    }

finish:

    if (GST_BUFFER_TIMESTAMP(buf) != GST_CLOCK_TIME_NONE) {
        g_snprintf(ts_str, sizeof(ts_str), "%" GST_TIME_FORMAT,
                   GST_TIME_ARGS(GST_BUFFER_TIMESTAMP(buf)));
    } else {
        g_strlcpy(ts_str, "none", sizeof(ts_str));
    }

    if (GST_BUFFER_DURATION(buf) != GST_CLOCK_TIME_NONE) {
        g_snprintf(dur_str, sizeof(dur_str), "%" GST_TIME_FORMAT,
                   GST_TIME_ARGS(GST_BUFFER_DURATION(buf)));
    } else {
        g_strlcpy(dur_str, "none", sizeof(dur_str));
    }

    GST_LOG("render  ******* < (%5d bytes, timestamp: %s, duration: %s"
            ", offset: %" G_GINT64_FORMAT ", offset_end: %" G_GINT64_FORMAT
            ", flags: %d) %p\n", GST_BUFFER_SIZE(buf), ts_str, dur_str,
            GST_BUFFER_OFFSET(buf), GST_BUFFER_OFFSET_END(buf),
            GST_MINI_OBJECT(buf)->flags, buf);

    /* Signal the handoff in case anyone is looking for that */
    if (sink->signal_handoffs)
        g_signal_emit(G_OBJECT(sink),
            gst_tidmaivideosink_signals[SIGNAL_HANDOFF], 0,buf,
                bsink->sinkpad);

    GST_DEBUG("Finish\n\n\n");

    return GST_FLOW_OK;

cleanup:

    if (!gst_tidmaivideosink_exit_display(sink))
        GST_ERROR("gst_tidmaivideosink_render: Could not close the"
                  " display on error\n");

    return GST_FLOW_UNEXPECTED;
}


/*******************************************************************************
 * gst_tidmaivideosink_change_state
 *
 * For now this function just handles the cleanup transition.
*******************************************************************************/
static GstStateChangeReturn gst_tidmaivideosink_change_state(
                                GstElement * element,
                                GstStateChange transition)
{
    GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
    GstTIDmaiVideoSink *dmaisink = GST_TIDMAIVIDEOSINK(element);

    GST_DEBUG("Begin\n");

    /* Handle ramp-up state changes.  Nothing to do here as of now. */
    switch (transition) {
        case GST_STATE_CHANGE_NULL_TO_READY:
            break;
        case GST_STATE_CHANGE_READY_TO_PAUSED:
            break;
        case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
            break;
        default:
            break;
    }

    /* Pass state changes to base class */
    ret = GST_ELEMENT_CLASS(parent_class)->change_state(element, transition);
    if (ret == GST_STATE_CHANGE_FAILURE)
        return ret;

    /* Handle ramp-down state changes */
    switch (transition) {
        case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
            break;
        case GST_STATE_CHANGE_PAUSED_TO_READY:
            break;
        case GST_STATE_CHANGE_READY_TO_NULL:
            /* Shutdown any display and frame copy devices */
            if (!gst_tidmaivideosink_exit_display(dmaisink)) {
                GST_ERROR("Failed to exit the display\n");
                goto error;
            }
            break;
        default:
            break;
    }

    GST_DEBUG("Finish\n");
    return ret;

error:
    GST_ELEMENT_ERROR(element, CORE, STATE_CHANGE, (NULL), (NULL));

    return GST_STATE_CHANGE_FAILURE;
}


/*******************************************************************************
 * gst_tidmaivideosink_get_caps
 *
 * This is mainly a place holder function.
*******************************************************************************/
static GstCaps *gst_tidmaivideosink_get_caps(GstBaseSink * bsink)
{
    GstTIDmaiVideoSink *sink;

    GST_DEBUG("Begin\n");

    sink = GST_TIDMAIVIDEOSINK(bsink);

    GST_DEBUG("Finish\n");

    return gst_caps_copy(gst_pad_get_pad_template_caps
               (GST_VIDEO_SINK_PAD(sink)));
}


/*******************************************************************************
 * gst_tidmaivideosink_set_caps
 *
 * This is mainly a place holder function.
*******************************************************************************/
static gboolean gst_tidmaivideosink_set_caps(GstBaseSink * bsink,
                    GstCaps * caps)
{
    /* Just return true for now.  I don't have anything to set here yet */
    return TRUE;
}


/******************************************************************************
 * gst_tidmaivideosink_alloc_display_buffers
 *
 * Allocate display buffers.
 ******************************************************************************/
static gboolean gst_tidmaivideosink_alloc_display_buffers(
                    GstTIDmaiVideoSink * sink, Int32 bufSize)
{
    BufferGfx_Attrs gfxAttrs = BufferGfx_Attrs_DEFAULT;

    if (sink->dAttrs.displayStd != Display_Std_V4L2) {
        GST_ERROR("useUserptrBufs=TRUE can only be used with V4L2 displays\n");
        return FALSE;
    }

    GST_INFO("Allocating %d display buffers", sink->dAttrs.numBufs);

    /* Set the dimensions for the display */
    if (VideoStd_getResolution(sink->dAttrs.videoStd, &gfxAttrs.dim.width,
        &gfxAttrs.dim.height) < 0) {
        GST_ERROR("Failed to calculate resolution of video standard\n");
        return FALSE;
    }

    /* Set the colorspace for the display.  On DM365, it will match that of
     * the input buffer and has already been set in the display attributes.  
     * For other platforms, unfortunately there is no default set and we need
     * to specify one.
     */
    #if defined(Platform_dm365) || defined(Platform_dm368)
        gfxAttrs.colorSpace = sink->dAttrs.colorSpace;
    #elif defined(Platform_dm6467) || defined(Platform_dm6467t)
        gfxAttrs.colorSpace = ColorSpace_YUV422PSEMI;
    #else /* default to UYVY */
        gfxAttrs.colorSpace = ColorSpace_UYVY;
    #endif

    /* Set the pitch (lineLength) for the display. */
    gfxAttrs.dim.lineLength =
        BufferGfx_calcLineLength(gfxAttrs.dim.width, gfxAttrs.colorSpace);

    /* On DM365 the display pitch must be aligned to a 32-byte boundary */
    #if defined(Platform_dm365) || defined(Platform_dm368)
        gfxAttrs.dim.lineLength = Dmai_roundUp(gfxAttrs.dim.lineLength, 32);
    #endif

    if (bufSize <= 0) {
        bufSize = gst_ti_calc_buffer_size(gfxAttrs.dim.width,
            gfxAttrs.dim.height, gfxAttrs.dim.lineLength, gfxAttrs.colorSpace);
    }

    gfxAttrs.bAttrs.useMask = gst_tidmaibuffer_VIDEOSINK_FREE;
    sink->hDispBufTab = gst_tidmaibuftab_new(sink->dAttrs.numBufs, bufSize,
        BufferGfx_getBufferAttrs(&gfxAttrs));
    gst_tidmaibuftab_set_blocking(sink->hDispBufTab, FALSE);

    return TRUE;
}


/******************************************************************************
 * gst_tidmaivideosink_open_osd
 *    Open the OSD display on certain platforms.
 ******************************************************************************/
static gboolean gst_tidmaivideosink_open_osd(GstTIDmaiVideoSink * sink)
{
    Display_Attrs   oAttrs = {0};
    Display_Attrs   aAttrs = {0};
    Buffer_Handle   hBuf;

    #if defined(Platform_dm365) || defined(Platform_dm368)
        oAttrs = Display_Attrs_DM365_OSD_DEFAULT;
        aAttrs = Display_Attrs_DM365_ATTR_DEFAULT;
    #elif defined(Platform_dm6446) || defined(Platform_dm355)
        oAttrs = Display_Attrs_DM6446_DM355_OSD_DEFAULT;
        aAttrs = Display_Attrs_DM6446_DM355_ATTR_DEFAULT;
    #else
        return TRUE;  /* OSD not supported on this patform -- do nothing */
    #endif

    /* Determine video output settings for OSD */
    aAttrs.videoOutput = sink->dAttrs.videoOutput;

    switch (aAttrs.videoOutput) {
        case Display_Output_COMPONENT:
            aAttrs.videoStd = VideoStd_480P;
            break;

        case Display_Output_COMPOSITE:
        default:
            oAttrs.videoOutput = aAttrs.videoOutput;

            if (sink->dAttrs.videoStd == VideoStd_D1_PAL) {
                oAttrs.videoStd = VideoStd_D1_PAL;
            } else {
                oAttrs.videoStd = VideoStd_D1_NTSC;
            }
            break;
    }

    /* Open the OSD and attribute planes */
    if (!(sink->hOsdAttrs = Display_create(NULL, &aAttrs))) {
        GST_ERROR("Failed to open the OSD attribute plane\n");
        return FALSE;
    }

    if (!(sink->hOsd = Display_create(NULL, &oAttrs))) {
        GST_ERROR("Failed to open the OSD plane\n");
        return FALSE;
    }

    /* Clear the attribute plane and make the OSD transparent */
    if (Display_get(sink->hOsdAttrs, &hBuf) < 0) {
        GST_ERROR("Failed to get OSD attribute buffer\n");
        return FALSE;
    }

    memset(Buffer_getUserPtr(hBuf), 0, Buffer_getSize(hBuf));

    if (Display_put(sink->hOsdAttrs, hBuf) < 0) {
        GST_ERROR("Failed to put OSD attribute buffer\n");
        return FALSE;
    }

    return TRUE;
}


/******************************************************************************
 * gst_tidmaivideosink_close_osd
 *    Close the OSD and attribute planes.
 ******************************************************************************/
static gboolean gst_tidmaivideosink_close_osd(GstTIDmaiVideoSink * sink)
{
    if (sink->hOsd) {
        if (Display_delete(sink->hOsd) < 0) {
            GST_ERROR("Failed to close the OSD plane\n");
            return FALSE;
        }
        sink->hOsd = NULL;
    }

    if (sink->hOsdAttrs) {
        if (Display_delete(sink->hOsdAttrs) < 0) {
            GST_ERROR("Failed to close the OSD attribute plane\n");
            return FALSE;
        }
        sink->hOsdAttrs = NULL;
    }

    return TRUE;
}

/******************************************************************************
 * gst_tidmaivideosink_fraction_is_multiple
 *     Return TRUE if frac1 is a multiple of frac2.
 ******************************************************************************/
static gboolean gst_tidmaivideosink_fraction_is_multiple(
                    GValue *frac1, GValue *frac2)
{
    GValue dividend = {0, };

    g_assert(GST_VALUE_HOLDS_FRACTION(frac1));
    g_assert(GST_VALUE_HOLDS_FRACTION(frac2));

    g_value_init(&dividend, GST_TYPE_FRACTION);
    g_assert(GST_VALUE_HOLDS_FRACTION(&dividend));

    /* Divide frac2 by frac1, and see if the result is a whole number */
    gst_tidmaivideosink_fraction_divide(&dividend, frac2, frac1);

    /* The divide function normalizes the result, so we know if the
     * denominator is 1, then frac1 evenly divides frac2
     */
    return gst_value_get_fraction_denominator(&dividend) == 1;
}

/******************************************************************************
 * gst_tidmaivideosink_fraction_divide
 *     Divide frac1 by frac2, and return the result in dividend.
 ******************************************************************************/
static gboolean gst_tidmaivideosink_fraction_divide(
                    GValue *dividend, GValue *frac1, GValue *frac2)
{
    GValue recip_frac2 = {0, };

    g_assert(GST_VALUE_HOLDS_FRACTION(frac1));
    g_assert(GST_VALUE_HOLDS_FRACTION(frac2));

    g_value_init(&recip_frac2, GST_TYPE_FRACTION);
    g_assert(GST_VALUE_HOLDS_FRACTION(&recip_frac2));

    /* Division by zero is not allowed */
    if (gst_value_get_fraction_numerator(frac2) == 0) {
        return FALSE;
    }

    /* Get the reciprical of frac1 */
    gst_value_set_fraction(&recip_frac2,
        gst_value_get_fraction_denominator(frac2),
        gst_value_get_fraction_numerator(frac2));
    
    /* Divide frac1 by frac2 */
    return gst_value_fraction_multiply(dividend, frac1, &recip_frac2);
}

/******************************************************************************
 * Custom ViM Settings for editing this file
 ******************************************************************************/
#if 0
 Tabs (use 4 spaces for indentation)
 vim:set tabstop=4:      /* Use 4 spaces for tabs          */
 vim:set shiftwidth=4:   /* Use 4 spaces for >> operations */
 vim:set expandtab:      /* Expand tabs into white spaces  */
#endif
