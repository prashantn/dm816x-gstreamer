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
#include "gsttiquicktime_mpeg4.h"

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

/* Constants */
#define gst_tividdec_CODEC_FREE 0x2

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
static void
    gst_tividdec_dispose(GObject * object);
static gboolean 
    gst_tividdec_set_query_pad(GstPad * pad, GstQuery * query);

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
    gobject_class->dispose      = GST_DEBUG_FUNCPTR(gst_tividdec_dispose);

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
 * gst_tividdec_dispose
 *****************************************************************************/
static void gst_tividdec_dispose(GObject * object)
{
    GstTIViddec *viddec = GST_TIVIDDEC(object);

    if (viddec->segment) {
        gst_segment_free(viddec->segment);
        viddec->segment = NULL;
    }

    G_OBJECT_CLASS(parent_class)->dispose (object);
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
    gst_pad_set_query_function(viddec->srcpad,
            GST_DEBUG_FUNCPTR(gst_tividdec_set_query_pad));

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

    viddec->waitOnDecodeThread  = NULL;
    viddec->waitOnDecodeDrain   = NULL;
    viddec->waitOnBufTab        = NULL;

    viddec->framerateNum        = 0;
    viddec->framerateDen        = 0;

    viddec->numOutputBufs       = 0UL;
    viddec->hOutBufTab          = NULL;
    viddec->circBuf             = NULL;

    viddec->sps_pps_data        = NULL;
    viddec->nal_code_prefix     = NULL;
    viddec->nal_length          = 0;

    viddec->segment             = gst_segment_new();
    viddec->totalDuration       = 0;
    viddec->totalBytes          = 0;

    viddec->mpeg4_quicktime_header = NULL;

    gst_tividdec_init_env(viddec);
}

/******************************************************************************
 * gst_tiauddec_set_query_pad
 *   This function reuturn stream duration and position.
 *****************************************************************************/
