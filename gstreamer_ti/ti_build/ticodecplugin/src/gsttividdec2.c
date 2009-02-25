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
#  include <config.h>
#endif

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
  PROP_FRAMERATE,       /* frameRate      (int)     */
  PROP_DISPLAY_BUFFER,  /* displayBuffer  (boolean) */
  PROP_GEN_TIMESTAMPS   /* genTimeStamps  (boolean) */
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
         "height=(int)[ 1, MAX ]"
    )
);

/* Constants */
#define gst_tividdec2_CODEC_FREE 0x2

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
static void*
 gst_tividdec2_queue_thread(void *arg);
static void
 gst_tividdec2_broadcast_queue_thread(GstTIViddec2 *viddec2);
static void
 gst_tividdec2_wait_on_queue_thread(GstTIViddec2 *viddec2,
     Int32 waitQueueSize);
static void
 gst_tividdec2_drain_pipeline(GstTIViddec2 *viddec2);
static GstClockTime
 gst_tividdec2_frame_duration(GstTIViddec2 *viddec2);
static gboolean
 gst_tividdec2_resizeBufTab(GstTIViddec2 *viddec2);
static gboolean 
    gst_tividdec2_codec_start (GstTIViddec2  *viddec2);
static gboolean 
    gst_tividdec2_codec_stop (GstTIViddec2  *viddec2);

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
        g_param_spec_int("frameRate",
            "Frame rate to play output",
            "Communicate this framerate to downstream elements.  The frame "
            "rate specified should be an integer.  If 29.97fps is desired, "
            "specify 30 for this setting",
            1, G_MAXINT32, 30, G_PARAM_WRITABLE));

    g_object_class_install_property(gobject_class, PROP_DISPLAY_BUFFER,
        g_param_spec_boolean("displayBuffer", "Display Buffer",
            "Display circular buffer status while processing",
            FALSE, G_PARAM_WRITABLE));

    g_object_class_install_property(gobject_class, PROP_GEN_TIMESTAMPS,
        g_param_spec_boolean("genTimeStamps", "Generate Time Stamps",
            "Set timestamps on output buffers",
            TRUE, G_PARAM_WRITABLE));
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

    viddec2->decodeDrained      = FALSE;
    viddec2->waitOnDecodeDrain  = NULL;

    viddec2->hInFifo            = NULL;

    viddec2->waitOnQueueThread  = NULL;
    viddec2->waitQueueSize      = 0;
    viddec2->waitOnDecodeThread = NULL;

    viddec2->framerateNum       = 0;
    viddec2->framerateDen       = 0;

    viddec2->numOutputBufs      = 0UL;
    viddec2->hOutBufTab         = NULL;
    viddec2->circBuf            = NULL;

    viddec2->sps_pps_data       = NULL;
    viddec2->nal_code_prefix    = NULL;
    viddec2->nal_length         = 0;
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
            viddec2->framerateNum = g_value_get_int(value);
            viddec2->framerateDen = 1;

            /* If 30fps was specified, use 29.97 */
            if (viddec2->framerateNum == 30) {
                viddec2->framerateNum = 30000;
                viddec2->framerateDen = 1001;
            }

            GST_LOG("setting \"frameRate\" to \"%2.2lf\"\n",
                (gdouble)viddec2->framerateNum /
                (gdouble)viddec2->framerateDen);
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
            viddec2->framerateNum = framerateNum;
            viddec2->framerateDen = framerateDen;
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

    /* Mime type not supported */
    else {
        GST_ERROR("stream type not supported");
        gst_object_unref(viddec2);
        return FALSE;
    }

    /* Report if the required codec was not found */
    if (!codec) {
        GST_ERROR("unable to find codec needed for stream");
        gst_object_unref(viddec2);
        return FALSE;
    }

    /* Shut-down any running video decoder */
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
    GstCaps              *caps;
    gboolean              ret;
    GstPad               *pad;
    char                 *string;

    pad = viddec2->srcpad;

    /* Create a UYVY caps object using the dimensions from the given buffer */
    BufferGfx_getDimensions(hBuf, &dim);

    caps =
        gst_caps_new_simple("video/x-raw-yuv",
            "format",    GST_TYPE_FOURCC,   GST_MAKE_FOURCC('U','Y','V','Y'),
            "framerate", GST_TYPE_FRACTION, viddec2->framerateNum,
                                            viddec2->framerateDen,
            "width",     G_TYPE_INT,        dim.width,
            "height",    G_TYPE_INT,        dim.height,
            NULL);

    /* Set the source pad caps */
    string = gst_caps_to_string(caps);
    GST_LOG("setting source caps to UYVY:  %s", string);
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
            /* maybe save and/or update the current segment (e.g. for output
             * clipping) or convert the event into one in a different format
             * (e.g. BYTES to TIME) or drop it and set a flag to send a
             * newsegment event in a different format later
             */
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
 * gst_tividdec2_chain
 *    This is the main processing routine.  This function receives a buffer
 *    from the sink pad, processes it, and pushes the result to the source
 *    pad.
 ******************************************************************************/
