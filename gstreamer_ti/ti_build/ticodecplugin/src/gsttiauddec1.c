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
  PROP_NUM_CHANNELS,    /* numChannels    (int)     */
  PROP_NUM_OUTPUT_BUFS, /* numOutputBufs  (int)     */
  PROP_DISPLAY_BUFFER,  /* displayBuffer  (boolean) */
  PROP_GEN_TIMESTAMPS,  /* genTimeStamps  (boolean) */
  PROP_RTCODECTHREAD    /* rtCodecThread  (boolean) */
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
        "mpegversion = (int) { 1, 2, 4 };"
     "audio/x-eac3")
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
 gst_tiauddec1_set_source_caps(GstTIAuddec1 *auddec1);
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
static void
 gst_tiauddec1_drain_pipeline(GstTIAuddec1 *auddec1);
static gboolean 
    gst_tiauddec1_codec_start (GstTIAuddec1  *auddec);
static gboolean 
    gst_tiauddec1_codec_stop (GstTIAuddec1  *auddec1);
static void 
    gst_tiauddec1_init_env(GstTIAuddec1 *auddec1);
static void
    gst_tiauddec1_dispose(GObject * object);
static gboolean 
    gst_tiauddec1_set_query_pad(GstPad * pad, GstQuery * query);
static gboolean
    gst_tiauddec1_codec_is_aac(GstTIAuddec1 *auddec1);

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
 * gst_tiauddec_dispose
 *****************************************************************************/
static void gst_tiauddec1_dispose(GObject * object)
{
    GstTIAuddec1 *auddec1 = GST_TIAUDDEC1(object);

    if (auddec1->segment) {
        gst_segment_free(auddec1->segment);
        auddec1->segment = NULL;
    }

    G_OBJECT_CLASS(parent_class)->dispose (object);
}


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
    gobject_class->dispose      = GST_DEBUG_FUNCPTR (gst_tiauddec1_dispose);

    gstelement_class->change_state = gst_tiauddec1_change_state;

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

    g_object_class_install_property(gobject_class, PROP_NUM_OUTPUT_BUFS,
        g_param_spec_int("numOutputBufs",
            "Number of Ouput Buffers",
            "Number of output buffers to allocate for codec",
            2, G_MAXINT32, 2, G_PARAM_WRITABLE));

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

    if (gst_ti_env_is_defined("GST_TI_TIAuddec1_RTCodecThread")) {
        auddec1->rtCodecThread = 
                gst_ti_env_get_boolean("GST_TI_TIAuddec1_RTCodecThread");
        GST_LOG("Setting RTCodecThread =%s\n", 
                    auddec1->rtCodecThread ? "TRUE" : "FALSE");
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
    gst_pad_set_query_function(auddec1->srcpad,
            GST_DEBUG_FUNCPTR(gst_tiauddec1_set_query_pad));

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

    auddec1->waitOnDecodeThread = NULL;
    auddec1->waitOnDecodeDrain  = NULL;
    
    auddec1->numOutputBufs      = 0UL;
    auddec1->hOutBufTab         = NULL;
    auddec1->circBuf            = NULL;

    auddec1->aac_header_data    = NULL;

    auddec1->segment            = gst_segment_new();
    auddec1->totalDuration      = 0;
    auddec1->totalBytes         = 0;
    auddec1->sampleRate         = 0;

    auddec1->rtCodecThread      = TRUE;

    gst_tiauddec1_init_env(auddec1);
}

/******************************************************************************
 * gst_tiauddec_set_query_pad
 *   This function reuturn stream duration and position.
 *****************************************************************************/
