/*
 * gsttiaudenc1.c
 *
 * This file defines the "TIAudenc1" element, which encodes an xDM 1.x audio
 * stream.
 *
 * Example usage (Encoding a PCM file with playback):
 *     gst-launch filesrc location=<audio file> ! audio/x-raw-int, 
 *         endianness=1234, signed=true, width=16, depth=16, rate=44100, 
 *         channels=2 ! tee name=t ! queue ! TIAudenc1 bitrate=64000 
 *         engineName="<engine name>" codecName="<codec name>" ! filesink 
 *         location="test.aac" t. ! queue ! alsasink sync=false
 *
 * Example usage (Encoding from alsasrc with playback):
 *     gst-launch ${DEBUG} alsasrc num-buffers=2000 ! audio/x-raw-int,
 *         endianness=1234, signed=true, width=16, depth=16, rate=44100, 
 *         channels=2 ! tee name=t ! queue ! TIAudenc1 bitrate=64000 
 *         engineName=encode codecName=aacheenc ! filesink location="test.aac"
 *         t. ! queue ! alsasink sync=false
 *
 * Notes:
 *  * This element currently assumes that the codec produces AAC-HE output.
 *
 * Original Author:
 *     Chase Maupin, Texas Instruments, Inc.
 *
 * Based on TIAuddec1 from:
 *     Brijesh Singh, Texas Instruments, Inc.
 *     Diego Dompe, RidgeRun
 *
 * Copyright (C) 2008-2010 Texas Instruments Incorporated - http://www.ti.com/
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
#include <gst/audio/audio.h>

#include <pthread.h>

#include <ti/sdo/dmai/Dmai.h>
#include <ti/sdo/dmai/Buffer.h>
#include <ti/sdo/dmai/ce/Aenc1.h>

#include "gsttiaudenc1.h"
#include "gsttidmaibuffertransport.h"
#include "gstticodecs.h"
#include "gsttithreadprops.h"
#include "gsttiquicktime_aac.h"
#include "gstticommonutils.h"

/* Enclare variable used to categorize GST_LOG output */
GST_DEBUG_CATEGORY_STATIC (gst_tiaudenc1_debug);
#define GST_CAT_DEFAULT gst_tiaudenc1_debug

/* Element property identifiers */
enum
{
  PROP_0,
  PROP_ENGINE_NAME,     /* engineName       (string)  */
  PROP_CODEC_NAME,      /* codecName        (string)  */
  PROP_NUM_CHANNELS,    /* numChannels      (int)     */
  PROP_BITRATE,         /* bitrate          (int)     */
  PROP_SAMPLEFREQ,      /* sample frequency (int)   */
  PROP_NUM_OUTPUT_BUFS, /* numOutputBufs    (int)     */
  PROP_DISPLAY_BUFFER,  /* displayBuffer    (boolean) */
  PROP_GEN_TIMESTAMPS   /* genTimeStamps    (boolean) */
};

/* Define sink (input) pad capabilities.  Currently, RAW is
 * supported.
 */
static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE(
    "sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS
    ("audio/x-raw-int, "
        "endianness = (int) " G_STRINGIFY (G_BYTE_ORDER) ", "
        "signed = (boolean)true, "
        "width = (int) 16, depth = (int) 16, "
        "rate = (int) {16000, 22050, 24000, 32000, 44100, 48000 }, "
        "channels = (int) [ 1, 2 ]")
);

/* Define source (output) pad capabilities.  Currently, AAC is supported. */
static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE(
    "src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS
    ("audio/mpeg, "
        "mpegversion = (int) { 4 }")
);

/* Enclare a global pointer to our element base class */
static GstElementClass *parent_class = NULL;

/* Static Function Enclarations */
static void
 gst_tiaudenc1_base_init(gpointer g_class);
static void
 gst_tiaudenc1_class_init(GstTIAudenc1Class *g_class);
static void
 gst_tiaudenc1_init(GstTIAudenc1 *object, GstTIAudenc1Class *g_class);
static void
 gst_tiaudenc1_set_property (GObject *object, guint prop_id,
     const GValue *value, GParamSpec *pspec);
static void
 gst_tiaudenc1_get_property (GObject *object, guint prop_id, GValue *value,
     GParamSpec *pspec);
static gboolean
 gst_tiaudenc1_set_sink_caps(GstPad *pad, GstCaps *caps);
static gboolean
 gst_tiaudenc1_set_source_caps(GstTIAudenc1 *audenc1);
static gboolean
 gst_tiaudenc1_sink_event(GstPad *pad, GstEvent *event);
static GstFlowReturn
 gst_tiaudenc1_chain(GstPad *pad, GstBuffer *buf);
static gboolean
 gst_tiaudenc1_init_audio(GstTIAudenc1 *audenc1);
static gboolean
 gst_tiaudenc1_exit_audio(GstTIAudenc1 *audenc1);
static GstStateChangeReturn
 gst_tiaudenc1_change_state(GstElement *element, GstStateChange transition);
static void*
 gst_tiaudenc1_encode_thread(void *arg);
static void
 gst_tiaudenc1_drain_pipeline(GstTIAudenc1 *audenc1);
static gboolean 
    gst_tiaudenc1_codec_start (GstTIAudenc1  *audenc);
static gboolean 
    gst_tiaudenc1_codec_stop (GstTIAudenc1  *audenc1);
static void 
    gst_tiaudenc1_init_env(GstTIAudenc1 *audenc1);

/******************************************************************************
 * gst_tiaudenc1_class_init_trampoline
 *    Boiler-plate function auto-generated by "make_element" script.
 ******************************************************************************/
static void gst_tiaudenc1_class_init_trampoline(gpointer g_class, gpointer data)
{
    parent_class = (GstElementClass*) g_type_class_peek_parent(g_class);
    gst_tiaudenc1_class_init((GstTIAudenc1Class*)g_class);
}