static gboolean gst_tividdec_set_query_pad(GstPad *pad, GstQuery *query)
{
    GstTIViddec  *viddec;
    gboolean     ret = FALSE;

    viddec    = GST_TIVIDDEC(gst_pad_get_parent(pad));
   
    ret = gst_ti_query_srcpad(pad, query, viddec->sinkpad, 
             viddec->totalDuration, viddec->totalBytes);

    gst_object_unref(viddec);

    return ret;
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
        GST_ELEMENT_ERROR(viddec, STREAM, NOT_IMPLEMENTED,
        ("stream type not supported"), (NULL));
        gst_object_unref(viddec);
        return FALSE;
    }

    /* Report if the required codec was not found */
    if (!codec) {
        GST_ELEMENT_ERROR(viddec, STREAM, CODEC_NOT_FOUND,
        ("unable to find codec needed for stream"), (NULL));
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
            /* if event format is byte then convert in time format */
            gst_ti_parse_newsegment(&event, viddec->segment, 
                &viddec->totalDuration, viddec->totalBytes);

            /* Propagate NEWSEGMENT to downstream elements */
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
 * gst_tividdec_populate_codec_header
 *  This function parses codec_data field to get addition H.264/MPEG-4 header 
 *  information.
 *****************************************************************************/
static gboolean gst_tividdec_populate_codec_header (GstTIViddec *viddec, 
    GstBuffer *buf)
{
    GstCaps      *caps = GST_BUFFER_CAPS(buf);

    if (!caps) {
        return FALSE;
    }

    /* If it's H.264 codec then get the sps, pps and nal headers */
    if (gst_is_h264_decoder(viddec->codecName) && 
            gst_h264_valid_quicktime_header(buf)) {
        GST_LOG("Parsing codec data to get SPS, PPS and NAL headers");
        viddec->nal_length = gst_h264_get_nal_length(buf);
        viddec->sps_pps_data = gst_h264_get_sps_pps_data(buf);
        viddec->nal_code_prefix = gst_h264_get_nal_prefix_code();
    }

    if (gst_is_mpeg4_decoder(viddec->codecName) && 
            gst_mpeg4_valid_quicktime_header(buf)) {
        GST_LOG("found MPEG4 quicktime header \n");
        viddec->mpeg4_quicktime_header = gst_mpeg4_get_header(buf);
    }

    return TRUE;
}

/******************************************************************************
 * gst_tividdec_parse_and_queue_buffer
 *  If needed then this function will parse the input buffer before putting
 *  in circular buffer.
 *****************************************************************************/
static gboolean gst_tividdec_parse_and_queue_buffer(GstTIViddec *viddec,
    GstBuffer *buf)
{
    if (viddec->sps_pps_data) {
        /* If demuxer has passed SPS and PPS NAL unit dump in codec_data field,
         * then we have a packetized h264 stream. We need to transform this 
         * stream into byte-stream.
         */
        if (gst_h264_parse_and_queue(viddec->circBuf, buf, 
                viddec->sps_pps_data, viddec->nal_code_prefix,
                viddec->nal_length) < 0) {
            GST_ELEMENT_ERROR(viddec, RESOURCE, WRITE,
            ("Failed to queue input buffer into circular buffer\n"), (NULL));
            return FALSE;
        }
    }
    else if (viddec->mpeg4_quicktime_header) {
        /* If demuxer has passed codec_data field then we need to prefix this
         * codec data in input stream.
         */
        if (gst_mpeg4_parse_and_queue(viddec->circBuf, buf, 
                viddec->mpeg4_quicktime_header) < 0) {
            GST_ELEMENT_ERROR(viddec, RESOURCE, WRITE,
            ("Failed to queue input buffer into circular buffer\n"), (NULL));
            return FALSE;
        }
    }
    else {
        /* Queue up the encoded data stream into a circular buffer */
        if (!gst_ticircbuffer_queue_data(viddec->circBuf, buf)) {
            GST_ELEMENT_ERROR(viddec, RESOURCE, WRITE,
            ("Failed to queue input buffer into circular buffer\n"), (NULL));
            return FALSE;
        }
    }

    return TRUE;
}

/******************************************************************************
 * gst_tividdec_chain
 *    This is the main processing routine.  This function receives a buffer
 *    from the sink pad, processes it, and pushes the result to the source
 *    pad.
 ******************************************************************************/
static GstFlowReturn gst_tividdec_chain(GstPad * pad, GstBuffer * buf)
{
    GstTIViddec   *viddec = GST_TIVIDDEC(GST_OBJECT_PARENT(pad));
    GstFlowReturn  flow   = GST_FLOW_OK;
    gboolean       checkResult;

    /* If the decode thread aborted, signal it to let it know it's ok to
     * shut down, and communicate the failure to the pipeline.
     */
    if (gst_tithread_check_status(viddec, TIThread_CODEC_ABORTED,
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
    if (viddec->hEngine == NULL) {
        if (!gst_tividdec_init_video(viddec)) {
            GST_ELEMENT_ERROR(viddec, RESOURCE, FAILED,
            ("unable to initialize video\n"), (NULL));
            flow = GST_FLOW_UNEXPECTED;
            goto exit;
        }
            
        /* Populate extra codec headers */
        if (gst_tividdec_populate_codec_header(viddec, buf)) {
            GST_LOG("Found extra header information for %s", viddec->codecName);
        }

        GST_TICIRCBUFFER_TIMESTAMP(viddec->circBuf) =
            GST_CLOCK_TIME_IS_VALID(GST_BUFFER_TIMESTAMP(buf)) ?
            GST_BUFFER_TIMESTAMP(buf) : 0ULL;
    }

    /* Parse and queue the encoded data stream into a circular buffer */
    if (!gst_tividdec_parse_and_queue_buffer(viddec, buf)) {
        GST_ELEMENT_ERROR(viddec, RESOURCE, WRITE,
        ("Failed to queue input buffer into circular buffer\n"), (NULL));
        flow = GST_FLOW_UNEXPECTED;
        goto exit;
    }

exit:
    gst_buffer_unref(buf);
    return flow;
}


/******************************************************************************
 * gst_tividdec_init_video
 *     Initialize or re-initializes the video stream
 ******************************************************************************/
static gboolean gst_tividdec_init_video(GstTIViddec *viddec)
{
    Rendezvous_Attrs    rzvAttrs = Rendezvous_Attrs_DEFAULT;
    struct sched_param  schedParam;
    pthread_attr_t      attr;

    GST_LOG("begin init_video\n");

    /* If video has already been initialized, shut down previous decoder */
    if (viddec->hEngine) {
        if (!gst_tividdec_exit_video(viddec)) {
            GST_ELEMENT_ERROR(viddec, RESOURCE, FAILED,
            ("failed to shut down existing video decoder\n"), (NULL));
            return FALSE;
        }
    }

    /* Make sure we know what codec we're using */
    if (!viddec->engineName) {
        GST_ELEMENT_ERROR(viddec, RESOURCE, FAILED,
        ("engine name not specified\n"), (NULL));
        return FALSE;
    }

    if (!viddec->codecName) {
        GST_ELEMENT_ERROR(viddec, RESOURCE, FAILED,
        ("codec name not specified\n"), (NULL));
        return FALSE;
    }

    /* Initialize thread status management */
    viddec->threadStatus = 0UL;
    pthread_mutex_init(&viddec->threadStatusMutex, NULL);

    /* Initialize rendezvous objects for making threads wait on conditions */
    viddec->waitOnDecodeThread  = Rendezvous_create(2, &rzvAttrs);
    viddec->waitOnDecodeDrain   = Rendezvous_create(100, &rzvAttrs);
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
        GST_ELEMENT_ERROR(viddec, RESOURCE, FAILED,
        ("failed to create decode thread\n"), (NULL));
        gst_tividdec_exit_video(viddec);
        return FALSE;
    }
    gst_tithread_set_status(viddec, TIThread_CODEC_CREATED);

    /* Destroy the custom thread attributes */
    if (pthread_attr_destroy(&attr)) {
        GST_WARNING("failed to destroy thread attrs\n");
        gst_tividdec_exit_video(viddec);
        return FALSE;
    }

    /* Make sure circular buffer and display buffers are created by decoder
     * thread.
     */
    Rendezvous_meet(viddec->waitOnDecodeThread);

    if (viddec->circBuf == NULL || viddec->hOutBufTab == NULL) {
        GST_ELEMENT_ERROR(viddec, RESOURCE, FAILED,
        ("decode thread failed to create circular or display buffer handles\n"),
        (NULL));
        return FALSE;
    }

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
            viddec, TIThread_CODEC_CREATED, checkResult)) {
        GST_LOG("shutting down decode thread\n");

        Rendezvous_force(viddec->waitOnDecodeThread);
        if (pthread_join(viddec->decodeThread, &thread_ret) == 0) {
            if (thread_ret == GstTIThreadFailure) {
                GST_DEBUG("decode thread exited with an error condition\n");
            }
        }
    }

    /* Shut down thread status management */
    viddec->threadStatus = 0UL;
    pthread_mutex_destroy(&viddec->threadStatusMutex);

    /* Shut-down any remaining items */
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

    if (viddec->mpeg4_quicktime_header) {
        GST_LOG("reseting quicktime mpeg4 header to NULL\n");
        viddec->mpeg4_quicktime_header = NULL;
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

        case GST_STATE_CHANGE_READY_TO_PAUSED:
            gst_segment_init (viddec->segment, GST_FORMAT_TIME);
            break;

        case GST_STATE_CHANGE_PAUSED_TO_READY:
            viddec->totalBytes       = 0;
            viddec->totalDuration    = 0;
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
        GST_ELEMENT_ERROR(viddec, RESOURCE, FAILED,
        ("failed to open codec engine \"%s\"\n", viddec->engineName), (NULL));
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
        GST_ELEMENT_ERROR(viddec, STREAM, CODEC_NOT_FOUND,
        ("failed to create video decoder: %s\n", viddec->codecName), (NULL));
        GST_LOG("closing codec engine\n");
        return FALSE;
    }

    /* Create a circular input buffer */
    viddec->circBuf =
        gst_ticircbuffer_new(Vdec_getInBufSize(viddec->hVd), 3, FALSE);

    if (viddec->circBuf == NULL) {
        GST_ELEMENT_ERROR(viddec, RESOURCE, NO_SPACE_LEFT,
        ("failed to create circular input buffer\n"), (NULL));
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

    /* By default, new buffers are marked as in-use by the codec */
    gfxAttrs.bAttrs.useMask = gst_tividdec_CODEC_FREE;

    viddec->hOutBufTab =
        BufTab_create(viddec->numOutputBufs, Vdec_getOutBufSize(viddec->hVd),
            BufferGfx_getBufferAttrs(&gfxAttrs));

    if (viddec->hOutBufTab == NULL) {
        GST_ELEMENT_ERROR(viddec, RESOURCE, NO_SPACE_LEFT,
        ("failed to create output buffers\n"), (NULL));
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
    if (viddec->circBuf) {
        GstTICircBuffer *circBuf;

        GST_LOG("freeing cicrular input buffer\n");

        circBuf              = viddec->circBuf;
        viddec->circBuf      = NULL;
        viddec->framerateNum = 0;
        viddec->framerateDen = 0;
        gst_ticircbuffer_unref(circBuf);
    }

    if (viddec->hOutBufTab) {

        /* Re-claim all output buffers that were pushed downstream, and then
         * delete the BufTab.
         */
        gst_ti_reclaim_buffers(viddec->hOutBufTab);

        GST_LOG("freeing output buffers\n");
        BufTab_delete(viddec->hOutBufTab);
        viddec->hOutBufTab = NULL;
    }

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
    Int            bufIdx;
    Int            ret, codecRet;

    GST_LOG("init video decode_thread \n");

    /* Initialize codec engine */
    ret = gst_tividdec_codec_start(viddec);

    /* Notify main thread that is ok to continue initialization */
    Rendezvous_meet(viddec->waitOnDecodeThread);
    Rendezvous_reset(viddec->waitOnDecodeThread);

    if (ret == FALSE) {
        GST_ELEMENT_ERROR(viddec, RESOURCE, FAILED,
        ("failed to start codec\n"), (NULL));
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
                GST_ELEMENT_ERROR(viddec, RESOURCE, READ,
                ("failed to get a free contiguous buffer from BufTab\n"),
                (NULL));
                goto thread_failure;
            }
        }
        
        /* Reset waitOnBufTab rendezvous handle to its orignal state */
        Rendezvous_reset(viddec->waitOnBufTab);

        /* Make sure the whole buffer is used for output */
        BufferGfx_resetDimensions(hDstBuf);

        /* Invoke the video decoder */
        GST_LOG("invoking the video decoder\n");
        codecRet        = Vdec_process(viddec->hVd, hEncDataWindow, hDstBuf);
        encDataConsumed = (codecFlushed) ? 0 :
                          Buffer_getNumBytesUsed(hEncDataWindow);

        if (codecRet < 0) {
            if (encDataConsumed <= 0) {
                encDataConsumed = 1;
            }

            GST_ERROR("failed to decode video buffer\n");
        }

        /* If no encoded data was used we cannot find the next frame */
        if (codecRet == Dmai_EBITERROR && 
            encDataConsumed == 0 && !codecFlushed) {
            GST_ELEMENT_ERROR(viddec, STREAM, DECODE, 
            ("fatal bit error\n"), (NULL));
            goto thread_failure;
        }

        if (codecRet > 0) {
            GST_LOG("Vdec_process returned success code %d\n", codecRet); 
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

        /* In case of codec failure or bit error, continue to wait for more 
         * data 
         */
        if ((codecRet < 0) || (codecRet == Dmai_EBITERROR)) {
            BufTab_freeBuf(hDstBuf);
            continue;
        }

        /* Obtain the display buffer returned by the codec (it may be a
         * different one than the one we passed it.
         */
        hDstBuf = Vdec_getDisplayBuf(viddec->hVd);

        /* Increment total bytes recieved */
        viddec->totalBytes += encDataConsumed;

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
                 gst_ti_correct_display_bufSize(hDstBuf));
            gst_buffer_set_caps(outBuf, GST_PAD_CAPS(viddec->srcpad));
            
            /* Set output buffer timestamp */ 
            if (viddec->genTimeStamps) {
                GST_BUFFER_TIMESTAMP(outBuf) = viddec->totalDuration;
                GST_BUFFER_DURATION(outBuf)  = frameDuration; 
                viddec->totalDuration       += GST_BUFFER_DURATION(outBuf);
            }
            else {
                GST_BUFFER_TIMESTAMP(outBuf) = GST_CLOCK_TIME_NONE;
            }

            /* Tell circular buffer how much time we consumed */
            gst_ticircbuffer_time_consumed(viddec->circBuf, frameDuration);

            /* Push the transport buffer to the source pad */
            GST_LOG("pushing buffer to source pad with timestamp : %" 
                    GST_TIME_FORMAT ", duration: %" GST_TIME_FORMAT,
                    GST_TIME_ARGS (GST_BUFFER_TIMESTAMP(outBuf)),
                    GST_TIME_ARGS (GST_BUFFER_DURATION(outBuf)));

            if (gst_pad_push(viddec->srcpad, outBuf) != GST_FLOW_OK) {
                GST_DEBUG("push to source pad failed\n");
                goto thread_failure;
            }

            /* Release buffers no longer in use by the codec */
            Buffer_freeUseMask(hDstBuf, gst_tividdec_CODEC_FREE);

            hDstBuf = Vdec_getDisplayBuf(viddec->hVd);
        }
    }

