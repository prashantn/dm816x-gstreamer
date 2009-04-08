/*
 * gsttiauddec1.c
 *
 * This file defines the "TIAuddec1" element, which decodes an xDM 1.x audio
 * stream.
 *
 * Example usage:
 *     gst-launch filesrc location=<audio file> !
 *         TIAuddec1 engineName="<engine name>" codecName="<codecName>" !
 *         fakesink silent=TRUE
 *
 * Notes:
 *  * If the upstream element (i.e. demuxer or typefind element) negotiates
 *    caps with TIAuddec1, the engineName and codecName properties will be
 *    auto-detected based on the mime type requested.  The engine and codec
 *    names used for particular mime types are defined in gsttiauddec1.h.
 *    Currently, they are set to use the engine and codec names provided with
 *    the TI evaluation codecs.
 *  * This element currently assumes that the codec produces RAW output.
 *
 * Original Author:
 *     Brijesh Singh, Texas Instruments, Inc.
 *
 * Contributions by:
 *     Diego Dompe, RidgeRun
 *
 * Copyright (C) $year Texas Instruments Incorporated - http://www.ti.com/
 * Copyright (C) 2009 RidgeRun
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

#include <pthread.h>

#include <ti/sdo/dmai/Dmai.h>
#include <ti/sdo/dmai/Buffer.h>
#include <ti/sdo/dmai/ce/Adec1.h>

#include "gsttiauddec1.h"
#include "gsttidmaibuffertransport.h"
#include "gstticodecs.h"
#include "gsttithreadprops.h"
#include "gsttiquicktime_aac.h"
#include "gstticommonutils.h"

/* Declare variable used to categorize GST_LOG output */
GST_DEBUG_CATEGORY_STATIC (gst_tiauddec1_debug);
#define GST_CAT_DEFAULT gst_tiauddec1_debug

/* Element property identifiers */
enum
{
  PROP_0,
  PROP_ENGINE_NAME,     /* engineName     (string)  */
  PROP_CODEC_NAME,      /* codecName      (string)  */
  PROP_NUM_OUTPUT_BUFS, /* numOutputBufs  (int)     */
  PROP_DISPLAY_BUFFER,  /* displayBuffer  (boolean) */
  PROP_GEN_TIMESTAMPS   /* genTimeStamps  (boolean) */
};

/* Define sink (input) pad capabilities.  Currently, AAC and MP3 are
 * supported.
 */
static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE(
    "sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS
    ("audio/mpeg, "
        "mpegversion = (int) { 1, 4 }")
);

/* Define source (output) pad capabilities.  Currently, RAW is supported. */
static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE(
    "src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS
    ("audio/x-raw-int, "
        "endianness = (int) " G_STRINGIFY (G_BYTE_ORDER) ", "
        "signed = (boolean)true, "
        "width = (int) 16, depth = (int) 16, "
        "rate = (int) {8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000 }, "
        "channels = (int) [ 1, 2 ]")
);


/* Declare a global pointer to our element base class */
static GstElementClass *parent_class = NULL;

/* Static Function Declarations */
static void
 gst_tiauddec1_base_init(gpointer g_class);
static void
 gst_tiauddec1_class_init(GstTIAuddec1Class *g_class);
static void
 gst_tiauddec1_init(GstTIAuddec1 *object, GstTIAuddec1Class *g_class);
static void
 gst_tiauddec1_set_property (GObject *object, guint prop_id,
     const GValue *value, GParamSpec *pspec);
static void
 gst_tiauddec1_get_property (GObject *object, guint prop_id, GValue *value,
     GParamSpec *pspec);
static gboolean
 gst_tiauddec1_set_sink_caps(GstPad *pad, GstCaps *caps);
static gboolean
 gst_tiauddec1_set_source_caps(GstPad *pad, gint sampleRate);
static gboolean
 gst_tiauddec1_sink_event(GstPad *pad, GstEvent *event);
static GstFlowReturn
 gst_tiauddec1_chain(GstPad *pad, GstBuffer *buf);
static gboolean
 gst_tiauddec1_init_audio(GstTIAuddec1 *auddec1);
static gboolean
 gst_tiauddec1_exit_audio(GstTIAuddec1 *auddec1);
static GstStateChangeReturn
 gst_tiauddec1_change_state(GstElement *element, GstStateChange transition);
static void*
 gst_tiauddec1_decode_thread(void *arg);
static void*
 gst_tiauddec1_queue_thread(void *arg);
static void
 gst_tiauddec1_broadcast_queue_thread(GstTIAuddec1 *auddec1);
static void
 gst_tiauddec1_wait_on_queue_thread(GstTIAuddec1 *auddec1, Int32 waitQueueSize);
static void
 gst_tiauddec1_drain_pipeline(GstTIAuddec1 *auddec1);
static gboolean 
    gst_tiauddec1_codec_start (GstTIAuddec1  *auddec);
static gboolean 
    gst_tiauddec1_codec_stop (GstTIAuddec1  *auddec1);
static void 
    gst_tiauddec1_init_env(GstTIAuddec1 *auddec1);

/******************************************************************************
 * gst_tiauddec1_class_init_trampoline
 *    Boiler-plate function auto-generated by "make_element" script.
 ******************************************************************************/
static void gst_tiauddec1_class_init_trampoline(gpointer g_class, gpointer data)
{
    parent_class = (GstElementClass*) g_type_class_peek_parent(g_class);
    gst_tiauddec1_class_init((GstTIAuddec1Class*)g_class);
}


/******************************************************************************
 * gst_tiauddec1_get_type
 *    Boiler-plate function auto-generated by "make_element" script.
 *    Defines function pointers for initialization routines for this element.
 ******************************************************************************/