/******************************************************************************
 * gst_tiaudenc1_get_type
 *    Boiler-plate function auto-generated by "make_element" script.
 *    Defines function pointers for initialization routines for this element.
 ******************************************************************************/
GType gst_tiaudenc1_get_type(void)
{
    static GType object_type = 0;

    if (G_UNLIKELY(object_type == 0)) {
        static const GTypeInfo object_info = {
            sizeof(GstTIAudenc1Class),
            gst_tiaudenc1_base_init,
            NULL,
            gst_tiaudenc1_class_init_trampoline,
            NULL,
            NULL,
            sizeof(GstTIAudenc1),
            0,
            (GInstanceInitFunc) gst_tiaudenc1_init
        };

        object_type = g_type_register_static((gst_element_get_type()),
                          "GstTIAudenc1", &object_info, (GTypeFlags)0);

        /* Initialize GST_LOG for this object */
        GST_DEBUG_CATEGORY_INIT(gst_tiaudenc1_debug, "TIAudenc1", 0,
            "TI xDM 1.x Audio Encoder");

        GST_LOG("initialized get_type\n");
    }

    return object_type;
};

/******************************************************************************
 * gst_tiaudenc1_base_init
 *    Boiler-plate function auto-generated by "make_element" script.
 *    Initializes element base class.
 ******************************************************************************/
static void gst_tiaudenc1_base_init(gpointer gclass)
{
    static GstElementDetails element_details = {
        "TI xDM 1.x Audio Encoder",
        "Codec/Encoder/Audio",
        "Encodes audio using an xDM 1.x-based codec",
        "Chase Maupin; Texas Instruments, Inc."
    };

    GstElementClass *element_class = GST_ELEMENT_CLASS(gclass);

    gst_element_class_add_pad_template(element_class,
        gst_static_pad_template_get (&src_factory));
    gst_element_class_add_pad_template(element_class,
        gst_static_pad_template_get (&sink_factory));
    gst_element_class_set_details(element_class, &element_details);
}


/******************************************************************************
 * gst_tiaudenc1_class_init
 *    Boiler-plate function auto-generated by "make_element" script.
 *    Initializes the TIAudenc1 class.
 ******************************************************************************/
