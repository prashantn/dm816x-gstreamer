/*
 * gsttividdec.c
 *
 * This file defines the "TIViddec" element, which decodes an xDM 0.9 video
 * stream.
 *
 * Example usage:
 *     gst-launch filesrc location=<video file> !
 *         TIViddec engineName="<engine name>" codecName="<codecName>" !
 *         fakesink silent=TRUE
 *
 * Notes:
 *  * If the upstream element (i.e. demuxer or typefind element) negotiates
 *    caps with TIViddec, the engineName and codecName properties will be
 *    auto-detected based on the mime type requested.  The engine and codec
 *    names used for particular mime types are defined in gsttividdec.h.
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
#include <semaphore.h>

#include <ti/sdo/dmai/Dmai.h>
#include <ti/sdo/dmai/VideoStd.h>
#include <ti/sdo/dmai/Buffer.h>
#include <ti/sdo/dmai/BufferGfx.h>
#include <ti/sdo/dmai/BufTab.h>
#include <ti/sdo/dmai/ce/Vdec.h>

#include "gsttividdec.h"
#include "gsttidmaibuffertransport.h"
#include "gstticodecs.h"
#include "gsttithreadprops.h"
#include "gsttiquicktime_h264.h"
#include "gstticommonutils.h"

/* Declare variable used to categorize GST_LOG output */
GST_DEBUG_CATEGORY_STATIC (gst_tividdec_debug);
#define GST_CAT_DEFAULT gst_tividdec_debug

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


/* Declare a global pointer to our element base class */
static GstElementClass *parent_class = NULL;

/* Static Function Declarations */
static void
 gst_tividdec_base_init(gpointer g_class);
static void
 gst_tividdec_class_init(GstTIViddecClass *g_class);
static void
 gst_tividdec_init(GstTIViddec *object, GstTIViddecClass *g_class);
static void
 gst_tividdec_set_property (GObject *object, guint prop_id,
     const GValue *value, GParamSpec *pspec);
static void
 gst_tividdec_get_property (GObject *object, guint prop_id, GValue *value,
     GParamSpec *pspec);
static gboolean
 gst_tividdec_set_sink_caps(GstPad *pad, GstCaps *caps);
static gboolean
 gst_tividdec_set_source_caps(GstTIViddec *viddec, Buffer_Handle hBuf);
static gboolean
 gst_tividdec_sink_event(GstPad *pad, GstEvent *event);
static GstFlowReturn
 gst_tividdec_chain(GstPad *pad, GstBuffer *buf);
static gboolean
 gst_tividdec_init_video(GstTIViddec *viddec);
static gboolean
 gst_tividdec_exit_video(GstTIViddec *viddec);
static GstStateChangeReturn
 gst_tividdec_change_state(GstElement *element, GstStateChange transition);
static void*
 gst_tividdec_decode_thread(void *arg);
static void*
 gst_tividdec_queue_thread(void *arg);
static void
 gst_tividdec_broadcast_queue_thread(GstTIViddec *viddec);
static void
 gst_tividdec_wait_on_queue_thread(GstTIViddec *viddec, Int32 waitQueueSize);
static void
 gst_tividdec_drain_pipeline(GstTIViddec *viddec);
static GstClockTime
 gst_tividdec_frame_duration(GstTIViddec *viddec);
static gboolean 
    gst_tividdec_codec_start (GstTIViddec  *viddec);
static gboolean 
    gst_tividdec_codec_stop (GstTIViddec  *viddec);
static void 
    gst_tividdec_init_env(GstTIViddec *viddec);

/******************************************************************************
 * gst_tividdec_class_init_trampoline
 *    Boiler-plate function auto-generated by "make_element" script.
 ******************************************************************************/
static void gst_tividdec_class_init_trampoline(gpointer g_class, gpointer data)
{
    parent_class = (GstElementClass*) g_type_class_peek_parent(g_class);
    gst_tividdec_class_init((GstTIViddecClass*)g_class);
}


/******************************************************************************
 * gst_tividdec_get_type
 *    Boiler-plate function auto-generated by "make_element" script.
 *    Defines function pointers for initialization routines for this element.
 ******************************************************************************/
GType gst_tividdec_get_type(void)
{
    static GType object_type = 0;

    if (G_UNLIKELY(object_type == 0)) {
        static const GTypeInfo object_info = {
            sizeof(GstTIViddecClass),
            gst_tividdec_base_init,
            NULL,
            gst_tividdec_class_init_trampoline,
            NULL,
            NULL,
            sizeof(GstTIViddec),
            0,
            (GInstanceInitFunc) gst_tividdec_init
        };

        object_type = g_type_register_static((gst_element_get_type()),
                          "GstTIViddec", &object_info, (GTypeFlags)0);

        /* Initialize GST_LOG for this object */
        GST_DEBUG_CATEGORY_INIT(gst_tividdec_debug, "TIViddec", 0,
            "TI xDM 0.9 Video Decoder");

        GST_LOG("initialized get_type\n");

    }

    return object_type;
};


/******************************************************************************
 * gst_tividdec_base_init
 *    Boiler-plate function auto-generated by "make_element" script.
 *    Initializes element base class.
 ******************************************************************************/
