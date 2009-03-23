/*
 * gsttividenc.c
 *
 * This file defines the "TIVidenc" element, which encodes an xDM 0.9 video
 * stream.
 *
 * Example usage:
 *     gst-launch videotestsrc !
 *         TIVidenc engineName="<engine name>" codecName="<codecName>" !
 *         fakesink silent=TRUE
 *
 * Notes:
 *  * This element currently assumes that video input is in  UYVY format.
 *  * If contiguousInputFrame=TRUE is set then it assumes that upstream is 
 *    sending contiguous buffer instead of malloced. And will used dmai 
 *    hw accelerated framecopy to copy the data in circular buffer instead 
 *    memcpy .
 *
 * Original Author:
 *     Brijesh Singh, Texas Instruments, Inc.
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
#include <ti/sdo/dmai/Buffer.h>
#include <ti/sdo/dmai/BufferGfx.h>
#include <ti/sdo/dmai/BufTab.h>
#include <ti/sdo/dmai/ce/Venc.h>
#include <ti/sdo/dmai/ColorSpace.h>

#include "gsttividenc.h"
#include "gsttidmaibuffertransport.h"
#include "gstticodecs.h"
#include "gsttithreadprops.h"

/* Declare variable used to categorize GST_LOG output */
GST_DEBUG_CATEGORY_STATIC (gst_tividenc_debug);
#define GST_CAT_DEFAULT gst_tividenc_debug

/* Element property identifiers */
enum
{
  PROP_0,
  PROP_ENGINE_NAME,     /* engineName     (string)  */
  PROP_CODEC_NAME,      /* codecName      (string)  */
  PROP_NUM_OUTPUT_BUFS, /* numOutputBuf   (int)     */
  PROP_NUM_INPUT_BUFS,  /* numInputBuf    (int)     */
  PROP_RESOLUTION,      /* resolution     (string)  */
  PROP_IN_COLORSPACE,   /* colorSpace     (string)  */
  PROP_BITRATE,         /* bitRate        (int)     */
  PROP_DISPLAY_BUFFER,  /* displayBuffer  (boolean) */
  PROP_CONTIG_INPUT_BUF,/* contiguousInputFrame  (boolean) */
  PROP_GEN_TIMESTAMPS   /* genTimeStamps  (boolean) */
};

/* Define source (output) pad capabilities.  Currently, MPEG and H264 are 
 * supported.
 */
static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE(
    "src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS
    ("video/mpeg, " 
     "mpegversion=(int){ 4 }, "         /* MPEG version 4 */
         "systemstream=(boolean)false, "
         "framerate=(fraction)[ 0, MAX ], "
         "width=(int)[ 1, MAX ], "
         "height=(int)[ 1, MAX ] ;"
     "video/x-h264, "                           /* H264   */
         "framerate=(fraction)[ 0, MAX ], "
         "width=(int)[ 1, MAX ], "
         "height=(int)[ 1, MAX ]"
    )
);

/* Define sink (input) pad capabilities.  Currently, UYVY supported. */
static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE(
    "sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS
    ("video/x-raw-yuv, "                        /* UYVY - YUV422 interleaved */
         "format=(fourcc)UYVY, "                
         "framerate=(fraction)[ 0, MAX ], "
         "width=(int)[ 1, MAX ], "
         "height=(int)[ 1, MAX ];"
    )
);

/* Declare a global pointer to our element base class */
static GstElementClass *parent_class = NULL;

/* Static Function Declarations */
static void
 gst_tividenc_base_init(gpointer g_class);
static void
 gst_tividenc_class_init(GstTIVidencClass *g_class);
static void
 gst_tividenc_init(GstTIVidenc *object, GstTIVidencClass *g_class);
static void
 gst_tividenc_set_property (GObject *object, guint prop_id,
     const GValue *value, GParamSpec *pspec);
static void
 gst_tividenc_get_property (GObject *object, guint prop_id, GValue *value,
     GParamSpec *pspec);
static gboolean
 gst_tividenc_set_sink_caps(GstPad *pad, GstCaps *caps);
static gboolean
 gst_tividenc_set_source_caps(GstTIVidenc *videnc, Buffer_Handle hBuf);
static gboolean
 gst_tividenc_sink_event(GstPad *pad, GstEvent *event);
static GstFlowReturn
 gst_tividenc_chain(GstPad *pad, GstBuffer *buf);
static gboolean
 gst_tividenc_init_video(GstTIVidenc *videnc);
static gboolean
 gst_tividenc_exit_video(GstTIVidenc *videnc);
static GstStateChangeReturn
 gst_tividenc_change_state(GstElement *element, GstStateChange transition);
static void*
 gst_tividenc_encode_thread(void *arg);
static void*
 gst_tividenc_queue_thread(void *arg);
static void
 gst_tividenc_broadcast_queue_thread(GstTIVidenc *videnc);
static void
 gst_tividenc_wait_on_queue_thread(GstTIVidenc *videnc, Int32 waitQueueSize);
static void
 gst_tividenc_drain_pipeline(GstTIVidenc *videnc);
static GstClockTime
 gst_tividenc_frame_duration(GstTIVidenc *videnc);
static ColorSpace_Type 
    gst_tividenc_find_colorSpace (const char *colorSpace);
static gboolean
    gst_tividenc_codec_start (GstTIVidenc *videnc);
static gboolean
    gst_tividenc_codec_stop (GstTIVidenc *videnc);

/******************************************************************************
 * gst_tividenc_class_init_trampoline
 *    Boiler-plate function auto-generated by "make_element" script.
 ******************************************************************************/
static void gst_tividenc_class_init_trampoline(gpointer g_class, gpointer data)
{
    parent_class = (GstElementClass*) g_type_class_peek_parent(g_class);
    gst_tividenc_class_init((GstTIVidencClass*)g_class);
}

/******************************************************************************
 * gst_tividenc_get_type
 *    Boiler-plate function auto-generated by "make_element" script.
 *    Defines function pointers for initialization routines for this element.
 ******************************************************************************/