static void gst_tiaudenc1_class_init(GstTIAudenc1Class *klass)
{
    GObjectClass    *gobject_class;
    GstElementClass *gstelement_class;

    gobject_class    = (GObjectClass*)    klass;
    gstelement_class = (GstElementClass*) klass;

    gobject_class->set_property = gst_tiaudenc1_set_property;
    gobject_class->get_property = gst_tiaudenc1_get_property;

    gstelement_class->change_state = gst_tiaudenc1_change_state;

    g_object_class_install_property(gobject_class, PROP_ENGINE_NAME,
        g_param_spec_string("engineName", "Engine Name",
            "Engine name used by Codec Engine", "unspecified",
            G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_CODEC_NAME,
        g_param_spec_string("codecName", "Codec Name", "Name of audio codec",
            "unspecified", G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_NUM_CHANNELS,
        g_param_spec_int("numChannels",
            "Number of Channels",
            "Number of audio channels",
            1, G_MAXINT32, 2, G_PARAM_WRITABLE));

    g_object_class_install_property(gobject_class, PROP_BITRATE,
        g_param_spec_int("bitrate", "Bitrate (bps)", "Bitrate in bits/sec",
            8000, 128000, 64000, G_PARAM_WRITABLE));

    g_object_class_install_property(gobject_class, PROP_SAMPLEFREQ,
        g_param_spec_int("samplefreq", "samplefreq (KHz)",
            "Sampe Frequency in KHz", 16000, 48000, 44100, G_PARAM_WRITABLE));

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
 * gst_tiaudenc1_init_env
 *  Initialize element property default by reading environment variables.
 *****************************************************************************/
static void gst_tiaudenc1_init_env(GstTIAudenc1 *audenc1)
{
    GST_LOG("gst_tiaudenc1_init_env - begin");
    
    if (gst_ti_env_is_defined("GST_TI_TIAudenc1_engineName")) {
        audenc1->engineName = gst_ti_env_get_string("GST_TI_TIAudenc1_engineName");
        GST_LOG("Setting engineName=%s\n", audenc1->engineName);
    }

    if (gst_ti_env_is_defined("GST_TI_TIAudenc1_codecName")) {
        audenc1->codecName = gst_ti_env_get_string("GST_TI_TIAudenc1_codecName");
        GST_LOG("Setting codecName=%s\n", audenc1->codecName);
    }
    
    if (gst_ti_env_is_defined("GST_TI_TIAudenc1_numOutputBufs")) {
        audenc1->numOutputBufs = 
                            gst_ti_env_get_int("GST_TI_TIAudenc1_numOutputBufs");
        GST_LOG("Setting numOutputBufs=%ld\n", audenc1->numOutputBufs);
    }

    if (gst_ti_env_is_defined("GST_TI_TIAudenc1_bitrate")) {
        audenc1->bitrate = gst_ti_env_get_int("GST_TI_TIAudenc1_bitrate");
        GST_LOG("Setting bitrate=%d\n", audenc1->bitrate);
    }

    if (gst_ti_env_is_defined("GST_TI_TIAudenc1_channels")) {
        audenc1->channels = gst_ti_env_get_int("GST_TI_TIAudenc1_channels");
        GST_LOG("Setting channels=%d\n", audenc1->channels);
    }

    if (gst_ti_env_is_defined("GST_TI_TIAudenc1_samplefreq")) {
        audenc1->samplefreq = gst_ti_env_get_int("GST_TI_TIAudenc1_samplefreq");
        GST_LOG("Setting samplefreq=%d\n", audenc1->samplefreq);
    }

    if (gst_ti_env_is_defined("GST_TI_TIAudenc1_displayBuffer")) {
        audenc1->displayBuffer = 
                gst_ti_env_get_boolean("GST_TI_TIAudenc1_displayBuffer");
        GST_LOG("Setting displayBuffer=%s\n",
                 audenc1->displayBuffer  ? "TRUE" : "FALSE");
    }
 
    if (gst_ti_env_is_defined("GST_TI_TIAudenc1_genTimeStamps")) {
        audenc1->genTimeStamps = 
                gst_ti_env_get_boolean("GST_TI_TIAudenc1_genTimeStamps");
        GST_LOG("Setting genTimeStamps =%s\n", 
                    audenc1->genTimeStamps ? "TRUE" : "FALSE");
    }

    GST_LOG("gst_tiaudenc1_init_env - end");
}

/******************************************************************************
 * gst_tiaudenc1_init
 *    Initializes a new element instance, instantiates pads and sets the pad
 *    callback functions.
 ******************************************************************************/
static void gst_tiaudenc1_init(GstTIAudenc1 *audenc1, GstTIAudenc1Class *gclass)
{
    /* Instantiate RAW audio sink pad.
     *
     * Fixate on our static template caps instead of writing a getcaps
     * function, which is overkill for this element.
     */
    audenc1->sinkpad =
        gst_pad_new_from_static_template(&sink_factory, "sink");
    gst_pad_set_setcaps_function(
        audenc1->sinkpad, GST_DEBUG_FUNCPTR(gst_tiaudenc1_set_sink_caps));
    gst_pad_set_event_function(
        audenc1->sinkpad, GST_DEBUG_FUNCPTR(gst_tiaudenc1_sink_event));
    gst_pad_set_chain_function(
        audenc1->sinkpad, GST_DEBUG_FUNCPTR(gst_tiaudenc1_chain));
    gst_pad_fixate_caps(audenc1->sinkpad,
        gst_caps_make_writable(
            gst_caps_copy(gst_pad_get_pad_template_caps(audenc1->sinkpad))));

    /* Instantiate encoded audio source pad.
     *
     * Fixate on our static template caps instead of writing a getcaps
     * function, which is overkill for this element.
     */
    audenc1->srcpad =
        gst_pad_new_from_static_template(&src_factory, "src");
    gst_pad_fixate_caps(audenc1->srcpad,
        gst_caps_make_writable(
            gst_caps_copy(gst_pad_get_pad_template_caps(audenc1->srcpad))));

    /* Add pads to TIAudenc1 element */
    gst_element_add_pad(GST_ELEMENT(audenc1), audenc1->sinkpad);
    gst_element_add_pad(GST_ELEMENT(audenc1), audenc1->srcpad);

    /* Initialize TIAudenc1 state */
    audenc1->engineName         = NULL;
    audenc1->codecName          = NULL;
    audenc1->displayBuffer      = FALSE;
    audenc1->genTimeStamps      = TRUE;

    audenc1->hEngine            = NULL;
    audenc1->hAe                = NULL;
    audenc1->channels           = 0;
    audenc1->bitrate            = 0;
    audenc1->samplefreq         = 0;
    audenc1->drainingEOS        = FALSE;
    audenc1->threadStatus       = 0UL;

    audenc1->waitOnEncodeThread = NULL;
    audenc1->waitOnEncodeDrain  = NULL;
    
    audenc1->numOutputBufs      = 0UL;
    audenc1->hOutBufTab         = NULL;
    audenc1->circBuf            = NULL;

    gst_tiaudenc1_init_env(audenc1);
}

/******************************************************************************
 * gst_tiaudenc1_set_property
 *     Set element properties when requested.
 ******************************************************************************/
static void gst_tiaudenc1_set_property(GObject *object, guint prop_id,
                const GValue *value, GParamSpec *pspec)
{
    GstTIAudenc1 *audenc1 = GST_TIAUDENC1(object);

    GST_LOG("begin set_property\n");

    switch (prop_id) {
        case PROP_ENGINE_NAME:
            if (audenc1->engineName) {
                g_free((gpointer)audenc1->engineName);
            }
            audenc1->engineName =
                (gchar*)g_malloc(strlen(g_value_get_string(value)) + 1);
            strcpy((gchar *)audenc1->engineName, g_value_get_string(value));
            GST_LOG("setting \"engineName\" to \"%s\"\n", audenc1->engineName);
            break;
        case PROP_CODEC_NAME:
            if (audenc1->codecName) {
                g_free((gpointer)audenc1->codecName);
            }
            audenc1->codecName =
                (gchar*)g_malloc(strlen(g_value_get_string(value)) + 1);
            strcpy((gchar*)audenc1->codecName, g_value_get_string(value));
            GST_LOG("setting \"codecName\" to \"%s\"\n", audenc1->codecName);
            break;
        case PROP_NUM_CHANNELS:
            audenc1->channels = g_value_get_int(value);
            GST_LOG("setting \"numChannels\" to \"%d\"\n",
                audenc1->channels);
            break;
        case PROP_BITRATE:
            audenc1->bitrate = g_value_get_int(value);
            GST_LOG("setting \"bitrate\" to \"%d\"\n",
                audenc1->bitrate);
            break;
        case PROP_SAMPLEFREQ:
            audenc1->samplefreq = g_value_get_int(value);
            GST_LOG("setting \"samplefreq\" to \"%d\"\n",
                audenc1->samplefreq);
            break;
        case PROP_NUM_OUTPUT_BUFS:
            audenc1->numOutputBufs = g_value_get_int(value);
            GST_LOG("setting \"numOutputBufs\" to \"%ld\"\n",
                audenc1->numOutputBufs);
            break;
        case PROP_DISPLAY_BUFFER:
            audenc1->displayBuffer = g_value_get_boolean(value);
            GST_LOG("setting \"displayBuffer\" to \"%s\"\n",
                audenc1->displayBuffer ? "TRUE" : "FALSE");
            break;
        case PROP_GEN_TIMESTAMPS:
            audenc1->genTimeStamps = g_value_get_boolean(value);
            GST_LOG("setting \"genTimeStamps\" to \"%s\"\n",
                audenc1->genTimeStamps ? "TRUE" : "FALSE");
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }

    GST_LOG("end set_property\n");
}

/******************************************************************************
 * gst_tiaudenc1_get_property
 *     Return values for requested element property.
 ******************************************************************************/
static void gst_tiaudenc1_get_property(GObject *object, guint prop_id,
                GValue *value, GParamSpec *pspec)
{
    GstTIAudenc1 *audenc1 = GST_TIAUDENC1(object);

    GST_LOG("begin get_property\n");

    switch (prop_id) {
        case PROP_ENGINE_NAME:
            g_value_set_string(value, audenc1->engineName);
            break;
        case PROP_CODEC_NAME:
            g_value_set_string(value, audenc1->codecName);
            break;
        case PROP_BITRATE:
            g_value_set_int(value, audenc1->bitrate);
            break;
        case PROP_SAMPLEFREQ:
            g_value_set_int(value, audenc1->samplefreq);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }

    GST_LOG("end get_property\n");
}

/******************************************************************************
 * gst_tiaudenc1_set_sink_caps
 *     Negotiate our sink pad capabilities.
 ******************************************************************************/
static gboolean gst_tiaudenc1_set_sink_caps(GstPad *pad, GstCaps *caps)
{
    GstTIAudenc1  *audenc1;
    GstStructure *capStruct;
    const gchar  *mime;
    char         *string;
    gint         samplefreq;
    gint         channels;

    audenc1    = GST_TIAUDENC1(gst_pad_get_parent(pad));
    capStruct = gst_caps_get_structure(caps, 0);
    mime      = gst_structure_get_name(capStruct);

    string = gst_caps_to_string(caps);
    GST_INFO("requested sink caps:  %s", string);
    g_free(string);

    /* Generic Audio Properties */
    if (!strncmp(mime, "audio/", 6)) {
        if (!gst_structure_get_int(capStruct, "rate", &samplefreq)) {
            samplefreq = 0;
        }

        if (!gst_structure_get_int(capStruct, "channels", &channels)) {
            channels = 0;
        }
    }

    /* Mime type not supported */
    else {
        GST_ELEMENT_ERROR(audenc1, STREAM, NOT_IMPLEMENTED, ("stream type not supported"), (NULL));
        gst_object_unref(audenc1);
        return FALSE;
    }

    /* Shut-down any running audio encoder */
    if (!gst_tiaudenc1_exit_audio(audenc1)) {
        gst_object_unref(audenc1);
        return FALSE;
    }

    /* Configure the element to use the detected samplefreq and channels, unless
     * they have been set using the set_property function.
     */
    if (!audenc1->samplefreq) {
        audenc1->samplefreq = samplefreq;
    }
    if (!audenc1->channels) {
        audenc1->channels = channels;
    }

    /* If we're still showing 0 channels, we were not able to determine the
     * number of channels from the input stream.  The channels will be
     * set to the default in gst_tiaudenc1_codec_start so print a
     * warning for now. 
     */
    if (audenc1->channels == 0) {
        GST_WARNING("Could not determine the number of channels in the "
            "input stream; Using default values.");
    }

    gst_object_unref(audenc1);

    GST_LOG("sink caps negotiation successful\n");
    return TRUE;
}


/******************************************************************************
 * gst_tiaudenc1_set_source_caps
 *     Negotiate our source pad capabilities.
 ******************************************************************************/
static gboolean gst_tiaudenc1_set_source_caps(GstTIAudenc1* audenc1)
{
    GstCaps  *caps;
    gboolean  ret;
    char     *string;
    GstTICodec *aacCodec = NULL;

    aacCodec = gst_ticodec_get_codec("AAC Audio Encoder");

    /* Create AAC source caps */
    if (aacCodec && (!strcmp(aacCodec->CE_CodecName, audenc1->codecName))) {
        caps =
            gst_caps_new_simple ("audio/mpeg",
                "mpegversion", G_TYPE_INT, 4,
                "channels", G_TYPE_INT, audenc1->channels,
                "rate", G_TYPE_INT, audenc1->samplefreq,
                "bitrate", G_TYPE_INT, audenc1->bitrate,
                NULL);

        /* Set the source pad caps */
        string = gst_caps_to_string(caps);
        GST_INFO("setting source caps to AAC:  %s", string);
        g_free(string);
    } else {
        GST_ELEMENT_ERROR(audenc1, STREAM, CODEC_NOT_FOUND,
        ("Unknown codecName (%s).  Failed to set src caps\n",
        audenc1->codecName), (NULL));
        return FALSE;
    }

    if (!gst_pad_set_caps(audenc1->srcpad, caps)) {
        ret = FALSE;
    }
    gst_caps_unref(caps);

    return ret;
}

/******************************************************************************
 * gst_tiaudenc1_sink_event
 *     Perform event processing on the input stream.  At the moment, this
 *     function isn't needed as this element doesn't currently perform any
 *     specialized event processing.  We'll leave it in for now in case we need
 *     it later on.
 ******************************************************************************/
static gboolean gst_tiaudenc1_sink_event(GstPad *pad, GstEvent *event)
{
    GstTIAudenc1 *audenc1;
    gboolean     ret;

    audenc1 = GST_TIAUDENC1(GST_OBJECT_PARENT(pad));

    GST_DEBUG("pad \"%s\" received:  %s\n", GST_PAD_NAME(pad),
        GST_EVENT_TYPE_NAME(event));

    switch (GST_EVENT_TYPE(event)) {

        case GST_EVENT_NEWSEGMENT:
            /* Propagate NEWSEGMENT to downstream elements */
            ret = gst_pad_push_event(audenc1->srcpad, event);
            break;

        case GST_EVENT_EOS:
            /* end-of-stream: process any remaining encoded frame data */
            GST_LOG("no more input; draining remaining encoded audio data\n");
            if (!audenc1->drainingEOS) {
               gst_tiaudenc1_drain_pipeline(audenc1);
            }

            /* Propagate EOS to downstream elements */
            ret = gst_pad_push_event(audenc1->srcpad, event);
            break;

        case GST_EVENT_FLUSH_STOP:
            ret = gst_pad_push_event(audenc1->srcpad, event);
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
 * gst_tiaudenc1_chain
 *    This is the main processing routine.  This function receives a buffer
 *    from the sink pad, processes it, and pushes the result to the source
 *    pad.
 ******************************************************************************/
static GstFlowReturn gst_tiaudenc1_chain(GstPad * pad, GstBuffer * buf)
{
    GstTIAudenc1  *audenc1 = GST_TIAUDENC1(GST_OBJECT_PARENT(pad));
    GstFlowReturn  flow    = GST_FLOW_OK;
    gboolean       checkResult;

    /* If the encode thread aborted, signal it to let it know it's ok to
     * shut down, and communicate the failure to the pipeline.
     */
    if (gst_tithread_check_status(audenc1, TIThread_CODEC_ABORTED,
            checkResult)) {
       flow = GST_FLOW_UNEXPECTED;
       goto exit;
    }

    /* If our engine handle is currently NULL, then either this is our first
     * buffer or the upstream element has re-negotiated our capabilities which
     * resulted in our engine being closed.  In either case, we need to
     * initialize (or re-initialize) our audio encoder to handle the new
     * stream.
     */
    if (audenc1->hEngine == NULL) {
        if (!gst_tiaudenc1_init_audio(audenc1)) {
            GST_ELEMENT_ERROR(audenc1, RESOURCE, FAILED,
            ("Unable to initialize audio\n"), (NULL));
            flow = GST_FLOW_UNEXPECTED;
            goto exit;
        }

        GST_TICIRCBUFFER_TIMESTAMP(audenc1->circBuf) =
            GST_CLOCK_TIME_IS_VALID(GST_BUFFER_TIMESTAMP(buf)) ?
            GST_BUFFER_TIMESTAMP(buf) : 0ULL;
    }

    /* Queue up the encoded data stream into a circular buffer */
    if (!gst_ticircbuffer_queue_data(audenc1->circBuf, buf)) {
        GST_ELEMENT_ERROR(audenc1, RESOURCE, WRITE,
        ("Failed to queue input buffer into circular buffer\n"), (NULL));
        flow = GST_FLOW_UNEXPECTED;
        goto exit;
    }

exit:
    gst_buffer_unref(buf);
    return flow;
}


/******************************************************************************
 * gst_tiaudenc1_init_audio
 *     Initialize or re-initializes the audio stream
 ******************************************************************************/
static gboolean gst_tiaudenc1_init_audio(GstTIAudenc1 * audenc1)
{
    Rendezvous_Attrs      rzvAttrs  = Rendezvous_Attrs_DEFAULT;
    struct sched_param    schedParam;
    pthread_attr_t        attr;

    GST_LOG("begin init_audio\n");

    /* If audio has already been initialized, shut down previous encoder */
    if (audenc1->hEngine) {
        if (!gst_tiaudenc1_exit_audio(audenc1)) {
            GST_ELEMENT_ERROR(audenc1, RESOURCE, FAILED,
            ("Failed to shut down existing audio encoder\n"), (NULL));
            return FALSE;
        }
    }

    /* Make sure we know what codec we're using */
    if (!audenc1->engineName) {
        GST_ELEMENT_ERROR(audenc1, RESOURCE, FAILED,
        ("Engine name not specified\n"), (NULL));
        return FALSE;
    }

    if (!audenc1->codecName) {
        GST_ELEMENT_ERROR(audenc1, RESOURCE, FAILED,
        ("Codec name not specified\n"), (NULL));
        return FALSE;
    }

    /* Initialize thread status management */
    audenc1->threadStatus = 0UL;
    pthread_mutex_init(&audenc1->threadStatusMutex, NULL);

    /* Initialize rendezvous objects for making threads wait on conditions */
    audenc1->waitOnEncodeThread = Rendezvous_create(2, &rzvAttrs);
    audenc1->waitOnEncodeDrain  = Rendezvous_create(100, &rzvAttrs);
    audenc1->drainingEOS        = FALSE;

    /* Initialize the custom thread attributes */
    if (pthread_attr_init(&attr)) {
        GST_WARNING("failed to initialize thread attrs\n");
        gst_tiaudenc1_exit_audio(audenc1);
        return FALSE;
    }

    /* Force the thread to use the system scope */
    if (pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM)) {
        GST_WARNING("failed to set scope attribute\n");
        gst_tiaudenc1_exit_audio(audenc1);
        return FALSE;
    }

    /* Force the thread to use custom scheduling attributes */
    if (pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED)) {
        GST_WARNING("failed to set schedule inheritance attribute\n");
        gst_tiaudenc1_exit_audio(audenc1);
        return FALSE;
    }

    /* Set the thread to be fifo real time scheduled */
    if (pthread_attr_setschedpolicy(&attr, SCHED_FIFO)) {
        GST_WARNING("failed to set FIFO scheduling policy\n");
        gst_tiaudenc1_exit_audio(audenc1);
        return FALSE;
    }

    /* Set the display thread priority */
    schedParam.sched_priority = GstTIAudioThreadPriority;
    if (pthread_attr_setschedparam(&attr, &schedParam)) {
        GST_WARNING("failed to set scheduler parameters\n");
        return FALSE;
    }

    /* Create encoder thread */
    if (pthread_create(&audenc1->encodeThread, &attr,
            gst_tiaudenc1_encode_thread, (void*)audenc1)) {
        GST_ELEMENT_ERROR(audenc1, RESOURCE, FAILED,
        ("Failed to create encode thread\n"), (NULL));
        gst_tiaudenc1_exit_audio(audenc1);
        return FALSE;
    }
    gst_tithread_set_status(audenc1, TIThread_CODEC_CREATED);

    /* Destroy the custom thread attributes */
    if (pthread_attr_destroy(&attr)) {
        GST_WARNING("failed to destroy thread attrs\n");
        gst_tiaudenc1_exit_audio(audenc1);
        return FALSE;
    }

    /* Make sure circular buffer and display buffer handles are created by
     * encoder thread.
     */
    Rendezvous_meet(audenc1->waitOnEncodeThread);

    if (audenc1->circBuf == NULL || audenc1->hOutBufTab == NULL) {
        GST_ELEMENT_ERROR(audenc1, RESOURCE, FAILED,
        ("Encode thread failed to create circbuf or display buffer handles\n"),
        (NULL));
        return FALSE;
    }

    GST_LOG("end init_audio\n");
    return TRUE;
}


/******************************************************************************
 * gst_tiaudenc1_exit_audio
 *    Shut down any running audio encoder, and reset the element state.
 ******************************************************************************/
static gboolean gst_tiaudenc1_exit_audio(GstTIAudenc1 *audenc1)
{
    gboolean checkResult;
    void*    thread_ret;

    GST_LOG("begin exit_audio\n");

    /* Drain the pipeline if it hasn't already been drained */
    if (!audenc1->drainingEOS) {
       gst_tiaudenc1_drain_pipeline(audenc1);
     }

    /* Shut down the encode thread */
    if (gst_tithread_check_status(
            audenc1, TIThread_CODEC_CREATED, checkResult)) {
        GST_LOG("shutting down encode thread\n");

        Rendezvous_force(audenc1->waitOnEncodeThread);
        if (pthread_join(audenc1->encodeThread, &thread_ret) == 0) {
            if (thread_ret == GstTIThreadFailure) {
                GST_DEBUG("encode thread exited with an error condition\n");
            }
        }
    }

    /* Shut down thread status management */
    audenc1->threadStatus = 0UL;
    pthread_mutex_destroy(&audenc1->threadStatusMutex);

    /* Shut down remaining items */
    if (audenc1->waitOnEncodeDrain) {
        Rendezvous_delete(audenc1->waitOnEncodeDrain);
        audenc1->waitOnEncodeDrain = NULL;
    }

    if (audenc1->waitOnEncodeThread) {
        Rendezvous_delete(audenc1->waitOnEncodeThread);
        audenc1->waitOnEncodeThread = NULL;
    }

    GST_LOG("end exit_audio\n");
    return TRUE;
}


/******************************************************************************
 * gst_tiaudenc1_change_state
 *     Manage state changes for the audio stream.  The gStreamer documentation
 *     states that state changes must be handled in this manner:
 *        1) Handle ramp-up states
 *        2) Pass state change to base class
 *        3) Handle ramp-down states
 ******************************************************************************/
static GstStateChangeReturn gst_tiaudenc1_change_state(GstElement *element,
                                GstStateChange transition)
{
    GstStateChangeReturn  ret    = GST_STATE_CHANGE_SUCCESS;
    GstTIAudenc1          *audenc1 = GST_TIAUDENC1(element);

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
            /* Shut down any running audio encoder */
            if (!gst_tiaudenc1_exit_audio(audenc1)) {
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
 * gst_tiaudenc1_codec_stop
 *     Release codec engine resources
 *****************************************************************************/
static gboolean gst_tiaudenc1_codec_stop (GstTIAudenc1  *audenc1)
{
    if (audenc1->circBuf) {
        GstTICircBuffer *circBuf;

        GST_LOG("freeing cicrular input buffer\n");

        circBuf                = audenc1->circBuf;
        audenc1->circBuf       = NULL;
        gst_ticircbuffer_unref(circBuf);
    }

    if (audenc1->hOutBufTab) {
        GST_LOG("freeing output buffers\n");
        gst_tidmaibuftab_unref(audenc1->hOutBufTab);
        audenc1->hOutBufTab = NULL;
    }

    if (audenc1->hAe) {
        GST_LOG("closing audio encoder\n");
        Aenc1_delete(audenc1->hAe);
        audenc1->hAe = NULL;
    }

    if (audenc1->hEngine) {
        GST_LOG("closing codec engine\n");
        Engine_close(audenc1->hEngine);
        audenc1->hEngine = NULL;
    }

    return TRUE;
}


/******************************************************************************
 * gst_tiaudenc1_codec_start
 *     Initialize codec engine
 *****************************************************************************/
static gboolean gst_tiaudenc1_codec_start (GstTIAudenc1  *audenc1)
{
    AUDENC1_Params          params    = Aenc1_Params_DEFAULT;
    AUDENC1_DynamicParams   dynParams = Aenc1_DynamicParams_DEFAULT;
    Buffer_Attrs            bAttrs    = Buffer_Attrs_DEFAULT;

    /* Override the default parameters to use the defaults specified or the
     * user settings.
     * Order of setting is:
     *    1.  Parameters set on command line
     *    2.  Settings detected during caps negotiation
     *    3.  Default values defined in gsttiaudenc1.h
     */
    params.sampleRate = audenc1->samplefreq == 0 ? TIAUDENC1_SAMPLEFREQ_DEFAULT:
                            audenc1->samplefreq;
    params.bitRate = audenc1->bitrate = audenc1->bitrate == 0 ? 
                            TIAUDENC1_BITRATE_DEFAULT : audenc1->bitrate;
    params.channelMode = audenc1->channels == 0 ? params.channelMode : 
                            audenc1->channels == 1 ? IAUDIO_1_0 : IAUDIO_2_0;

    /* Initialize dynamic parameters */
    dynParams.sampleRate = params.sampleRate;
    dynParams.bitRate = params.bitRate;
    dynParams.channelMode = params.channelMode;

    /* Open the codec engine */
    GST_LOG("opening codec engine \"%s\"\n", audenc1->engineName);
    audenc1->hEngine = Engine_open((Char *) audenc1->engineName, NULL, NULL);

    if (audenc1->hEngine == NULL) {
        GST_ELEMENT_ERROR(audenc1, RESOURCE, READ,
        ("Failed to open codec engine \"%s\"\n", audenc1->engineName), (NULL));
        return FALSE;
    }

    /* Initialize audio encoder */
    GST_LOG("opening audio encoder \"%s\"\n", audenc1->codecName);
    audenc1->hAe = Aenc1_create(audenc1->hEngine, (Char*)audenc1->codecName,
                      &params, &dynParams);

    if (audenc1->hAe == NULL) {
        GST_ELEMENT_ERROR(audenc1, RESOURCE, FAILED,
        ("Failed to create audio encoder: %s\n", audenc1->codecName), (NULL));
        GST_ELEMENT_ERROR(audenc1, RESOURCE, FAILED,
        ("This may be caused by specifying channels/bitrate "
         "combinations that are too high for your codec.  Please "
         "make sure that channels * bitrate does not exceed the max "
         "bitrate supported by your codec.  Current settings are:\n"
         "\tbitrate = %d\n\tchannels = %d\n\ttotal bitrate = %d\n",
         audenc1->bitrate, audenc1->channels,
         audenc1->bitrate * audenc1->channels), (NULL));
        GST_LOG("closing codec engine\n");
        return FALSE;
    }

    /* Set up a circular input buffer capable of holding 3 RAW frames */
    audenc1->circBuf = gst_ticircbuffer_new(
                            Aenc1_getInBufSize(audenc1->hAe), 3, FALSE);

    if (audenc1->circBuf == NULL) {
        GST_ELEMENT_ERROR(audenc1, RESOURCE, NO_SPACE_LEFT,
        ("Failed to create circular input buffer\n"), (NULL));
        return FALSE;
    }

    /* Display buffer contents if displayBuffer=TRUE was specified */
    gst_ticircbuffer_set_display(audenc1->circBuf, audenc1->displayBuffer);

    /* Define the number of display buffers to allocate.  This number must be
     * at least 2, If this has not been set via set_property(), default to the
     * minimal value.
      */
    if (audenc1->numOutputBufs == 0) {
        audenc1->numOutputBufs = 2;
    }

    /* Create codec output buffers.  
     */
    GST_LOG("creating output buffers\n");
      
    /* By default, new buffers are marked as in-use by the codec */
    bAttrs.useMask = gst_tidmaibuffer_CODEC_FREE;

    audenc1->hOutBufTab = gst_tidmaibuftab_new(audenc1->numOutputBufs, 
        Aenc1_getOutBufSize(audenc1->hAe), &bAttrs);

    if (audenc1->hOutBufTab == NULL) {
        GST_ELEMENT_ERROR(audenc1, RESOURCE, NO_SPACE_LEFT,
        ("Failed to create output buffer\n"), (NULL));
        return FALSE;
    }

    return TRUE;
}


/******************************************************************************
 * gst_tiaudenc1_encode_thread
 *     Call the audio codec to process a full input buffer
 ******************************************************************************/
static void* gst_tiaudenc1_encode_thread(void *arg)
{
    GstTIAudenc1   *audenc1    = GST_TIAUDENC1(gst_object_ref(arg));
    void          *threadRet = GstTIThreadSuccess;
    Buffer_Handle  hDstBuf;
    Int32          encDataConsumed;
    GstBuffer     *encDataWindow = NULL;
    GstClockTime   encDataTime;
    Buffer_Handle  hEncDataWindow;
    GstBuffer     *outBuf;
    GstClockTime   sampleDuration;
    guint          sampleRate;
    guint          numSamples;
    Int            bufIdx;
    Int            ret;

    GST_LOG("starting audenc encode thread\n");

    /* Initialize codec engine */
    ret = gst_tiaudenc1_codec_start(audenc1);

    /* Notify main thread that it is ok to continue initialization */
    Rendezvous_meet(audenc1->waitOnEncodeThread);
    Rendezvous_reset(audenc1->waitOnEncodeThread);

    if (ret == FALSE) {
        GST_ELEMENT_ERROR(audenc1, RESOURCE, FAILED,
        ("Failed to start codec\n"), (NULL));
        goto thread_exit;
    }

    while (TRUE) {

        /* Obtain an raw data frame */
        encDataWindow  = gst_ticircbuffer_get_data(audenc1->circBuf);
        encDataTime    = GST_BUFFER_TIMESTAMP(encDataWindow);
        hEncDataWindow = GST_TIDMAIBUFFERTRANSPORT_DMAIBUF(encDataWindow);

        /* Check if there is enough encoded data to be sent to the codec.
         * The last frame of data may not be sufficient to meet the codec
         * requirements for the amount of input data.  If so just throw
         * away the last bit of data rather than filling with bogus
         * data.
         */
        if (GST_BUFFER_SIZE(encDataWindow) <
            Aenc1_getInBufSize(audenc1->hAe)) {
            GST_LOG("Not enough audio data remains\n");
            if (!audenc1->drainingEOS) {
                goto thread_failure;
            }
            goto thread_exit;
        }

        /* Obtain a free output buffer for the encoded data */
        if (!(hDstBuf = gst_tidmaibuftab_get_buf(audenc1->hOutBufTab))) {
            GST_ELEMENT_ERROR(audenc1, RESOURCE, READ,
                ("Failed to get a free contiguous buffer from BufTab\n"),
                (NULL));
            goto thread_exit;
        }

        /* Invoke the audio encoder */
        GST_LOG("Invoking the audio encoder at 0x%08lx with %u bytes\n",
            (unsigned long)Buffer_getUserPtr(hEncDataWindow),
            GST_BUFFER_SIZE(encDataWindow));
        ret             = Aenc1_process(audenc1->hAe, hEncDataWindow, hDstBuf);
        encDataConsumed = Buffer_getNumBytesUsed(hEncDataWindow);

        if (ret < 0) {
            GST_ELEMENT_ERROR(audenc1, STREAM, ENCODE,
            ("Failed to encode audio buffer\n"), (NULL));
            goto thread_failure;
        }

        /* If no encoded data was used we cannot find the next frame */
        if (ret == Dmai_EBITERROR && encDataConsumed == 0) {
            GST_ELEMENT_ERROR(audenc1, STREAM, ENCODE,
            ("Fatal bit error\n"), (NULL));
            goto thread_failure;
        }

        if (ret > 0) {
            GST_LOG("Aenc1_process returned success code %d\n", ret); 
        }

        sampleRate     = audenc1->samplefreq;
        numSamples     = encDataConsumed / (2 * audenc1->channels) ;
        sampleDuration = GST_FRAMES_TO_CLOCK_TIME(numSamples, sampleRate);

        /* Release the reference buffer, and tell the circular buffer how much
         * data was consumed.
         */
        ret = gst_ticircbuffer_data_consumed(audenc1->circBuf, encDataWindow,
                  encDataConsumed);
        encDataWindow = NULL;

        if (!ret) {
            goto thread_failure;
        }

        /* Set the source pad capabilities based on the encoded frame
         * properties.
         */
        gst_tiaudenc1_set_source_caps(audenc1);

        /* Create a DMAI transport buffer object to carry a DMAI buffer to
         * the source pad.  The transport buffer knows how to release the
         * buffer for re-use in this element when the source pad calls
         * gst_buffer_unref().
         */
        outBuf = gst_tidmaibuffertransport_new(hDstBuf, audenc1->hOutBufTab);
        gst_buffer_set_data(outBuf, GST_BUFFER_DATA(outBuf),
            Buffer_getNumBytesUsed(hDstBuf));
        gst_buffer_set_caps(outBuf, GST_PAD_CAPS(audenc1->srcpad));

        /* Set timestamp on output buffer */
        if (audenc1->genTimeStamps) {
            GST_BUFFER_DURATION(outBuf)     = sampleDuration;
            GST_BUFFER_TIMESTAMP(outBuf)    = encDataTime;
        }
        else {
            GST_BUFFER_TIMESTAMP(outBuf)    = GST_CLOCK_TIME_NONE;
        }

        /* Tell circular buffer how much time we consumed */
        gst_ticircbuffer_time_consumed(audenc1->circBuf, sampleDuration);

        /* Push the transport buffer to the source pad */
        GST_LOG("pushing buffer to source pad with timestamp : %"
                GST_TIME_FORMAT ", duration: %" GST_TIME_FORMAT,
                GST_TIME_ARGS (GST_BUFFER_TIMESTAMP(outBuf)),
                GST_TIME_ARGS (GST_BUFFER_DURATION(outBuf)));

        if (gst_pad_push(audenc1->srcpad, outBuf) != GST_FLOW_OK) {
            GST_DEBUG("push to source pad failed\n");
            goto thread_failure;
        }

        /* Release buffers no longer in use by the codec */
        Buffer_freeUseMask(hDstBuf, gst_tidmaibuffer_CODEC_FREE);
    }

thread_failure:

    gst_tithread_set_status(audenc1, TIThread_CODEC_ABORTED);
    gst_ticircbuffer_consumer_aborted(audenc1->circBuf);
    threadRet = GstTIThreadFailure;

thread_exit:

    /* Re-claim any buffers owned by the codec */
    bufIdx = BufTab_getNumBufs(GST_TIDMAIBUFTAB_BUFTAB(audenc1->hOutBufTab));

    while (bufIdx-- > 0) {
        Buffer_Handle hBuf = BufTab_getBuf(
            GST_TIDMAIBUFTAB_BUFTAB(audenc1->hOutBufTab), bufIdx);
        Buffer_freeUseMask(hBuf, gst_tidmaibuffer_CODEC_FREE);
    }

    /* Release the last buffer we retrieved from the circular buffer */
    if (encDataWindow) {
        gst_ticircbuffer_data_consumed(audenc1->circBuf, encDataWindow, 0);
    }

    /* We have to wait to shut down this thread until we can guarantee that
     * no more input buffers will be queued into the circular buffer
     * (we're about to delete it).  
     */
    Rendezvous_meet(audenc1->waitOnEncodeThread);
    Rendezvous_reset(audenc1->waitOnEncodeThread);

    /* Notify main thread that we are done draining before we shutdown the
     * codec, or we will hang.  We proceed in this order so the EOS event gets
     * propagated downstream before we attempt to shut down the codec.  The
     * codec-shutdown process will block until all BufTab buffers have been
     * released, and downstream-elements may hang on to buffers until
     * they get the EOS.
     */
    Rendezvous_force(audenc1->waitOnEncodeDrain);

    /* Initialize codec engine */
    if (gst_tiaudenc1_codec_stop(audenc1) < 0) {
        GST_ERROR("failed to stop codec\n");
        GST_ELEMENT_ERROR(audenc1, RESOURCE, FAILED,
        ("Failed to stop codec\n"), (NULL));
    }

    gst_object_unref(audenc1);

    GST_LOG("exit audio encode_thread (%d)\n", (int)threadRet);
    return threadRet;
}


/******************************************************************************
 * gst_tiaudenc1_drain_pipeline
 *    Wait for the encode thread to finish processing queued input data.
 ******************************************************************************/
static void gst_tiaudenc1_drain_pipeline(GstTIAudenc1 *audenc1)
{
    gboolean checkResult;

    /* If the encode thread hasn't been created, there is nothing to drain. */
    if (!gst_tithread_check_status(
             audenc1, TIThread_CODEC_CREATED, checkResult)) {
        return;
    }

    audenc1->drainingEOS = TRUE;
    gst_ticircbuffer_drain(audenc1->circBuf, TRUE);

    /* Tell the encode thread that it is ok to shut down */
    Rendezvous_force(audenc1->waitOnEncodeThread);

    /* Wait for the encoder to finish draining */
    Rendezvous_meet(audenc1->waitOnEncodeDrain);

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