static void gst_tividdec_base_init(gpointer gclass)
{
    static GstElementDetails element_details = {
        "TI xDM 0.9 Video Decoder",
        "Codec/Decoder/Video",
        "Decodes video using an xDM 0.9-based codec",
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
 * gst_tividdec_class_init
 *    Boiler-plate function auto-generated by "make_element" script.
 *    Initializes the TIViddec class.
 ******************************************************************************/
static void gst_tividdec_class_init(GstTIViddecClass *klass)
{
    GObjectClass    *gobject_class;
    GstElementClass *gstelement_class;

    gobject_class    = (GObjectClass*)    klass;
    gstelement_class = (GstElementClass*) klass;

    gobject_class->set_property = gst_tividdec_set_property;
    gobject_class->get_property = gst_tividdec_get_property;

    gstelement_class->change_state = gst_tividdec_change_state;

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
 * gst_tividdec_init_env
 *****************************************************************************/
static void gst_tividdec_init_env(GstTIViddec *viddec)
{
    GST_LOG("gst_tividdec_init_env - begin\n");

    if (gst_ti_env_is_defined("GST_TI_TIViddec_engineName")) {
        viddec->engineName = gst_ti_env_get_string("GST_TI_TIViddec_engineName");
        GST_LOG("Setting engineName=%s\n", viddec->engineName);
    }

    if (gst_ti_env_is_defined("GST_TI_TIViddec_codecName")) {
        viddec->codecName = gst_ti_env_get_string("GST_TI_TIViddec_codecName");
        GST_LOG("Setting codecName=%s\n", viddec->codecName);
    }
    
    if (gst_ti_env_is_defined("GST_TI_TIViddec_numOutputBufs")) {
        viddec->numOutputBufs = 
                            gst_ti_env_get_int("GST_TI_TIViddec_numOutputBufs");
        GST_LOG("Setting numOutputBufs=%ld\n", viddec->numOutputBufs);
    }

    if (gst_ti_env_is_defined("GST_TI_TIViddec_displayBuffer")) {
        viddec->displayBuffer = 
                gst_ti_env_get_boolean("GST_TI_TIViddec_displayBuffer");
        GST_LOG("Setting displayBuffer=%s\n",
                 viddec->displayBuffer  ? "TRUE" : "FALSE");
    }
 
    if (gst_ti_env_is_defined("GST_TI_TIViddec_genTimeStamps")) {
        viddec->genTimeStamps = 
                gst_ti_env_get_boolean("GST_TI_TIViddec_genTimeStamps");
        GST_LOG("Setting genTimeStamps =%s\n", 
                    viddec->genTimeStamps ? "TRUE" : "FALSE");
    }

    if (gst_ti_env_is_defined("GST_TI_TIViddec_frameRate")) {
        viddec->framerateNum = 
            gst_ti_env_get_int("GST_TI_TIViddec_frameRate");
        GST_LOG("Setting frameRate=%d\n", viddec->framerateNum);
    }

    GST_LOG("gst_tividdec_init_env - end\n");
}


/******************************************************************************
 * gst_tividdec_init
 *    Initializes a new element instance, instantiates pads and sets the pad
 *    callback functions.
 ******************************************************************************/
static void gst_tividdec_init(GstTIViddec *viddec, GstTIViddecClass *gclass)
{
    /* Instantiate encoded video sink pad.
     *
     * Fixate on our static template caps instead of writing a getcaps
     * function, which is overkill for this element.
     */
    viddec->sinkpad =
        gst_pad_new_from_static_template(&sink_factory, "sink");
    gst_pad_set_setcaps_function(
        viddec->sinkpad, GST_DEBUG_FUNCPTR(gst_tividdec_set_sink_caps));
    gst_pad_set_event_function(
        viddec->sinkpad, GST_DEBUG_FUNCPTR(gst_tividdec_sink_event));
    gst_pad_set_chain_function(
        viddec->sinkpad, GST_DEBUG_FUNCPTR(gst_tividdec_chain));
    gst_pad_fixate_caps(viddec->sinkpad,
        gst_caps_make_writable(
            gst_caps_copy(gst_pad_get_pad_template_caps(viddec->sinkpad))));

    /* Instantiate deceoded video source pad.
     *
     * Fixate on our static template caps instead of writing a getcaps
     * function, which is overkill for this element.
     */
    viddec->srcpad =
        gst_pad_new_from_static_template(&src_factory, "src");
    gst_pad_fixate_caps(viddec->srcpad,
        gst_caps_make_writable(
            gst_caps_copy(gst_pad_get_pad_template_caps(viddec->srcpad))));

    /* Add pads to TIViddec element */
    gst_element_add_pad(GST_ELEMENT(viddec), viddec->sinkpad);
    gst_element_add_pad(GST_ELEMENT(viddec), viddec->srcpad);

    /* Initialize TIViddec state */
    viddec->engineName          = NULL;
    viddec->codecName           = NULL;
    viddec->displayBuffer       = FALSE;
    viddec->genTimeStamps       = TRUE;

    viddec->hEngine             = NULL;
    viddec->hVd                 = NULL;
    viddec->drainingEOS         = FALSE;
    viddec->threadStatus        = 0UL;

    viddec->decodeDrained       = FALSE;
    viddec->waitOnDecodeDrain   = NULL;
    viddec->waitOnDecodeThread  = NULL;
    viddec->waitOnBufTab        = NULL;

    viddec->hInFifo             = NULL;

    viddec->waitOnQueueThread   = NULL;
    viddec->waitQueueSize       = 0;

    viddec->framerateNum        = 0;
    viddec->framerateDen        = 0;

    viddec->numOutputBufs       = 0UL;
    viddec->hOutBufTab          = NULL;
    viddec->circBuf             = NULL;

    viddec->sps_pps_data        = NULL;
    viddec->nal_code_prefix     = NULL;
    viddec->nal_length          = 0;

    gst_tividdec_init_env(viddec);
}


/******************************************************************************
 * gst_tividdec_set_property
 *     Set element properties when requested.
 ******************************************************************************/
static void gst_tividdec_set_property(GObject *object, guint prop_id,
                const GValue *value, GParamSpec *pspec)
{
    GstTIViddec *viddec = GST_TIVIDDEC(object);

    GST_LOG("begin set_property\n");

    switch (prop_id) {
        case PROP_ENGINE_NAME:
            if (viddec->engineName) {
                g_free((gpointer)viddec->engineName);
            }
            viddec->engineName =
                (gchar*)g_malloc(strlen(g_value_get_string(value)) + 1);
            strcpy((gchar *)viddec->engineName, g_value_get_string(value));
            GST_LOG("setting \"engineName\" to \"%s\"\n", viddec->engineName);
            break;
        case PROP_CODEC_NAME:
            if (viddec->codecName) {
                g_free((gpointer)viddec->codecName);
            }
            viddec->codecName =
                (gchar*)g_malloc(strlen(g_value_get_string(value)) + 1);
            strcpy((gchar*)viddec->codecName, g_value_get_string(value));
            GST_LOG("setting \"codecName\" to \"%s\"\n", viddec->codecName);
            break;
        case PROP_NUM_OUTPUT_BUFS:
            viddec->numOutputBufs = g_value_get_int(value);
            GST_LOG("setting \"numOutputBufs\" to \"%ld\"\n",
                viddec->numOutputBufs);
            break;
        case PROP_FRAMERATE:
        {
            viddec->framerateNum = g_value_get_int(value);
            viddec->framerateDen = 1;

            /* If 30fps was specified, use 29.97 */
            if (viddec->framerateNum == 30) {
                viddec->framerateNum = 30000;
                viddec->framerateDen = 1001;
            }

            GST_LOG("setting \"frameRate\" to \"%2.2lf\"\n",
                (gdouble)viddec->framerateNum / (gdouble)viddec->framerateDen);
            break;
        }
        case PROP_DISPLAY_BUFFER:
            viddec->displayBuffer = g_value_get_boolean(value);
            GST_LOG("setting \"displayBuffer\" to \"%s\"\n",
                viddec->displayBuffer ? "TRUE" : "FALSE");
            break;
        case PROP_GEN_TIMESTAMPS:
            viddec->genTimeStamps = g_value_get_boolean(value);
            GST_LOG("setting \"genTimeStamps\" to \"%s\"\n",
                viddec->genTimeStamps ? "TRUE" : "FALSE");
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }

    GST_LOG("end set_property\n");
}

/******************************************************************************
 * gst_tividdec_get_property
 *     Return values for requested element property.
 ******************************************************************************/
static void gst_tividdec_get_property(GObject *object, guint prop_id,
                GValue *value, GParamSpec *pspec)
{
    GstTIViddec *viddec = GST_TIVIDDEC(object);

    GST_LOG("begin get_property\n");

    switch (prop_id) {
        case PROP_ENGINE_NAME:
            g_value_set_string(value, viddec->engineName);
            break;
        case PROP_CODEC_NAME:
            g_value_set_string(value, viddec->codecName);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }

    GST_LOG("end get_property\n");
}


/******************************************************************************
 * gst_tividdec_set_sink_caps
 *     Negotiate our sink pad capabilities.
 ******************************************************************************/
static gboolean gst_tividdec_set_sink_caps(GstPad *pad, GstCaps *caps)
{
    GstTIViddec  *viddec;
    GstStructure *capStruct;
    const gchar  *mime;
    GstTICodec   *codec = NULL;

    viddec    = GST_TIVIDDEC(gst_pad_get_parent(pad));
    capStruct = gst_caps_get_structure(caps, 0);
    mime      = gst_structure_get_name(capStruct);

    GST_INFO("requested sink caps:  %s", gst_caps_to_string(caps));

    /* Generic Video Properties */
    if (!strncmp(mime, "video/", 6)) {
        gint  framerateNum;
        gint  framerateDen;

        if (gst_structure_get_fraction(capStruct, "framerate", &framerateNum,
                &framerateDen)) {
            viddec->framerateNum = framerateNum;
            viddec->framerateDen = framerateDen;
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
            gst_object_unref(viddec);
            return FALSE;
        }

        if (mpegversion == 2) {
            codec = gst_ticodec_get_codec("MPEG2 Video Decoder");
        }
        else if (mpegversion == 4) {
            codec = gst_ticodec_get_codec("MPEG4 Video Decoder");
        }
        else {
            gst_object_unref(viddec);
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
        gst_object_unref(viddec);
        return FALSE;
    }

    /* Report if the required codec was not found */
    if (!codec) {
        GST_ERROR("unable to find codec needed for stream");
        gst_object_unref(viddec);
        return FALSE;
    }

    /* Shut-down any running video decoder */
    if (!gst_tividdec_exit_video(viddec)) {
        gst_object_unref(viddec);
        return FALSE;
    }

    /* Configure the element to use the detected engine name and codec, unless
     * they have been using the set_property function.
     */
    if (!viddec->engineName) {
        viddec->engineName = codec->CE_EngineName;
    }
    if (!viddec->codecName) {
        viddec->codecName = codec->CE_CodecName;
    }

    gst_object_unref(viddec);

    GST_LOG("sink caps negotiation successful\n");
    return TRUE;
}


/******************************************************************************
 * gst_tividdec_set_source_caps
 *     Negotiate our source pad capabilities.
 ******************************************************************************/
static gboolean gst_tividdec_set_source_caps(
                    GstTIViddec *viddec, Buffer_Handle hBuf)
{
    BufferGfx_Dimensions  dim;
    GstCaps              *caps;
    gboolean              ret;
    GstPad               *pad;
    char                 *string;

    pad = viddec->srcpad;

    /* Create a UYVY caps object using the dimensions from the given buffer */
    BufferGfx_getDimensions(hBuf, &dim);

    caps =
        gst_caps_new_simple("video/x-raw-yuv",
            "format",    GST_TYPE_FOURCC,   GST_MAKE_FOURCC('U','Y','V','Y'),
            "framerate", GST_TYPE_FRACTION, viddec->framerateNum,
                                            viddec->framerateDen,
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
 * gst_tividdec_sink_event
 *     Perform event processing on the input stream.  At the moment, this
 *     function isn't needed as this element doesn't currently perform any
 *     specialized event processing.  We'll leave it in for now in case we need
 *     it later on.
 ******************************************************************************/
static gboolean gst_tividdec_sink_event(GstPad *pad, GstEvent *event)
{
    GstTIViddec *viddec;
    gboolean     ret;

    viddec = GST_TIVIDDEC(GST_OBJECT_PARENT(pad));

    GST_DEBUG("pad \"%s\" received:  %s\n", GST_PAD_NAME(pad),
        GST_EVENT_TYPE_NAME(event));

    switch (GST_EVENT_TYPE(event)) {

        case GST_EVENT_NEWSEGMENT:
            /* maybe save and/or update the current segment (e.g. for output
             * clipping) or convert the event into one in a different format
             * (e.g. BYTES to TIME) or drop it and set a flag to send a
             * newsegment event in a different format later
             */
            ret = gst_pad_push_event(viddec->srcpad, event);
            break;

        case GST_EVENT_EOS:
            /* end-of-stream: process any remaining encoded frame data */
            GST_LOG("no more input; draining remaining encoded video data\n");

            if (!viddec->drainingEOS) {
               gst_tividdec_drain_pipeline(viddec);
             }

            /* Propagate EOS to downstream elements */
            ret = gst_pad_push_event(viddec->srcpad, event);
            break;

        case GST_EVENT_FLUSH_STOP:
            ret = gst_pad_push_event(viddec->srcpad, event);
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
 * gst_tividdec_chain
 *    This is the main processing routine.  This function receives a buffer
 *    from the sink pad, processes it, and pushes the result to the source
 *    pad.
 ******************************************************************************/
static GstFlowReturn gst_tividdec_chain(GstPad * pad, GstBuffer * buf)
{
    GstTIViddec *viddec = GST_TIVIDDEC(GST_OBJECT_PARENT(pad));
    gboolean     checkResult;

    /* If any thread aborted, communicate it to the pipeline */
    if (gst_tithread_check_status(viddec, TIThread_ANY_ABORTED, checkResult)) {
       gst_buffer_unref(buf);
       return GST_FLOW_UNEXPECTED;
    }

    /* If our engine handle is currently NULL, then either this is our first
     * buffer or the upstream element has re-negotiated our capabilities which
     * resulted in our engine being closed.  In either case, we need to
     * initialize (or re-initialize) our video decoder to handle the new
     * stream.
     */
    if (viddec->hEngine == NULL) {
        if (!gst_tividdec_init_video(viddec)) {
            GST_ERROR("unable to initialize video\n");
            gst_buffer_unref(buf);
            return GST_FLOW_UNEXPECTED;
        }
            
        /* check if we have recieved buffer from qtdemuxer. To do this,
         * we will verify if codec_data field has a valid avcC header.
         */
        if (gst_is_h264_decoder(viddec->codecName) && 
                gst_h264_valid_quicktime_header(buf)) {
            viddec->nal_length = gst_h264_get_nal_length(buf);
            viddec->sps_pps_data = gst_h264_get_sps_pps_data(buf);
            viddec->nal_code_prefix = gst_h264_get_nal_prefix_code();
        }

        GST_TICIRCBUFFER_TIMESTAMP(viddec->circBuf) =
            GST_CLOCK_TIME_IS_VALID(GST_BUFFER_TIMESTAMP(buf)) ?
            GST_BUFFER_TIMESTAMP(buf) : 0ULL;
    }

    /* Don't queue up too many buffers -- if we collect too many input buffers
     * without consuming them we'll run out of memory.  Once we reach a
     * threshold, block until the queue thread removes some buffers.
     */
    Rendezvous_reset(viddec->waitOnQueueThread);
    if (Fifo_getNumEntries(viddec->hInFifo) > 500) {
        gst_tividdec_wait_on_queue_thread(viddec, 400);
    }

    /* If demuxer has passed SPS and PPS NAL unit dump in codec_data field,
     * then we have a packetized h264 stream. We need to transform this stream
     * into byte-stream.
     */
    if (viddec->sps_pps_data) {
        if (gst_h264_parse_and_fifo_put(viddec->hInFifo, buf, 
                viddec->sps_pps_data, viddec->nal_code_prefix,
                viddec->nal_length) < 0) {
            GST_ERROR("Failed to send buffer to queue thread\n");
            gst_buffer_unref(buf);
            return GST_FLOW_UNEXPECTED;
        }
    }
    else {    
        /* Queue up the encoded data stream into a circular buffer */
        if (Fifo_put(viddec->hInFifo, buf) < 0) {
            GST_ERROR("Failed to send buffer to queue thread\n");
            gst_buffer_unref(buf);
            return GST_FLOW_UNEXPECTED;
        }
    }
    return GST_FLOW_OK;
}


/******************************************************************************
 * gst_tividdec_init_video
 *     Initialize or re-initializes the video stream
 ******************************************************************************/
static gboolean gst_tividdec_init_video(GstTIViddec *viddec)
{
    Rendezvous_Attrs      rzvAttrs  = Rendezvous_Attrs_DEFAULT;
    struct sched_param    schedParam;
    pthread_attr_t        attr;
    Fifo_Attrs            fAttrs    = Fifo_Attrs_DEFAULT;

    GST_LOG("begin init_video\n");

    /* Set up the queue fifo */
    viddec->hInFifo = Fifo_create(&fAttrs);

    /* If video has already been initialized, shut down previous decoder */
    if (viddec->hEngine) {
        if (!gst_tividdec_exit_video(viddec)) {
            GST_ERROR("failed to shut down existing video decoder\n");
            return FALSE;
        }
    }

    /* Make sure we know what codec we're using */
    if (!viddec->engineName) {
        GST_ERROR("engine name not specified\n");
        return FALSE;
    }

    if (!viddec->codecName) {
        GST_ERROR("codec name not specified\n");
        return FALSE;
    }

    /* Initialize thread status management */
    viddec->threadStatus = 0UL;
    pthread_mutex_init(&viddec->threadStatusMutex, NULL);

    /* Initialize rendezvous objects for making threads wait on conditions */
    viddec->waitOnDecodeDrain   = Rendezvous_create(100, &rzvAttrs);
    viddec->waitOnQueueThread   = Rendezvous_create(100, &rzvAttrs);
    viddec->waitOnDecodeThread  = Rendezvous_create(2, &rzvAttrs);
    viddec->waitOnBufTab        = Rendezvous_create(100, &rzvAttrs);
    viddec->drainingEOS         = FALSE;

    /* Initialize custom thread attributes */
    if (pthread_attr_init(&attr)) {
        GST_WARNING("failed to initialize thread attrs\n");
        gst_tividdec_exit_video(viddec);
        return FALSE;
    }

    /* Force the thread to use the system scope */
    if (pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM)) {
        GST_WARNING("failed to set scope attribute\n");
        gst_tividdec_exit_video(viddec);
        return FALSE;
    }

    /* Force the thread to use custom scheduling attributes */
    if (pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED)) {
        GST_WARNING("failed to set schedule inheritance attribute\n");
        gst_tividdec_exit_video(viddec);
        return FALSE;
    }

    /* Set the thread to be fifo real time scheduled */
    if (pthread_attr_setschedpolicy(&attr, SCHED_FIFO)) {
        GST_WARNING("failed to set FIFO scheduling policy\n");
        gst_tividdec_exit_video(viddec);
        return FALSE;
    }

    /* Set the display thread priority */
    schedParam.sched_priority = GstTIVideoThreadPriority;
    if (pthread_attr_setschedparam(&attr, &schedParam)) {
        GST_WARNING("failed to set scheduler parameters\n");
        return FALSE;
    }

    /* Create decoder thread */
    if (pthread_create(&viddec->decodeThread, &attr,
            gst_tividdec_decode_thread, (void*)viddec)) {
        GST_ERROR("failed to create decode thread\n");
        gst_tividdec_exit_video(viddec);
        return FALSE;
    }
    gst_tithread_set_status(viddec, TIThread_DECODE_CREATED);

    /* Wait for decoder thread to finish initilization before creating queue
     * thread.
     */
    Rendezvous_meet(viddec->waitOnDecodeThread);

    /* Make sure circular buffer and display buffers are created by decoder
     * thread.
     */
    if (viddec->circBuf == NULL || viddec->hOutBufTab == NULL) {
        GST_LOG("decode thread failed to create circular or display buffer"
                " handles\n");
        return FALSE;
    }

    /* Create queue thread */
    if (pthread_create(&viddec->queueThread, NULL,
            gst_tividdec_queue_thread, (void*)viddec)) {
        GST_ERROR("failed to create queue thread\n");
        gst_tividdec_exit_video(viddec);
        return FALSE;
    }
    gst_tithread_set_status(viddec, TIThread_QUEUE_CREATED);

    GST_LOG("end init_video\n");
    return TRUE;
}


/******************************************************************************
 * gst_tividdec_exit_video
 *    Shut down any running video decoder, and reset the element state.
 ******************************************************************************/
static gboolean gst_tividdec_exit_video(GstTIViddec *viddec)
{
    void*    thread_ret;
    gboolean checkResult;

    GST_LOG("begin exit_video\n");

    /* Drain the pipeline if it hasn't already been drained */
    if (!viddec->drainingEOS) {
       gst_tividdec_drain_pipeline(viddec);
     }

    /* Shut down the decode thread */
    if (gst_tithread_check_status(
            viddec, TIThread_DECODE_CREATED, checkResult)) {
        GST_LOG("shutting down decode thread\n");

        if (pthread_join(viddec->decodeThread, &thread_ret) == 0) {
            if (thread_ret == GstTIThreadFailure) {
                GST_DEBUG("decode thread exited with an error condition\n");
            }
        }
    }

    /* Shut down the queue thread */
    if (gst_tithread_check_status(
            viddec, TIThread_QUEUE_CREATED, checkResult)) {
        GST_LOG("shutting down queue thread\n");

        /* Unstop the queue thread if needed, and wait for it to finish */
        /* Push the gst_ti_flush_fifo buffer to let the queue thread know
         * when the Fifo has finished draining.  If the Fifo is currently
         * empty when we get to this point, then pushing this buffer will
         * also unblock the encode/decode thread if it is currently blocked
         * on a Fifo_get().  Our first thought was to use DMAI's Fifo_flush()
         * routine here, but this method assumes the Fifo to be empty and
         * will leak any buffer still in the Fifo.
         */
        if (Fifo_put(viddec->hInFifo,&gst_ti_flush_fifo) < 0) {
            GST_ERROR("Could not put flush value to Fifo\n");
        }

        if (pthread_join(viddec->queueThread, &thread_ret) == 0) {
            if (thread_ret == GstTIThreadFailure) {
                GST_DEBUG("queue thread exited with an error condition\n");
            }
        }
    }

    /* Shut down thread status management */
    viddec->threadStatus = 0UL;
    pthread_mutex_destroy(&viddec->threadStatusMutex);

    /* Shut-down any remaining items */
    if (viddec->hInFifo) {
        Fifo_delete(viddec->hInFifo);
        viddec->hInFifo = NULL;
    }

    if (viddec->waitOnQueueThread) {
        Rendezvous_delete(viddec->waitOnQueueThread);
        viddec->waitOnQueueThread = NULL;
    }

    if (viddec->waitOnDecodeDrain) {
        Rendezvous_delete(viddec->waitOnDecodeDrain);
        viddec->waitOnDecodeDrain = NULL;
    }

    if (viddec->waitOnDecodeThread) {
        Rendezvous_delete(viddec->waitOnDecodeThread);
        viddec->waitOnDecodeThread = NULL;
    }

    if (viddec->waitOnBufTab) {
        Rendezvous_delete(viddec->waitOnBufTab);
        viddec->waitOnBufTab = NULL;
    }

    if (viddec->circBuf) {
        GST_LOG("freeing cicrular input buffer\n");
        gst_ticircbuffer_unref(viddec->circBuf);
        viddec->circBuf      = NULL;
        viddec->framerateNum = 0;
        viddec->framerateDen = 0;
    }

    if (viddec->hOutBufTab) {
        GST_LOG("freeing output buffers\n");
        BufTab_delete(viddec->hOutBufTab);
        viddec->hOutBufTab = NULL;
    }

    if (viddec->sps_pps_data) {
        GST_LOG("freeing sps_pps buffers\n");
        gst_buffer_unref(viddec->sps_pps_data);
        viddec->sps_pps_data = NULL;
    }

    if (viddec->nal_code_prefix) {
        GST_LOG("freeing nal code prefix buffers\n");
        gst_buffer_unref(viddec->nal_code_prefix);
        viddec->nal_code_prefix = NULL;
    }

    if (viddec->nal_length) {
        GST_LOG("reseting nal length to zero\n");
        viddec->nal_length = 0;
    }

    GST_LOG("end exit_video\n");
    return TRUE;
}


/******************************************************************************
 * gst_tividdec_change_state
 *     Manage state changes for the video stream.  The gStreamer documentation
 *     states that state changes must be handled in this manner:
 *        1) Handle ramp-up states
 *        2) Pass state change to base class
 *        3) Handle ramp-down states
 ******************************************************************************/
static GstStateChangeReturn gst_tividdec_change_state(GstElement *element,
                                GstStateChange transition)
{
    GstStateChangeReturn  ret    = GST_STATE_CHANGE_SUCCESS;
    GstTIViddec          *viddec = GST_TIVIDDEC(element);

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
            if (!gst_tividdec_exit_video(viddec)) {
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
 * gst_tividdec2_codec_start
 *     Initialize codec engine
 *****************************************************************************/
static gboolean gst_tividdec_codec_start (GstTIViddec  *viddec)
{
    VIDDEC_Params         params    = Vdec_Params_DEFAULT;
    VIDDEC_DynamicParams  dynParams = Vdec_DynamicParams_DEFAULT;
    BufferGfx_Attrs       gfxAttrs  = BufferGfx_Attrs_DEFAULT;

    /* Open the codec engine */
    GST_LOG("opening codec engine \"%s\"\n", viddec->engineName);
    viddec->hEngine = Engine_open((Char *) viddec->engineName, NULL, NULL);

    if (viddec->hEngine == NULL) {
        GST_ERROR("failed to open codec engine \"%s\"\n", viddec->engineName);
        return FALSE;
    }

    /* Initialize video decoder */
    params.forceChromaFormat = XDM_YUV_422ILE;
    params.maxWidth          = VideoStd_D1_WIDTH;
    params.maxHeight         = VideoStd_D1_PAL_HEIGHT;

    GST_LOG("opening video decoder \"%s\"\n", viddec->codecName);
    viddec->hVd = Vdec_create(viddec->hEngine, (Char*)viddec->codecName,
                      &params, &dynParams);

    if (viddec->hVd == NULL) {
        GST_ERROR("failed to create video decoder: %s\n", viddec->codecName);
        GST_LOG("closing codec engine\n");
        return FALSE;
    }

    /* Create a circular input buffer */
    viddec->circBuf =
        gst_ticircbuffer_new(Vdec_getInBufSize(viddec->hVd), 3, FALSE);

    if (viddec->circBuf == NULL) {
        GST_ERROR("failed to create circular input buffer\n");
        return FALSE;
    }

    /* Display buffer contents if displayBuffer=TRUE was specified */
    gst_ticircbuffer_set_display(viddec->circBuf, viddec->displayBuffer);

    /* Define the number of display buffers to allocate.  This number must be
     * at least 2, but should be more if codecs don't return a display buffer
     * after every process call.  If this has not been set via set_property(),
     * default to 3 output buffers.
     */
    if (viddec->numOutputBufs == 0) {
        viddec->numOutputBufs = 3;
    }

    /* Create codec output buffers */
    GST_LOG("creating output buffer table\n");
    gfxAttrs.colorSpace     = ColorSpace_UYVY;
    gfxAttrs.dim.width      = params.maxWidth;
    gfxAttrs.dim.height     = params.maxHeight;
    gfxAttrs.dim.lineLength = BufferGfx_calcLineLength(
                                  gfxAttrs.dim.width, gfxAttrs.colorSpace);
    gfxAttrs.bAttrs.useMask = gst_tidmaibuffertransport_GST_FREE;

    viddec->hOutBufTab =
        BufTab_create(viddec->numOutputBufs, Vdec_getOutBufSize(viddec->hVd),
            BufferGfx_getBufferAttrs(&gfxAttrs));

    if (viddec->hOutBufTab == NULL) {
        GST_ERROR("failed to create output buffers\n");
        return FALSE;
    }

    /* Tell the Vdec module that hOutBufTab will be used for display buffers */
    Vdec_setBufTab(viddec->hVd, viddec->hOutBufTab);

    return TRUE;
}

/******************************************************************************
 * gst_tividdec2_codec_stop
 *     Release codec engine resources
 *****************************************************************************/
static gboolean gst_tividdec_codec_stop (GstTIViddec  *viddec)
{
    /* Shut down remaining items */
    if (viddec->hVd) {
        GST_LOG("closing video decoder\n");
        Vdec_delete(viddec->hVd);
        viddec->hVd = NULL;
    }

    if (viddec->hEngine) {
        GST_LOG("closing codec engine\n");
        Engine_close(viddec->hEngine);
        viddec->hEngine = NULL;
    }

    return TRUE;
}

/******************************************************************************
 * gst_tividdec_decode_thread
 *     Call the video codec to process a full input buffer
 ******************************************************************************/
static void* gst_tividdec_decode_thread(void *arg)
{
    GstTIViddec   *viddec         = GST_TIVIDDEC(gst_object_ref(arg));
    GstBuffer     *encDataWindow  = NULL;
    Buffer_Attrs   bAttrs         = Buffer_Attrs_DEFAULT;
    gboolean       codecFlushed   = FALSE;
    void          *threadRet      = GstTIThreadSuccess;
    Buffer_Handle  hDummyInputBuf = NULL;
    Buffer_Handle  hDstBuf;
    Int32          encDataConsumed;
    GstClockTime   encDataTime;
    GstClockTime   frameDuration;
    Buffer_Handle  hEncDataWindow;
    GstBuffer     *outBuf;
    Int            ret;

    GST_LOG("init video decode_thread \n");

    /* Initialize codec engine */
    ret = gst_tividdec_codec_start(viddec);

    /* Notify main thread if it is waiting to create queue thread */
    Rendezvous_meet(viddec->waitOnDecodeThread);

    if (ret == FALSE) {
        GST_ERROR("failed to start codec\n");
        goto thread_exit;
    }

    /* Calculate the duration of a single frame in this stream */
    frameDuration = gst_tividdec_frame_duration(viddec);

    /* Main thread loop */
    while (TRUE) {

        /* Obtain an encoded data frame */
        encDataWindow  = gst_ticircbuffer_get_data(viddec->circBuf);
        encDataTime    = GST_BUFFER_TIMESTAMP(encDataWindow);
        hEncDataWindow = GST_TIDMAIBUFFERTRANSPORT_DMAIBUF(encDataWindow);

        /* If we received a data frame of zero size, there is no more data to
         * process -- exit the thread.  If we weren't told that we are
         * draining the pipeline, something is not right, so exit with an
         * error.
         */
        if (GST_BUFFER_SIZE(encDataWindow) == 0) {
            GST_LOG("no video data remains\n");
            if (!viddec->drainingEOS) {
                goto thread_failure;
            }

            /* When no input remains, we must flush any remaining display
             * frames out of the codec and push them to the sink.
             */
            if (!codecFlushed) {
                Vdec_flush(viddec->hVd);

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
        /* If we are not able to find free buffer from BufTab then decoder 
         * thread will be blocked on waitOnBufTab rendezvous. And this will be 
         * woke-up by dmaitransportbuffer finalize method.
         */
        hDstBuf = BufTab_getFreeBuf(viddec->hOutBufTab);
        if (hDstBuf == NULL) {
            GST_LOG("Failed to get free buffer, waiting on bufTab\n");
            Rendezvous_meet(viddec->waitOnBufTab);
            hDstBuf = BufTab_getFreeBuf(viddec->hOutBufTab);

            if (hDstBuf == NULL) {
                GST_ERROR("failed to get a free contiguous buffer"
                          " from BufTab\n");
                goto thread_failure;
            }
        }
        
        /* Reset waitOnBufTab rendezvous handle to its orignal state */
        Rendezvous_reset(viddec->waitOnBufTab);

        /* Make sure the whole buffer is used for output */
        BufferGfx_resetDimensions(hDstBuf);

        /* Invoke the video decoder */
        GST_LOG("invoking the video decoder\n");
        ret             = Vdec_process(viddec->hVd, hEncDataWindow, hDstBuf);
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
            GST_LOG("Vdec_process returned success code %d\n", ret); 
        }

        /* Release the reference buffer, and tell the circular buffer how much
         * data was consumed.
         */
        ret = gst_ticircbuffer_data_consumed(viddec->circBuf, encDataWindow,
                  encDataConsumed);
        encDataWindow = NULL;

        if (!ret) {
            goto thread_failure;
        }

        /* Obtain the display buffer returned by the codec (it may be a
         * different one than the one we passed it.
         */
        hDstBuf = Vdec_getDisplayBuf(viddec->hVd);

        /* If we were given back decoded frame, push it to the source pad */
        while (hDstBuf) {

            /* Set the source pad capabilities based on the decoded frame
             * properties.
             */
            gst_tividdec_set_source_caps(viddec, hDstBuf);

            /* Create a DMAI transport buffer object to carry a DMAI buffer to
             * the source pad.  The transport buffer knows how to release the
             * buffer for re-use in this element when the source pad calls
             * gst_buffer_unref().
             */
            outBuf = gst_tidmaibuffertransport_new(hDstBuf,
                                                    viddec->waitOnBufTab);
            gst_buffer_set_data(outBuf, GST_BUFFER_DATA(outBuf),
                 gst_ti_calculate_display_bufSize(hDstBuf));
            gst_buffer_set_caps(outBuf, GST_PAD_CAPS(viddec->srcpad));

            /* If we have a valid time stamp, set it on the buffer */
            if (viddec->genTimeStamps &&
                GST_CLOCK_TIME_IS_VALID(encDataTime)) {
                GST_LOG("video timestamp value: %llu\n", encDataTime);
                GST_BUFFER_TIMESTAMP(outBuf) = encDataTime;
                GST_BUFFER_DURATION(outBuf)  = frameDuration;
            }
            else {
                GST_BUFFER_TIMESTAMP(outBuf) = GST_CLOCK_TIME_NONE;
            }

            /* Tell circular buffer how much time we consumed */
            gst_ticircbuffer_time_consumed(viddec->circBuf, frameDuration);

            /* Push the transport buffer to the source pad */
            GST_LOG("pushing display buffer to source pad\n");

            if (gst_pad_push(viddec->srcpad, outBuf) != GST_FLOW_OK) {
                GST_DEBUG("push to source pad failed\n");
                goto thread_failure;
            }

            hDstBuf = Vdec_getDisplayBuf(viddec->hVd);
        }
    }

thread_failure:

    /* If encDataWindow is non-NULL, something bad happened before we had a
     * chance to release it.  Release it now so we don't block the pipeline.
     * We release it by telling the circular buffer that we're done with it and
     * consumed no data.
     */
    if (encDataWindow) {
        gst_ticircbuffer_data_consumed(viddec->circBuf, encDataWindow, 0);
    }

    gst_tithread_set_status(viddec, TIThread_DECODE_ABORTED);
    threadRet = GstTIThreadFailure;
    gst_ticircbuffer_consumer_aborted(viddec->circBuf);
    Rendezvous_force(viddec->waitOnQueueThread);

thread_exit:
    /* stop codec engine */
    if (gst_tividdec_codec_stop(viddec) < 0) {
        GST_ERROR("failed to stop codec\n");
    }

    /* Notify main thread if it is waiting on decode thread shut-down */
    viddec->decodeDrained = TRUE;
    Rendezvous_force(viddec->waitOnDecodeDrain);

    gst_object_unref(viddec);

    GST_LOG("exit video decode_thread (%d)\n", (int)threadRet);
    return threadRet;
}


/******************************************************************************
 * gst_tividdec_queue_thread 
 *     Add an input buffer to the circular buffer            
 ******************************************************************************/
static void* gst_tividdec_queue_thread(void *arg)
{
    GstTIViddec* viddec    = GST_TIVIDDEC(gst_object_ref(arg));
    void*        threadRet = GstTIThreadSuccess;
    GstBuffer*   encData;
    Int          fifoRet;

    while (TRUE) {

        /* Get the next input buffer (or block until one is ready) */
        fifoRet = Fifo_get(viddec->hInFifo, &encData);

        if (fifoRet < 0) {
            GST_ERROR("Failed to get buffer from input fifo\n");
            goto thread_failure;
        }

        if (encData == (GstBuffer *)(&gst_ti_flush_fifo)) {
            GST_DEBUG("Processed last input buffer from Fifo; exiting.\n");
            goto thread_exit;
        }

/* This code is if'ed out for now until more work has been done for state
 * transitions.  For now we do not want to print this message repeatedly
 * which will happen when flushing the fifo when the decode thread has
 * exited.
 */
        /* Send the buffer to the circular buffer */
        if (!gst_ticircbuffer_queue_data(viddec->circBuf, encData)) {
#if 0
            GST_ERROR("queue thread could not queue data\n");
            GST_ERROR("queue thread encData size = %d\n", GST_BUFFER_SIZE(encData));
            gst_buffer_unref(encData);
            goto thread_failure;
#else
            ; /* Do nothing */
#endif
        }

        /* Release the buffer we received from the sink pad */
        gst_buffer_unref(encData);

        /* If we've reached the EOS, start draining the circular buffer when
         * there are no more buffers in the FIFO.
         */
        if (viddec->drainingEOS && Fifo_getNumEntries(viddec->hInFifo) == 0) {
            gst_ticircbuffer_drain(viddec->circBuf, TRUE);
        }

        /* Unblock any pending puts to our Fifo if we have reached our
         * minimum threshold.
         */
        gst_tividdec_broadcast_queue_thread(viddec);
    }

thread_failure:
    gst_tithread_set_status(viddec, TIThread_QUEUE_ABORTED);
    threadRet = GstTIThreadFailure;

thread_exit:
    gst_object_unref(viddec);
    return threadRet;
}


/******************************************************************************
 * gst_tividdec_wait_on_queue_thread
 *    Wait for the queue thread to consume buffers from the fifo until
 *    there are only "waitQueueSize" items remaining.
 ******************************************************************************/
static void gst_tividdec_wait_on_queue_thread(GstTIViddec *viddec,
                Int32 waitQueueSize)
{
    viddec->waitQueueSize = waitQueueSize;
    Rendezvous_meet(viddec->waitOnQueueThread);
}


/******************************************************************************
 * gst_tividdec_broadcast_queue_thread
 *    Broadcast when queue thread has processed enough buffers from the
 *    fifo to unblock anyone waiting to queue some more.
 ******************************************************************************/
static void gst_tividdec_broadcast_queue_thread(GstTIViddec *viddec)
{
    if (viddec->waitQueueSize < Fifo_getNumEntries(viddec->hInFifo)) {
          return;
    } 
    Rendezvous_force(viddec->waitOnQueueThread);
}


/******************************************************************************
 * gst_tividdec_drain_pipeline
 *    Push any remaining input buffers through the queue and decode threads
 ******************************************************************************/
static void gst_tividdec_drain_pipeline(GstTIViddec *viddec)
{
    gboolean checkResult;

    viddec->drainingEOS = TRUE;

    /* If the processing threads haven't been created, there is nothing to
     * drain.
     */
    if (!gst_tithread_check_status(
             viddec, TIThread_DECODE_CREATED, checkResult)) {
        return;
    }
    if (!gst_tithread_check_status(
             viddec, TIThread_QUEUE_CREATED, checkResult)) {
        return;
    }

    /* If the queue fifo still has entries in it, it will drain the
     * circular buffer once all input buffers have been added to the
     * circular buffer.  If the fifo is already empty, we must drain
     * the circular buffer here.
     */
    if (Fifo_getNumEntries(viddec->hInFifo) == 0) {
        gst_ticircbuffer_drain(viddec->circBuf, TRUE);
    }
    else {
        Rendezvous_force(viddec->waitOnQueueThread);
    }

    /* Wait for the decoder to drain */
    if (!viddec->decodeDrained) {
        Rendezvous_meet(viddec->waitOnDecodeDrain);
    }
    viddec->decodeDrained = FALSE;

}


/******************************************************************************
 * gst_tividdec_frame_duration
 *    Return the duration of a single frame in nanoseconds.
 ******************************************************************************/
static GstClockTime gst_tividdec_frame_duration(GstTIViddec *viddec)
{
    /* Default to 29.97 if the frame rate was not specified */
    if (viddec->framerateNum == 0 && viddec->framerateDen == 0) {
        GST_WARNING("framerate not specified; using 29.97fps");
        viddec->framerateNum = 30000;
        viddec->framerateDen = 1001;
    }

    return (GstClockTime)
        ((1 / ((gdouble)viddec->framerateNum/(gdouble)viddec->framerateDen))
         * GST_SECOND);
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