GType gst_tiauddec1_get_type(void)
{
    static GType object_type = 0;

    if (G_UNLIKELY(object_type == 0)) {
        static const GTypeInfo object_info = {
            sizeof(GstTIAuddec1Class),
            gst_tiauddec1_base_init,
            NULL,
            gst_tiauddec1_class_init_trampoline,
            NULL,
            NULL,
            sizeof(GstTIAuddec1),
            0,
            (GInstanceInitFunc) gst_tiauddec1_init
        };

        object_type = g_type_register_static((gst_element_get_type()),
                          "GstTIAuddec1", &object_info, (GTypeFlags)0);

        /* Initialize GST_LOG for this object */
        GST_DEBUG_CATEGORY_INIT(gst_tiauddec1_debug, "TIAuddec1", 0,
            "TI xDM 1.x Audio Decoder");

        GST_LOG("initialized get_type\n");

    }

    return object_type;
};


/******************************************************************************
 * gst_tiauddec1_base_init
 *    Boiler-plate function auto-generated by "make_element" script.
 *    Initializes element base class.
 ******************************************************************************/
static void gst_tiauddec1_base_init(gpointer gclass)
{
    static GstElementDetails element_details = {
        "TI xDM 1.x Audio Decoder",
        "Codec/Decoder/Audio",
        "Decodes audio using an xDM 1.x-based codec",
        "Brijesh Singh; Texas Instruments, Inc."
    };

    GstElementClass *element_class = GST_ELEMENT_CLASS(gclass);

    gst_element_class_add_pad_template(element_class,
        gst_static_pad_template_get (&src_factory));
    gst_element_class_add_pad_template(element_class,
        gst_static_pad_template_get (&sink_factory));
    gst_element_class_set_details(element_class, &element_details);

}


/******************************************************************************
 * gst_tiauddec1_class_init
 *    Boiler-plate function auto-generated by "make_element" script.
 *    Initializes the TIAuddec1 class.
 ******************************************************************************/
