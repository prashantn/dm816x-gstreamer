/*
 * gsttividdec2.c
 *
 * This file defines the "TIViddec2" element, which decodes an xDM 1.2 video
 * stream.
 *
 * Example usage:
 *     gst-launch filesrc location=<video file> !
 *         TIViddec2 engineName="<engine name>" codecName="<codecName>" !
 *         fakesink silent=TRUE
 *
 * Notes:
 *  * If the upstream element (i.e. demuxer or typefind element) negotiates
 *    caps with TIViddec2, the engineName and codecName properties will be
 *    auto-detected based on the mime type requested.  The engine and codec
 *    names used for particular mime types are defined in gsttividdec2.h.
 *    Currently, they are set to use the engine and codec names provided with
 *    the TI evaluation codecs.
 *  * This element currently assumes that the codec produces UYVY output.
 *
 * Original Author:
 *     Don Darling, Texas Instruments, Inc.
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
#  include <config.h>
#endif

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <gst/gst.h>

#include <ti/sdo/dmai/Dmai.h>
#include <ti/sdo/dmai/VideoStd.h>
#include <ti/sdo/dmai/Cpu.h>
#include <ti/sdo/dmai/Buffer.h>
#include <ti/sdo/dmai/BufferGfx.h>
#include <ti/sdo/dmai/BufTab.h>
#include <ti/sdo/dmai/ce/Vdec2.h>

#include "gsttividdec2.h"
#include "gsttidmaibuffertransport.h"
#include "gstticodecs.h"
#include "gsttithreadprops.h"
#include "gsttiquicktime_h264.h"
#include "gstticommonutils.h"
#include "gsttiquicktime_mpeg4.h"

/* Declare variable used to categorize GST_LOG output */
GST_DEBUG_CATEGORY_STATIC (gst_tividdec2_debug);
#define GST_CAT_DEFAULT gst_tividdec2_debug

/* Element property identifiers */
enum
{
  PROP_0,
  PROP_ENGINE_NAME,     /* engineName     (string)  */
  PROP_CODEC_NAME,      /* codecName      (string)  */
  PROP_NUM_OUTPUT_BUFS, /* numOutputBufs  (int)     */
  PROP_FRAMERATE,       /* framerate      (GstFraction) */
  PROP_DISPLAY_BUFFER,  /* displayBuffer  (boolean) */
  PROP_GEN_TIMESTAMPS,  /* genTimeStamps  (boolean) */
  PROP_RTCODECTHREAD,   /* rtCodecThread (boolean) */
  PROP_PAD_ALLOC_OUTBUFS /* padAllocOutbufs (boolean) */
};

/* Define sink (input) pad capabilities.  Currently, MPEG and H264 are 
 * supported.
 */
static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE(
    "sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS
    ("video/mpeg, " 
     "mpegversion=(int){ 2, 4 }, "  /* MPEG versions 2 and 4 */
         "systemstream=(boolean)false, "
         "framerate=(fraction)[ 0, MAX ], "
         "width=(int)[ 1, MAX ], "
         "height=(int)[ 1, MAX ] ;"
     "video/x-h264, "                             /* H264                  */
         "framerate=(fraction)[ 0, MAX ], "
         "width=(int)[ 1, MAX ], "
         "height=(int)[ 1, MAX ] ;"
     "video/x-divx, "                             /* DivX (MPEG-4 ASP)     */
         "divxversion=(int)[ 0, MAX ], "
         "width=(int)[ 1, MAX ], "
         "height=(int)[ 1, MAX ] ;"
     "video/x-xvid, "                             /* XviD (MPEG-4 ASP)     */
         "width=(int)[ 1, MAX ], "
         "height=(int)[ 1, MAX ]"
    )
);


/* Define source (output) pad capabilities.  Currently, UYVY is supported. */
static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE(
    "src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS
    ("video/x-raw-yuv, "                        /* UYVY */
         "format=(fourcc)UYVY, "
         "framerate=(fraction)[ 0, MAX ], "
         "width=(int)[ 1, MAX ], "
         "height=(int)[ 1, MAX ];"
    "video/x-raw-yuv, "                        /* NV12 */
         "format=(fourcc)NV12, "
         "framerate=(fraction)[ 0, MAX ], "
         "width=(int)[ 1, MAX ], "
         "height=(int)[ 1, MAX ];"
    "video/x-raw-yuv, "                        /* I420 */
         "format=(fourcc)I420, "
         "framerate=(fraction)[ 0, MAX ], "
         "width=(int)[ 1, MAX ], "
         "height=(int)[ 1, MAX ];"
    )
);

/* Declare a global pointer to our element base class */
static GstElementClass *parent_class = NULL;

/* Static Function Declarations */
static void
 gst_tividdec2_base_init(gpointer g_class);
static void
 gst_tividdec2_class_init(GstTIViddec2Class *g_class);
static void
 gst_tividdec2_init(GstTIViddec2 *object, GstTIViddec2Class *g_class);
static void
 gst_tividdec2_set_property (GObject *object, guint prop_id,
     const GValue *value, GParamSpec *pspec);
static void
 gst_tividdec2_get_property (GObject *object, guint prop_id, GValue *value,
     GParamSpec *pspec);
static gboolean
 gst_tividdec2_set_sink_caps(GstPad *pad, GstCaps *caps);
static gboolean
 gst_tividdec2_set_source_caps(GstTIViddec2 *viddec2, Buffer_Handle hBuf);
static gboolean
 gst_tividdec2_set_source_caps_base(GstTIViddec2 *viddec2, Int32 width,
     Int32 height, ColorSpace_Type colorSpace);
static gboolean
 gst_tividdec2_sink_event(GstPad *pad, GstEvent *event);
static GstFlowReturn
 gst_tividdec2_chain(GstPad *pad, GstBuffer *buf);
static gboolean
 gst_tividdec2_init_video(GstTIViddec2 *viddec2);
static gboolean
 gst_tividdec2_exit_video(GstTIViddec2 *viddec2);
static GstStateChangeReturn
 gst_tividdec2_change_state(GstElement *element, GstStateChange transition);
static void*
 gst_tividdec2_decode_thread(void *arg);
static void
 gst_tividdec2_drain_pipeline(GstTIViddec2 *viddec2);
static GstClockTime
 gst_tividdec2_frame_duration(GstTIViddec2 *viddec2);
static gboolean
 gst_tividdec2_resizeBufTab(GstTIViddec2 *viddec2);
static gboolean
    gst_tividdec2_codec_start (GstTIViddec2  *viddec2, GstBuffer **padBuffer);
static gboolean 
    gst_tividdec2_codec_stop (GstTIViddec2  *viddec2);
static void 
    gst_tividdec2_init_env(GstTIViddec2 *viddec2);
static void
    gst_tividdec2_dispose(GObject * object);
static gboolean 
    gst_tividdec2_set_query_pad(GstPad * pad, GstQuery * query);

/******************************************************************************
 * gst_tividdec2_class_init_trampoline
 *    Boiler-plate function auto-generated by "make_element" script.
 ******************************************************************************/
static void gst_tividdec2_class_init_trampoline(gpointer g_class,
                gpointer data)
{
    parent_class = (GstElementClass*) g_type_class_peek_parent(g_class);
    gst_tividdec2_class_init((GstTIViddec2Class*)g_class);
}


/******************************************************************************
 * gst_tividdec2_get_type
 *    Boiler-plate function auto-generated by "make_element" script.
 *    Defines function pointers for initialization routines for this element.
 ******************************************************************************/
GType gst_tividdec2_get_type(void)
{
    static GType object_type = 0;

    if (G_UNLIKELY(object_type == 0)) {
        static const GTypeInfo object_info = {
            sizeof(GstTIViddec2Class),
            gst_tividdec2_base_init,
            NULL,
            gst_tividdec2_class_init_trampoline,
            NULL,
            NULL,
            sizeof(GstTIViddec2),
            0,
            (GInstanceInitFunc) gst_tividdec2_init
        };

        object_type = g_type_register_static((gst_element_get_type()),
                          "GstTIViddec2", &object_info, (GTypeFlags)0);

        /* Initialize GST_LOG for this object */
        GST_DEBUG_CATEGORY_INIT(gst_tividdec2_debug, "TIViddec2", 0,
            "TI xDM 1.2 Video Decoder");

        GST_LOG("initialized get_type\n");
    }

    return object_type;
};


/******************************************************************************
 * gst_tividdec2_base_init
 *    Boiler-plate function auto-generated by "make_element" script.
 *    Initializes element base class.
 ******************************************************************************/
static void gst_tividdec2_base_init(gpointer gclass)
{
    static GstElementDetails element_details = {
        "TI xDM 1.2 Video Decoder",
        "Codec/Decoder/Video",
        "Decodes video using an xDM 1.2-based codec",
        "Don Darling; Texas Instruments, Inc."
    };

    GstElementClass *element_class = GST_ELEMENT_CLASS(gclass);

    gst_element_class_add_pad_template(element_class,
        gst_static_pad_template_get (&src_factory));
    gst_element_class_add_pad_template(element_class,
        gst_static_pad_template_get (&sink_factory));
    gst_element_class_set_details(element_class, &element_details);

}