GType gst_tividenc_get_type(void)
{
    static GType object_type = 0;

    if (G_UNLIKELY(object_type == 0)) {
        static const GTypeInfo object_info = {
            sizeof(GstTIVidencClass),
            gst_tividenc_base_init,
            NULL,
            gst_tividenc_class_init_trampoline,
            NULL,
            NULL,
            sizeof(GstTIVidenc),
            0,
            (GInstanceInitFunc) gst_tividenc_init
        };

        object_type = g_type_register_static((gst_element_get_type()),
                          "GstTIVidenc", &object_info, (GTypeFlags)0);

        /* Initialize GST_LOG for this object */
        GST_DEBUG_CATEGORY_INIT(gst_tividenc_debug, "TIVidenc", 0,
            "TI xDM 0.9 Video Encoder");

        GST_LOG("initialized get_type\n");

    }

    return object_type;
};

/******************************************************************************
 * gst_tividenc_base_init
 *    Boiler-plate function auto-generated by "make_element" script.
 *    Initializes element base class.
 ******************************************************************************/
static void gst_tividenc_base_init(gpointer gclass)
{
    static GstElementDetails element_details = {
        "TI xDM 0.9 Video Encoder",
        "Codec/Encoder/Video",
        "Encodes video using an xDM 0.9-based codec",
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
 * gst_tividenc_class_init
 *    Boiler-plate function auto-generated by "make_element" script.
 *    Initializes the TIVidenc class.
 ******************************************************************************/
static void gst_tividenc_class_init(GstTIVidencClass *klass)
{
    GObjectClass    *gobject_class;
    GstElementClass *gstelement_class;

    gobject_class    = (GObjectClass*)    klass;
    gstelement_class = (GstElementClass*) klass;

    gobject_class->set_property = gst_tividenc_set_property;
    gobject_class->get_property = gst_tividenc_get_property;

    gstelement_class->change_state = gst_tividenc_change_state;

    g_object_class_install_property(gobject_class, PROP_ENGINE_NAME,
        g_param_spec_string("engineName", "Engine Name",
            "Engine name used by Codec Engine", "unspecified",
            G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_CODEC_NAME,
        g_param_spec_string("codecName", "Codec Name", "Name of video codec",
            "unspecified", G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_IN_COLORSPACE,
        g_param_spec_string("iColorSpace", "Input ColorSpace", 
            "Only UYVY is supported",
            "UYVY", G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_RESOLUTION,
        g_param_spec_string("resolution", "Input resolution", 
            "The resolution of the input image ('width'x'height')",
            "720x480", G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_CONTIG_INPUT_BUF,
        g_param_spec_boolean("contiguousInputFrame", "Contiguous Input frame",
            "Set this if elemenet recieved contiguous input frame",
            FALSE, G_PARAM_WRITABLE));

    g_object_class_install_property(gobject_class, PROP_BITRATE,
        g_param_spec_long("bitRate",
            "encoder bit rate",
            "Set encoder bit rate",
            1, G_MAXINT32, 2000000, G_PARAM_WRITABLE));

    /* Number of input buffers in the circular queue.*/
    g_object_class_install_property(gobject_class, PROP_NUM_INPUT_BUFS,
        g_param_spec_int("numInputBufs",
            "Number of Input buffers",
            "Number of input buffers in circular queue",
            1, G_MAXINT32, 1, G_PARAM_WRITABLE));

    /* We allow more than two  output buffer because this is the buffer that
     * is sent to the downstream element.  It may be that we need to have
     * more than 2 buffer if the downstream element doesn't give the buffer
     * back in time for the codec to use.
     */
    g_object_class_install_property(gobject_class, PROP_NUM_OUTPUT_BUFS,
        g_param_spec_int("numOutputBufs",
            "Number of Ouput Buffers",
            "Number of output buffers to allocate for codec",
            3, G_MAXINT32, 3, G_PARAM_WRITABLE));

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
 * gst_tividenc_init
 *    Initializes a new element instance, instantiates pads and sets the pad
 *    callback functions.
 ******************************************************************************/
static void gst_tividenc_init(GstTIVidenc *videnc, GstTIVidencClass *gclass)
{
    /* Instantiate encoded video sink pad.
     *
     * Fixate on our static template caps instead of writing a getcaps
     * function, which is overkill for this element.
     */
    videnc->sinkpad =
        gst_pad_new_from_static_template(&sink_factory, "sink");
    gst_pad_set_setcaps_function(
        videnc->sinkpad, GST_DEBUG_FUNCPTR(gst_tividenc_set_sink_caps));
    gst_pad_set_event_function(
        videnc->sinkpad, GST_DEBUG_FUNCPTR(gst_tividenc_sink_event));
    gst_pad_set_chain_function(
        videnc->sinkpad, GST_DEBUG_FUNCPTR(gst_tividenc_chain));
    gst_pad_fixate_caps(videnc->sinkpad,
        gst_caps_make_writable(
            gst_caps_copy(gst_pad_get_pad_template_caps(videnc->sinkpad))));

    /* Instantiate deceoded video source pad.
     *
     * Fixate on our static template caps instead of writing a getcaps
     * function, which is overkill for this element.
     */
    videnc->srcpad =
        gst_pad_new_from_static_template(&src_factory, "src");
    gst_pad_fixate_caps(videnc->srcpad,
        gst_caps_make_writable(
            gst_caps_copy(gst_pad_get_pad_template_caps(videnc->srcpad))));

    /* Add pads to TIVidenc element */
    gst_element_add_pad(GST_ELEMENT(videnc), videnc->sinkpad);
    gst_element_add_pad(GST_ELEMENT(videnc), videnc->srcpad);

    /* Initialize TIVidenc state */
    videnc->engineName              = NULL;
    videnc->codecName               = NULL;
    videnc->displayBuffer           = FALSE;
    videnc->genTimeStamps           = TRUE;

    videnc->hEngine                 = NULL;
    videnc->hVe                     = NULL;
    videnc->drainingEOS             = FALSE;
    videnc->threadStatus            = 0UL;

    videnc->encodeDrained           = FALSE;
    videnc->waitOnEncodeDrain       = NULL;

    videnc->hInFifo                 = NULL;

    videnc->waitOnQueueThread       = NULL;
    videnc->waitQueueSize           = 0;

    videnc->waitOnEncodeThread      = NULL;

    videnc->framerateNum            = 0;
    videnc->framerateDen            = 0;
    videnc->hOutBufTab              = NULL;
    videnc->circBuf                 = NULL;

    videnc->bitRate                 = -1;
    videnc->numOutputBufs           = 0;
    videnc->numInputBufs            = 0;
    videnc->width                   = -1;
    videnc->height                  = -1;
    videnc->colorSpace              = -1;

    videnc->contiguousInputFrame    = FALSE;
}
/******************************************************************************
 * gst_tividenc_find_colorSpace 
 *****************************************************************************/
static ColorSpace_Type gst_tividenc_find_colorSpace (const char *colorSpace)
{
    if (!strcasecmp(colorSpace, "UYVY")) {
        return ColorSpace_UYVY;
    }

    return ColorSpace_NOTSET;

}
/******************************************************************************
 * gst_tividenc_set_property
 *     Set element properties when requested.
 ******************************************************************************/
static void gst_tividenc_set_property(GObject *object, guint prop_id,
                const GValue *value, GParamSpec *pspec)
{
    GstTIVidenc *videnc = GST_TIVIDENC(object);

    GST_LOG("begin set_property\n");

    switch (prop_id) {
        case PROP_RESOLUTION:
            if (videnc->resolution) {
                g_free((gpointer)videnc->resolution);
            }
            videnc->resolution =
                (gchar*)g_malloc(strlen(g_value_get_string(value)) + 1);
            strcpy((gchar *)videnc->resolution, g_value_get_string(value));
            sscanf(g_value_get_string(value), "%dx%d", &videnc->width,
                    &videnc->height);
            GST_LOG("setting \"resolution\" to \"%dx%d\"\n",
                videnc->width, videnc->height);
            break;
        case PROP_IN_COLORSPACE:
            if (videnc->iColor) {
                g_free((gpointer)videnc->iColor);
            }
            videnc->iColor =
                (gchar*)g_malloc(strlen(g_value_get_string(value)) + 1);
            strcpy((gchar *)videnc->iColor, g_value_get_string(value));
            videnc->colorSpace =   
                    gst_tividenc_find_colorSpace(g_value_get_string(value));
            GST_LOG("setting \"iColorSpace\" to \"%d\"\n", videnc->colorSpace);
            break;
        case PROP_ENGINE_NAME:
            if (videnc->engineName) {
                g_free((gpointer)videnc->engineName);
            }
            videnc->engineName =
                (gchar*)g_malloc(strlen(g_value_get_string(value)) + 1);
            strcpy((gchar *)videnc->engineName, g_value_get_string(value));
            GST_LOG("setting \"engineName\" to \"%s\"\n", videnc->engineName);
            break;
        case PROP_CODEC_NAME:
            if (videnc->codecName) {
                g_free((gpointer)videnc->codecName);
            }
            videnc->codecName =
                (gchar*)g_malloc(strlen(g_value_get_string(value)) + 1);
            strcpy((gchar*)videnc->codecName, g_value_get_string(value));
            GST_LOG("setting \"codecName\" to \"%s\"\n", videnc->codecName);
            break;
        case PROP_NUM_OUTPUT_BUFS:
            videnc->numOutputBufs = g_value_get_int(value);
            GST_LOG("setting \"numOutputBufs\" to \"%d\" \n",
                     videnc->numOutputBufs);
            break;
        case PROP_BITRATE:
            videnc->bitRate = g_value_get_long(value);
            GST_LOG("setting \"bitRate\" to \"%ld\" \n",
                     videnc->bitRate);
            break;
        case PROP_NUM_INPUT_BUFS:
            videnc->numInputBufs = g_value_get_int(value);
            GST_LOG("setting \"numInputBufs\" to \"%d\" \n",
                     videnc->numInputBufs);
            break;
        case PROP_CONTIG_INPUT_BUF:
            videnc->contiguousInputFrame = g_value_get_boolean(value);
            GST_LOG("setting \"contiguousInputFrame\" to \"%s\"\n",
                videnc->contiguousInputFrame ? "TRUE" : "FALSE");
            break;
        case PROP_DISPLAY_BUFFER:
            videnc->displayBuffer = g_value_get_boolean(value);
            GST_LOG("setting \"displayBuffer\" to \"%s\"\n",
                videnc->displayBuffer ? "TRUE" : "FALSE");
            break;
        case PROP_GEN_TIMESTAMPS:
            videnc->genTimeStamps = g_value_get_boolean(value);
            GST_LOG("setting \"genTimeStamps\" to \"%s\"\n",
                videnc->genTimeStamps ? "TRUE" : "FALSE");
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }

    GST_LOG("end set_property\n");
}

/******************************************************************************
 * gst_tividenc_get_property
 *     Return values for requested element property.
 ******************************************************************************/
static void gst_tividenc_get_property(GObject *object, guint prop_id,
                GValue *value, GParamSpec *pspec)
{
    GstTIVidenc *videnc = GST_TIVIDENC(object);

    GST_LOG("begin get_property\n");

    switch (prop_id) {
        case PROP_ENGINE_NAME:
            g_value_set_string(value, videnc->engineName);
            break;
        case PROP_CODEC_NAME:
            g_value_set_string(value, videnc->codecName);
            break;
        case PROP_IN_COLORSPACE:
            g_value_set_string(value, videnc->iColor);
            break;
        case PROP_RESOLUTION:
            g_value_set_string(value, videnc->resolution);
            break;
        case PROP_BITRATE:
            g_value_set_long(value, videnc->bitRate);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }

    GST_LOG("end get_property\n");
}

/******************************************************************************
 * gst_tividenc_set_sink_caps
 *     Negotiate our sink pad capabilities.
 ******************************************************************************/
static gboolean gst_tividenc_set_sink_caps(GstPad *pad, GstCaps *caps)
{
    GstTIVidenc  *videnc;
    GstStructure *capStruct;
    const gchar  *mime;
    char         *string;

    videnc    = GST_TIVIDENC(gst_pad_get_parent(pad));
    capStruct = gst_caps_get_structure(caps, 0);
    mime      = gst_structure_get_name(capStruct);

    string = gst_caps_to_string(caps);
    GST_INFO("requested sink caps:  %s", string);
    g_free(string);

    /* Generic Video Properties */
    if (!strncmp(mime, "video/", 6)) {
        gint  framerateNum;
        gint  framerateDen;

        if (gst_structure_get_fraction(capStruct, "framerate", &framerateNum,
                &framerateDen)) {
            videnc->framerateNum = framerateNum;
            videnc->framerateDen = framerateDen;
        }
    }

    /* RAW YUV input */
    if (!strcmp(mime,"video/x-raw-yuv")) {
        guint32 fourcc;

        if (gst_structure_get_fourcc(capStruct, "format", &fourcc)) {

            switch (fourcc) {
                case GST_MAKE_FOURCC('U', 'Y', 'V', 'Y'):
                    videnc->colorSpace = ColorSpace_UYVY;
                    break;

                default:
                    GST_ERROR("unsupported fourcc in video stream\n");
                    gst_object_unref(videnc);
                    return FALSE;
            }

            if (!gst_structure_get_int(capStruct, "width", &videnc->width)) {
                        videnc->width = 0;
            }

            if (!gst_structure_get_int(capStruct, "height", &videnc->height)) {
                        videnc->height = 0;
            }
        }
    }

    /* Mime type not supported */
    else {
        GST_ERROR("stream type not supported");
        gst_object_unref(videnc);
        return FALSE;
    }

    /* Shut-down any running video encoder */
    if (!gst_tividenc_exit_video(videnc)) {
        gst_object_unref(videnc);
        return FALSE;
    }

    gst_object_unref(videnc);

    GST_LOG("sink caps negotiation successful\n");
    return TRUE;
}


/******************************************************************************
 * gst_tividenc_set_source_caps
 *     Negotiate our source pad capabilities.
 ******************************************************************************/
static gboolean gst_tividenc_set_source_caps(
                    GstTIVidenc *videnc, Buffer_Handle hBuf)
{
    GstCaps              *caps = NULL;
    gboolean              ret;
    GstPad               *pad;
    char                 *string;

    pad = videnc->srcpad;

    /* Create H264 cap */
    if (strstr(videnc->codecName, "h264enc")) {
        caps =
            gst_caps_new_simple("video/x-h264",
                "framerate",    GST_TYPE_FRACTION,  videnc->framerateNum,
                                                    videnc->framerateDen,
                "width",        G_TYPE_INT,         videnc->width,
                "height",       G_TYPE_INT,         videnc->height,
                NULL);

        string = gst_caps_to_string(caps);
        GST_LOG("setting source caps to x-h264: %s", string);
        g_free(string);
    }
 
    /* Create MPEG4 cap */
    if (strstr(videnc->codecName, "mpeg4enc")) {
        gint mpegversion = 4;

        caps =
            gst_caps_new_simple("video/mpeg",
                "mpegversion",  G_TYPE_INT,         mpegversion,
                "framerate",    GST_TYPE_FRACTION,  videnc->framerateNum,
                                                    videnc->framerateDen,
                "width",        G_TYPE_INT,         videnc->width,
                "height",       G_TYPE_INT,         videnc->height,
                NULL);

        string = gst_caps_to_string(caps);
        GST_LOG("setting source caps to mpeg4: %s", string);
        g_free(string);
    }

    /* Set the source pad caps */
    ret = gst_pad_set_caps(pad, caps);
    gst_caps_unref(caps);

    return ret;
}

/******************************************************************************
 * gst_tividenc_sink_event
 *     Perform event processing on the input stream.  At the moment, this
 *     function isn't needed as this element doesn't currently perform any
 *     specialized event processing.  We'll leave it in for now in case we need
 *     it later on.
 ******************************************************************************/
static gboolean gst_tividenc_sink_event(GstPad *pad, GstEvent *event)
{
    GstTIVidenc *videnc;
    gboolean     ret;

    videnc = GST_TIVIDENC(GST_OBJECT_PARENT(pad));

    GST_DEBUG("pad \"%s\" received:  %s\n", GST_PAD_NAME(pad),
        GST_EVENT_TYPE_NAME(event));

    switch (GST_EVENT_TYPE(event)) {

        case GST_EVENT_NEWSEGMENT:
            /* maybe save and/or update the current segment (e.g. for output
             * clipping) or convert the event into one in a different format
             * (e.g. BYTES to TIME) or drop it and set a flag to send a
             * newsegment event in a different format later
             */
            ret = gst_pad_push_event(videnc->srcpad, event);
            break;

        case GST_EVENT_EOS:
            /* end-of-stream: process any remaining encoded frame data */
            GST_LOG("no more input; draining remaining encoded video data\n");

            if (!videnc->drainingEOS) {
               gst_tividenc_drain_pipeline(videnc);
             }

            /* Propagate EOS to downstream elements */
            ret = gst_pad_push_event(videnc->srcpad, event);
            break;

        case GST_EVENT_FLUSH_STOP:
            ret = gst_pad_push_event(videnc->srcpad, event);
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
 * gst_tividenc_chain
 *    This is the main processing routine.  This function receives a buffer
 *    from the sink pad, processes it, and pushes the result to the source
 *    pad.
 ******************************************************************************/
static GstFlowReturn gst_tividenc_chain(GstPad * pad, GstBuffer * buf)
{
    GstTIVidenc *videnc = GST_TIVIDENC(GST_OBJECT_PARENT(pad));
    gboolean     checkResult;

    /* If any thread aborted, communicate it to the pipeline */
    if (gst_tithread_check_status(videnc, TIThread_ANY_ABORTED, checkResult)) {
       gst_buffer_unref(buf);
       return GST_FLOW_UNEXPECTED;
    }

    /* If our engine handle is currently NULL, then either this is our first
     * buffer or the upstream element has re-negotiated our capabilities which
     * resulted in our engine being closed.  In either case, we need to
     * initialize (or re-initialize) our video encoder to handle the new
     * stream.
     */
    if (videnc->hEngine == NULL) {
        videnc->upstreamBufSize =  GST_BUFFER_SIZE(buf);
        if (!gst_tividenc_init_video(videnc)) {
            GST_ERROR("unable to initialize video\n");
            gst_buffer_unref(buf);
            return GST_FLOW_UNEXPECTED;
        }

        GST_TICIRCBUFFER_TIMESTAMP(videnc->circBuf) =
            GST_CLOCK_TIME_IS_VALID(GST_BUFFER_TIMESTAMP(buf)) ?
            GST_BUFFER_TIMESTAMP(buf) : 0ULL;
    }

    /* Don't queue up too many buffers -- if we collect too many input buffers
     * without consuming them we'll run out of memory.  Once we reach a
     * threshold, block until the queue thread removes some buffers.
     */
    Rendezvous_reset(videnc->waitOnQueueThread);
    if (Fifo_getNumEntries(videnc->hInFifo) > videnc->queueMaxBuffers) {
        GST_LOG("Blocking upstream and waiting for encoder to process \
                some buffers\n");
        gst_tividenc_wait_on_queue_thread(videnc, 2);
    }

    /* Queue up the encoded data stream into a circular buffer */
    if (Fifo_put(videnc->hInFifo, buf) < 0) {
        GST_ERROR("Failed to send buffer to queue thread\n");
        gst_buffer_unref(buf);
        return GST_FLOW_UNEXPECTED;
    }

    return GST_FLOW_OK;
}

/******************************************************************************
 * gst_tividenc_init_video
 *     Initialize or re-initializes the video stream
 ******************************************************************************/
static gboolean gst_tividenc_init_video(GstTIVidenc *videnc)
{
    Rendezvous_Attrs      rzvAttrs  = Rendezvous_Attrs_DEFAULT;
    struct sched_param    schedParam;
    pthread_attr_t        attr;
    Fifo_Attrs            fAttrs    = Fifo_Attrs_DEFAULT;

    GST_LOG("begin init_video\n");

    /* If video has already been initialized, shut down previous encoder */
    if (videnc->hEngine) {
        if (!gst_tividenc_exit_video(videnc)) {
            GST_ERROR("failed to shut down existing video encoder\n");
            return FALSE;
        }
    }

    /* Make sure we know what codec we're using */
    if (!videnc->engineName) {
        GST_ERROR("engine name not specified\n");
        return FALSE;
    }

    if (!videnc->codecName) {
        GST_ERROR("codec name not specified\n");
        return FALSE;
    }

    /* Set up the queue fifo */
    videnc->hInFifo = Fifo_create(&fAttrs);

    /* Initialize rendezvous objects for making threads wait on conditions */
    videnc->waitOnEncodeDrain   = Rendezvous_create(100, &rzvAttrs);
    videnc->waitOnQueueThread   = Rendezvous_create(100, &rzvAttrs);
    videnc->waitOnEncodeThread  = Rendezvous_create(2, &rzvAttrs);
    videnc->drainingEOS         = FALSE;

    /* Initialize thread status management */
    videnc->threadStatus = 0UL;
    pthread_mutex_init(&videnc->threadStatusMutex, NULL);

    /* Initialize custom thread attributes */
    if (pthread_attr_init(&attr)) {
        GST_WARNING("failed to initialize thread attrs\n");
        gst_tividenc_exit_video(videnc);
        return FALSE;
    }

    /* Force the thread to use the system scope */
    if (pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM)) {
        GST_WARNING("failed to set scope attribute\n");
        gst_tividenc_exit_video(videnc);
        return FALSE;
    }

    /* Force the thread to use custom scheduling attributes */
    if (pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED)) {
        GST_WARNING("failed to set schedule inheritance attribute\n");
        gst_tividenc_exit_video(videnc);
        return FALSE;
    }

    /* Set the thread to be fifo real time scheduled */
    if (pthread_attr_setschedpolicy(&attr, SCHED_FIFO)) {
        GST_WARNING("failed to set FIFO scheduling policy\n");
        gst_tividenc_exit_video(videnc);
        return FALSE;
    }

    /* Set the display thread priority */
    schedParam.sched_priority = GstTIVideoThreadPriority;
    if (pthread_attr_setschedparam(&attr, &schedParam)) {
        GST_WARNING("failed to set scheduler parameters\n");
        return FALSE;
    }

    /* Create encoder thread */
    if (pthread_create(&videnc->encodeThread, &attr,
            gst_tividenc_encode_thread, (void*)videnc)) {
        GST_ERROR("failed to create encode thread\n");
        gst_tividenc_exit_video(videnc);
        return FALSE;
    }
    gst_tithread_set_status(videnc, TIThread_DECODE_CREATED);


    /* Wait for encoder thread to finish initilization before creating queue
     * thread.
     */
    Rendezvous_meet(videnc->waitOnEncodeThread);

    /* Make sure circular buffer and display buffer handles are created by
     * decoder thread.
     */
    if (videnc->circBuf == NULL || videnc->hOutBufTab == NULL) {
        GST_ERROR("encode thread failed to create circbuf or display buffer"
                  " handles\n");
        return FALSE;
    }

    /* Create queue thread */
    if (pthread_create(&videnc->queueThread, NULL,
            gst_tividenc_queue_thread, (void*)videnc)) {
        GST_ERROR("failed to create queue thread\n");
        gst_tividenc_exit_video(videnc);
        return FALSE;
    }
    gst_tithread_set_status(videnc, TIThread_QUEUE_CREATED);

    GST_LOG("end init_video\n");
    return TRUE;
}

/******************************************************************************
 * gst_tividenc_exit_video
 *    Shut down any running video encoder, and reset the element state.
 ******************************************************************************/
static gboolean gst_tividenc_exit_video(GstTIVidenc *videnc)
{
    void*    thread_ret;
    gboolean checkResult;

    GST_LOG("begin exit_video\n");

    /* Drain the pipeline if it hasn't already been drained */
    if (!videnc->drainingEOS) {
       gst_tividenc_drain_pipeline(videnc);
     }

    /* Shut down the encode thread */
    if (gst_tithread_check_status(
            videnc, TIThread_DECODE_CREATED, checkResult)) {
        GST_LOG("shutting down encode thread\n");

        if (pthread_join(videnc->encodeThread, &thread_ret) == 0) {
            if (thread_ret == GstTIThreadFailure) {
                GST_DEBUG("encode thread exited with an error condition\n");
            }
        }
    }

    /* Shut down the queue thread */
    if (gst_tithread_check_status(
            videnc, TIThread_QUEUE_CREATED, checkResult)) {
        GST_LOG("shutting down queue thread\n");

        /* Unstop the queue thread if needed, and wait for it to finish */
        Fifo_flush(videnc->hInFifo);

        if (pthread_join(videnc->queueThread, &thread_ret) == 0) {
            if (thread_ret == GstTIThreadFailure) {
                GST_DEBUG("queue thread exited with an error condition\n");
            }
        }
    }

    /* Shut down thread status management */
    videnc->threadStatus = 0UL;
    pthread_mutex_destroy(&videnc->threadStatusMutex);

    /* Shut down remaining items */
    if (videnc->hInFifo) {
        Fifo_delete(videnc->hInFifo);
        videnc->hInFifo = NULL;
    }

    if (videnc->waitOnQueueThread) {
        Rendezvous_delete(videnc->waitOnQueueThread);
        videnc->waitOnQueueThread = NULL;
    }

    if (videnc->waitOnEncodeThread) {
        Rendezvous_delete(videnc->waitOnEncodeThread);
        videnc->waitOnEncodeThread = NULL;
    }

    if (videnc->waitOnEncodeDrain) {
        Rendezvous_delete(videnc->waitOnEncodeDrain);
        videnc->waitOnEncodeDrain = NULL;
    }

    if (videnc->circBuf) {
        GST_LOG("freeing cicrular input buffer\n");
        gst_ticircbuffer_unref(videnc->circBuf);
        videnc->circBuf      = NULL;
        videnc->framerateNum = 0;
        videnc->framerateDen = 0;
    }

    if (videnc->hOutBufTab) {
        GST_LOG("freeing output buffers\n");
        BufTab_delete(videnc->hOutBufTab);
        videnc->hOutBufTab = NULL;
    }

    GST_LOG("end exit_video\n");
    return TRUE;
}


/******************************************************************************
 * gst_tividenc_change_state
 *     Manage state changes for the video stream.  The gStreamer documentation
 *     states that state changes must be handled in this manner:
 *        1) Handle ramp-up states
 *        2) Pass state change to base class
 *        3) Handle ramp-down states
 ******************************************************************************/
static GstStateChangeReturn gst_tividenc_change_state(GstElement *element,
                                GstStateChange transition)
{
    GstStateChangeReturn  ret    = GST_STATE_CHANGE_SUCCESS;
    GstTIVidenc          *videnc = GST_TIVIDENC(element);

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
            /* Shut down any running video encoder */
            if (!gst_tividenc_exit_video(videnc)) {
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
 * gst_tividenc_codec_stop
 *   stop codec engine
 *****************************************************************************/
static gboolean gst_tividenc_codec_stop (GstTIVidenc *videnc)
{
    if (videnc->hVe) {
        GST_LOG("closing video encoder\n");
        Venc_delete(videnc->hVe);
        videnc->hVe = NULL;
    }

    if (videnc->hEngine) {
        GST_LOG("closing codec engine\n");
        Engine_close(videnc->hEngine);
        videnc->hEngine = NULL;
    }

    return TRUE;
}


/******************************************************************************
 * gst_tividenc_codec_start
 *   start codec engine
 *****************************************************************************/
static gboolean gst_tividenc_codec_start (GstTIVidenc *videnc)
{
    VIDENC_Params         params    = Venc_Params_DEFAULT;
    VIDENC_DynamicParams  dynParams = Venc_DynamicParams_DEFAULT;
    BufferGfx_Attrs       gfxAttrs  = BufferGfx_Attrs_DEFAULT;
    Int                   inBufSize, outBufSize;

    /* Open the codec engine */
    GST_LOG("opening codec engine \"%s\"\n", videnc->engineName);
    videnc->hEngine = Engine_open((Char *) videnc->engineName, NULL, NULL);

    if (videnc->hEngine == NULL) {
        GST_ERROR("failed to open codec engine \"%s\"\n", videnc->engineName);
        return FALSE;
    }

    /* Initialize video encoder */
    params.inputChromaFormat = XDM_YUV_422ILE;
    params.maxWidth          = videnc->width;
    params.maxHeight         = videnc->height;

    /* Set up codec parameters depending on bit rate */
    if (videnc->bitRate < 0) {
        /* Variable bit rate */
        params.rateControlPreset = IVIDEO_NONE;

        /* If variable bit rate use a bogus bit rate value (> 0) since it will
         * ignored.
         */
        params.maxBitRate = 2000000;
    }
    else {
        /* Constant bit rate */
        params.rateControlPreset = IVIDEO_LOW_DELAY;
        params.maxBitRate = videnc->bitRate;
    }

    dynParams.targetBitRate = params.maxBitRate;
    dynParams.inputWidth    = videnc->width;
    dynParams.inputHeight   = videnc->height;

    GST_LOG("configuring video encoder width=%ld, height=%ld, colorSpace=%s"
            " bitRate=%ld\n", dynParams.inputWidth, dynParams.inputHeight,
             videnc->colorSpace == ColorSpace_UYVY ? "UYVY" : "notset",
             params.maxBitRate);

    GST_LOG("opening video encoder \"%s\"\n", videnc->codecName);
    videnc->hVe = Venc_create(videnc->hEngine, (Char*)videnc->codecName,
                      &params, &dynParams);

    if (videnc->hVe == NULL) {
        GST_ERROR("failed to create video encoder: %s\n", videnc->codecName);
        GST_LOG("closing codec engine\n");
        return FALSE;
    }

    inBufSize = BufferGfx_calcLineLength(videnc->width, 
                videnc->colorSpace) * videnc->height;
    outBufSize = inBufSize;
    
    /* Create a circular input buffer */
    if (videnc->numInputBufs == 0) {
        videnc->numInputBufs = 2;
    }
    GST_LOG("creating %d buffers in circular buffer with size %d\n", 
                videnc->numInputBufs, inBufSize);

    videnc->circBuf =
        gst_ticircbuffer_new(inBufSize, videnc->numInputBufs, TRUE);

    if (videnc->circBuf == NULL) {
        GST_ERROR("failed to create circular input buffer\n");
        return FALSE;
    }
    
    /* Calculate the maximum number of buffers allowed in queue before
     * blocking upstream.
     */
    videnc->queueMaxBuffers = (inBufSize / videnc->upstreamBufSize) + 3;
    GST_LOG("setting max queue threadshold to %d\n", videnc->queueMaxBuffers);

    /* Create codec output buffers */
    gfxAttrs.colorSpace     = videnc->colorSpace;
    gfxAttrs.dim.width      = videnc->width;
    gfxAttrs.dim.height     = videnc->height;
    gfxAttrs.dim.lineLength = BufferGfx_calcLineLength(
                                  gfxAttrs.dim.width, gfxAttrs.colorSpace);

    gfxAttrs.bAttrs.memParams.align = 128;
    gfxAttrs.bAttrs.useMask = gst_tidmaibuffertransport_GST_FREE;

    if (videnc->numOutputBufs == 0) {
        videnc->numOutputBufs = 3;
    }

    GST_LOG("creating %d buffers in output buffer table width size %d\n",
                videnc->numOutputBufs, outBufSize);

    videnc->hOutBufTab =
        BufTab_create(videnc->numOutputBufs, outBufSize,
            BufferGfx_getBufferAttrs(&gfxAttrs));

    if (videnc->hOutBufTab == NULL) {
        GST_ERROR("failed to create output buffers\n");
        return FALSE;
    }

    /* If element is configured to recieve contiguous input frame from the
     * upstream then configure the graphics object attributes used. This 
     * attribute will be used by cicular buffer while creating framecopy job.
     */
    if (videnc->contiguousInputFrame && 
            gst_ticircbuffer_set_bufferGfx_attrs(videnc->circBuf, 
            &gfxAttrs) < 0) {
        GST_ERROR("failed to set the graphics attribute on circular buffer\n");
        return FALSE;
    }

    /* Display buffer contents if displayBuffer=TRUE was specified */
    gst_ticircbuffer_set_display(videnc->circBuf, videnc->displayBuffer);

    return TRUE;
}

/******************************************************************************
 * gst_tividenc_encode_thread
 *     Call the video codec to process a full input buffer
 ******************************************************************************/
static void* gst_tividenc_encode_thread(void *arg)
{
    GstTIVidenc         *videnc         = GST_TIVIDENC(gst_object_ref(arg));
    GstBuffer           *encDataWindow  = NULL;
    BufferGfx_Attrs     gfxAttrs        = BufferGfx_Attrs_DEFAULT;
    void                *threadRet      = GstTIThreadSuccess;
    Buffer_Handle       hDstBuf, hInBuf;
    Int32               encDataConsumed;
    GstClockTime        encDataTime;
    GstClockTime        frameDuration;
    Buffer_Handle       hEncDataWindow;
    GstBuffer           *outBuf;
    Int                 ret;

    GST_LOG("starting  video encode thread\n");

    /* Calculate the duration of a single frame in this stream */
    frameDuration = gst_tividenc_frame_duration(videnc);

    /* Initialize codec engine */
    ret = gst_tividenc_codec_start(videnc);

    /* Notify main thread if it is waiting to create queue thread */
    Rendezvous_meet(videnc->waitOnEncodeThread);

    if (ret == FALSE) {
        GST_ERROR("failed to start codec\n");
        goto thread_exit;
    }

    /* Main thread loop */
    while (TRUE) {

        /* Obtain an encoded data frame */
        encDataWindow  = gst_ticircbuffer_get_data(videnc->circBuf);
        encDataTime    = GST_BUFFER_TIMESTAMP(encDataWindow);
        hEncDataWindow = GST_TIDMAIBUFFERTRANSPORT_DMAIBUF(encDataWindow);

        /* If we received a data frame of zero size, there is no more data to
         * process -- exit the thread.  If we weren't told that we are
         * draining the pipeline, something is not right, so exit with an
         * error.
         */
        if (GST_BUFFER_SIZE(encDataWindow) == 0) {
            GST_LOG("no video data remains\n");
            if (!videnc->drainingEOS) {
                goto thread_failure;
            }
            goto thread_exit;
        }

        /* Obtain a free output buffer for the encoded data */
        hDstBuf = BufTab_getFreeBuf(videnc->hOutBufTab);
        if (hDstBuf == NULL) {
            GST_ERROR("failed to get a free contiguous buffer from BufTab\n");
            goto thread_failure;
        }

        /* Make sure the whole buffer is used for output */
        BufferGfx_resetDimensions(hDstBuf);

        /* Create a reference graphics buffer object.
         * If reference flag is set to TRUE then no buffer will be allocated
         * instead resulting buffer will be reference to already existing
         * memory area from the circular buffer. 
         */
        gfxAttrs.bAttrs.reference   = TRUE;
        gfxAttrs.dim.width          = videnc->width;
        gfxAttrs.dim.height         = videnc->height;
        gfxAttrs.colorSpace         = videnc->colorSpace;
        gfxAttrs.dim.lineLength     = BufferGfx_calcLineLength(videnc->width,
                                            videnc->colorSpace);

        hInBuf = Buffer_create(Buffer_getSize(hEncDataWindow),
                                BufferGfx_getBufferAttrs(&gfxAttrs));
        Buffer_setUserPtr(hInBuf, Buffer_getUserPtr(hEncDataWindow));
        Buffer_setNumBytesUsed(hInBuf,Buffer_getSize(hInBuf));

        /* Invoke the video encoder */
        GST_LOG("invoking the video encoder\n");
        ret             = Venc_process(videnc->hVe, hInBuf, hDstBuf);
        encDataConsumed = Buffer_getNumBytesUsed(hEncDataWindow);

        if (ret < 0) {
            GST_ERROR("failed to encode video buffer\n");
            goto thread_failure;
        }

        /* If no encoded data was used we cannot find the next frame */
        if (ret == Dmai_EBITERROR && encDataConsumed == 0) {
            GST_ERROR("fatal bit error\n");
            goto thread_failure;
        }

        if (ret > 0) {
            GST_LOG("Venc_process returned success code %d\n", ret); 
        }

        /* Free the graphics object */
        Buffer_delete(hInBuf);

        /* Release the reference buffer, and tell the circular buffer how much
         * data was consumed.
         */
        ret = gst_ticircbuffer_data_consumed(videnc->circBuf, encDataWindow,
                  encDataConsumed);
        encDataWindow = NULL;

        if (!ret) {
            goto thread_failure;
        }

        /* Set the source pad capabilities based on the encoded frame
         * properties.
         */
        gst_tividenc_set_source_caps(videnc, hDstBuf);

        /* Create a DMAI transport buffer object to carry a DMAI buffer to
         * the source pad.  The transport buffer knows how to release the
         * buffer for re-use in this element when the source pad calls
         * gst_buffer_unref().
         */
        outBuf = gst_tidmaibuffertransport_new(hDstBuf);
        gst_buffer_set_data(outBuf, GST_BUFFER_DATA(outBuf),
            Buffer_getNumBytesUsed(hDstBuf));
        gst_buffer_set_caps(outBuf, GST_PAD_CAPS(videnc->srcpad));

        /* If we have a valid time stamp, set it on the buffer */
        if (videnc->genTimeStamps &&
            GST_CLOCK_TIME_IS_VALID(encDataTime)) {
            GST_LOG("video timestamp value: %llu\n", encDataTime);
            GST_BUFFER_TIMESTAMP(outBuf) = encDataTime;
            GST_BUFFER_DURATION(outBuf)  = frameDuration;
        }
        else {
            GST_BUFFER_TIMESTAMP(outBuf) = GST_CLOCK_TIME_NONE;
        }

        /* Tell circular buffer how much time we consumed */
        gst_ticircbuffer_time_consumed(videnc->circBuf, frameDuration);

        /* Push the transport buffer to the source pad */
        GST_LOG("pushing display buffer to source pad\n");

        if (gst_pad_push(videnc->srcpad, outBuf) != GST_FLOW_OK) {
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
        gst_ticircbuffer_data_consumed(videnc->circBuf, encDataWindow, 0);
    }

    gst_tithread_set_status(videnc, TIThread_DECODE_ABORTED);
    threadRet = GstTIThreadFailure;
    gst_ticircbuffer_consumer_aborted(videnc->circBuf);

thread_exit: 
    /* Stop codec engine */
    if (gst_tividenc_codec_stop(videnc) < 0) {
        GST_ERROR("failed to stop codec\n");
    }

    /* Notify main thread if it is waiting on decode thread shut-down */
    videnc->encodeDrained = TRUE;
    Rendezvous_force(videnc->waitOnQueueThread);
    Rendezvous_force(videnc->waitOnEncodeDrain);

    gst_object_unref(videnc);

    GST_LOG("exit video encode_thread (%d)\n", (int)threadRet);
    return threadRet;
}


/******************************************************************************
 * gst_tividenc_queue_thread 
 *     Add an input buffer to the circular buffer            
 ******************************************************************************/
static void* gst_tividenc_queue_thread(void *arg)
{
    GstTIVidenc* videnc    = GST_TIVIDENC(gst_object_ref(arg));
    void*        threadRet = GstTIThreadSuccess;
    GstBuffer*   encData;
    Int          fifoRet;

    while (TRUE) {

        /* Get the next input buffer (or block until one is ready) */
        fifoRet = Fifo_get(videnc->hInFifo, &encData);

        if (fifoRet < 0) {
            GST_ERROR("Failed to get buffer from input fifo\n");
            goto thread_failure;
        }

        /* Did the video thread flush the fifo? */
        if (fifoRet == Dmai_EFLUSH) {
            goto thread_exit;
        }

        /* Send the buffer to the circular buffer */
        if (!gst_ticircbuffer_queue_data(videnc->circBuf, encData)) {
            goto thread_failure;
        }
        /* Release the buffer we received from the sink pad */
        gst_buffer_unref(encData);

        /* If we've reached the EOS, start draining the circular buffer when
         * there are no more buffers in the FIFO.
         */
        if (videnc->drainingEOS && Fifo_getNumEntries(videnc->hInFifo) == 0) {
            gst_ticircbuffer_drain(videnc->circBuf, TRUE);
        }

        /* Unblock any pending puts to our Fifo if we have reached our
         * minimum threshold.
         */
        gst_tividenc_broadcast_queue_thread(videnc);
    }

thread_failure:
    gst_tithread_set_status(videnc, TIThread_QUEUE_ABORTED);
    threadRet = GstTIThreadFailure;

thread_exit:
    gst_object_unref(videnc);
    return threadRet;
}


/******************************************************************************
 * gst_tividenc_wait_on_queue_thread
 *    Wait for the queue thread to consume buffers from the fifo until
 *    there are only "waitQueueSize" items remaining.
 ******************************************************************************/
static void gst_tividenc_wait_on_queue_thread(GstTIVidenc *videnc,
                Int32 waitQueueSize)
{
    videnc->waitQueueSize = waitQueueSize;
    Rendezvous_meet(videnc->waitOnQueueThread);
}


/******************************************************************************
 * gst_tividenc_broadcast_queue_thread
 *    Broadcast when queue thread has processed enough buffers from the
 *    fifo to unblock anyone waiting to queue some more.
 ******************************************************************************/
static void gst_tividenc_broadcast_queue_thread(GstTIVidenc *videnc)
{
    if (videnc->waitQueueSize < Fifo_getNumEntries(videnc->hInFifo)) {
          return;
    } 
    Rendezvous_force(videnc->waitOnQueueThread);
}


/******************************************************************************
 * gst_tividenc_drain_pipeline
 *    Push any remaining input buffers through the queue and encode threads
 ******************************************************************************/
static void gst_tividenc_drain_pipeline(GstTIVidenc *videnc)
{
    gboolean checkResult;

    videnc->drainingEOS = TRUE;

    /* If the processing threads haven't been created, there is nothing to
     * drain.
     */
    if (!gst_tithread_check_status(
             videnc, TIThread_DECODE_CREATED, checkResult)) {
        return;
    }
    if (!gst_tithread_check_status(
             videnc, TIThread_QUEUE_CREATED, checkResult)) {
        return;
    }

    /* If the queue fifo still has entries in it, it will drain the
     * circular buffer once all input buffers have been added to the
     * circular buffer.  If the fifo is already empty, we must drain
     * the circular buffer here.
     */
    if (Fifo_getNumEntries(videnc->hInFifo) == 0) {
        gst_ticircbuffer_drain(videnc->circBuf, TRUE);
    }
    else {
        Rendezvous_force(videnc->waitOnQueueThread);
    }

    /* Wait for the encoder to drain */
    if (!videnc->encodeDrained) {
        Rendezvous_meet(videnc->waitOnEncodeDrain);
    }
    videnc->encodeDrained = FALSE;

}


/******************************************************************************
 * gst_tividenc_frame_duration
 *    Return the duration of a single frame in nanoseconds.
 ******************************************************************************/
static GstClockTime gst_tividenc_frame_duration(GstTIVidenc *videnc)
{
    /* Default to 29.97 if the frame rate was not specified */
    if (videnc->framerateNum == 0 && videnc->framerateDen == 0) {
        GST_WARNING("framerate not specified; using 29.97fps");
        videnc->framerateNum = 30000;
        videnc->framerateDen = 1001;
    }

    return (GstClockTime)
        ((1 / ((gdouble)videnc->framerateNum/(gdouble)videnc->framerateDen))
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