static GstFlowReturn gst_tividdec2_chain(GstPad * pad, GstBuffer * buf)
{
    GstTIViddec2 *viddec2 = GST_TIVIDDEC2(GST_OBJECT_PARENT(pad));
    gboolean     checkResult;

    /* If any thread aborted, communicate it to the pipeline */
    if (gst_tithread_check_status(
            viddec2, TIThread_ANY_ABORTED, checkResult)) {
       gst_buffer_unref(buf);
       return GST_FLOW_UNEXPECTED;
    }

    /* If our engine handle is currently NULL, then either this is our first
     * buffer or the upstream element has re-negotiated our capabilities which
     * resulted in our engine being closed.  In either case, we need to
     * initialize (or re-initialize) our video decoder to handle the new
     * stream.
     */
    if (viddec2->hEngine == NULL) {
        if (!gst_tividdec2_init_video(viddec2)) {
            GST_ERROR("unable to initialize video\n");
            gst_buffer_unref(buf);
            return GST_FLOW_UNEXPECTED;
        }

        /* check if we have recieved buffer from qtdemuxer. To do this,
         * we will verify if codec_data field has a valid avcC header.
         */
        if (gst_is_h264_decoder(viddec2->codecName) && 
                gst_h264_valid_quicktime_header(buf)) {
            viddec2->nal_length = gst_h264_get_nal_length(buf);
            viddec2->sps_pps_data = gst_h264_get_sps_pps_data(buf);
            viddec2->nal_code_prefix = gst_h264_get_nal_prefix_code();
        }

        GST_TICIRCBUFFER_TIMESTAMP(viddec2->circBuf) =
            GST_CLOCK_TIME_IS_VALID(GST_BUFFER_TIMESTAMP(buf)) ?
            GST_BUFFER_TIMESTAMP(buf) : 0ULL;
    }

    /* Don't queue up too many buffers -- if we collect too many input buffers
     * without consuming them we'll run out of memory.  Once we reach a
     * threshold, block until the queue thread removes some buffers.
     */
    Rendezvous_reset(viddec2->waitOnQueueThread);
    if (Fifo_getNumEntries(viddec2->hInFifo) > 500) {
        gst_tividdec2_wait_on_queue_thread(viddec2, 400);
    }

    /* If demuxer has passed SPS and PPS NAL unit dump in codec_data field,
     * then we have a packetized h264 stream. We need to transform this stream
     * into byte-stream.
     */
    if (viddec2->sps_pps_data) {
        if (gst_h264_parse_and_fifo_put(viddec2->hInFifo, buf, 
                viddec2->sps_pps_data, viddec2->nal_code_prefix,
                viddec2->nal_length) < 0) {
            GST_ERROR("Failed to send buffer to queue thread\n");
            gst_buffer_unref(buf);
            return GST_FLOW_UNEXPECTED;
        }
    }
    else {
        /* Queue up the encoded data stream into a circular buffer */
        if (Fifo_put(viddec2->hInFifo, buf) < 0) {
            GST_ERROR("Failed to send buffer to queue thread\n");
            gst_buffer_unref(buf);
            return GST_FLOW_UNEXPECTED;
        }
    }

    return GST_FLOW_OK;
}


