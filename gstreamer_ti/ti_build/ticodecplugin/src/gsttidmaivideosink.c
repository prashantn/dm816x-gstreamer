/*
 * gsttidmaivideosink.c:
 *
 * Original Author:
 *     Chase Maupin, Texas Instruments, Inc.
 *     derived from fakesink
 *
 * Copyright (C) $year Texas Instruments Incorporated - http://www.ti.com/
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
 * Y8C8 - YUV 422 semi planar. The dm6467 VDCE outputs this format after a
 *        color conversion.The format consists of two planes: one with the
 *        Y component and one with the CbCr components interleaved (hence semi)  *
 *        See the LSP VDCE documentation for a thorough description of this
 *        format.
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
  PROP_CONTIG_INPUT_BUF
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
 gst_tidmaivideosink_preroll(GstBaseSink * bsink, GstBuffer * buffer);
static int
 gst_tidmaivideosink_videostd_get_attrs(VideoStd_Type videoStd,
     VideoStd_Attrs * attrs);
static gboolean
 gst_tidmaivideosink_init_display(GstTIDmaiVideoSink * sink, ColorSpace_Type);
static gboolean
 gst_tidmaivideosink_exit_display(GstTIDmaiVideoSink * sink);
static gboolean
 gst_tidmaivideosink_set_display_attrs(GstTIDmaiVideoSink * sink);
static GstFlowReturn
 gst_tidmaivideosink_render(GstBaseSink * bsink, GstBuffer * buffer);
static gboolean
 gst_tidmaivideosink_event(GstBaseSink * bsink, GstEvent * event);
static void 
    gst_tidmaivideosink_init_env(GstTIDmaiVideoSink *sink);

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
            "\t1080P_30, 1080P_25, 1080P_24\n",
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
            "(OMAP3530 only)", -1, G_MAXINT, -1, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_FRAMERATE,
        g_param_spec_int("framerate", "frame rate of video",
            "Frame rate of the video expressed as an integer", -1, G_MAXINT,
            -1, G_PARAM_READWRITE));

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
    /* Set the default values to NULL or -1.  If the user specifies a value 
     * then the element will be non-null when the display is created.  
     * Anything that has a NULL value will be initialized with DMAI defaults 
     * in the gst_tidmaivideosink_init_display function.
     */
    dmaisink->displayStd     = NULL;
    dmaisink->displayDevice  = NULL;
    dmaisink->videoStd       = NULL;
    dmaisink->videoOutput    = NULL;
    dmaisink->numBufs        = -1;
    dmaisink->framerate      = -1;
    dmaisink->rotation       = -1;
    dmaisink->tempDmaiBuf    = NULL;
    dmaisink->accelFrameCopy = TRUE;
    dmaisink->autoselect     = FALSE;
    dmaisink->prevVideoStd   = 0;

    dmaisink->signal_handoffs = DEFAULT_SIGNAL_HANDOFFS;

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
            sink->framerate = g_value_get_int(value);
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
            g_value_set_int(value, sink->framerate);
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

    switch (videoStd) {
        case VideoStd_1080P_24:
            vattrs->framerate = 24;
            break;
        case VideoStd_SIF_PAL:
        case VideoStd_D1_PAL:
        case VideoStd_1080P_25:
        case VideoStd_1080I_25:
            vattrs->framerate = 25;
            break;
        case VideoStd_CIF:
        case VideoStd_SIF_NTSC:
        case VideoStd_D1_NTSC:
        case VideoStd_1080I_30:
        case VideoStd_1080P_30:
            vattrs->framerate = 30;
            break;
        case VideoStd_576P:
        case VideoStd_720P_50:
            vattrs->framerate = 50;
            break;
        case VideoStd_480P:
        case VideoStd_720P_60:
            vattrs->framerate = 60;
            break;

        #if defined(Platform_omap3530)
        case VideoStd_VGA:
            vattrs->framerate = 60;
            break;
        #endif

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
                  "\tsink input attrs framerate = %d\n\n"
                  "Display Attributes:\n"
                  "\tdisplay width = %d\n"
                  "\tdisplay height = %d\n\n"
                  "Proposed Standard (%d) Attributes:\n"
                  "\tstandard width = %ld\n"
                  "\tstandard height = %ld\n"
                  "\tstandard framerate = %d\n",
                  sink->iattrs.width, sink->iattrs.height,
                  sink->iattrs.framerate, dwidth, dheight, i,
                  tattrs.width, tattrs.height, tattrs.framerate);

        if (((tattrs.width - sink->iattrs.width) <= dwidth) &&
            ((tattrs.height - sink->iattrs.height) <= dheight)) {
            /* Check if the frame rate is an exact match, if not check if
             * it is a multiple of the input frame rate.  If neither case
             * is true then this is not a compatable videostd.
             */
            if (sink->iattrs.framerate == tattrs.framerate)
                bestmatch = i;
            else if (!(tattrs.framerate % sink->iattrs.framerate)) {
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
 *    an input frame rate of 30fps.  This is to help determine later based
 *    on the output standard whether to repeat frames or not.
 *
 ******************************************************************************/
static void gst_tidmaivideosink_check_set_framerate(GstTIDmaiVideoSink * sink)
{
    GST_DEBUG("Begin\n");

    /* Check if the framerate was set by the user on the command line */
    if (sink->framerate != -1) {
        sink->iattrs.framerate = sink->framerate;
        return;
    }

    /* Was the frame rate set to a positive non-zero value based on the stream
     * attributes?  If not set to 30fps as a default.
     */
    if (sink->iattrs.framerate <= 0)
        sink->iattrs.framerate = 30;

    GST_DEBUG("Finish\n");
    return;
}


/******************************************************************************
 * gst_tidmaivideosink_get_framerepeat
 *
 *    This function will look at the output display frame rate and the
 *    input frame rate and determine how many times a frame should be
 *    repeated.  If the output is not an integer multiple of the input
 *    then 1 is returned to indicate that there will be no frame
 *    repeating.
 *
 ******************************************************************************/
static int gst_tidmaivideosink_get_framerepeat(GstTIDmaiVideoSink * sink)
{
    int num_repeat;

    GST_DEBUG("Begin\n");

    if ((sink->oattrs.framerate % sink->iattrs.framerate) != 0)
        num_repeat = 1;
    else
        num_repeat = sink->oattrs.framerate / sink->iattrs.framerate;

    GST_DEBUG("Finish\n");
    return num_repeat;
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
            #if defined(Platform_omap3530)
            else if (!strcmp(sink->videoStd, "VGA"))
                return VideoStd_VGA;
            #endif
            else {
                GST_ERROR("Invalid videoStd entered (%s).  "
                "Please choose from:\n"
                "\tAUTO (if supported), CIF, SIF_NTSC, SIF_PAL, VGA, D1_NTSC\n"
                "\tD1_PAL, 480P, 576P, 720P_60, 720P_50, 1080I_30, 1080I_25\n"
                "\t1080P_30, 1080P_25, 1080P_24\n", sink->videoStd);
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
            #if defined(Platform_omap3530)
            else if (!strcmp(sink->videoOutput, "DVI"))
                return Display_Output_DVI;
            else if (!strcmp(sink->videoOutput, "LCD"))
                return Display_Output_LCD;
            #endif
            else {
                GST_ERROR("Invalid videoOutput entered (%s)."
                    "Please choose from:\n"
                    "\tSVIDEO, COMPOSITE, COMPONENT, LCD, DVI\n",
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
static gboolean gst_tidmaivideosink_set_display_attrs(GstTIDmaiVideoSink *sink)
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
        #if defined(Platform_omap3530)
        case Cpu_Device_OMAP3530:
            sink->dAttrs = Display_Attrs_O3530_VID_DEFAULT;
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

    /* Set rotation on OMAP35xx */
    #if defined(Platform_omap3530)
    if (sink->cpu_dev == Cpu_Device_OMAP3530) {
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
              "\tframerate = %d\n",
              sink->oattrs.videostd, sink->oattrs.width,
              sink->oattrs.height, sink->oattrs.framerate);

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

    if (sink->tempDmaiBuf) {
        GST_DEBUG("Freeing temporary DMAI buffer\n");
        Buffer_delete(sink->tempDmaiBuf);
        sink->tempDmaiBuf = NULL;
    }

    GST_DEBUG("Finish\n");

    return TRUE;
}


/*******************************************************************************
 * gst_tidmaivideosink_init_display
 *
 * This function will intialize the display.  To do so it will:
 * 
 * 1.  Determine the Cpu device and set the defaults for that device
 * 2.  If the user specified display parameters on the command line
 *     override the defaults with those parameters.
 * 3.  Create the display device handle
 * 4.  Create the frame copy device handle
 * 
 *
 * TODO: As of now this function will need to be updated for how to set the
 *       default display attributes whenever a new device is added.  Hopefully
 *       there is a way around that.
*******************************************************************************/
static gboolean gst_tidmaivideosink_init_display(GstTIDmaiVideoSink * sink,
    ColorSpace_Type colorSpace)
{
    Resize_Attrs rAttrs = Resize_Attrs_DEFAULT;
    Ccv_Attrs ccvAttrs = Ccv_Attrs_DEFAULT;
    Framecopy_Attrs fcAttrs = Framecopy_Attrs_DEFAULT;
    

    GST_DEBUG("Begin\n");

    /* This is an extra check that the display was not already created */
    if (sink->hDisplay != NULL)
        return TRUE;

    /* This loop will exit if one of the following conditions occurs:
     * 1.  The display was created
     * 2.  The display standard specified by the user was invalid
     * 3.  autoselect was enabled and no working standard could be found
     */
    while (TRUE) {
        if (!gst_tidmaivideosink_set_display_attrs(sink)) {
            GST_ERROR("Error while trying to set the display attributes\n");
            return FALSE;
        }

        /* Create the display device using the attributes set above */
        sink->hDisplay = Display_create(NULL, &sink->dAttrs);

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
    BufferGfx_Attrs       gfxAttrs  = BufferGfx_Attrs_DEFAULT;
    Buffer_Handle         hDispBuf  = NULL;
    Buffer_Handle         inBuf     = NULL;
    GstCaps              *temp_caps = GST_BUFFER_CAPS(buf);
    GstStructure         *structure = NULL;
    GstTIDmaiVideoSink   *sink      = GST_TIDMAIVIDEOSINK_CAST(bsink);
    BufferGfx_Dimensions  dim;
    gchar                 dur_str[64];
    gchar                 ts_str[64];
    gfloat                heightper;
    gfloat                widthper;
    gint                  framerateDen;
    gint                  framerateNum;
    gint                  height;
    gint                  i;
    gint                  origHeight;
    gint                  origWidth;
    gint                  width;
    ColorSpace_Type       inBufColorSpace;
    guint32               fourcc;

    GST_DEBUG("\n\n\nBegin\n");

    structure = gst_caps_get_structure(temp_caps, 0);
    /* The width and height of the input buffer are collected here
     * so that it can be checked against the width and height of the
     * display buffer.
     */
    gst_structure_get_int(structure, "width", &width);
    gst_structure_get_int(structure, "height", &height);

    /* Map input buffer fourcc to dmai color space  */
    gst_structure_get_fourcc(structure, "format", &fourcc);

    switch (fourcc) {
        case GST_MAKE_FOURCC('U', 'Y', 'V', 'Y'):
            inBufColorSpace = ColorSpace_UYVY;
            break;
        case GST_MAKE_FOURCC('Y', '8', 'C', '8'):
            inBufColorSpace = ColorSpace_YUV422PSEMI;
            break;
        default:
            GST_ERROR("unsupport fourcc\n");
            goto cleanup;
    }
        
    /* If the input buffer is non dmai buffer, then allocate dmai buffer and 
     *  copy input buffer in dmai buffer using memcpy routine. 
     *  This logic will help to display non-dmai buffers. (e.g the video
     *  generated via videotestsrc plugin.
     */
    if (GST_IS_TIDMAIBUFFERTRANSPORT(buf)) {
        inBuf = GST_TIDMAIBUFFERTRANSPORT_DMAIBUF(buf);
    } else {
        /* allocate DMAI buffer */
        if (sink->tempDmaiBuf == NULL) {

            GST_DEBUG("\nInput buffer is non-dmai, allocating new buffer");
            gfxAttrs.bAttrs.reference   = sink->contiguousInputFrame;
            gfxAttrs.dim.width          = width;
            gfxAttrs.dim.height         = height;
            gfxAttrs.dim.lineLength     = BufferGfx_calcLineLength(width, 
                                            inBufColorSpace);
            gfxAttrs.colorSpace         = inBufColorSpace;
            sink->tempDmaiBuf           = Buffer_create(buf->size,
                                           BufferGfx_getBufferAttrs(&gfxAttrs));

            if (sink->tempDmaiBuf == NULL) {
                GST_ERROR("Failed to allocate memory for input buffer\n");
                goto cleanup;
            }
        }
        inBuf = sink->tempDmaiBuf;
        
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
        gst_structure_get_fraction(structure, "framerate",
            &framerateNum, &framerateDen);

        /* Set the input videostd attrs so that when the display is created
         * we can know what size it needs to be.
         */
        sink->iattrs.width  = width;
        sink->iattrs.height = height;

        /* Set the input frame rate.  Round to the nearest integer */
        sink->iattrs.framerate =
            (int)(((gdouble) framerateNum / framerateDen) + .5);

        GST_DEBUG("Frame rate numerator = %d\n", framerateNum);
        GST_DEBUG("Frame rate denominator = %d\n", framerateDen);
        GST_DEBUG("Frame rate rounded = %d\n", sink->iattrs.framerate);

        GST_DEBUG("Display Handle does not exist.  Creating a display\n");

        if (!gst_tidmaivideosink_init_display(sink, inBufColorSpace)) {
            GST_ERROR("Unable to initialize display\n");
            goto cleanup;
        }

        /* Calculate the required framerepeat now that we have initialized
         * the display and know the output frame rate.
         */
        sink->framerepeat = gst_tidmaivideosink_get_framerepeat(sink);
    }

    for (i = 0; i < sink->framerepeat; i++) {

        /* Get a buffer from the display driver */
        if (Display_get(sink->hDisplay, &hDispBuf) < 0) {
            GST_ERROR("Failed to get display buffer\n");
            goto cleanup;
        }

        /* Retrieve the dimensions of the display buffer */
        BufferGfx_getDimensions(hDispBuf, &dim);
        GST_LOG("Display size %dx%d pitch %d\n",
                (Int) dim.width, (Int) dim.height, (Int) dim.lineLength);


        if (sink->resizer) {

            /* TODO: Update DMAI CCv module to support resizing and 
             *  color conversion. 
             */
            if (sink->cpu_dev == Cpu_Device_DM6467) {
                GST_LOG("This plugin does not support resizer on"
                        " DM6467.\n");
                goto cleanup;
            }

            /* resize video image while maintaining the aspect ratio */
            BufferGfx_getDimensions(hDispBuf, &dim);
            if (width > height) {

                origHeight = dim.height;
                widthper   = (float)dim.width / width;
                dim.height = height * widthper;

                if (dim.height > origHeight)
                    dim.height = origHeight;
                else
                    dim.y = (origHeight - dim.height) / 2;

            } else {

                origWidth = dim.width;
                heightper = (float)dim.height / height;
                dim.width = width * heightper;

                if (dim.width > origWidth)
                    dim.width = origWidth;
                else
                    dim.x = (origWidth - dim.width) / 2;
            }
            BufferGfx_setDimensions(hDispBuf, &dim);

            /* Configure resizer */
            if (Resize_config(sink->hResize, inBuf, hDispBuf) < 0) {
                GST_LOG("Failed to configure resizer\n");
                goto cleanup;
            }

            if (Resize_execute(sink->hResize, inBuf, hDispBuf) < 0) {
                GST_LOG("Failed to execute resizer\n");
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
            if (width > dim.width) {
                GST_INFO("Input image width (%d) greater than display width"
                         " (%ld)\n Image cropped to fit screen\n",
                         width, dim.width);
                dim.x = 0;
            } else {
                dim.x     = ((dim.width - width) / 2) & ~1;
                dim.width = width;
            }

            if (height > dim.height) {
                GST_INFO("Input image height (%d) greater than display height"
                         " (%ld)\n Image cropped to fit screen\n",
                         height, dim.height);
                dim.y = 0;
            } else {
                dim.y      = (dim.height - height) / 2;
                dim.height = height;
            }
            BufferGfx_setDimensions(hDispBuf, &dim);

            /* DM6467 Only: Color convert the 420Psemi decoded buffer from
             * the video thread to the 422Psemi display.
             */
            if (sink->cpu_dev == Cpu_Device_DM6467 && 
                    inBufColorSpace != ColorSpace_YUV422PSEMI) {

                /* Configure the 420->422 color conversion job */
                if (Ccv_config(sink->hCcv, inBuf, hDispBuf) < 0) {
                    GST_ERROR("Failed to configure CCV job\n");
                    goto cleanup;
                }

                if (Ccv_execute(sink->hCcv, inBuf, hDispBuf) < 0) {
                    GST_ERROR("Failed to execute CCV job\n");
                    goto cleanup;
                }
            } else {
                if (Framecopy_config(sink->hFc, inBuf, hDispBuf) < 0) {
                    GST_ERROR("Failed to configure framecopy\n");
                    goto cleanup;
                }

                if (Framecopy_execute(sink->hFc, inBuf, hDispBuf) < 0) {
                    GST_ERROR("Failed to execute resizer\n");
                    goto cleanup;
                }
            }
        }

        BufferGfx_resetDimensions(hDispBuf);

        /* Send filled buffer to display device driver to be displayed */
        if (Display_put(sink->hDisplay, hDispBuf) < 0) {
            GST_ERROR("Failed to put display buffer\n");
            goto cleanup;
        }
    }

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
 * Thisis mainly a place holder function.
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
 * Thisis mainly a place holder function.
*******************************************************************************/
static gboolean gst_tidmaivideosink_set_caps(GstBaseSink * bsink,
                    GstCaps * caps)
{
    /* Just return true for now.  I don't have anything to set here yet */
    return TRUE;
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