static void gst_tiauddec1_class_init(GstTIAuddec1Class *klass)
{
    GObjectClass    *gobject_class;
    GstElementClass *gstelement_class;

    gobject_class    = (GObjectClass*)    klass;
    gstelement_class = (GstElementClass*) klass;

    gobject_class->set_property = gst_tiauddec1_set_property;
    gobject_class->get_property = gst_tiauddec1_get_property;

    gstelement_class->change_state = gst_tiauddec1_change_state;

    g_object_class_install_property(gobject_class, PROP_ENGINE_NAME,
        g_param_spec_string("engineName", "Engine Name",
            "Engine name used by Codec Engine", "unspecified",
            G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_CODEC_NAME,
        g_param_spec_string("codecName", "Codec Name", "Name of audio codec",
            "unspecified", G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_NUM_OUTPUT_BUFS,
        g_param_spec_int("numOutputBufs",
            "Number of Ouput Buffers",
            "Number of output buffers to allocate for codec",
            2, G_MAXINT32, 2, G_PARAM_WRITABLE));

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
 * gst_tiauddec1_init_env
 *  Initialize element property default by reading environment variables.
 *****************************************************************************/
static void gst_tiauddec1_init_env(GstTIAuddec1 *auddec1)
{
    GST_LOG("gst_tiauddec1_init_env - begin");
    
    if (gst_ti_env_is_defined("GST_TI_TIAuddec1_engineName")) {
        auddec1->engineName = gst_ti_env_get_string("GST_TI_TIAuddec1_engineName");
        GST_LOG("Setting engineName=%s\n", auddec1->engineName);
    }

    if (gst_ti_env_is_defined("GST_TI_TIAuddec1_codecName")) {
        auddec1->codecName = gst_ti_env_get_string("GST_TI_TIAuddec1_codecName");
        GST_LOG("Setting codecName=%s\n", auddec1->codecName);
    }
    
    if (gst_ti_env_is_defined("GST_TI_TIAuddec1_numOutputBufs")) {
        auddec1->numOutputBufs = 
                            gst_ti_env_get_int("GST_TI_TIAuddec1_numOutputBufs");
        GST_LOG("Setting numOutputBufs=%ld\n", auddec1->numOutputBufs);
    }

    if (gst_ti_env_is_defined("GST_TI_TIAuddec1_displayBuffer")) {
        auddec1->displayBuffer = 
                gst_ti_env_get_boolean("GST_TI_TIAuddec1_displayBuffer");
        GST_LOG("Setting displayBuffer=%s\n",
                 auddec1->displayBuffer  ? "TRUE" : "FALSE");
    }
 
    if (gst_ti_env_is_defined("GST_TI_TIAuddec1_genTimeStamps")) {
        auddec1->genTimeStamps = 
                gst_ti_env_get_boolean("GST_TI_TIAuddec1_genTimeStamps");
        GST_LOG("Setting genTimeStamps =%s\n", 
                    auddec1->genTimeStamps ? "TRUE" : "FALSE");
    }

    GST_LOG("gst_tiauddec1_init_env - end");
}

/******************************************************************************
 * gst_tiauddec1_init
 *    Initializes a new element instance, instantiates pads and sets the pad
 *    callback functions.
 ******************************************************************************/
static void gst_tiauddec1_init(GstTIAuddec1 *auddec1, GstTIAuddec1Class *gclass)
{
    /* Instantiate encoded audio sink pad.
     *
     * Fixate on our static template caps instead of writing a getcaps
     * function, which is overkill for this element.
     */
    auddec1->sinkpad =
        gst_pad_new_from_static_template(&sink_factory, "sink");
    gst_pad_set_setcaps_function(
        auddec1->sinkpad, GST_DEBUG_FUNCPTR(gst_tiauddec1_set_sink_caps));
    gst_pad_set_event_function(
        auddec1->sinkpad, GST_DEBUG_FUNCPTR(gst_tiauddec1_sink_event));
    gst_pad_set_chain_function(
        auddec1->sinkpad, GST_DEBUG_FUNCPTR(gst_tiauddec1_chain));
    gst_pad_fixate_caps(auddec1->sinkpad,
        gst_caps_make_writable(
            gst_caps_copy(gst_pad_get_pad_template_caps(auddec1->sinkpad))));

    /* Instantiate deceoded audio source pad.
     *
     * Fixate on our static template caps instead of writing a getcaps
     * function, which is overkill for this element.
     */
    auddec1->srcpad =
        gst_pad_new_from_static_template(&src_factory, "src");
    gst_pad_fixate_caps(auddec1->srcpad,
        gst_caps_make_writable(
            gst_caps_copy(gst_pad_get_pad_template_caps(auddec1->srcpad))));

    /* Add pads to TIAuddec1 element */
    gst_element_add_pad(GST_ELEMENT(auddec1), auddec1->sinkpad);
    gst_element_add_pad(GST_ELEMENT(auddec1), auddec1->srcpad);

    /* Initialize TIAuddec1 state */
    auddec1->engineName         = NULL;
    auddec1->codecName          = NULL;
    auddec1->displayBuffer      = FALSE;
    auddec1->genTimeStamps      = TRUE;

    auddec1->hEngine            = NULL;
    auddec1->hAd                = NULL;
    auddec1->channels           = 0;
    auddec1->drainingEOS        = FALSE;
    auddec1->threadStatus       = 0UL;

    auddec1->decodeDrained      = FALSE;
    auddec1->waitOnDecodeDrain  = NULL;
    auddec1->waitOnBufTab       = NULL;

    auddec1->hInFifo            = NULL;

    auddec1->waitOnQueueThread  = NULL;
    auddec1->waitQueueSize      = 0;

    auddec1->waitOnDecodeThread = NULL;
    
    auddec1->numOutputBufs      = 0UL;
    auddec1->hOutBufTab         = NULL;
    auddec1->circBuf            = NULL;

    auddec1->aac_header_data    = NULL;

    gst_tiauddec1_init_env(auddec1);
}


/******************************************************************************
 * gst_tiauddec1_set_property
 *     Set element properties when requested.
 ******************************************************************************/
static void gst_tiauddec1_set_property(GObject *object, guint prop_id,
                const GValue *value, GParamSpec *pspec)
{
    GstTIAuddec1 *auddec1 = GST_TIAUDDEC1(object);

    GST_LOG("begin set_property\n");

    switch (prop_id) {
        case PROP_ENGINE_NAME:
            if (auddec1->engineName) {
                g_free((gpointer)auddec1->engineName);
            }
            auddec1->engineName =
                (gchar*)g_malloc(strlen(g_value_get_string(value)) + 1);
            strcpy((gchar *)auddec1->engineName, g_value_get_string(value));
            GST_LOG("setting \"engineName\" to \"%s\"\n", auddec1->engineName);
            break;
        case PROP_CODEC_NAME:
            if (auddec1->codecName) {
                g_free((gpointer)auddec1->codecName);
            }
            auddec1->codecName =
                (gchar*)g_malloc(strlen(g_value_get_string(value)) + 1);
            strcpy((gchar*)auddec1->codecName, g_value_get_string(value));
            GST_LOG("setting \"codecName\" to \"%s\"\n", auddec1->codecName);
            break;
        case PROP_NUM_OUTPUT_BUFS:
            auddec1->numOutputBufs = g_value_get_int(value);
            GST_LOG("setting \"numOutputBufs\" to \"%ld\"\n",
                auddec1->numOutputBufs);
            break;
        case PROP_DISPLAY_BUFFER:
            auddec1->displayBuffer = g_value_get_boolean(value);
            GST_LOG("setting \"displayBuffer\" to \"%s\"\n",
                auddec1->displayBuffer ? "TRUE" : "FALSE");
            break;
        case PROP_GEN_TIMESTAMPS:
            auddec1->genTimeStamps = g_value_get_boolean(value);
            GST_LOG("setting \"genTimeStamps\" to \"%s\"\n",
                auddec1->genTimeStamps ? "TRUE" : "FALSE");
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }

    GST_LOG("end set_property\n");
}

/******************************************************************************
 * gst_tiauddec1_get_property
 *     Return values for requested element property.
 ******************************************************************************/
static void gst_tiauddec1_get_property(GObject *object, guint prop_id,
                GValue *value, GParamSpec *pspec)
{
    GstTIAuddec1 *auddec1 = GST_TIAUDDEC1(object);

    GST_LOG("begin get_property\n");

    switch (prop_id) {
        case PROP_ENGINE_NAME:
            g_value_set_string(value, auddec1->engineName);
            break;
        case PROP_CODEC_NAME:
            g_value_set_string(value, auddec1->codecName);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }

    GST_LOG("end get_property\n");
}


/******************************************************************************
 * gst_tiauddec1_set_sink_caps
 *     Negotiate our sink pad capabilities.
 ******************************************************************************/
static gboolean gst_tiauddec1_set_sink_caps(GstPad *pad, GstCaps *caps)
{
    GstTIAuddec1  *auddec1;
    GstStructure *capStruct;
    const gchar  *mime;
    char         *string;
    GstTICodec   *codec = NULL;
    gint rate;

    auddec1    = GST_TIAUDDEC1(gst_pad_get_parent(pad));
    capStruct = gst_caps_get_structure(caps, 0);
    mime      = gst_structure_get_name(capStruct);

    string = gst_caps_to_string(caps);
    GST_INFO("requested sink caps:  %s", string);
    g_free(string);

    /* Generic Audio Properties */
    if (!strncmp(mime, "audio/", 6)) {

        if (!gst_structure_get_int(capStruct, "rate", &rate)) {
            rate = 0;
        }

        if (!gst_structure_get_int(capStruct, "channels", &auddec1->channels)) {
            /* Default to 2 channels if not specified */
            auddec1->channels = 2;
        }

    }

    /* MPEG Audio */
    if (!strcmp(mime, "audio/mpeg")) {
        gint bitrate;
        gint mpegversion;
        gint layer;

        if (!gst_structure_get_int(capStruct, "bitrate", &bitrate)) {
            bitrate = 0;
        }

        if (!gst_structure_get_int(capStruct, "mpegversion", &mpegversion)) {
            mpegversion = 0;
        }

        if (!gst_structure_get_int(capStruct, "layer", &layer)) {
            /* Default to layer 2 if not specified */
            layer = 2;
        }

        /* Use MP3 Decoder for MPEG1 Layer 2 */
        if (mpegversion == 1 && layer == 2) {
            codec = gst_ticodec_get_codec("MPEG1L2 Audio Decoder");
        }

        /* Use MP3 Decoder for MP3 */
        else if (mpegversion == 1 && layer == 3) {
            codec = gst_ticodec_get_codec("MPEG1L3 Audio Decoder");
        }

        /* Use AAC Decoder for MPEG4 */
        else if (mpegversion == 4) { 
            codec = gst_ticodec_get_codec("AAC Audio Decoder");
            auddec1->aac_header_data = gst_aac_header_create(rate, 
                            auddec1->channels);
        }

        /* MPEG version not supported */
        else {
            GST_ERROR("MPEG version not supported");
            gst_object_unref(auddec1);
            return FALSE;
        }
    }

    /* Mime type not supported */
    else {
        GST_ERROR("stream type not supported");
        gst_object_unref(auddec1);
        return FALSE;
    }

    /* Report if the required codec was not found */
    if (!codec) {
        GST_ERROR("unable to find codec needed for stream");
        gst_object_unref(auddec1);
        return FALSE;
    }

    /* Shut-down any running audio decoder */
    if (!gst_tiauddec1_exit_audio(auddec1)) {
        gst_object_unref(auddec1);
        return FALSE;
    }

    /* Configure the element to use the detected engine name and codec, unless
     * they have been using the set_property function.
     */
    if (!auddec1->engineName) {
        auddec1->engineName = codec->CE_EngineName;
    }
    if (!auddec1->codecName) {
        auddec1->codecName = codec->CE_CodecName;
    }

    gst_object_unref(auddec1);

    GST_LOG("sink caps negotiation successful\n");
    return TRUE;
}


/******************************************************************************
 * gst_tiauddec1_set_source_caps
 *     Negotiate our source pad capabilities.
 ******************************************************************************/
static gboolean gst_tiauddec1_set_source_caps(GstPad *pad, gint sampleRate)
{
    GstCaps  *caps;
    gboolean  ret;
    char     *string;

    caps =
        gst_caps_new_simple ("audio/x-raw-int",
            "endianness", G_TYPE_INT,     G_BYTE_ORDER,
            "signed",     G_TYPE_BOOLEAN, TRUE,
            "width",      G_TYPE_INT,     16,
            "depth",      G_TYPE_INT,     16,
            "rate",       G_TYPE_INT,     sampleRate,
            "channels",   G_TYPE_INT,     2,
            NULL);

    /* Set the source pad caps */
    string = gst_caps_to_string(caps);
    GST_LOG("setting source caps to RAW:  %s", string);
    g_free(string);

    if (!gst_pad_set_caps(pad, caps)) {
        ret = FALSE;
    }
    gst_caps_unref(caps);

    return ret;
}


/******************************************************************************
 * gst_tiauddec1_sink_event
 *     Perform event processing on the input stream.  At the moment, this
 *     function isn't needed as this element doesn't currently perform any
 *     specialized event processing.  We'll leave it in for now in case we need
 *     it later on.
 ******************************************************************************/
static gboolean gst_tiauddec1_sink_event(GstPad *pad, GstEvent *event)
{
    GstTIAuddec1 *auddec1;
    gboolean     ret;

    auddec1 = GST_TIAUDDEC1(GST_OBJECT_PARENT(pad));

    GST_DEBUG("pad \"%s\" received:  %s\n", GST_PAD_NAME(pad),
        GST_EVENT_TYPE_NAME(event));

    switch (GST_EVENT_TYPE(event)) {

        case GST_EVENT_NEWSEGMENT:
            /* maybe save and/or update the current segment (e.g. for output
             * clipping) or convert the event into one in a different format
             * (e.g. BYTES to TIME) or drop it and set a flag to send a
             * newsegment event in a different format later
             */
            ret = gst_pad_push_event(auddec1->srcpad, event);
            break;

        case GST_EVENT_EOS:
            /* end-of-stream: process any remaining encoded frame data */
            GST_LOG("no more input; draining remaining encoded audio data\n");
            if (!auddec1->drainingEOS) {
               gst_tiauddec1_drain_pipeline(auddec1);
            }

            /* Propagate EOS to downstream elements */
            ret = gst_pad_push_event(auddec1->srcpad, event);
            break;

        case GST_EVENT_FLUSH_STOP:
            ret = gst_pad_push_event(auddec1->srcpad, event);
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
 * gst_tiauddec1_chain
 *    This is the main processing routine.  This function receives a buffer
 *    from the sink pad, processes it, and pushes the result to the source
 *    pad.
 ******************************************************************************/
static GstFlowReturn gst_tiauddec1_chain(GstPad * pad, GstBuffer * buf)
{
    GstTIAuddec1 *auddec1 = GST_TIAUDDEC1(GST_OBJECT_PARENT(pad));
    gboolean     checkResult;

    /* If any thread aborted, communicate it to the pipeline */
    if (gst_tithread_check_status(auddec1, TIThread_ANY_ABORTED, checkResult)) {
       gst_buffer_unref(buf);
       return GST_FLOW_UNEXPECTED;
    }

    /* If our engine handle is currently NULL, then either this is our first
     * buffer or the upstream element has re-negotiated our capabilities which
     * resulted in our engine being closed.  In either case, we need to
     * initialize (or re-initialize) our audio decoder to handle the new
     * stream.
     */
    if (auddec1->hEngine == NULL) {
        if (!gst_tiauddec1_init_audio(auddec1)) {
            GST_ERROR("unable to initialize audio\n");
            gst_buffer_unref(buf);
            return GST_FLOW_UNEXPECTED;
        }

        /* If we are decoding aac stream, then check whether stream has valid 
         * ADIF or ADTS header. If not, then add ADIF header. This header is 
         * created by reading parsing codec_data field passed via demuxer.
         *
         * Note: This is used when qtdemuxer is used for playing mp4 files.
         */
        if (auddec1->aac_header_data) {

            if (!gst_aac_valid_header(GST_BUFFER_DATA(buf))) {

                GST_LOG("Adding auto-generated ADIF header.\n");

                /* Queue up the aac header data into a circular buffer */
                if (Fifo_put(auddec1->hInFifo, auddec1->aac_header_data) < 0) {
                    GST_ERROR("Failed to send buffer to queue thread\n");
                    gst_buffer_unref(buf);
                    return GST_FLOW_UNEXPECTED;
                }
            }
        }

        GST_TICIRCBUFFER_TIMESTAMP(auddec1->circBuf) =
            GST_CLOCK_TIME_IS_VALID(GST_BUFFER_TIMESTAMP(buf)) ?
            GST_BUFFER_TIMESTAMP(buf) : 0ULL;
    }

    /* Don't queue up too many buffers -- if we collect too many input buffers
     * without consuming them we'll run out of memory.  Once we reach a
     * threshold, block until the queue thread removes some buffers.
     */
    Rendezvous_reset(auddec1->waitOnQueueThread);
    if (Fifo_getNumEntries(auddec1->hInFifo) > 2000) {
        gst_tiauddec1_wait_on_queue_thread(auddec1, 1800);
    }

    /* Queue up the encoded data stream into a circular buffer */
    if (Fifo_put(auddec1->hInFifo, buf) < 0) {
        GST_ERROR("Failed to send buffer to queue thread\n");
        gst_buffer_unref(buf);
        return GST_FLOW_UNEXPECTED;
    }

    return GST_FLOW_OK;
}


/******************************************************************************
 * gst_tiauddec1_init_audio
 *     Initialize or re-initializes the audio stream
 ******************************************************************************/
static gboolean gst_tiauddec1_init_audio(GstTIAuddec1 * auddec1)
{
    Rendezvous_Attrs      rzvAttrs  = Rendezvous_Attrs_DEFAULT;
    struct sched_param    schedParam;
    pthread_attr_t        attr;
    Fifo_Attrs              fAttrs    = Fifo_Attrs_DEFAULT;

    GST_LOG("begin init_audio\n");

    /* If audio has already been initialized, shut down previous decoder */
    if (auddec1->hEngine) {
        if (!gst_tiauddec1_exit_audio(auddec1)) {
            GST_ERROR("failed to shut down existing audio decoder\n");
            return FALSE;
        }
    }

    /* Make sure we know what codec we're using */
    if (!auddec1->engineName) {
        GST_ERROR("engine name not specified\n");
        return FALSE;
    }

    if (!auddec1->codecName) {
        GST_ERROR("codec name not specified\n");
        return FALSE;
    }

    /* Set up the queue fifo */
    auddec1->hInFifo = Fifo_create(&fAttrs);

    /* Initialize thread status management */
    auddec1->threadStatus = 0UL;
    pthread_mutex_init(&auddec1->threadStatusMutex, NULL);

    /* Initialize rendezvous objects for making threads wait on conditions */
    auddec1->waitOnDecodeDrain  = Rendezvous_create(100, &rzvAttrs);
    auddec1->waitOnQueueThread  = Rendezvous_create(100, &rzvAttrs);
    auddec1->waitOnDecodeThread = Rendezvous_create(2, &rzvAttrs);
    auddec1->waitOnBufTab       = Rendezvous_create(100, &rzvAttrs);
    auddec1->drainingEOS        = FALSE;

    /* Initialize the custom thread attributes */
    if (pthread_attr_init(&attr)) {
        GST_WARNING("failed to initialize thread attrs\n");
        gst_tiauddec1_exit_audio(auddec1);
        return FALSE;
    }

    /* Force the thread to use the system scope */
    if (pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM)) {
        GST_WARNING("failed to set scope attribute\n");
        gst_tiauddec1_exit_audio(auddec1);
        return FALSE;
    }

    /* Force the thread to use custom scheduling attributes */
    if (pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED)) {
        GST_WARNING("failed to set schedule inheritance attribute\n");
        gst_tiauddec1_exit_audio(auddec1);
        return FALSE;
    }

    /* Set the thread to be fifo real time scheduled */
    if (pthread_attr_setschedpolicy(&attr, SCHED_FIFO)) {
        GST_WARNING("failed to set FIFO scheduling policy\n");
        gst_tiauddec1_exit_audio(auddec1);
        return FALSE;
    }

    /* Set the display thread priority */
    schedParam.sched_priority = GstTIAudioThreadPriority;
    if (pthread_attr_setschedparam(&attr, &schedParam)) {
        GST_WARNING("failed to set scheduler parameters\n");
        return FALSE;
    }

    /* Create decoder thread */
    if (pthread_create(&auddec1->decodeThread, &attr,
            gst_tiauddec1_decode_thread, (void*)auddec1)) {
        GST_ERROR("failed to create decode thread\n");
        gst_tiauddec1_exit_audio(auddec1);
        return FALSE;
    }
    gst_tithread_set_status(auddec1, TIThread_DECODE_CREATED);

    /* Wait for decoder thread to finish initilization before creating queue
     * thread.
     */
    Rendezvous_meet(auddec1->waitOnDecodeThread);

    /* Make sure circular buffer and display buffer handles are created by
     * decoder thread.
     */
    if (auddec1->circBuf == NULL || auddec1->hOutBufTab == NULL) {
        GST_ERROR("decode thread failed to create circbuf or display buffer"
                  " handles\n");
        return FALSE;
    }

    /* Create queue thread */
    if (pthread_create(&auddec1->queueThread, NULL,
            gst_tiauddec1_queue_thread, (void*)auddec1)) {
        GST_ERROR("failed to create queue thread\n");
        gst_tiauddec1_exit_audio(auddec1);
        return FALSE;
    }
    gst_tithread_set_status(auddec1, TIThread_QUEUE_CREATED);

    GST_LOG("end init_audio\n");
    return TRUE;
}


/******************************************************************************
 * gst_tiauddec1_exit_audio
 *    Shut down any running audio decoder, and reset the element state.
 ******************************************************************************/
static gboolean gst_tiauddec1_exit_audio(GstTIAuddec1 *auddec1)
{
    gboolean checkResult;
    void*    thread_ret;

    GST_LOG("begin exit_audio\n");

    /* Drain the pipeline if it hasn't already been drained */
    if (!auddec1->drainingEOS) {
       gst_tiauddec1_drain_pipeline(auddec1);
     }

    /* Shut down the decode thread */
    if (gst_tithread_check_status(
            auddec1, TIThread_DECODE_CREATED, checkResult)) {
        GST_LOG("shutting down decode thread\n");

        if (pthread_join(auddec1->decodeThread, &thread_ret) == 0) {
            if (thread_ret == GstTIThreadFailure) {
                GST_DEBUG("decode thread exited with an error condition\n");
            }
        }
    }

    /* Shut down the queue thread */
    if (gst_tithread_check_status(
            auddec1, TIThread_QUEUE_CREATED, checkResult)) {
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
        if (Fifo_put(auddec1->hInFifo,&gst_ti_flush_fifo) < 0) {
            GST_ERROR("Could not put flush value to Fifo\n");
        }

        if (pthread_join(auddec1->queueThread, &thread_ret) == 0) {
            if (thread_ret == GstTIThreadFailure) {
                GST_DEBUG("queue thread exited with an error condition\n");
            }
        }
    }

    /* Shut down thread status management */
    auddec1->threadStatus = 0UL;
    pthread_mutex_destroy(&auddec1->threadStatusMutex);

    /* Shut down remaining items */
    if (auddec1->hInFifo) {
        Fifo_delete(auddec1->hInFifo);
        auddec1->hInFifo = NULL;
    }

    if (auddec1->waitOnQueueThread) {
        Rendezvous_delete(auddec1->waitOnQueueThread);
        auddec1->waitOnQueueThread = NULL;
    }

    if (auddec1->waitOnDecodeDrain) {
        Rendezvous_delete(auddec1->waitOnDecodeDrain);
        auddec1->waitOnDecodeDrain = NULL;
    }

    if (auddec1->waitOnDecodeThread) {
        Rendezvous_delete(auddec1->waitOnDecodeThread);
        auddec1->waitOnDecodeThread = NULL;
    }

    if (auddec1->waitOnBufTab) {
        Rendezvous_delete(auddec1->waitOnBufTab);
        auddec1->waitOnBufTab = NULL;
    }

    if (auddec1->circBuf) {
        GST_LOG("freeing cicrular input buffer\n");
        gst_ticircbuffer_unref(auddec1->circBuf);
        auddec1->circBuf       = NULL;
    }

    if (auddec1->hOutBufTab) {
        GST_LOG("freeing output buffer\n");
        BufTab_delete(auddec1->hOutBufTab);
        auddec1->hOutBufTab = NULL;
    }

    GST_LOG("end exit_audio\n");
    return TRUE;
}


/******************************************************************************
 * gst_tiauddec1_change_state
 *     Manage state changes for the audio stream.  The gStreamer documentation
 *     states that state changes must be handled in this manner:
 *        1) Handle ramp-up states
 *        2) Pass state change to base class
 *        3) Handle ramp-down states
 ******************************************************************************/
static GstStateChangeReturn gst_tiauddec1_change_state(GstElement *element,
                                GstStateChange transition)
{
    GstStateChangeReturn  ret    = GST_STATE_CHANGE_SUCCESS;
    GstTIAuddec1          *auddec1 = GST_TIAUDDEC1(element);

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
            /* Shut down any running audio decoder */
            if (!gst_tiauddec1_exit_audio(auddec1)) {
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
 * gst_tiauddec1_codec_stop
 *     Release codec engine resources
 *****************************************************************************/
static gboolean gst_tiauddec1_codec_stop (GstTIAuddec1  *auddec1)
{
    if (auddec1->hAd) {
        GST_LOG("closing audio decoder\n");
        Adec1_delete(auddec1->hAd);
        auddec1->hAd = NULL;
    }

    if (auddec1->hEngine) {
        GST_LOG("closing codec engine\n");
        Engine_close(auddec1->hEngine);
        auddec1->hEngine = NULL;
    }

    return TRUE;
}


/******************************************************************************
 * gst_tiauddec1_codec_start
 *     Initialize codec engine
 *****************************************************************************/
static gboolean gst_tiauddec1_codec_start (GstTIAuddec1  *auddec1)
{
    AUDDEC1_Params          params    = Adec1_Params_DEFAULT;
    AUDDEC1_DynamicParams   dynParams = Adec1_DynamicParams_DEFAULT;
    Buffer_Attrs            bAttrs    = Buffer_Attrs_DEFAULT;

    /* Open the codec engine */
    GST_LOG("opening codec engine \"%s\"\n", auddec1->engineName);
    auddec1->hEngine = Engine_open((Char *) auddec1->engineName, NULL, NULL);

    if (auddec1->hEngine == NULL) {
        GST_ERROR("failed to open codec engine \"%s\"\n", auddec1->engineName);
        return FALSE;
    }

    /* Initialize audio decoder */
    GST_LOG("opening audio decoder \"%s\"\n", auddec1->codecName);
    auddec1->hAd = Adec1_create(auddec1->hEngine, (Char*)auddec1->codecName,
                      &params, &dynParams);

    if (auddec1->hAd == NULL) {
        GST_ERROR("failed to create audio decoder: %s\n", auddec1->codecName);
        GST_LOG("closing codec engine\n");
        return FALSE;
    }

    /* Set up a circular input buffer capable of holding two encoded frames */
    auddec1->circBuf = gst_ticircbuffer_new(
                            Adec1_getInBufSize(auddec1->hAd) * 10, 3, FALSE);

    if (auddec1->circBuf == NULL) {
        GST_ERROR("failed to create circular input buffer\n");
        return FALSE;
    }

    /* Display buffer contents if displayBuffer=TRUE was specified */
    gst_ticircbuffer_set_display(auddec1->circBuf, auddec1->displayBuffer);

    /* Define the number of display buffers to allocate.  This number must be
     * at least 2, If this has not been set via set_property(), default to the
     * minimal value.
      */
    if (auddec1->numOutputBufs == 0) {
        auddec1->numOutputBufs = 2;
    }

    /* Create codec output buffers.  
     */
    GST_LOG("creating output buffers\n");
      
    bAttrs.useMask = gst_tidmaibuffertransport_GST_FREE;

    auddec1->hOutBufTab =
        BufTab_create(auddec1->numOutputBufs, 
            Adec1_getOutBufSize(auddec1->hAd), &bAttrs);

    if (auddec1->hOutBufTab == NULL) {
        GST_ERROR("failed to create output buffer\n");
        return FALSE;
    }

    return TRUE;
}


/******************************************************************************
 * gst_tiauddec1_decode_thread
 *     Call the audio codec to process a full input buffer
 ******************************************************************************/
static void* gst_tiauddec1_decode_thread(void *arg)
{
    GstTIAuddec1   *auddec1    = GST_TIAUDDEC1(gst_object_ref(arg));
    void          *threadRet = GstTIThreadSuccess;
    Buffer_Handle  hDstBuf;
    Int32          encDataConsumed;
    GstBuffer     *encDataWindow = NULL;
    GstClockTime   encDataTime;
    Buffer_Handle  hEncDataWindow;
    GstBuffer     *outBuf;
    Int            ret;
    guint          sampleDataSize;
    GstClockTime   sampleDuration;
    guint          sampleRate;

    GST_LOG("starting auddec decode thread\n");

    /* Initialize codec engine */
    ret = gst_tiauddec1_codec_start(auddec1);

    /* Notify main thread if it is waiting to create queue thread */
    Rendezvous_meet(auddec1->waitOnDecodeThread);

    if (ret == FALSE) {
        GST_ERROR("failed to start codec\n");
        goto thread_exit;
    }

    while (TRUE) {

        /* Obtain an encoded data frame */
        encDataWindow  = gst_ticircbuffer_get_data(auddec1->circBuf);
        encDataTime    = GST_BUFFER_TIMESTAMP(encDataWindow);
        hEncDataWindow = GST_TIDMAIBUFFERTRANSPORT_DMAIBUF(encDataWindow);

        if (GST_BUFFER_SIZE(encDataWindow) == 0) {
            GST_LOG("no audio data remains\n");
            if (!auddec1->drainingEOS) {
                goto thread_failure;
            }
            goto thread_exit;
        }

        /* Obtain a free output buffer for the decoded data */
        /* If we are not able to find free buffer from BufTab then decoder 
         * thread will be blocked on waitOnBufTab rendezvous. And this will be 
         * woke-up by dmaitransportbuffer finalize method.
         */
        hDstBuf = BufTab_getFreeBuf(auddec1->hOutBufTab);
        if (hDstBuf == NULL) {
            Rendezvous_meet(auddec1->waitOnBufTab);
            hDstBuf = BufTab_getFreeBuf(auddec1->hOutBufTab);

            if (hDstBuf == NULL) {
                GST_ERROR("failed to get a free contiguous buffer"
                          " from BufTab\n");
                goto thread_exit;
            }
        }

        /* Reset waitOnBufTab rendezvous handle to its orignal state */
        Rendezvous_reset(auddec1->waitOnBufTab);

        /* Invoke the audio decoder */
        GST_LOG("Invoking the audio decoder at 0x%08lx with %u bytes\n",
            (unsigned long)Buffer_getUserPtr(hEncDataWindow),
            GST_BUFFER_SIZE(encDataWindow));
        ret             = Adec1_process(auddec1->hAd, hEncDataWindow, hDstBuf);
        encDataConsumed = Buffer_getNumBytesUsed(hEncDataWindow);

        if (ret < 0) {
            GST_ERROR("failed to decode audio buffer\n");
            goto thread_failure;
        }

        /* If no encoded data was used we cannot find the next frame */
        if (ret == Dmai_EBITERROR && encDataConsumed == 0) {
            GST_ERROR("fatal bit error\n");
            goto thread_failure;
        }

        if (ret > 0) {
            GST_LOG("Adec1_process returned success code %d\n", ret); 
        }

        /* DMAI currently doesn't provide a way to retrieve the number of
         * samples decoded or the duration of the decoded audio data.  For
         * now, derive the duration from the number of bytes decoded by the
         * codec.
         */
        sampleDataSize = Buffer_getNumBytesUsed(hDstBuf);
        sampleRate     = Adec1_getSampleRate(auddec1->hAd);
        sampleDuration = (GstClockTime)
            (((gdouble)(sampleDataSize) / (gdouble)auddec1->channels /
              (gdouble) 2 / (gdouble)sampleRate)
            * GST_SECOND);

        /* Release the reference buffer, and tell the circular buffer how much
         * data was consumed.
         */
        ret = gst_ticircbuffer_data_consumed(auddec1->circBuf, encDataWindow,
                  encDataConsumed);
        encDataWindow = NULL;

        if (!ret) {
            goto thread_failure;
        }

        /* Set the source pad capabilities based on the decoded frame
         * properties.
         */
        gst_tiauddec1_set_source_caps(
            auddec1->srcpad, Adec1_getSampleRate(auddec1->hAd));

        /* Create a DMAI transport buffer object to carry a DMAI buffer to
         * the source pad.  The transport buffer knows how to release the
         * buffer for re-use in this element when the source pad calls
         * gst_buffer_unref().
         */
        outBuf = gst_tidmaibuffertransport_new(hDstBuf, 
                                                auddec1->waitOnBufTab);
        gst_buffer_set_data(outBuf, GST_BUFFER_DATA(outBuf),
            Buffer_getNumBytesUsed(hDstBuf));
        gst_buffer_set_caps(outBuf, GST_PAD_CAPS(auddec1->srcpad));

        /* If we have a valid time stamp, set it on the buffer */
        if (auddec1->genTimeStamps && GST_CLOCK_TIME_IS_VALID(encDataTime)) {
            GST_LOG("audio timestamp value: %llu\n", encDataTime);
            GST_BUFFER_TIMESTAMP(outBuf) = encDataTime;
            GST_BUFFER_DURATION(outBuf)  = sampleDuration;
        }
        else {
            GST_BUFFER_TIMESTAMP(outBuf) = GST_CLOCK_TIME_NONE;
        }

        /* Tell circular buffer how much time we consumed */
        gst_ticircbuffer_time_consumed(auddec1->circBuf, sampleDuration);

        /* Push the transport buffer to the source pad */
        GST_LOG("pushing buffer to source pad\n");

        if (gst_pad_push(auddec1->srcpad, outBuf) != GST_FLOW_OK) {
            GST_DEBUG("push to source pad failed\n");
            goto thread_failure;
        }
    }

thread_failure:

    /* If encDataWindow is non-NULL, something bad happened before we had a
     * chance to release it.  Release it now so we don't block the pipeline.
     * We release it by telling the circular buffer that we're done with it and
     * consumed no data.
     */
    if (encDataWindow) {
        gst_ticircbuffer_data_consumed(auddec1->circBuf, encDataWindow, 0);
    }

    gst_tithread_set_status(auddec1, TIThread_DECODE_ABORTED);
    threadRet = GstTIThreadFailure;
    gst_ticircbuffer_consumer_aborted(auddec1->circBuf);
    Rendezvous_force(auddec1->waitOnQueueThread);

thread_exit:

    /* Initialize codec engine */
    if (gst_tiauddec1_codec_stop(auddec1) < 0) {
        GST_ERROR("failed to stop codec\n");
    }

    /* Notify main thread if it is waiting on decode thread shut-down */
    auddec1->decodeDrained = TRUE;
    Rendezvous_force(auddec1->waitOnDecodeDrain);

    gst_object_unref(auddec1);

    GST_LOG("exit audio decode_thread (%d)\n", (int)threadRet);
    return threadRet;
}


/******************************************************************************
 * gst_tiauddec1_queue_thread 
 *     Add an input buffer to the circular buffer            
 ******************************************************************************/
static void* gst_tiauddec1_queue_thread(void *arg)
{
    GstTIAuddec1* auddec1    = GST_TIAUDDEC1(gst_object_ref(arg));
    void*        threadRet = GstTIThreadSuccess;
    GstBuffer*   encData;
    Int          fifoRet;

    while (TRUE) {

        /* Get the next input buffer (or block until one is ready) */
        fifoRet = Fifo_get(auddec1->hInFifo, &encData);

        if (fifoRet < 0) {
            GST_ERROR("failed to get buffer from audio thread\n");
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
        if (!gst_ticircbuffer_queue_data(auddec1->circBuf, encData)) {
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
        if (auddec1->drainingEOS && Fifo_getNumEntries(auddec1->hInFifo) == 0) {
            gst_ticircbuffer_drain(auddec1->circBuf, TRUE);
        }   

        /* Unblock any pending puts to our Fifo if we have reached our
         * minimum threshold.
         */
        gst_tiauddec1_broadcast_queue_thread(auddec1);
    }

thread_failure:
    gst_tithread_set_status(auddec1, TIThread_QUEUE_ABORTED);
    threadRet = GstTIThreadFailure;

thread_exit:
    gst_object_unref(auddec1);
    return threadRet;
}


/******************************************************************************
 * gst_tiauddec1_wait_on_queue_thread
 *    Wait for the queuethread to process data
 ******************************************************************************/
static void gst_tiauddec1_wait_on_queue_thread(GstTIAuddec1 *auddec1,
                Int32 waitQueueSize)
{
    auddec1->waitQueueSize = waitQueueSize;
    Rendezvous_meet(auddec1->waitOnQueueThread);
}


/******************************************************************************
 * gst_tiauddec1_broadcast_queue_thread
 *    Broadcast when the queue thread has processed enough buffers from the
 *    fifo to unblock anyone waiting to queue some more.
 ******************************************************************************/
static void gst_tiauddec1_broadcast_queue_thread(GstTIAuddec1 *auddec1)
{
    if (auddec1->waitQueueSize < Fifo_getNumEntries(auddec1->hInFifo)) {
          return;
    } 
    Rendezvous_force(auddec1->waitOnQueueThread);
}


/******************************************************************************
 * gst_tiauddec1_drain_pipeline
 *    Push any remaining input buffers through the queue and decode threads
 ******************************************************************************/
static void gst_tiauddec1_drain_pipeline(GstTIAuddec1 *auddec1)
{
    gboolean checkResult;

    auddec1->drainingEOS = TRUE;

    /* If the processing threads haven't been created, there is nothing to
     * drain.
     */
    if (!gst_tithread_check_status(
             auddec1, TIThread_QUEUE_CREATED, checkResult)) {
        return;
    }
    if (!gst_tithread_check_status(
             auddec1, TIThread_DECODE_CREATED, checkResult)) {
        return;
    }

    /* If the queue fifo still has entries in it, it will drain the
     * circular buffer once all input buffers have been added to the
     * circular buffer.  If the fifo is already empty, we must drain
     * the circular buffer here.
     */
    if (Fifo_getNumEntries(auddec1->hInFifo) == 0) {
        gst_ticircbuffer_drain(auddec1->circBuf, TRUE);
    }
    else {
        Rendezvous_force(auddec1->waitOnQueueThread);
    }

    /* Wait for the decoder to drain */
    if (!auddec1->decodeDrained) {
        Rendezvous_meet(auddec1->waitOnDecodeDrain);
    }
    auddec1->decodeDrained = FALSE;

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