/******************************************************************************
 * gst_tividdec2_init_video
 *     Initialize or re-initializes the video stream
 ******************************************************************************/
static gboolean gst_tividdec2_init_video(GstTIViddec2 *viddec2)
{
    Rendezvous_Attrs       rzvAttrs  = Rendezvous_Attrs_DEFAULT;
    struct sched_param     schedParam;
    pthread_attr_t         attr;
    Fifo_Attrs             fAttrs    = Fifo_Attrs_DEFAULT;

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
        GST_ERROR("engine name not specified\n");
        return FALSE;
    }

    if (!viddec2->codecName) {
        GST_ERROR("codec name not specified\n");
        return FALSE;
    }

    /* Set up the queue fifo */
    viddec2->hInFifo = Fifo_create(&fAttrs);

    /* Initialize thread status management */
    viddec2->threadStatus = 0UL;
    pthread_mutex_init(&viddec2->threadStatusMutex, NULL);

    /* Initialize rendezvous objects for making threads wait on conditions */
    viddec2->waitOnDecodeDrain  = Rendezvous_create(100, &rzvAttrs);
    viddec2->waitOnQueueThread  = Rendezvous_create(100, &rzvAttrs);
    viddec2->waitOnDecodeThread = Rendezvous_create(2, &rzvAttrs);
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
    if (pthread_create(&viddec2->decodeThread, &attr,
            gst_tividdec2_decode_thread, (void*)viddec2)) {
        GST_ERROR("failed to create decode thread\n");
        gst_tividdec2_exit_video(viddec2);
        return FALSE;
    }
    gst_tithread_set_status(viddec2, TIThread_DECODE_CREATED);

    /* Wait for decoder thread to finish initilization before creating queue
     * thread.
     */
    Rendezvous_meet(viddec2->waitOnDecodeThread);

    /* Make sure circular buffer and display buffer handles are created by 
     * decoder thread.
     */
    if (viddec2->circBuf == NULL || viddec2->hOutBufTab == NULL) {
        GST_ERROR("decode thread failed to create circbuf or display buffer"
                  " handles\n");
        return FALSE;
    }

    /* Create queue thread */
    if (pthread_create(&viddec2->queueThread, NULL,
            gst_tividdec2_queue_thread, (void*)viddec2)) {
        GST_ERROR("failed to create queue thread\n");
        gst_tividdec2_exit_video(viddec2);
        return FALSE;
    }
    gst_tithread_set_status(viddec2, TIThread_QUEUE_CREATED);

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
            viddec2, TIThread_DECODE_CREATED, checkResult)) {
        GST_LOG("shutting down decode thread\n");

        if (pthread_join(viddec2->decodeThread, &thread_ret) == 0) {
            if (thread_ret == GstTIThreadFailure) {
                GST_DEBUG("decode thread exited with an error condition\n");
            }
        }
    }

    /* Shut down the queue thread */
    if (gst_tithread_check_status(
            viddec2, TIThread_QUEUE_CREATED, checkResult)) {
        GST_LOG("shutting down queue thread\n");

        /* Unstop the queue thread if needed, and wait for it to finish */
        Fifo_flush(viddec2->hInFifo);

        if (pthread_join(viddec2->queueThread, &thread_ret) == 0) {
            if (thread_ret == GstTIThreadFailure) {
                GST_DEBUG("queue thread exited with an error condition\n");
            }
        }
    }

    /* Shut down thread status management */
    viddec2->threadStatus = 0UL;
    pthread_mutex_destroy(&viddec2->threadStatusMutex);

    /* Shut down any remaining items */
    if (viddec2->hInFifo) {
        Fifo_delete(viddec2->hInFifo);
        viddec2->hInFifo = NULL;
    }

    if (viddec2->waitOnQueueThread) {
        Rendezvous_delete(viddec2->waitOnQueueThread);
        viddec2->waitOnQueueThread = NULL;
    }

    if (viddec2->waitOnDecodeDrain) {
        Rendezvous_delete(viddec2->waitOnDecodeDrain);
        viddec2->waitOnDecodeDrain = NULL;
    }

    if (viddec2->waitOnDecodeThread) {
        Rendezvous_delete(viddec2->waitOnDecodeThread);
        viddec2->waitOnDecodeThread = NULL;
    }

    if (viddec2->circBuf) {
        GST_LOG("freeing cicrular input buffer\n");
        gst_ticircbuffer_unref(viddec2->circBuf);
        viddec2->circBuf      = NULL;
        viddec2->framerateNum = 0;
        viddec2->framerateDen = 0;
    }

    if (viddec2->hOutBufTab) {
        GST_LOG("freeing output buffers\n");
        BufTab_delete(viddec2->hOutBufTab);
        viddec2->hOutBufTab = NULL;
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
static gboolean gst_tividdec2_codec_start (GstTIViddec2  *viddec2)
{
    VIDDEC2_Params         params    = Vdec2_Params_DEFAULT;
    VIDDEC2_DynamicParams  dynParams = Vdec2_DynamicParams_DEFAULT;
    BufferGfx_Attrs        gfxAttrs  = BufferGfx_Attrs_DEFAULT;
    Cpu_Device             device;
    ColorSpace_Type        colorSpace;
    Int                    defaultNumBufs;

    /* Open the codec engine */
    GST_LOG("opening codec engine \"%s\"\n", viddec2->engineName);
    viddec2->hEngine = Engine_open((Char *) viddec2->engineName, NULL, NULL);

    if (viddec2->hEngine == NULL) {
        GST_ERROR("failed to open codec engine \"%s\"\n", viddec2->engineName);
        return FALSE;
    }

    /* Determine which device the application is running on */
    if (Cpu_getDevice(NULL, &device) < 0) {
        GST_ERROR("Failed to determine target board\n");
        return FALSE;
    }

    /* Set up codec parameters depending on device */
    if (device == Cpu_Device_DM6467) {
        params.forceChromaFormat = XDM_YUV_420P;
        params.maxWidth          = VideoStd_1080I_WIDTH;
        params.maxHeight         = VideoStd_1080I_HEIGHT + 8;
        colorSpace               = ColorSpace_YUV420PSEMI;
        defaultNumBufs           = 5;
    } else {
        params.forceChromaFormat = XDM_YUV_422ILE;
        params.maxWidth          = VideoStd_D1_WIDTH;
        params.maxHeight         = VideoStd_D1_PAL_HEIGHT;
        colorSpace               = ColorSpace_UYVY;
        defaultNumBufs           = 3;
    }

    GST_LOG("opening video decoder \"%s\"\n", viddec2->codecName);
    viddec2->hVd = Vdec2_create(viddec2->hEngine, (Char*)viddec2->codecName,
                      &params, &dynParams);

    if (viddec2->hVd == NULL) {
        GST_ERROR("failed to create video decoder: %s\n", viddec2->codecName);
        GST_LOG("closing codec engine\n");
        return FALSE;
    }

    /* Record that we haven't processed the first frame yet */
    viddec2->firstFrame = TRUE;

    /* Create a circular input buffer */
    viddec2->circBuf =
        gst_ticircbuffer_new(Vdec2_getInBufSize(viddec2->hVd), 3, FALSE);

    if (viddec2->circBuf == NULL) {
        GST_ERROR("failed to create circular input buffer\n");
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

    /* Create codec output buffers */
    GST_LOG("creating output buffer table\n");
    gfxAttrs.colorSpace     = colorSpace;
    gfxAttrs.dim.width      = params.maxWidth;
    gfxAttrs.dim.height     = params.maxHeight;
    gfxAttrs.dim.lineLength = BufferGfx_calcLineLength(
                                  gfxAttrs.dim.width, gfxAttrs.colorSpace);

    /* Both the codec and the GStreamer pipeline can own a buffer */
    gfxAttrs.bAttrs.useMask = gst_tidmaibuffertransport_GST_FREE |
                              gst_tividdec2_CODEC_FREE;

    viddec2->hOutBufTab =
        BufTab_create(viddec2->numOutputBufs, Vdec2_getOutBufSize(viddec2->hVd),
            BufferGfx_getBufferAttrs(&gfxAttrs));

    if (viddec2->hOutBufTab == NULL) {
        GST_ERROR("failed to create output buffers\n");
        return FALSE;
    }

    /* Tell the Vdec module that hOutBufTab will be used for display buffers */
    Vdec2_setBufTab(viddec2->hVd, viddec2->hOutBufTab);

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
    Buffer_Attrs   bAttrs         = Buffer_Attrs_DEFAULT;
    gboolean       codecFlushed   = FALSE;
    void          *threadRet      = GstTIThreadSuccess;
    Buffer_Handle  hDummyInputBuf = NULL;
    Buffer_Handle  hDstBuf;
    Buffer_Handle  hFreeBuf;
    Int32          encDataConsumed;
    GstClockTime   encDataTime;
    GstClockTime   frameDuration;
    Buffer_Handle  hEncDataWindow;
    GstBuffer     *outBuf;
    Int            ret;

    GST_LOG("init video decode_thread \n");

    /* Initialize codec engine */
    ret = gst_tividdec2_codec_start(viddec2);

    /* Notify main thread if it is waiting to create queue thread */
    Rendezvous_meet(viddec2->waitOnDecodeThread);

    if (ret == FALSE) {
        GST_ERROR("failed to start codec\n");
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
        hDstBuf = BufTab_getFreeBuf(viddec2->hOutBufTab);
        if (hDstBuf == NULL) {
            GST_ERROR("failed to get a free contiguous buffer from BufTab\n");
            goto thread_failure;
        }

        /* Make sure the whole buffer is used for output */
        BufferGfx_resetDimensions(hDstBuf);

        /* Invoke the video decoder */
        GST_LOG("invoking the video decoder\n");
        ret             = Vdec2_process(viddec2->hVd, hEncDataWindow, hDstBuf);
        encDataConsumed = (codecFlushed) ? 0 :
                          Buffer_getNumBytesUsed(hEncDataWindow);

        if (ret < 0) {
            GST_ERROR("failed to decode video buffer\n");
            goto thread_failure;
        }

        /* If no encoded data was used we cannot find the next frame */
        if (ret == Dmai_EBITERROR && encDataConsumed == 0 && !codecFlushed) {
            GST_ERROR("fatal bit error\n");
            goto thread_failure;
        }

        if (ret > 0) {
            GST_LOG("Vdec2_process returned success code %d\n", ret); 
        }

        /* Release the reference buffer, and tell the circular buffer how much
         * data was consumed.
         */
        ret = gst_ticircbuffer_data_consumed(viddec2->circBuf, encDataWindow,
                  encDataConsumed);
        encDataWindow = NULL;

        if (!ret) {
            goto thread_failure;
        }

        /* Resize the BufTab after the first frame has been processed.  The
         * codec may not know it's buffer requirements before the first frame
         * has been decoded.
         */
        if (viddec2->firstFrame) {
            if (!gst_tividdec2_resizeBufTab(viddec2)) {
                GST_ERROR("failed to re-partition decode buffers after "
                          "processing first frame\n");
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
            outBuf = gst_tidmaibuffertransport_new(hDstBuf);
            gst_buffer_set_data(outBuf, GST_BUFFER_DATA(outBuf),
                Buffer_getNumBytesUsed(hDstBuf));
            gst_buffer_set_caps(outBuf, GST_PAD_CAPS(viddec2->srcpad));

            /* If we have a valid time stamp, set it on the buffer */
            if (viddec2->genTimeStamps &&
                GST_CLOCK_TIME_IS_VALID(encDataTime)) {
                GST_LOG("video timestamp value: %llu\n", encDataTime);
                GST_BUFFER_TIMESTAMP(outBuf) = encDataTime;
                GST_BUFFER_DURATION(outBuf)  = frameDuration;
            }
            else {
                GST_BUFFER_TIMESTAMP(outBuf) = GST_CLOCK_TIME_NONE;
            }

            /* Tell circular buffer how much time we consumed */
            gst_ticircbuffer_time_consumed(viddec2->circBuf, frameDuration);

            /* Push the transport buffer to the source pad */
            GST_LOG("pushing display buffer to source pad\n");

            if (gst_pad_push(viddec2->srcpad, outBuf) != GST_FLOW_OK) {
                GST_DEBUG("push to source pad failed\n");
                goto thread_failure;
            }

            hDstBuf = Vdec2_getDisplayBuf(viddec2->hVd);
        }

        /* Release buffers no longer in use by the codec */
        hFreeBuf = Vdec2_getFreeBuf(viddec2->hVd);
        while (hFreeBuf) {
            Buffer_freeUseMask(hFreeBuf, gst_tividdec2_CODEC_FREE);
            hFreeBuf = Vdec2_getFreeBuf(viddec2->hVd);
        }

    }

thread_failure:

    /* If encDataWindow is non-NULL, something bad happened before we had a
     * chance to release it.  Release it now so we don't block the pipeline.
     * We release it by telling the circular buffer that we're done with it and
     * consumed no data.
     */
    if (encDataWindow) {
        gst_ticircbuffer_data_consumed(viddec2->circBuf, encDataWindow, 0);
    }

    gst_tithread_set_status(viddec2, TIThread_DECODE_ABORTED);
    threadRet = GstTIThreadFailure;
    gst_ticircbuffer_consumer_aborted(viddec2->circBuf);
    Rendezvous_force(viddec2->waitOnQueueThread);

thread_exit:

    /* Initialize codec engine */
    if (gst_tividdec2_codec_stop(viddec2) < 0) {
        GST_ERROR("failed to stop codec\n");
    }

    /* Notify main thread if it is waiting on decode thread shut-down */
    viddec2->decodeDrained = TRUE;
    Rendezvous_force(viddec2->waitOnDecodeDrain);

    gst_object_unref(viddec2);

    GST_LOG("exit video decode_thread (%d)\n", (int)threadRet);
    return threadRet;
}


/******************************************************************************
 * gst_tividdec2_queue_thread 
 *     Add an input buffer to the circular buffer            
 ******************************************************************************/
static void* gst_tividdec2_queue_thread(void *arg)
{
    GstTIViddec2* viddec2    = GST_TIVIDDEC2(gst_object_ref(arg));
    void*        threadRet = GstTIThreadSuccess;
    GstBuffer*   encData;
    Int          fifoRet;

    while (TRUE) {

        /* Get the next input buffer (or block until one is ready) */
        fifoRet = Fifo_get(viddec2->hInFifo, &encData);

        if (fifoRet < 0) {
            GST_ERROR("Failed to get buffer from input fifo\n");
            goto thread_failure;
        }

        /* Did the video thread flush the fifo? */
        if (fifoRet == Dmai_EFLUSH) {
            goto thread_exit;
        }

        /* Send the buffer to the circular buffer */
        if (!gst_ticircbuffer_queue_data(viddec2->circBuf, encData)) {
            goto thread_failure;
        }

        /* Release the buffer we received from the sink pad */
        gst_buffer_unref(encData);

        /* If we've reached the EOS, start draining the circular buffer when
         * there are no more buffers in the FIFO.
         */
        if (viddec2->drainingEOS && 
            Fifo_getNumEntries(viddec2->hInFifo) == 0) {
            gst_ticircbuffer_drain(viddec2->circBuf, TRUE);
        }

        /* Unblock any pending puts to our Fifo if we have reached our
         * minimum threshold.
         */
        gst_tividdec2_broadcast_queue_thread(viddec2);
    }

thread_failure:
    gst_tithread_set_status(viddec2, TIThread_QUEUE_ABORTED);
    threadRet = GstTIThreadFailure;

thread_exit:
    gst_object_unref(viddec2);
    return threadRet;
}


/******************************************************************************
 * gst_tividdec2_wait_on_queue_thread
 *    Wait for the queue thread to consume buffers from the fifo until
 *    there are only "waitQueueSize" items remaining.
 ******************************************************************************/
static void gst_tividdec2_wait_on_queue_thread(GstTIViddec2 *viddec2,
                Int32 waitQueueSize)
{
    viddec2->waitQueueSize = waitQueueSize;
    Rendezvous_meet(viddec2->waitOnQueueThread);
}


/******************************************************************************
 * gst_tividdec2_broadcast_queue_thread
 *    Broadcast when queue thread has processed enough buffers from the
 *    fifo to unblock anyone waiting to queue some more.
 ******************************************************************************/
static void gst_tividdec2_broadcast_queue_thread(GstTIViddec2 *viddec2)
{
    if (viddec2->waitQueueSize < Fifo_getNumEntries(viddec2->hInFifo)) {
          return;
    } 
    Rendezvous_force(viddec2->waitOnQueueThread);
}


/******************************************************************************
 * gst_tividdec2_drain_pipeline
 *    Push any remaining input buffers through the queue and decode threads
 ******************************************************************************/
static void gst_tividdec2_drain_pipeline(GstTIViddec2 *viddec2)
{
    gboolean checkResult;

    viddec2->drainingEOS = TRUE;

    /* If the processing threads haven't been created, there is nothing to
     * drain.
     */
    if (!gst_tithread_check_status(
             viddec2, TIThread_DECODE_CREATED, checkResult)) {
        return;
    }
    if (!gst_tithread_check_status(
             viddec2, TIThread_QUEUE_CREATED, checkResult)) {
        return;
    }

    /* If the queue fifo still has entries in it, it will drain the
     * circular buffer once all input buffers have been added to the
     * circular buffer.  If the fifo is already empty, we must drain
     * the circular buffer here.
     */
    if (Fifo_getNumEntries(viddec2->hInFifo) == 0) {
        gst_ticircbuffer_drain(viddec2->circBuf, TRUE);
    }
    else {
        Rendezvous_force(viddec2->waitOnQueueThread);
    }

    /* Wait for the decoder to drain */
    if (!viddec2->decodeDrained) {
        Rendezvous_meet(viddec2->waitOnDecodeDrain);
    }
    viddec2->decodeDrained = FALSE;

}


/******************************************************************************
 * gst_tividdec2_frame_duration
 *    Return the duration of a single frame in nanoseconds.
 ******************************************************************************/
static GstClockTime gst_tividdec2_frame_duration(GstTIViddec2 *viddec2)
{
    /* Default to 29.97 if the frame rate was not specified */
    if (viddec2->framerateNum == 0 && viddec2->framerateDen == 0) {
        GST_WARNING("framerate not specified; using 29.97fps");
        viddec2->framerateNum = 30000;
        viddec2->framerateDen = 1001;
    }

    return (GstClockTime)
        ((1 / ((gdouble)viddec2->framerateNum/(gdouble)viddec2->framerateDen))
         * GST_SECOND);
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