/******************************************************************************
 * gst_tividdec2_dispose
 *****************************************************************************/
static void gst_tividdec2_dispose(GObject * object)
{
    GstTIViddec2 *viddec2 = GST_TIVIDDEC2(object);

    if (viddec2->segment) {
        gst_segment_free(viddec2->segment);
        viddec2->segment = NULL;
    }

    G_OBJECT_CLASS(parent_class)->dispose (object);
}



/******************************************************************************
 * gst_tividdec2_class_init
 *    Boiler-plate function auto-generated by "make_element" script.
 *    Initializes the TIViddec2 class.
 ******************************************************************************/
static void gst_tividdec2_class_init(GstTIViddec2Class *klass)
{
    GObjectClass    *gobject_class;
    GstElementClass *gstelement_class;

    gobject_class    = (GObjectClass*)    klass;
    gstelement_class = (GstElementClass*) klass;

    gobject_class->set_property = gst_tividdec2_set_property;
    gobject_class->get_property = gst_tividdec2_get_property;
    gobject_class->dispose      = GST_DEBUG_FUNCPTR(gst_tividdec2_dispose);

    gstelement_class->change_state = gst_tividdec2_change_state;

    g_object_class_install_property(gobject_class, PROP_ENGINE_NAME,
        g_param_spec_string("engineName", "Engine Name",
            "Engine name used by Codec Engine", "unspecified",
            G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_CODEC_NAME,
        g_param_spec_string("codecName", "Codec Name", "Name of video codec",
            "unspecified", G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_NUM_OUTPUT_BUFS,
        g_param_spec_int("numOutputBufs",
            "Number of Ouput Buffers",
            "Number of output buffers to allocate for codec",
            2, G_MAXINT32, 3, G_PARAM_WRITABLE));

    g_object_class_install_property(gobject_class, PROP_FRAMERATE,
        gst_param_spec_fraction("framerate", "frame rate of video",
            "Frame rate of the video expressed as a fraction.  A value "
            "of 0/1 indicates the framerate is not specified", 0, 1,
            G_MAXINT, 1, 0, 1, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_DISPLAY_BUFFER,
        g_param_spec_boolean("displayBuffer", "Display Buffer",
            "Display circular buffer status while processing",
            FALSE, G_PARAM_WRITABLE));

    g_object_class_install_property(gobject_class, PROP_RTCODECTHREAD,
        g_param_spec_boolean("RTCodecThread", "Real time codec thread",
            "Exectue codec calls in real-time thread",
            TRUE, G_PARAM_WRITABLE));

    g_object_class_install_property(gobject_class, PROP_GEN_TIMESTAMPS,
        g_param_spec_boolean("genTimeStamps", "Generate Time Stamps",
            "Set timestamps on output buffers",
            TRUE, G_PARAM_WRITABLE));

    g_object_class_install_property(gobject_class, PROP_PAD_ALLOC_OUTBUFS,
        g_param_spec_boolean("padAllocOutbufs", "Use pad allocation",
            "Try to allocate buffers with pad allocation",
            FALSE, G_PARAM_WRITABLE));
}

/******************************************************************************
 * gst_tividdec2_init_env
 *****************************************************************************/
static void gst_tividdec2_init_env(GstTIViddec2 *viddec2)
{
    GST_LOG("gst_tividdec2_init_env - begin\n");

    if (gst_ti_env_is_defined("GST_TI_TIViddec2_engineName")) {
        viddec2->engineName = gst_ti_env_get_string("GST_TI_TIViddec2_engineName");
        GST_LOG("Setting engineName=%s\n", viddec2->engineName);
    }

    if (gst_ti_env_is_defined("GST_TI_TIViddec2_codecName")) {
        viddec2->codecName = gst_ti_env_get_string("GST_TI_TIViddec2_codecName");
        GST_LOG("Setting codecName=%s\n", viddec2->codecName);
    }
    
    if (gst_ti_env_is_defined("GST_TI_TIViddec2_numOutputBufs")) {
        viddec2->numOutputBufs = 
                            gst_ti_env_get_int("GST_TI_TIViddec2_numOutputBufs");
        GST_LOG("Setting numOutputBufs=%ld\n", viddec2->numOutputBufs);
    }

    if (gst_ti_env_is_defined("GST_TI_TIViddec2_displayBuffer")) {
        viddec2->displayBuffer = 
                gst_ti_env_get_boolean("GST_TI_TIViddec2_displayBuffer");
        GST_LOG("Setting displayBuffer=%s\n",
                 viddec2->displayBuffer  ? "TRUE" : "FALSE");
    }
 
    if (gst_ti_env_is_defined("GST_TI_TIViddec2_genTimeStamps")) {
        viddec2->genTimeStamps = 
                gst_ti_env_get_boolean("GST_TI_TIViddec2_genTimeStamps");
        GST_LOG("Setting genTimeStamps =%s\n", 
                    viddec2->genTimeStamps ? "TRUE" : "FALSE");
    }

    if (gst_ti_env_is_defined("GST_TI_TIViddec2_framerateNum") &&
        gst_ti_env_is_defined("GST_TI_TIViddec2_framerateDen")) {
        gst_value_set_fraction(&viddec2->framerate, 
            gst_ti_env_get_int("GST_TI_TIViddec2_framerateNum"),
            gst_ti_env_get_int("GST_TI_TIViddec2_framerateDen"));
        GST_LOG("Setting framerate=%d/%d\n", 
            gst_value_get_fraction_numerator(&viddec2->framerate),
            gst_value_get_fraction_denominator(&viddec2->framerate));
    }
    else if (gst_ti_env_is_defined("GST_TI_TIViddec2_frameRate")) {
        gst_value_set_fraction(&viddec2->framerate, 
            gst_ti_env_get_int("GST_TI_TIViddec2_frameRate"), 1);
        GST_LOG("Setting framerate=%d/%d\n", 
            gst_value_get_fraction_numerator(&viddec2->framerate),
            gst_value_get_fraction_denominator(&viddec2->framerate));
    }

    if (gst_ti_env_is_defined("GST_TI_TIViddec2_RTCodecThread")) {
        viddec2->rtCodecThread = 
                gst_ti_env_get_boolean("GST_TI_TIViddec2_RTCodecThread");
        GST_LOG("Setting RTCodecThread =%s\n", 
                    viddec2->rtCodecThread ? "TRUE" : "FALSE");
    }

    if (gst_ti_env_is_defined("GST_TI_TIViddec2_padAllocOutbufs")) {
        viddec2->padAllocOutbufs = 
                gst_ti_env_get_boolean("GST_TI_TIViddec2_padAllocOutbufs");
        GST_LOG("Setting padAllocOutbufs =%s\n", 
                    viddec2->padAllocOutbufs ? "TRUE" : "FALSE");
    }

    GST_LOG("gst_tividdec2_init_env - end\n");
}

/******************************************************************************
 * gst_tividdec2_init
 *    Initializes a new element instance, instantiates pads and sets the pad
 *    callback functions.
 ******************************************************************************/
static void gst_tividdec2_init(GstTIViddec2 *viddec2, GstTIViddec2Class *gclass)
{
    /* Instantiate encoded video sink pad.
     *
     * Fixate on our static template caps instead of writing a getcaps
     * function, which is overkill for this element.
     */
    viddec2->sinkpad =
        gst_pad_new_from_static_template(&sink_factory, "sink");
    gst_pad_set_setcaps_function(
        viddec2->sinkpad, GST_DEBUG_FUNCPTR(gst_tividdec2_set_sink_caps));
    gst_pad_set_event_function(
        viddec2->sinkpad, GST_DEBUG_FUNCPTR(gst_tividdec2_sink_event));
    gst_pad_set_chain_function(
        viddec2->sinkpad, GST_DEBUG_FUNCPTR(gst_tividdec2_chain));
    gst_pad_fixate_caps(viddec2->sinkpad,
        gst_caps_make_writable(
            gst_caps_copy(gst_pad_get_pad_template_caps(viddec2->sinkpad))));

    /* Instantiate deceoded video source pad.
     *
     * Fixate on our static template caps instead of writing a getcaps
     * function, which is overkill for this element.
     */
    viddec2->srcpad =
        gst_pad_new_from_static_template(&src_factory, "src");
    gst_pad_fixate_caps(viddec2->srcpad,
        gst_caps_make_writable(
            gst_caps_copy(gst_pad_get_pad_template_caps(viddec2->srcpad))));
    gst_pad_set_query_function(viddec2->srcpad,
            GST_DEBUG_FUNCPTR(gst_tividdec2_set_query_pad));

    /* Add pads to TIViddec2 element */
    gst_element_add_pad(GST_ELEMENT(viddec2), viddec2->sinkpad);
    gst_element_add_pad(GST_ELEMENT(viddec2), viddec2->srcpad);

    /* Initialize TIViddec2 state */
    viddec2->engineName         = NULL;
    viddec2->codecName          = NULL;
    viddec2->displayBuffer      = FALSE;
    viddec2->genTimeStamps      = TRUE;

    viddec2->hEngine            = NULL;
    viddec2->hVd                = NULL;
    viddec2->drainingEOS        = FALSE;
    viddec2->threadStatus       = 0UL;
    viddec2->firstFrame         = TRUE;

    viddec2->waitOnDecodeThread = NULL;
    viddec2->waitOnDecodeDrain  = NULL;

    viddec2->numOutputBufs      = 0UL;
    viddec2->hOutBufTab         = NULL;
    viddec2->padAllocOutbufs    = FALSE;
    viddec2->circBuf            = NULL;

    viddec2->sps_pps_data       = NULL;
    viddec2->nal_code_prefix    = NULL;
    viddec2->nal_length         = 0;

    viddec2->segment            = gst_segment_new();
    viddec2->totalDuration      = 0;
    viddec2->totalBytes         = 0;

    viddec2->mpeg4_quicktime_header = NULL;

    viddec2->rtCodecThread      = TRUE;

    viddec2->width              = 0;
    viddec2->height             = 0;

    /* Initialize GValue members */
    memset(&viddec2->framerate, 0, sizeof(GValue));
    g_value_init(&viddec2->framerate, GST_TYPE_FRACTION);
    g_assert(GST_VALUE_HOLDS_FRACTION(&viddec2->framerate));
    gst_value_set_fraction(&viddec2->framerate, 0, 1);

    gst_tividdec2_init_env(viddec2);
}

/******************************************************************************
 * gst_tividdec_set_query_pad
 *   This function reuturn stream duration and position.
 *****************************************************************************/
static gboolean gst_tividdec2_set_query_pad(GstPad *pad, GstQuery *query)
{
    GstTIViddec2  *viddec2;
    gboolean     ret = FALSE;

    viddec2    = GST_TIVIDDEC2(gst_pad_get_parent(pad));
   
    ret = gst_ti_query_srcpad(pad, query, viddec2->sinkpad, 
             viddec2->totalDuration, viddec2->totalBytes);

    gst_object_unref(viddec2);

    return ret;
}


/******************************************************************************
 * gst_tividdec2_set_property
 *     Set element properties when requested.
 ******************************************************************************/
static void gst_tividdec2_set_property(GObject *object, guint prop_id,
                const GValue *value, GParamSpec *pspec)
{
    GstTIViddec2 *viddec2 = GST_TIVIDDEC2(object);

    GST_LOG("begin set_property\n");

    switch (prop_id) {
        case PROP_ENGINE_NAME:
            if (viddec2->engineName) {
                g_free((gpointer)viddec2->engineName);
            }
            viddec2->engineName =
                (gchar*)g_malloc(strlen(g_value_get_string(value)) + 1);
            strcpy((gchar *)viddec2->engineName, g_value_get_string(value));
            GST_LOG("setting \"engineName\" to \"%s\"\n", viddec2->engineName);
            break;
        case PROP_CODEC_NAME:
            if (viddec2->codecName) {
                g_free((gpointer)viddec2->codecName);
            }
            viddec2->codecName =
                (gchar*)g_malloc(strlen(g_value_get_string(value)) + 1);
            strcpy((gchar*)viddec2->codecName, g_value_get_string(value));
            GST_LOG("setting \"codecName\" to \"%s\"\n", viddec2->codecName);
            break;
        case PROP_NUM_OUTPUT_BUFS:
            viddec2->numOutputBufs = g_value_get_int(value);
            GST_LOG("setting \"numOutputBufs\" to \"%ld\"\n",
                viddec2->numOutputBufs);
            break;
        case PROP_FRAMERATE:
        {
            g_value_copy(value, &viddec2->framerate);
            GST_LOG("Setting framerate=%d/%d\n", 
                gst_value_get_fraction_numerator(&viddec2->framerate),
                gst_value_get_fraction_denominator(&viddec2->framerate));
            break;
        }
        case PROP_DISPLAY_BUFFER:
            viddec2->displayBuffer = g_value_get_boolean(value);
            GST_LOG("setting \"displayBuffer\" to \"%s\"\n",
                viddec2->displayBuffer ? "TRUE" : "FALSE");
            break;
        case PROP_GEN_TIMESTAMPS:
            viddec2->genTimeStamps = g_value_get_boolean(value);
            GST_LOG("setting \"genTimeStamps\" to \"%s\"\n",
                viddec2->genTimeStamps ? "TRUE" : "FALSE");
            break;
        case PROP_RTCODECTHREAD:
            viddec2->rtCodecThread = g_value_get_boolean(value);
            GST_LOG("setting \"RTCodecThread\" to \"%s\"\n",
                viddec2->rtCodecThread ? "TRUE" : "FALSE");
            break;
        case PROP_PAD_ALLOC_OUTBUFS:
            viddec2->padAllocOutbufs = g_value_get_boolean(value);
            GST_LOG("setting \"padAllocOutbufs\" to \"%s\"\n",
                viddec2->padAllocOutbufs ? "TRUE" : "FALSE");
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }

    GST_LOG("end set_property\n");
}

/******************************************************************************
 * gst_tividdec2_get_property
 *     Return values for requested element property.
 ******************************************************************************/
static void gst_tividdec2_get_property(GObject *object, guint prop_id,
                GValue *value, GParamSpec *pspec)
{
    GstTIViddec2 *viddec2 = GST_TIVIDDEC2(object);

    GST_LOG("begin get_property\n");

    switch (prop_id) {
        case PROP_ENGINE_NAME:
            g_value_set_string(value, viddec2->engineName);
            break;
        case PROP_CODEC_NAME:
            g_value_set_string(value, viddec2->codecName);
            break;
        case PROP_FRAMERATE:
            g_value_copy(&viddec2->framerate, value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }

    GST_LOG("end get_property\n");
}


/******************************************************************************
 * gst_tividdec2_set_sink_caps
 *     Negotiate our sink pad capabilities.
 ******************************************************************************/
static gboolean gst_tividdec2_set_sink_caps(GstPad *pad, GstCaps *caps)
{
    GstTIViddec2 *viddec2;
    GstStructure *capStruct;
    const gchar  *mime;
    GstTICodec   *codec = NULL;

    viddec2   = GST_TIVIDDEC2(gst_pad_get_parent(pad));
    capStruct = gst_caps_get_structure(caps, 0);
    mime      = gst_structure_get_name(capStruct);

    GST_INFO("requested sink caps:  %s", gst_caps_to_string(caps));

    /* Generic Video Properties */
    if (!strncmp(mime, "video/", 6)) {
        gint  framerateNum;
        gint  framerateDen;

        if (gst_structure_get_fraction(capStruct, "framerate", &framerateNum,
                &framerateDen)) {
            if (gst_value_get_fraction_numerator(&viddec2->framerate) == 0) {
                gst_value_set_fraction(&viddec2->framerate, framerateNum,
                    framerateDen);
            }
        }

        if (viddec2->width == 0 && 
            !gst_structure_get_int(capStruct, "width", &viddec2->width)) {
             viddec2->width = 0;
        }

        if (viddec2->height == 0 && 
            !gst_structure_get_int(capStruct, "height", &viddec2->height)) {
             viddec2->height = 0;
        }
    }

    /* MPEG Decode */
    if (!strcmp(mime, "video/mpeg")) {
        gboolean  systemstream;
        gint      mpegversion;

        /* Retreive video properties */
        if (!gst_structure_get_int(capStruct, "mpegversion", &mpegversion)) {
            mpegversion = 0; 
        }

        if (!gst_structure_get_boolean(capStruct, "systemstream",
                 &systemstream)) {
            systemstream = FALSE;
        }

        /* Determine the correct decoder to use */
        if (systemstream) {
            gst_object_unref(viddec2);
            return FALSE;
        }

        if (mpegversion == 2) {
            codec = gst_ticodec_get_codec("MPEG2 Video Decoder");
        }
        else if (mpegversion == 4) {
            codec = gst_ticodec_get_codec("MPEG4 Video Decoder");
        }
        else {
            gst_object_unref(viddec2);
            return FALSE;
        }
    }

    /* H.264 Decode */
    else if (!strcmp(mime, "video/x-h264")) {
        codec = gst_ticodec_get_codec("H.264 Video Decoder");
    }

    /* MPEG-4 ASP Decode (DivX / XviD) */
    else if (!strcmp(mime, "video/x-divx") || !strcmp(mime, "video/x-xvid")) {
        codec = gst_ticodec_get_codec("MPEG4 Video Decoder");
    }

    /* Mime type not supported */
    else {
        GST_ELEMENT_ERROR(viddec2, STREAM, NOT_IMPLEMENTED,
        ("stream type not supported"), (NULL));
        gst_object_unref(viddec2);
        return FALSE;
    }

    /* Report if the required codec was not found */
    if (!codec) {
        GST_ELEMENT_ERROR(viddec2, STREAM, CODEC_NOT_FOUND,
        ("unable to find codec needed for stream"), (NULL));
        gst_object_unref(viddec2);
        return FALSE;
    }

    /* Shut down any running video decoder */
    if (!gst_tividdec2_exit_video(viddec2)) {
        gst_object_unref(viddec2);
        return FALSE;
    }

    /* Configure the element to use the detected engine name and codec, unless
     * they have been using the set_property function.
     */
    if (!viddec2->engineName) {
        viddec2->engineName = codec->CE_EngineName;
    }

    if (!viddec2->codecName) {
        viddec2->codecName = codec->CE_CodecName;
    }
    
    gst_object_unref(viddec2);

    GST_LOG("sink caps negotiation successful\n");
    return TRUE;
}


/******************************************************************************
 * gst_tividdec2_set_source_caps
 *     Negotiate our source pad capabilities.
 ******************************************************************************/
static gboolean gst_tividdec2_set_source_caps(
                    GstTIViddec2 *viddec2, Buffer_Handle hBuf)
{
    BufferGfx_Dimensions  dim;
    BufferGfx_Attrs       gfxAttrs = BufferGfx_Attrs_DEFAULT;

    /* Create a caps object using the dimensions from the given buffer */
    BufferGfx_getDimensions(hBuf, &dim);

    /* Retrieve the graphics attribute so we know the colorspace */
    Buffer_getAttrs(hBuf, BufferGfx_getBufferAttrs(&gfxAttrs));

    /* Call the base function with the buffer metadata */
    return gst_tividdec2_set_source_caps_base(viddec2, dim.width, dim.height,
        gfxAttrs.colorSpace);
}


/******************************************************************************
 * gst_tividdec2_set_source_caps_base
 *     Negotiate our source pad capabilities.
 ******************************************************************************/
static gboolean gst_tividdec2_set_source_caps_base(
                    GstTIViddec2 *viddec2, Int32 width, Int32 height,
                    ColorSpace_Type colorSpace)
{
    GstCaps              *caps;
    GValue                fourcc = {0};
    gboolean              ret;
    GstPad               *pad;
    char                 *string;

    pad = viddec2->srcpad;

    /* Determine the fourcc from the colorspace */
    g_value_init(&fourcc, GST_TYPE_FOURCC);
    g_assert(GST_VALUE_HOLDS_FOURCC(&fourcc));

    switch (colorSpace) {
        case ColorSpace_UYVY:
            gst_value_set_fourcc(&fourcc, GST_MAKE_FOURCC('U','Y','V','Y'));
            break;
        case ColorSpace_YUV420PSEMI:
            gst_value_set_fourcc(&fourcc, GST_MAKE_FOURCC('N','V','1','2'));
            break;
        case ColorSpace_YUV422PSEMI:
            gst_value_set_fourcc(&fourcc, GST_MAKE_FOURCC('N','V','1','6'));
            break;
        case ColorSpace_YUV420P:
            gst_value_set_fourcc(&fourcc, GST_MAKE_FOURCC('I','4','2','0'));
            break;
        default:
            GST_ERROR("unsupported colorspace\n");
            return FALSE;
    }

    /* Create caps structure */
    caps =
        gst_caps_new_simple("video/x-raw-yuv",
            "format",    GST_TYPE_FOURCC,   gst_value_get_fourcc(&fourcc),
            "framerate", GST_TYPE_FRACTION,
                gst_value_get_fraction_numerator(&viddec2->framerate),
                gst_value_get_fraction_denominator(&viddec2->framerate),
            "width",     G_TYPE_INT,        width,
            "height",    G_TYPE_INT,        height,
            NULL);

    /* Set the source pad caps */
    string = gst_caps_to_string(caps);
    GST_LOG("setting source caps to: %s", string);
    g_free(string);
    ret = gst_pad_set_caps(pad, caps);
    gst_caps_unref(caps);

    return ret;
}


/******************************************************************************
 * gst_tividdec2_sink_event
 *     Perform event processing on the input stream.  At the moment, this
 *     function isn't needed as this element doesn't currently perform any
 *     specialized event processing.  We'll leave it in for now in case we need
 *     it later on.
 ******************************************************************************/
static gboolean gst_tividdec2_sink_event(GstPad *pad, GstEvent *event)
{
    GstTIViddec2 *viddec2;
    gboolean      ret;

    viddec2 = GST_TIVIDDEC2(GST_OBJECT_PARENT(pad));

    GST_DEBUG("pad \"%s\" received:  %s\n", GST_PAD_NAME(pad),
        GST_EVENT_TYPE_NAME(event));

    switch (GST_EVENT_TYPE(event)) {

        case GST_EVENT_NEWSEGMENT:
            /* if event format is byte then convert in time format */
            gst_ti_parse_newsegment(&event, viddec2->segment, 
                &viddec2->totalDuration, viddec2->totalBytes);

            /* Propagate NEWSEGMENT to downstream elements */
            ret = gst_pad_push_event(viddec2->srcpad, event);
            break;

        case GST_EVENT_EOS:
            /* end-of-stream: process any remaining encoded frame data */
            GST_LOG("no more input; draining remaining encoded video data\n");

            if (!viddec2->drainingEOS) {
               gst_tividdec2_drain_pipeline(viddec2);
             }

            /* Propagate EOS to downstream elements */
            ret = gst_pad_push_event(viddec2->srcpad, event);
            break;

        case GST_EVENT_FLUSH_STOP:
            ret = gst_pad_push_event(viddec2->srcpad, event);
            break;

        /* Unhandled events */
        case GST_EVENT_BUFFERSIZE:
        case GST_EVENT_CUSTOM_BOTH:
        case GST_EVENT_CUSTOM_BOTH_OOB:
        case GST_EVENT_CUSTOM_DOWNSTREAM:
        case GST_EVENT_CUSTOM_DOWNSTREAM_OOB:
        case GST_EVENT_CUSTOM_UPSTREAM:
        case GST_EVENT_FLUSH_START:
        case GST_EVENT_NAVIGATION:
        case GST_EVENT_QOS:
        case GST_EVENT_SEEK:
        case GST_EVENT_TAG:
        default:
            ret = gst_pad_event_default(pad, event);
            break;

    }

    return ret;

}

/******************************************************************************
 * gst_tividdec2_populate_codec_header
 *  This function parses codec_data field to get addition H.264/MPEG-4 header 
 *  information.
 *****************************************************************************/
static gboolean gst_tividdec2_populate_codec_header (GstTIViddec2 *viddec2, 
    GstBuffer *buf)
{
    GstCaps      *caps = GST_BUFFER_CAPS(buf);

    if (!caps) {
        return FALSE;
    }

    /* If it's H.264 codec then get the sps, pps and nal headers */
    if (gst_is_h264_decoder(viddec2->codecName) && 
            gst_h264_valid_quicktime_header(buf)) {
        GST_LOG("Parsing codec data to get SPS, PPS and NAL headers");
        viddec2->nal_length = gst_h264_get_nal_length(buf);
        viddec2->sps_pps_data = gst_h264_get_sps_pps_data(buf);
        viddec2->nal_code_prefix = gst_h264_get_nal_prefix_code();
    }

    if (gst_is_mpeg4_decoder(viddec2->codecName) && 
            gst_mpeg4_valid_quicktime_header(buf)) {
        GST_LOG("found MPEG4 quicktime header \n");
        viddec2->mpeg4_quicktime_header = gst_mpeg4_get_header(buf);
    }

    return TRUE;
}

/******************************************************************************
 * gst_tividdec2_parse_and_queue_buffer
 *  If needed then this function will parse the input buffer before putting
 *  in circular buffer.
 *****************************************************************************/
static gboolean gst_tividdec2_parse_and_queue_buffer(GstTIViddec2 *viddec2,
    GstBuffer *buf)
{
    if (viddec2->sps_pps_data) {
        /* If demuxer has passed SPS and PPS NAL unit dump in codec_data field,
         * then we have a packetized h264 stream. We need to transform this 
         * stream into byte-stream.
         */
        if (gst_h264_parse_and_queue(viddec2->circBuf, buf, 
                viddec2->sps_pps_data, viddec2->nal_code_prefix,
                viddec2->nal_length) < 0) {
            GST_ELEMENT_ERROR(viddec2, RESOURCE, WRITE,
            ("Failed to queue input buffer into circular buffer\n"), (NULL));
            return FALSE;
        }
    }
    else if (viddec2->mpeg4_quicktime_header) {
        /* If demuxer has passed codec_data field then we need to prefix this
         * codec data in input stream.
         */
        if (gst_mpeg4_parse_and_queue(viddec2->circBuf, buf, 
                viddec2->mpeg4_quicktime_header) < 0) {
            GST_ELEMENT_ERROR(viddec2, RESOURCE, WRITE,
            ("Failed to queue input buffer into circular buffer\n"), (NULL));
            return FALSE;
        }
    }
    else {
        /* Queue up the encoded data stream into a circular buffer */
        if (!gst_ticircbuffer_queue_data(viddec2->circBuf, buf)) {
            GST_ELEMENT_ERROR(viddec2, RESOURCE, WRITE,
            ("Failed to queue input buffer into circular buffer\n"), (NULL));
            return FALSE;
        }
    }

    return TRUE;
}

/******************************************************************************
 * gst_tividdec2_chain
 *    This is the main processing routine.  This function receives a buffer
 *    from the sink pad, processes it, and pushes the result to the source
 *    pad.
 ******************************************************************************/
static GstFlowReturn gst_tividdec2_chain(GstPad * pad, GstBuffer * buf)
{
    GstTIViddec2  *viddec2 = GST_TIVIDDEC2(GST_OBJECT_PARENT(pad));
    GstFlowReturn  flow    = GST_FLOW_OK;
    gboolean       checkResult;


    /* If the decode thread aborted, signal it to let it know it's ok to
     * shut down, and communicate the failure to the pipeline.
     */
    if (gst_tithread_check_status(viddec2, TIThread_CODEC_ABORTED,
            checkResult)) {
        flow = GST_FLOW_UNEXPECTED;
        goto exit;
    }

    /* If our engine handle is currently NULL, then either this is our first
     * buffer or the upstream element has re-negotiated our capabilities which
     * resulted in our engine being closed.  In either case, we need to
     * initialize (or re-initialize) our video decoder to handle the new
     * stream.
     */
    if (viddec2->hEngine == NULL) {
        if (!gst_tividdec2_init_video(viddec2)) {
            GST_ELEMENT_ERROR(viddec2, RESOURCE, FAILED,
            ("unable to initialize video\n"), (NULL));
            flow = GST_FLOW_UNEXPECTED;
            goto exit;
        }

        /* Populate extra codec headers */
        if (gst_tividdec2_populate_codec_header(viddec2, buf)) {
            GST_LOG("Found extra header information for %s",viddec2->codecName);
        }

        GST_TICIRCBUFFER_TIMESTAMP(viddec2->circBuf) =
            GST_CLOCK_TIME_IS_VALID(GST_BUFFER_TIMESTAMP(buf)) ?
            GST_BUFFER_TIMESTAMP(buf) : 0ULL;
    }

    /* Parse and queue the encoded data stream into a circular buffer */
    if (!gst_tividdec2_parse_and_queue_buffer(viddec2, buf)) {
        GST_ELEMENT_ERROR(viddec2, RESOURCE, WRITE,
        ("Failed to queue input buffer into circular buffer\n"), (NULL));
        flow = GST_FLOW_UNEXPECTED;
        goto exit;
    }

exit:
    gst_buffer_unref(buf);
    return flow;
}


/******************************************************************************
 * gst_tividdec2_init_video
 *     Initialize or re-initializes the video stream
 ******************************************************************************/
static gboolean gst_tividdec2_init_video(GstTIViddec2 *viddec2)
{
    Rendezvous_Attrs    rzvAttrs = Rendezvous_Attrs_DEFAULT;
    struct sched_param  schedParam;
    pthread_attr_t      attr;

    GST_LOG("begin init_video\n");

    /* If video has already been initialized, shut down previous decoder */
    if (viddec2->hEngine) {
        if (!gst_tividdec2_exit_video(viddec2)) {
            GST_ERROR("failed to shut down existing video decoder\n");
            return FALSE;
        }
    }

    /* Make sure we know what codec we're using */
    if (!viddec2->engineName) {
        GST_ELEMENT_ERROR(viddec2, RESOURCE, FAILED,
        ("engine name not specified\n"), (NULL));
        return FALSE;
    }

    if (!viddec2->codecName) {
        GST_ELEMENT_ERROR(viddec2, RESOURCE, FAILED,
        ("codec name not specified\n"), (NULL));
        return FALSE;
    }

    /* Initialize thread status management */
    viddec2->threadStatus = 0UL;
    pthread_mutex_init(&viddec2->threadStatusMutex, NULL);

    /* Initialize rendezvous objects for making threads wait on conditions */
    viddec2->waitOnDecodeThread = Rendezvous_create(2, &rzvAttrs);
    viddec2->waitOnDecodeDrain  = Rendezvous_create(100, &rzvAttrs);
    viddec2->drainingEOS        = FALSE;

    /* Initialize custom thread attributes */
    if (pthread_attr_init(&attr)) {
        GST_WARNING("failed to initialize thread attrs\n");
        gst_tividdec2_exit_video(viddec2);
        return FALSE;
    }

    /* Force the thread to use the system scope */
    if (pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM)) {
        GST_WARNING("failed to set scope attribute\n");
        gst_tividdec2_exit_video(viddec2);
        return FALSE;
    }

    /* Force the thread to use custom scheduling attributes */
    if (pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED)) {
        GST_WARNING("failed to set schedule inheritance attribute\n");
        gst_tividdec2_exit_video(viddec2);
        return FALSE;
    }

    /* Set the thread to be fifo real time scheduled */
    if (pthread_attr_setschedpolicy(&attr, SCHED_FIFO)) {
        GST_WARNING("failed to set FIFO scheduling policy\n");
        gst_tividdec2_exit_video(viddec2);
        return FALSE;
    }

    /* Set the display thread priority */
    schedParam.sched_priority = GstTIVideoThreadPriority;
    if (pthread_attr_setschedparam(&attr, &schedParam)) {
        GST_WARNING("failed to set scheduler parameters\n");
        return FALSE;
    }

    /* Create decoder thread */
    if (pthread_create(&viddec2->decodeThread, viddec2->rtCodecThread ? 
            &attr : NULL, gst_tividdec2_decode_thread, (void*)viddec2)) {
        GST_ELEMENT_ERROR(viddec2, RESOURCE, FAILED,
        ("failed to create decode thread\n"), (NULL));
        gst_tividdec2_exit_video(viddec2);
        return FALSE;
    }
    gst_tithread_set_status(viddec2, TIThread_CODEC_CREATED);

    /* Destroy the custom thread attributes */
    if (pthread_attr_destroy(&attr)) {
        GST_WARNING("failed to destroy thread attrs\n");
        gst_tividdec2_exit_video(viddec2);
        return FALSE;
    }

    /* Make sure circular buffer and display buffer handles are created by 
     * decoder thread.
     */
    Rendezvous_meet(viddec2->waitOnDecodeThread);

    if (viddec2->circBuf == NULL) {
        GST_ELEMENT_ERROR(viddec2, RESOURCE, FAILED,
        ("decode thread failed to create circbuf handles\n"),
        (NULL));
        return FALSE;
    }

    GST_LOG("end init_video\n");
    return TRUE;
}


/******************************************************************************
 * gst_tividdec2_exit_video
 *    Shut down any running video decoder, and reset the element state.
 ******************************************************************************/
static gboolean gst_tividdec2_exit_video(GstTIViddec2 *viddec2)
{
    void*    thread_ret;
    gboolean checkResult;

    GST_LOG("begin exit_video\n");

    /* Drain the pipeline if it hasn't already been drained */
    if (!viddec2->drainingEOS) {
       gst_tividdec2_drain_pipeline(viddec2);
     }

    /* Shut down the decode thread */
    if (gst_tithread_check_status(
            viddec2, TIThread_CODEC_CREATED, checkResult)) {
        GST_LOG("shutting down decode thread\n");

        Rendezvous_force(viddec2->waitOnDecodeThread);
        if (pthread_join(viddec2->decodeThread, &thread_ret) == 0) {
            if (thread_ret == GstTIThreadFailure) {
                GST_DEBUG("decode thread exited with an error condition\n");
            }
        }
    }

    /* Shut down thread status management */
    viddec2->threadStatus = 0UL;
    pthread_mutex_destroy(&viddec2->threadStatusMutex);

    /* Shut down any remaining items */
    if (viddec2->waitOnDecodeDrain) {
        Rendezvous_delete(viddec2->waitOnDecodeDrain);
        viddec2->waitOnDecodeDrain = NULL;
    }

    if (viddec2->waitOnDecodeThread) {
        Rendezvous_delete(viddec2->waitOnDecodeThread);
        viddec2->waitOnDecodeThread = NULL;
    }

    if (viddec2->sps_pps_data) {
        GST_LOG("freeing sps_pps buffers\n");
        gst_buffer_unref(viddec2->sps_pps_data);
        viddec2->sps_pps_data = NULL;
    }

    if (viddec2->nal_code_prefix) {
        GST_LOG("freeing nal code prefix buffers\n");
        gst_buffer_unref(viddec2->nal_code_prefix);
        viddec2->nal_code_prefix = NULL;
    }

    if (viddec2->nal_length) {
        GST_LOG("reseting nal length to zero\n");
        viddec2->nal_length = 0;
    }

    if (viddec2->mpeg4_quicktime_header) {
        GST_LOG("reseting quicktime mpeg4 header to NULL\n");
        viddec2->mpeg4_quicktime_header = NULL;
    }

    GST_LOG("end exit_video\n");
    return TRUE;
}


/******************************************************************************
 * gst_tividdec2_change_state
 *     Manage state changes for the video stream.  The gStreamer documentation
 *     states that state changes must be handled in this manner:
 *        1) Handle ramp-up states
 *        2) Pass state change to base class
 *        3) Handle ramp-down states
 ******************************************************************************/
static GstStateChangeReturn gst_tividdec2_change_state(GstElement *element,
                                GstStateChange transition)
{
    GstStateChangeReturn  ret    = GST_STATE_CHANGE_SUCCESS;
    GstTIViddec2          *viddec2 = GST_TIVIDDEC2(element);

    GST_LOG("begin change_state (%d)\n", transition);

    /* Handle ramp-up state changes */
    switch (transition) {
        case GST_STATE_CHANGE_NULL_TO_READY:
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
        case GST_STATE_CHANGE_READY_TO_NULL:
            /* Shut down any running video decoder */
            if (!gst_tividdec2_exit_video(viddec2)) {
                return GST_STATE_CHANGE_FAILURE;
            }
            break;

        case GST_STATE_CHANGE_READY_TO_PAUSED:
            gst_segment_init(viddec2->segment, GST_FORMAT_TIME);
            break;

        case GST_STATE_CHANGE_PAUSED_TO_READY:
            viddec2->totalBytes       = 0;
            viddec2->totalDuration    = 0;
            break;

        default:
            break;
    }

    GST_LOG("end change_state\n");
    return ret;
}

/******************************************************************************
 * gst_tividdec2_codec_stop
 *     free codec engine resources
 *****************************************************************************/
static gboolean gst_tividdec2_codec_stop (GstTIViddec2  *viddec2)
{
    if (viddec2->circBuf) {
        GstTICircBuffer *circBuf;

        GST_LOG("freeing cicrular input buffer\n");

        circBuf               = viddec2->circBuf;
        viddec2->circBuf      = NULL;
        gst_value_set_fraction(&viddec2->framerate, 0, 1);
        gst_ticircbuffer_unref(circBuf);
    }

    if (viddec2->hOutBufTab) {
        GST_LOG("freeing output buffers\n");
        gst_tidmaibuftab_unref(viddec2->hOutBufTab);
        viddec2->hOutBufTab = NULL;
    }

    /* Shut down remaining items */
    if (viddec2->hVd) {
        GST_LOG("closing video decoder\n");
        Vdec2_delete(viddec2->hVd);
        viddec2->hVd = NULL;
    }

    if (viddec2->hEngine) {
        GST_LOG("closing codec engine\n");
        Engine_close(viddec2->hEngine);
        viddec2->hEngine = NULL;
    }

    return TRUE;
}

/******************************************************************************
 * gst_tividdec2_codec_start
 *     Initialize codec engine
 *****************************************************************************/
static gboolean gst_tividdec2_codec_start (GstTIViddec2  *viddec2,
           GstBuffer **padBuffer)
{
    VIDDEC2_Params         params      = Vdec2_Params_DEFAULT;
    VIDDEC2_DynamicParams  dynParams   = Vdec2_DynamicParams_DEFAULT;
    BufferGfx_Attrs        gfxAttrs    = BufferGfx_Attrs_DEFAULT;
    BufTab_Handle          codecBufTab = NULL;
    Cpu_Device             device;
    ColorSpace_Type        colorSpace;
    Int                    defaultNumBufs;

    /* Open the codec engine */
    GST_LOG("opening codec engine \"%s\"\n", viddec2->engineName);
    viddec2->hEngine = Engine_open((Char *) viddec2->engineName, NULL, NULL);

    if (viddec2->hEngine == NULL) {
        GST_ELEMENT_ERROR(viddec2, RESOURCE, FAILED,
        ("failed to open codec engine \"%s\"\n", viddec2->engineName), (NULL));
        return FALSE;
    }

    /* Determine which device the application is running on */
    if (Cpu_getDevice(NULL, &device) < 0) {
        GST_ELEMENT_ERROR(viddec2, RESOURCE, FAILED,
        ("Failed to determine target board\n"), (NULL));
        return FALSE;
    }

    /* Set up codec parameters depending on device */
    switch(device) {
        case Cpu_Device_DM6467:
            #if defined(Platform_dm6467t)
            params.forceChromaFormat = XDM_YUV_420SP;
            params.maxFrameRate      = 60000;
            params.maxBitRate        = 30000000;
            #else
            params.forceChromaFormat = XDM_YUV_420P;
            #endif
            params.maxWidth          = VideoStd_1080I_WIDTH;
            params.maxHeight         = VideoStd_1080I_HEIGHT + 8;
            colorSpace               = ColorSpace_YUV420PSEMI;
            defaultNumBufs           = 5;
            break;
        #if defined(Platform_dm365)
        case Cpu_Device_DM365:
            params.forceChromaFormat = XDM_YUV_420SP;
            params.maxWidth          = VideoStd_720P_WIDTH;
            params.maxHeight         = VideoStd_720P_HEIGHT;
            colorSpace               = ColorSpace_YUV420PSEMI;
            defaultNumBufs           = 4;
            break;
        #endif
        #if defined(Platform_dm368)
        case Cpu_Device_DM368:
            params.forceChromaFormat = XDM_YUV_420SP;
            params.maxWidth          = VideoStd_720P_WIDTH;
            params.maxHeight         = VideoStd_720P_HEIGHT;
            colorSpace               = ColorSpace_YUV420PSEMI;
            defaultNumBufs           = 4;
            break;
        #endif
        #if defined(Platform_omapl138)
        case Cpu_Device_OMAPL138:
            params.forceChromaFormat = XDM_YUV_420P;
            params.maxWidth          = VideoStd_D1_WIDTH;
            params.maxHeight         = VideoStd_D1_PAL_HEIGHT;
            colorSpace               = ColorSpace_YUV420P;
            defaultNumBufs           = 3;
            break;
        #endif
        #if defined(Platform_dm3730)
        case Cpu_Device_DM3730:
            params.maxWidth          = VideoStd_720P_WIDTH;
            params.maxHeight         = VideoStd_720P_HEIGHT;
            params.forceChromaFormat = XDM_YUV_422ILE;
            colorSpace               = ColorSpace_UYVY;
            defaultNumBufs           = 3;
            break;
        #endif
        default:
            params.forceChromaFormat = XDM_YUV_422ILE;
            params.maxWidth          = VideoStd_D1_WIDTH;
            params.maxHeight         = VideoStd_D1_PAL_HEIGHT;
            colorSpace               = ColorSpace_UYVY;
            defaultNumBufs           = 3;
            break;
    }

    /* If height and width is passed then configure codec params with this information */
    if (viddec2->width > 0 && viddec2->height > 0) {
        params.maxWidth = viddec2->width;
        params.maxHeight = viddec2->height;
    }

    GST_LOG("opening video decoder \"%s\"\n", viddec2->codecName);
    viddec2->hVd = Vdec2_create(viddec2->hEngine, (Char*)viddec2->codecName,
                      &params, &dynParams);

    if (viddec2->hVd == NULL) {
        GST_ELEMENT_ERROR(viddec2, STREAM, CODEC_NOT_FOUND,
        ("failed to create video decoder: %s\n", viddec2->codecName), (NULL));
        GST_LOG("closing codec engine\n");
        return FALSE;
    }

    /* Record that we haven't processed the first frame yet */
    viddec2->firstFrame = TRUE;

    /* Create a circular input buffer */
    viddec2->circBuf =
        gst_ticircbuffer_new(Vdec2_getInBufSize(viddec2->hVd), 3, FALSE);

    if (viddec2->circBuf == NULL) {
        GST_ELEMENT_ERROR(viddec2, RESOURCE, NO_SPACE_LEFT,
        ("failed to create circular input buffer\n"), (NULL));
        return FALSE;
    }

    /* Display buffer contents if displayBuffer=TRUE was specified */
    gst_ticircbuffer_set_display(viddec2->circBuf, viddec2->displayBuffer);

    /* Define the number of display buffers to allocate.  This number must be
     * at least 2, but should be more if codecs don't return a display buffer
     * after every process call.  If this has not been set via set_property(),
     * default to the value set above based on device type.
     */
    if (viddec2->numOutputBufs == 0) {
        viddec2->numOutputBufs = defaultNumBufs;
    }

    /* Try to allocate a buffer from downstream.  To do this, we must first
     * set the framerate to a reasonable default if one hasn't been specified,
     * and we need to set the source pad caps with the stream information we
     * have so far.
     */
    gst_tividdec2_frame_duration(viddec2);
    gst_tividdec2_set_source_caps_base(viddec2, params.maxWidth,
        params.maxHeight, colorSpace);

    *padBuffer = NULL;
    if (viddec2->padAllocOutbufs) {
        if (gst_pad_alloc_buffer(viddec2->srcpad, 0,
            Vdec2_getOutBufSize(viddec2->hVd), GST_PAD_CAPS(viddec2->srcpad),
            padBuffer) != GST_FLOW_OK) {
            GST_LOG("failed to allocate a downstream buffer\n");
            *padBuffer = NULL;
        }

        if (*padBuffer && !GST_IS_TIDMAIBUFFERTRANSPORT(*padBuffer)) {
            GST_LOG("downstream buffer is not a DMAI buffer; disabling use of "
                "pad-allocated buffers\n");
            gst_buffer_unref(*padBuffer);
            *padBuffer = NULL;
        }

        if (*padBuffer) {
            codecBufTab = Buffer_getBufTab(
                GST_TIDMAIBUFFERTRANSPORT_DMAIBUF(*padBuffer));

            if (!codecBufTab) {
                GST_LOG("downstream buffer is not a BufTab member; disabling "
                    "use of pad-allocated buffers\n");
                gst_buffer_unref(*padBuffer);
                *padBuffer = NULL;
            }
        }
    }

    /* If we can't use pad-allocated buffers, allocate our own BufTab for
     * output buffers to push downstream.
     */
    if (!(*padBuffer)) {

        GST_LOG("creating output buffer table\n");
        gfxAttrs.colorSpace     = colorSpace;
        gfxAttrs.dim.width      = params.maxWidth;
        gfxAttrs.dim.height     = params.maxHeight;
        gfxAttrs.dim.lineLength = BufferGfx_calcLineLength(
                                      gfxAttrs.dim.width, gfxAttrs.colorSpace);

        /* By default, new buffers are marked as in-use by the codec */
        gfxAttrs.bAttrs.useMask = gst_tidmaibuffer_CODEC_FREE;

        viddec2->hOutBufTab = gst_tidmaibuftab_new(
            viddec2->numOutputBufs, Vdec2_getOutBufSize(viddec2->hVd),
            BufferGfx_getBufferAttrs(&gfxAttrs));

        codecBufTab = GST_TIDMAIBUFTAB_BUFTAB(viddec2->hOutBufTab);
    }

    /* The value of codecBufTab should now either point to a downstream
     * BufTab or our own BufTab.
     */
    if (codecBufTab == NULL) {
        GST_ELEMENT_ERROR(viddec2, RESOURCE, NO_SPACE_LEFT,
            ("no BufTab available for codec output\n"), (NULL));
        return FALSE;
    }

    /* Tell the Vdec module what BufTab it will be using for its output */
    Vdec2_setBufTab(viddec2->hVd, codecBufTab);

    return TRUE;
}

/******************************************************************************
 * gst_tividdec2_decode_thread
 *     Call the video codec to process a full input buffer
 *     NOTE: some dsplink API's (e.g RingIO) does not support mult-threading 
 *     because of this limitation we need to execute all codec API's in single 
 *     thread. 
 ******************************************************************************/
static void* gst_tividdec2_decode_thread(void *arg)
{
    GstTIViddec2  *viddec2        = GST_TIVIDDEC2(gst_object_ref(arg));
    GstBuffer     *encDataWindow  = NULL;
    GstBuffer     *padBuffer      = NULL;
    Buffer_Attrs   bAttrs         = Buffer_Attrs_DEFAULT;
    gboolean       codecFlushed   = FALSE;
    gboolean       usePadBufs     = FALSE;
    void          *threadRet      = GstTIThreadSuccess;
    Buffer_Handle  hDummyInputBuf = NULL;
    Buffer_Handle  hDstBuf;
    Buffer_Handle  hFreeBuf;
    Int32          encDataConsumed;
    GstClockTime   encDataTime;
    GstClockTime   frameDuration;
    Buffer_Handle  hEncDataWindow;
    GstBuffer     *outBuf;
    Int            bufIdx;
    Int            ret, codecRet;

    GST_LOG("init video decode_thread \n");

    /* Initialize codec engine */
    ret = gst_tividdec2_codec_start(viddec2, &padBuffer);
    usePadBufs = (padBuffer != NULL);

    /* Notify main thread that is ok to continue initialization */
    Rendezvous_meet(viddec2->waitOnDecodeThread);
    Rendezvous_reset(viddec2->waitOnDecodeThread);

    if (ret == FALSE) {
        GST_ELEMENT_ERROR(viddec2, RESOURCE, FAILED, 
        ("failed to start codec\n"), (NULL));
        goto thread_exit;
    }

    /* Calculate the duration of a single frame in this stream */
    frameDuration = gst_tividdec2_frame_duration(viddec2);

    /* Main thread loop */
    while (TRUE) {

        /* Obtain an encoded data frame */
        encDataWindow  = gst_ticircbuffer_get_data(viddec2->circBuf);
        encDataTime    = GST_BUFFER_TIMESTAMP(encDataWindow);
        hEncDataWindow = GST_TIDMAIBUFFERTRANSPORT_DMAIBUF(encDataWindow);

        /* If we received a data frame of zero size, there is no more data to
         * process -- exit the thread.  If we weren't told that we are
         * draining the pipeline, something is not right, so exit with an
         * error.
         */
        if (GST_BUFFER_SIZE(encDataWindow) == 0) {
            GST_LOG("no video data remains\n");
            if (!viddec2->drainingEOS) {
                goto thread_failure;
            }

            /* When no input remains, we must flush any remaining display
             * frames out of the codec and push them to the sink.
             */
            if (!codecFlushed) {
                Vdec2_flush(viddec2->hVd);

                /* Create a dummy input dummy buffer for the process call.
                 * After a flush the codec ignores the input buffer, but since
                 * Codec Engine still address translates the buffer, it needs
                 * to exist.
                 */
                hEncDataWindow = hDummyInputBuf = Buffer_create(1, &bAttrs);
                Buffer_setNumBytesUsed(hDummyInputBuf, 1);
                codecFlushed   = TRUE;
            }
            else {
                Buffer_delete(hDummyInputBuf);
                goto thread_exit;
            }
        }

        /* Obtain a free output buffer for the decoded data */
        if (usePadBufs) {

            /* First time through this loop, padBuffer will already be set
             * to the buffer we got in codec_start.  It will be NULL for every
             * frame after that.
             */
            if (G_LIKELY(!padBuffer)) {
                if (gst_pad_alloc_buffer(viddec2->srcpad, 0, 0,
                        GST_PAD_CAPS(viddec2->srcpad), &padBuffer)
                        != GST_FLOW_OK) {
                    GST_ELEMENT_ERROR(viddec2, RESOURCE, READ,
                        ("failed to allocate a downstream buffer\n"), (NULL));
                    padBuffer = NULL;
                    goto thread_exit;
                }
            }
            hDstBuf = GST_TIDMAIBUFFERTRANSPORT_DMAIBUF(padBuffer);
            gst_buffer_unref(padBuffer);
            padBuffer = NULL;

            /* Set the CODEC_FREE flag -- this isn't done automatically when
             * allocating buffers from downstream.
             */
            Buffer_setUseMask(hDstBuf, Buffer_getUseMask(hDstBuf) |
                gst_tidmaibuffer_CODEC_FREE);

        }
        else if (!(hDstBuf = gst_tidmaibuftab_get_buf(viddec2->hOutBufTab))) {
            GST_ELEMENT_ERROR(viddec2, RESOURCE, READ,
                ("failed to get a free contiguous buffer from BufTab\n"), 
                (NULL));
            goto thread_exit;
        }

        /* Make sure the whole buffer is used for output */
        BufferGfx_resetDimensions(hDstBuf);

        /* Invoke the video decoder */
        GST_LOG("invoking the video decoder\n");
        codecRet        = Vdec2_process(viddec2->hVd, hEncDataWindow, hDstBuf);
        encDataConsumed = (codecFlushed) ? 0 :
                          Buffer_getNumBytesUsed(hEncDataWindow);

        if (codecRet < 0) {
            if (encDataConsumed <= 0) {
                encDataConsumed = 1;
            }

            BufTab_freeBuf(hDstBuf);
            GST_ERROR("failed to decode video buffer\n");
        }

        if (codecRet > 0) {
            GST_LOG("Vdec2_process returned success code %d\n", codecRet); 

            /* In the case of bit errors, the codec may not return the buffer
             * via the Vdec2_getFreeBuf API, so mark it as unused now.
             */
            if (codecRet == Dmai_EBITERROR) {
                BufTab_freeBuf(hDstBuf);

                /* If no encoded data was used we cannot find the next frame */
                if (encDataConsumed == 0 && !codecFlushed) {
                    GST_ELEMENT_ERROR(viddec2, STREAM, DECODE,
                    ("fatal bit error\n"), (NULL));
                    goto thread_failure;
                }
            }
        }

        /* Increment total bytes recieved */
        viddec2->totalBytes += encDataConsumed;

        /* Release the reference buffer, and tell the circular buffer how much
         * data was consumed.
         */
        ret = gst_ticircbuffer_data_consumed(viddec2->circBuf, encDataWindow,
                  encDataConsumed);
        encDataWindow = NULL;

        if (!ret) {
            goto thread_failure;
        }

        /* In case of codec failure or bit error, continue to wait for more 
         * data 
         */
        if ((codecRet < 0) || (codecRet == Dmai_EBITERROR)) {
            continue;
        }

        /* Resize the BufTab after the first frame has been processed.  The
         * codec may not know it's buffer requirements before the first frame
         * has been decoded.
         */
        if (viddec2->firstFrame) {
            if (!gst_tividdec2_resizeBufTab(viddec2)) {
                GST_ELEMENT_ERROR(viddec2, RESOURCE, READ,
                ("failed to re-partition decode buffers after processing"
                 "first frame\n"), (NULL));
                goto thread_failure;
            }
            viddec2->firstFrame = FALSE;
        }

        /* Obtain the display buffer returned by the codec (it may be a
         * different one than the one we passed it.
         */
        hDstBuf = Vdec2_getDisplayBuf(viddec2->hVd);

        /* If we were given back decoded frame, push it to the source pad */
        while (hDstBuf) {

            /* Set the source pad capabilities based on the decoded frame
             * properties.
             */
            gst_tividdec2_set_source_caps(viddec2, hDstBuf);

            /* Create a DMAI transport buffer object to carry a DMAI buffer to
             * the source pad.  The transport buffer knows how to release the
             * buffer for re-use in this element when the source pad calls
             * gst_buffer_unref().
             */
            outBuf = gst_tidmaibuffertransport_new(
                hDstBuf, viddec2->hOutBufTab);
            gst_buffer_set_data(outBuf, GST_BUFFER_DATA(outBuf),
                gst_ti_correct_display_bufSize(hDstBuf));
            gst_buffer_set_caps(outBuf, GST_PAD_CAPS(viddec2->srcpad));

            /* Set output buffer timestamp */ 
            if (viddec2->genTimeStamps) {
                GST_BUFFER_TIMESTAMP(outBuf) = viddec2->totalDuration;
                GST_BUFFER_DURATION(outBuf)  = frameDuration; 
                viddec2->totalDuration       += GST_BUFFER_DURATION(outBuf);
            }
            else {
                GST_BUFFER_TIMESTAMP(outBuf) = GST_CLOCK_TIME_NONE;
            }

            /* Tell circular buffer how much time we consumed */
            gst_ticircbuffer_time_consumed(viddec2->circBuf, frameDuration);

            /* Push the transport buffer to the source pad */
            GST_LOG("pushing buffer to source pad with timestamp : %" 
                    GST_TIME_FORMAT ", duration: %" GST_TIME_FORMAT,
                    GST_TIME_ARGS (GST_BUFFER_TIMESTAMP(outBuf)),
                    GST_TIME_ARGS (GST_BUFFER_DURATION(outBuf)));

            if (gst_pad_push(viddec2->srcpad, outBuf) != GST_FLOW_OK) {
                GST_DEBUG("push to source pad failed\n");
                goto thread_failure;
            }

            hDstBuf = Vdec2_getDisplayBuf(viddec2->hVd);
        }

        /* Release buffers no longer in use by the codec */
        hFreeBuf = Vdec2_getFreeBuf(viddec2->hVd);
        while (hFreeBuf) {
            Buffer_freeUseMask(hFreeBuf, gst_tidmaibuffer_CODEC_FREE);
            hFreeBuf = Vdec2_getFreeBuf(viddec2->hVd);
        }

    }

thread_failure:

    gst_tithread_set_status(viddec2, TIThread_CODEC_ABORTED);
    gst_ticircbuffer_consumer_aborted(viddec2->circBuf);
    threadRet = GstTIThreadFailure;

thread_exit:

    /* Re-claim any buffers owned by the codec */
    if (viddec2->hOutBufTab) {
        bufIdx =
            BufTab_getNumBufs(GST_TIDMAIBUFTAB_BUFTAB(viddec2->hOutBufTab));

        while (bufIdx-- > 0) {
            Buffer_Handle hBuf = BufTab_getBuf(
                GST_TIDMAIBUFTAB_BUFTAB(viddec2->hOutBufTab), bufIdx);
            Buffer_freeUseMask(hBuf, gst_tidmaibuffer_CODEC_FREE);
        }
    }

    /* Release the last buffer we retrieved from the circular buffer */
    if (encDataWindow) {
        gst_ticircbuffer_data_consumed(viddec2->circBuf, encDataWindow, 0);
    }

    /* We have to wait to shut down this thread until we can guarantee that
     * no more input buffers will be queued into the circular buffer
     * (we're about to delete it).  
     */
    Rendezvous_meet(viddec2->waitOnDecodeThread);
    Rendezvous_reset(viddec2->waitOnDecodeThread);

    /* Notify main thread that we are done draining before we shutdown the
     * codec, or we will hang.  We proceed in this order so the EOS event gets
     * propagated downstream before we attempt to shut down the codec.  The
     * codec-shutdown process will block until all BufTab buffers have been
     * released, and downstream-elements may hang on to buffers until
     * they get the EOS.
     */
    Rendezvous_force(viddec2->waitOnDecodeDrain);

    /* stop codec engine */
    if (gst_tividdec2_codec_stop(viddec2) < 0) {
        GST_ERROR("failed to stop codec\n");
    }

    gst_object_unref(viddec2);

    GST_LOG("exit video decode_thread (%d)\n", (int)threadRet);
    return threadRet;
}


/******************************************************************************
 * gst_tividdec2_drain_pipeline
 *    Wait for the decode thread to finish processing queued input data.
 ******************************************************************************/
static void gst_tividdec2_drain_pipeline(GstTIViddec2 *viddec2)
{
    gboolean checkResult;

    /* If the decode thread hasn't been created, there is nothing to drain. */
    if (!gst_tithread_check_status(
             viddec2, TIThread_CODEC_CREATED, checkResult)) {
        return;
    }

    viddec2->drainingEOS = TRUE;
    gst_ticircbuffer_drain(viddec2->circBuf, TRUE);

    /* Tell the decode thread that it is ok to shut down */
    Rendezvous_force(viddec2->waitOnDecodeThread);

    /* Wait for the decoder to finish draining */
    Rendezvous_meet(viddec2->waitOnDecodeDrain);
}


/******************************************************************************
 * gst_tividdec2_frame_duration
 *    Return the duration of a single frame in nanoseconds.
 ******************************************************************************/
static GstClockTime gst_tividdec2_frame_duration(GstTIViddec2 *viddec2)
{
    /* Default to 29.97 if the frame rate was not specified */
    if (gst_value_get_fraction_numerator(&viddec2->framerate) == 0) {
        GST_WARNING("framerate not specified; using 29.97fps");
        gst_value_set_fraction(&viddec2->framerate, 30000,
            1001);
    }

    return
     ((GstClockTime) gst_value_get_fraction_denominator(&viddec2->framerate)) *
     GST_SECOND /
     ((GstClockTime) gst_value_get_fraction_numerator(&viddec2->framerate));
}


/******************************************************************************
 * gst_tividdec2_resizeBufTab
 ******************************************************************************/
static gboolean gst_tividdec2_resizeBufTab(GstTIViddec2 *viddec2)
{
    BufTab_Handle hBufTab = Vdec2_getBufTab(viddec2->hVd);
    Int           numBufs;
    Int           numCodecBuffers;
    Int           numExpBufs;
    Buffer_Handle hBuf;
    Int32         frameSize;

    /* How many buffers can the codec keep at one time? */
    numCodecBuffers = Vdec2_getMinOutBufs(viddec2->hVd);

    if (numCodecBuffers < 0) {
        GST_ERROR("failed to get buffer requirements\n");
        return FALSE;
    }

    /*
     * Total number of frames needed are the number of buffers the codec
     * can keep at any time, plus the number of frames in the pipeline.
     */
    numBufs = numCodecBuffers + viddec2->numOutputBufs;

    /* Get the size of output buffers needed from codec */
    frameSize = Vdec2_getOutBufSize(viddec2->hVd);

    /*
     * Get the first buffer of the BufTab to determine buffer characteristics.
     * All buffers in a BufTab share the same characteristics.
     */
    hBuf = BufTab_getBuf(hBufTab, 0);

    /* Do we need to resize the BufTab? */
    if (numBufs > BufTab_getNumBufs(hBufTab) ||
        frameSize < Buffer_getSize(hBuf)) {

        /* Should we break the current buffers in to many smaller buffers? */
        if (frameSize < Buffer_getSize(hBuf)) {

            /*
             * Chunk the larger buffers of the BufTab in to smaller buffers
             * to accomodate the codec requirements.
             */
            numExpBufs = BufTab_chunk(hBufTab, numBufs, frameSize);

            if (numExpBufs < 0) {
                GST_ERROR("failed to chunk %d bufs size %ld to %d bufs size "
                    "%ld\n", BufTab_getNumBufs(hBufTab), Buffer_getSize(hBuf),
                    numBufs, frameSize);
                return FALSE;
            }

            /*
             * Did the current BufTab fit the chunked buffers,
             * or do we need to expand the BufTab (numExpBufs > 0)?
             */
            if (BufTab_expand(hBufTab, numExpBufs) < 0) {
                GST_ERROR("failed to expand BufTab with %d buffers\n",
                    numExpBufs);
                return FALSE;
            }
        }
        else {
            /* Just expand the BufTab with more buffers */
            if (BufTab_expand(hBufTab, numCodecBuffers) < 0) {
                GST_ERROR("Failed to expand BufTab with %d buffers\n",
                    numCodecBuffers);
                return FALSE;
            }
        }
    }

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