thread_failure:

    gst_tithread_set_status(viddec, TIThread_CODEC_ABORTED);
    gst_ticircbuffer_consumer_aborted(viddec->circBuf);
    threadRet = GstTIThreadFailure;

thread_exit:

    /* Re-claim any buffers owned by the codec */
    bufIdx = BufTab_getNumBufs(viddec->hOutBufTab);

    while (bufIdx-- > 0) {
        Buffer_Handle hBuf = BufTab_getBuf(viddec->hOutBufTab, bufIdx);
        Buffer_freeUseMask(hBuf, gst_tividdec_CODEC_FREE);
    }

    /* Release the last buffer we retrieved from the circular buffer */
    if (encDataWindow) {
        gst_ticircbuffer_data_consumed(viddec->circBuf, encDataWindow, 0);
    }

    /* We have to wait to shut down this thread until we can guarantee that
     * no more input buffers will be queued into the circular buffer
     * (we're about to delete it).  
     */
    Rendezvous_meet(viddec->waitOnDecodeThread);
    Rendezvous_reset(viddec->waitOnDecodeThread);
 
    /* Notify main thread that we are done draining before we shutdown the
     * codec, or we will hang.  We proceed in this order so the EOS event gets
     * propagated downstream before we attempt to shut down the codec.  The
     * codec-shutdown process will block until all BufTab buffers have been
     * released, and downstream-elements may hang on to buffers until
     * they get the EOS.
     */
    Rendezvous_force(viddec->waitOnDecodeDrain);

    /* Stop codec engine */
    if (gst_tividdec_codec_stop(viddec) < 0) {
        GST_ERROR("failed to stop codec\n");
    }

    gst_object_unref(viddec);

    GST_LOG("exit video decode_thread (%d)\n", (int)threadRet);
    return threadRet;
}


/******************************************************************************
 * gst_tividdec_drain_pipeline
 *    Wait for the decode thread to finish processing queued input data.
 ******************************************************************************/
static void gst_tividdec_drain_pipeline(GstTIViddec *viddec)
{
    gboolean checkResult;

    /* If the decode thread hasn't been created, there is nothing to drain */
    if (!gst_tithread_check_status(
             viddec, TIThread_CODEC_CREATED, checkResult)) {
        return;
    }

    viddec->drainingEOS = TRUE;
    gst_ticircbuffer_drain(viddec->circBuf, TRUE);

    /* Tell the decode thread that it is ok to shut down */
    Rendezvous_force(viddec->waitOnDecodeThread);

    /* Wait for the decoder to finish draining */
    Rendezvous_meet(viddec->waitOnDecodeDrain);
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