static gboolean gst_tiauddec1_set_query_pad(GstPad *pad, GstQuery *query)
{
    GstTIAuddec1  *auddec1;
    gboolean     ret = FALSE;

    auddec1    = GST_TIAUDDEC1(gst_pad_get_parent(pad));
   
    ret = gst_ti_query_srcpad(pad, query, auddec1->sinkpad, 
             auddec1->totalDuration, auddec1->totalBytes);

    gst_object_unref(auddec1);

    return ret;
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
        case PROP_NUM_CHANNELS:
            auddec1->channels = g_value_get_int(value);
            GST_LOG("setting \"numChannels\" to \"%d\"\n",
                auddec1->channels);
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
        case PROP_RTCODECTHREAD:
            auddec1->rtCodecThread = g_value_get_boolean(value);
            GST_LOG("setting \"RTCodecThread\" to \"%s\"\n",
                auddec1->rtCodecThread ? "TRUE" : "FALSE");
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

    auddec1    = GST_TIAUDDEC1(gst_pad_get_parent(pad));
    capStruct = gst_caps_get_structure(caps, 0);
    mime      = gst_structure_get_name(capStruct);

    string = gst_caps_to_string(caps);
    GST_INFO("requested sink caps:  %s", string);
    g_free(string);

    /* Shut-down any running audio decoder */
    if (!gst_tiauddec1_exit_audio(auddec1)) {
        gst_object_unref(auddec1);
        return FALSE;
    }

    /* Generic Audio Properties */
    if (!strncmp(mime, "audio/", 6)) {

        if (!gst_structure_get_int(capStruct, "rate", &auddec1->sampleRate)) {
            auddec1->sampleRate = 0;
        }

        if (!gst_structure_get_int(capStruct, "channels", &auddec1->channels)) {
            auddec1->channels = 0;
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
        else if (mpegversion == 4 || mpegversion == 2) { 
            codec = gst_ticodec_get_codec("AAC Audio Decoder");
            auddec1->aac_header_data = gst_aac_header_create( 
                            auddec1->sampleRate, auddec1->channels);
        }

        /* MPEG version not supported */
        else {
            GST_ELEMENT_ERROR(auddec1, STREAM, NOT_IMPLEMENTED,
            ("MPEG version not supported"), (NULL));
            gst_object_unref(auddec1);
            return FALSE;
        }
    }

    /* AC3 Audio */
    else if (!strcmp(mime, "audio/x-eac3")) {
        codec = gst_ticodec_get_codec("AC3 Audio Decoder");
    }

    /* Mime type not supported */
    else {
        GST_ELEMENT_ERROR(auddec1, STREAM, NOT_IMPLEMENTED,
        ("stream type not supported"), (NULL));
        gst_object_unref(auddec1);
        return FALSE;
    }

    /* Report if the required codec was not found */
    if (!codec) {
        GST_ELEMENT_ERROR(auddec1, STREAM, CODEC_NOT_FOUND,
        ( "unable to find codec needed for stream"), (NULL));
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
static gboolean gst_tiauddec1_set_source_caps(GstTIAuddec1* auddec1)
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
            "rate",       G_TYPE_INT,     auddec1->sampleRate,
            "channels",   G_TYPE_INT,     auddec1->channels,
            NULL);

    /* Set the source pad caps */
    string = gst_caps_to_string(caps);
    GST_LOG("setting source caps to RAW:  %s", string);
    g_free(string);

    if (!gst_pad_set_caps(auddec1->srcpad, caps)) {
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
            /* Parse new segment event (if needed then convert from byte format
             * to time format).
             */
            gst_ti_parse_newsegment(&event, auddec1->segment,
                &auddec1->totalDuration, auddec1->totalBytes);

            /* Propagate NEWSEGMENT to downstream elements */
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
    GstTIAuddec1  *auddec1 = GST_TIAUDDEC1(GST_OBJECT_PARENT(pad));
    GstFlowReturn  flow    = GST_FLOW_OK;
    gboolean       checkResult;

    /* If the decode thread aborted, signal it to let it know it's ok to
     * shut down, and communicate the failure to the pipeline.
     */
    if (gst_tithread_check_status(auddec1, TIThread_CODEC_ABORTED,
            checkResult)) {
        flow = GST_FLOW_UNEXPECTED;
        goto exit;
    }

    /* If our engine handle is currently NULL, then either this is our first
     * buffer or the upstream element has re-negotiated our capabilities which
     * resulted in our engine being closed.  In either case, we need to
     * initialize (or re-initialize) our audio decoder to handle the new
     * stream.
     */
    if (auddec1->hEngine == NULL) {
        if (!gst_tiauddec1_init_audio(auddec1)) {
            GST_ELEMENT_ERROR(auddec1, RESOURCE, FAILED,
            ("unable to initialize audio\n"), (NULL));
            flow = GST_FLOW_UNEXPECTED;
            goto exit;
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
                if (!gst_ticircbuffer_queue_data(auddec1->circBuf,
                    auddec1->aac_header_data)) {
                    GST_ELEMENT_ERROR(auddec1, RESOURCE, WRITE,
                    ("Failed to send buffer to queue thread\n"), (NULL));
                    flow = GST_FLOW_UNEXPECTED;
                    goto exit;
                }
            }
        }

        GST_TICIRCBUFFER_TIMESTAMP(auddec1->circBuf) =
            GST_CLOCK_TIME_IS_VALID(GST_BUFFER_TIMESTAMP(buf)) ?
            GST_BUFFER_TIMESTAMP(buf) : 0ULL;
    }

    /* Queue up the encoded data stream into a circular buffer */
    if (!gst_ticircbuffer_queue_data(auddec1->circBuf, buf)) {
        GST_ELEMENT_ERROR(auddec1, RESOURCE, WRITE,
        ("Failed to queue input buffer into circular buffer\n"), (NULL));
        flow = GST_FLOW_UNEXPECTED;
        goto exit;
    }

exit:
    gst_buffer_unref(buf);
    return flow;
}


/******************************************************************************
 * gst_tiauddec1_init_audio
 *     Initialize or re-initializes the audio stream
 ******************************************************************************/
static gboolean gst_tiauddec1_init_audio(GstTIAuddec1 * auddec1)
{
    Rendezvous_Attrs    rzvAttrs  = Rendezvous_Attrs_DEFAULT;
    struct sched_param  schedParam;
    pthread_attr_t      attr;

    GST_LOG("begin init_audio\n");

    /* If audio has already been initialized, shut down previous decoder */
    if (auddec1->hEngine) {
        if (!gst_tiauddec1_exit_audio(auddec1)) {
            GST_ELEMENT_ERROR(auddec1, RESOURCE, FAILED,
            ("failed to shut down existing audio decoder\n"), (NULL));
            return FALSE;
        }
    }

    /* Make sure we know what codec we're using */
    if (!auddec1->engineName) {
        GST_ELEMENT_ERROR(auddec1, RESOURCE, FAILED,
        ("engine name not specified\n"), (NULL));
        return FALSE;
    }

    if (!auddec1->codecName) {
        GST_ELEMENT_ERROR(auddec1, RESOURCE, FAILED,
        ("codec name not specified\n"), (NULL));
        return FALSE;
    }

    /* Initialize thread status management */
    auddec1->threadStatus = 0UL;
    pthread_mutex_init(&auddec1->threadStatusMutex, NULL);

    /* Initialize rendezvous objects for making threads wait on conditions */
    auddec1->waitOnDecodeThread = Rendezvous_create(2, &rzvAttrs);
    auddec1->waitOnDecodeDrain  = Rendezvous_create(100, &rzvAttrs);
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
    if (pthread_create(&auddec1->decodeThread, auddec1->rtCodecThread ? 
        &attr : NULL, gst_tiauddec1_decode_thread, (void*)auddec1)) {
        GST_ELEMENT_ERROR(auddec1, RESOURCE, FAILED,
        ("failed to create decode thread\n"), (NULL));
        gst_tiauddec1_exit_audio(auddec1);
        return FALSE;
    }
    gst_tithread_set_status(auddec1, TIThread_CODEC_CREATED);

    /* Destroy the custom thread attributes */
    if (pthread_attr_destroy(&attr)) {
        GST_WARNING("failed to destroy thread attrs\n");
        gst_tiauddec1_exit_audio(auddec1);
        return FALSE;
    }

    /* Make sure circular buffer and display buffer handles are created by
     * decoder thread.
     */
    Rendezvous_meet(auddec1->waitOnDecodeThread);

    if (auddec1->circBuf == NULL || auddec1->hOutBufTab == NULL) {
        GST_ELEMENT_ERROR(auddec1, RESOURCE, FAILED,
        ("decode thread failed to create circbuf or display buffer handles\n"),
        (NULL));
        return FALSE;
    }

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
            auddec1, TIThread_CODEC_CREATED, checkResult)) {
        GST_LOG("shutting down decode thread\n");

        Rendezvous_force(auddec1->waitOnDecodeThread);
        if (pthread_join(auddec1->decodeThread, &thread_ret) == 0) {
            if (thread_ret == GstTIThreadFailure) {
                GST_DEBUG("decode thread exited with an error condition\n");
            }
        }
    }

    /* Shut down thread status management */
    auddec1->threadStatus = 0UL;
    pthread_mutex_destroy(&auddec1->threadStatusMutex);

    /* Shut down remaining items */
    if (auddec1->aac_header_data) {
        gst_buffer_unref(auddec1->aac_header_data);
        auddec1->aac_header_data = NULL;
    }

    if (auddec1->waitOnDecodeDrain) {
        Rendezvous_delete(auddec1->waitOnDecodeDrain);
        auddec1->waitOnDecodeDrain = NULL;
    }

    if (auddec1->waitOnDecodeThread) {
        Rendezvous_delete(auddec1->waitOnDecodeThread);
        auddec1->waitOnDecodeThread = NULL;
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

        case GST_STATE_CHANGE_READY_TO_PAUSED:
            gst_segment_init(auddec1->segment, GST_FORMAT_TIME);
            break;

        case GST_STATE_CHANGE_PAUSED_TO_READY:
            auddec1->totalBytes      = 0;
            auddec1->totalDuration   = 0;
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
    if (auddec1->circBuf) {
        GstTICircBuffer *circBuf;

        GST_LOG("freeing cicrular input buffer\n");

        circBuf          = auddec1->circBuf;
        auddec1->circBuf = NULL;
        gst_ticircbuffer_unref(circBuf);
    }

    if (auddec1->hOutBufTab) {
        GST_LOG("freeing output buffers\n");
        gst_tidmaibuftab_unref(auddec1->hOutBufTab);
        auddec1->hOutBufTab = NULL;
    }

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
        GST_ELEMENT_ERROR(auddec1, RESOURCE, FAILED,
        ("failed to open codec engine \"%s\"\n", auddec1->engineName), (NULL));
        return FALSE;
    }

    if (gst_tiauddec1_codec_is_aac(auddec1)) {
        #if defined (Platform_dm365) || defined(Platform_dm368)
        params.dataEndianness = XDM_LE_16;
        #else
        ; /* do nothing */
        #endif
    }

    /* Initialize audio decoder */
    GST_LOG("opening audio decoder \"%s\"\n", auddec1->codecName);
    auddec1->hAd = Adec1_create(auddec1->hEngine, (Char*)auddec1->codecName,
                      &params, &dynParams);

    if (auddec1->hAd == NULL) {
        GST_ELEMENT_ERROR(auddec1, STREAM, CODEC_NOT_FOUND,
        ("failed to create audio decoder: %s\n", auddec1->codecName), (NULL));
        GST_LOG("closing codec engine\n");
        return FALSE;
    }

    /* Set up a circular input buffer capable of holding two encoded frames */
    auddec1->circBuf = gst_ticircbuffer_new(
                            Adec1_getInBufSize(auddec1->hAd), 30, FALSE);

    if (auddec1->circBuf == NULL) {
        GST_ELEMENT_ERROR(auddec1, RESOURCE, NO_SPACE_LEFT,
        ("failed to create circular input buffer\n"), (NULL));
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

    /* If we're still showing 0 channels, we were not able to determine the
     * number of channels from the input stream.  Default to 2 channels and
     * generate a warning.
     */
    if (auddec1->channels == 0) {
        GST_WARNING("Could not determine the number of channels in the "
            "input stream; defaulting to 2");
        auddec1->channels = 2;
    }

    /* Create codec output buffers.  
     */
    GST_LOG("creating output buffers\n");
      
    /* By default, new buffers are marked as in-use by the codec */
    bAttrs.useMask = gst_tidmaibuffer_CODEC_FREE;

    auddec1->hOutBufTab = gst_tidmaibuftab_new(auddec1->numOutputBufs, 
        Adec1_getOutBufSize(auddec1->hAd), &bAttrs);

    if (auddec1->hOutBufTab == NULL) {
        GST_ELEMENT_ERROR(auddec1, RESOURCE, NO_SPACE_LEFT,
        ("failed to create output buffer\n"), (NULL));
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
    guint          sampleDataSize;
    GstClockTime   sampleDuration;
    guint          sampleRate;
    guint          numSamples;
    guint          offset;
    Int            bufIdx;
    Int            ret;

    GST_LOG("starting auddec decode thread\n");

    /* Initialize codec engine */
    ret = gst_tiauddec1_codec_start(auddec1);

    /* Notify main thread that is ok to continue initialization */
    Rendezvous_meet(auddec1->waitOnDecodeThread);
    Rendezvous_reset(auddec1->waitOnDecodeThread);

    if (ret == FALSE) {
        GST_ELEMENT_ERROR(auddec1, STREAM, FAILED,
        ("failed to start codec\n"), (NULL));
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
        if (!(hDstBuf = gst_tidmaibuftab_get_buf(auddec1->hOutBufTab))) {
            GST_ELEMENT_ERROR(auddec1, RESOURCE, READ,
                ("failed to get a free contiguous buffer from BufTab\n"), 
                (NULL));
            goto thread_exit;
        }

        /* Invoke the audio decoder */
        GST_LOG("Invoking the audio decoder at 0x%08lx with %u bytes\n",
            (unsigned long)Buffer_getUserPtr(hEncDataWindow),
            GST_BUFFER_SIZE(encDataWindow));
        ret             = Adec1_process(auddec1->hAd, hEncDataWindow, hDstBuf);
        encDataConsumed = Buffer_getNumBytesUsed(hEncDataWindow);

        if (ret < 0) {
            GST_ELEMENT_ERROR(auddec1, STREAM, DECODE,
            ("failed to decode audio buffer\n"), (NULL));
            goto thread_failure;
        }

        /* If no encoded data was used we cannot find the next frame */
        if (ret == Dmai_EBITERROR && encDataConsumed == 0) {
            GST_ELEMENT_ERROR(auddec1, STREAM, DECODE,
            ("fatal bit error\n"), (NULL));
            goto thread_failure;
        }

        if (ret > 0) {
            GST_LOG("Adec1_process returned success code %d\n", ret); 
        }

        /* Release the reference buffer, and tell the circular buffer how much
         * data was consumed.
         */
        ret = gst_ticircbuffer_data_consumed(auddec1->circBuf, encDataWindow,
                  encDataConsumed);
        encDataWindow = NULL;

        if (!ret) {
            goto thread_failure;
        }

        sampleDataSize = Buffer_getNumBytesUsed(hDstBuf);
        if (sampleDataSize) {
            /* DMAI currently doesn't provide a way to retrieve the number of
             * samples decoded or the duration of the decoded audio data.  For
             * now, derive the duration from the number of bytes decoded by the
             * codec.
             */
            sampleRate     = Adec1_getSampleRate(auddec1->hAd);

            /* Some codecs does not set the sample rate. In those case use the 
             * sample rate from the cap negotitation.
             */
            if (sampleRate == 0) {
                sampleRate = auddec1->sampleRate;
            }
            else {
                auddec1->sampleRate = sampleRate;
            }

            numSamples     = sampleDataSize / (2 * auddec1->channels) ;
            sampleDuration = GST_FRAMES_TO_CLOCK_TIME(numSamples, sampleRate);
            encDataTime    = auddec1->totalDuration;
            offset         = GST_CLOCK_TIME_TO_FRAMES(auddec1->totalDuration,
                                                    sampleRate);

            /* Increment total bytes recieved */
            auddec1->totalBytes += encDataConsumed;

            /* Set the source pad capabilities based on the decoded frame
             * properties.
             */
            gst_tiauddec1_set_source_caps(auddec1);

            /* Create a DMAI transport buffer object to carry a DMAI buffer to
             * the source pad.  The transport buffer knows how to release the
             * buffer for re-use in this element when the source pad calls
             * gst_buffer_unref().
             */
            outBuf = gst_tidmaibuffertransport_new(
                hDstBuf, auddec1->hOutBufTab);
            gst_buffer_set_data(outBuf, GST_BUFFER_DATA(outBuf),
                Buffer_getNumBytesUsed(hDstBuf));
            gst_buffer_set_caps(outBuf, GST_PAD_CAPS(auddec1->srcpad));

            /* Set timestamp on output buffer */
            if (auddec1->genTimeStamps) {
                GST_BUFFER_OFFSET(outBuf)       = offset;
                GST_BUFFER_DURATION(outBuf)     = sampleDuration;
                GST_BUFFER_TIMESTAMP(outBuf)    = encDataTime;
                auddec1->totalDuration  += GST_BUFFER_DURATION (outBuf);
            }
            else {
                GST_BUFFER_TIMESTAMP(outBuf)    = GST_CLOCK_TIME_NONE;
            }

            /* Tell circular buffer how much time we consumed */
            gst_ticircbuffer_time_consumed(auddec1->circBuf, sampleDuration);

            /* Push the transport buffer to the source pad */
            GST_LOG("pushing buffer to source pad with timestamp : %"
                GST_TIME_FORMAT ", duration: %" GST_TIME_FORMAT,
                GST_TIME_ARGS (GST_BUFFER_TIMESTAMP(outBuf)),
                GST_TIME_ARGS (GST_BUFFER_DURATION(outBuf)));

            if (gst_pad_push(auddec1->srcpad, outBuf) != GST_FLOW_OK) {
                GST_DEBUG("push to source pad failed\n");
                goto thread_failure;
            }
        }

        /* Release buffers no longer in use by the codec */
        Buffer_freeUseMask(hDstBuf, gst_tidmaibuffer_CODEC_FREE);
    }

thread_failure:

    gst_tithread_set_status(auddec1, TIThread_CODEC_ABORTED);
    gst_ticircbuffer_consumer_aborted(auddec1->circBuf);
    threadRet = GstTIThreadFailure;

thread_exit:

    /* Re-claim any buffers owned by the codec */
    bufIdx = BufTab_getNumBufs(GST_TIDMAIBUFTAB_BUFTAB(auddec1->hOutBufTab));

    while (bufIdx-- > 0) {
        Buffer_Handle hBuf = BufTab_getBuf(
            GST_TIDMAIBUFTAB_BUFTAB(auddec1->hOutBufTab), bufIdx);
        Buffer_freeUseMask(hBuf, gst_tidmaibuffer_CODEC_FREE);
    }

    /* Release the last buffer we retrieved from the circular buffer */
    if (encDataWindow) {
        gst_ticircbuffer_data_consumed(auddec1->circBuf, encDataWindow, 0);
    }

    /* We have to wait to shut down this thread until we can guarantee that
     * no more input buffers will be queued into the circular buffer
     * (we're about to delete it).  
     */
    Rendezvous_meet(auddec1->waitOnDecodeThread);
    Rendezvous_reset(auddec1->waitOnDecodeThread);

    /* Notify main thread that we are done draining before we shutdown the
     * codec, or we will hang.  We proceed in this order so the EOS event gets
     * propagated downstream before we attempt to shut down the codec.  The
     * codec-shutdown process will block until all BufTab buffers have been
     * released, and downstream-elements may hang on to buffers until
     * they get the EOS.
     */
    Rendezvous_force(auddec1->waitOnDecodeDrain);

    /* Initialize codec engine */
    if (gst_tiauddec1_codec_stop(auddec1) < 0) {
        GST_ERROR("failed to stop codec\n");
    }

    gst_object_unref(auddec1);

    GST_LOG("exit audio decode_thread (%d)\n", (int)threadRet);
    return threadRet;
}


/******************************************************************************
 * gst_tiauddec1_drain_pipeline
 *    Wait for the decode thread to finish processing queued input data.
 ******************************************************************************/
static void gst_tiauddec1_drain_pipeline(GstTIAuddec1 *auddec1)
{
    gboolean checkResult;

    /* If the decode thread hasn't been created, there is nothing to drain. */
    if (!gst_tithread_check_status(
             auddec1, TIThread_CODEC_CREATED, checkResult)) {
        return;
    }

    auddec1->drainingEOS = TRUE;
    gst_ticircbuffer_drain(auddec1->circBuf, TRUE);

    /* Tell the decode thread that it is ok to shut down */
    Rendezvous_force(auddec1->waitOnDecodeThread);

    /* Wait for the decoder to finish draining */
    Rendezvous_meet(auddec1->waitOnDecodeDrain);

}

/******************************************************************************
 * gst_tiauddec1_codec_is_aac
 *     Return TRUE if we are decoding an AAC stream
 ******************************************************************************/
static gboolean gst_tiauddec1_codec_is_aac(GstTIAuddec1 *auddec1)
{
    GstTICodec *codec = gst_ticodec_get_codec("AAC Audio Decoder");
    return (codec && !strcmp(codec->CE_CodecName, auddec1->codecName));
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
