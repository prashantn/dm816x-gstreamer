/*
 * gsttividenc1.c
 *
 * This file defines the "TIVidenc1" element, which encodes an xDM 1.x video
 * stream.
 *
 * Example usage:
 *     gst-launch videotestsrc num-buffers=10 !
 *         TIVidenc1 engineName="<engine name>" codecName="<codecName>" !
 *         fakesink silent=TRUE
 *
 * Notes:
 *  * This element currently assumes that video input is in  UYVY or Y8C8
 *    formats.
 *
 *    Note: On DM6467, VDCE is used to perform Ccv from YUV422P semi to
 *    YUV420P semi.
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
#include <ti/sdo/dmai/ce/Venc1.h>
#include <ti/sdo/dmai/Cpu.h>
#include <ti/sdo/dmai/Ccv.h>
#include <ti/sdo/dmai/ColorSpace.h>
#include <ti/sdo/dmai/Framecopy.h>

#include "gsttividenc1.h"
#include "gsttidmaibuffertransport.h"
#include "gstticodecs.h"
#include "gsttithreadprops.h"
#include "gstticommonutils.h"

/* Declare variable used to categorize GST_LOG output */
GST_DEBUG_CATEGORY_STATIC (gst_tividenc1_debug);
#define GST_CAT_DEFAULT gst_tividenc1_debug

/* Element property identifiers */
enum
{
  PROP_0,
  PROP_ENGINE_NAME,     /* engineName     (string)  */
  PROP_CODEC_NAME,      /* codecName      (string)  */
  PROP_RESOLUTION,      /* resolution     (string)  */
  PROP_BITRATE,         /* bitrate        (int)     */
  PROP_IN_COLORSPACE,   /* iColorSpace    (string)  */
  PROP_NUM_OUTPUT_BUFS, /* numOutputBuf   (int)     */
  PROP_FRAMERATE,       /* frameRate      (int)     */
  PROP_CONTIG_INPUT_BUF,/* contiguousInputFrame (boolean) */
  PROP_GEN_TIMESTAMPS   /* genTimeStamps  (boolean) */
};

/* Define source (output) pad capabilities.  Currently, MPEG2/4 and H264 are 
 * supported.
 */
static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE(
    "src",
    GST_PAD_SRC,
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

/* Define sink (input) pad capabilities.  Currently, UYVY and Y8C8 supported.
 *
 * UYVY - YUV 422 interleaved corresponding to V4L2_PIX_FMT_UYVY in v4l2
 * Y8C8 - YUV 422 semi planar. The dm6467 VDCE outputs this format after a
 *        color conversion.The format consists of two planes: one with the
 *        Y component and one with the CbCr components interleaved (hence semi)  *
 *        See the LSP VDCE documentation for a thorough description of this
 *        format.
 */
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
     "video/x-raw-yuv, "                        /* Y8C8 - YUV422 semi planar */
         "format=(fourcc)Y8C8, "               
         "framerate=(fraction)[ 0, MAX ], "
         "width=(int)[ 1, MAX ], "
         "height=(int)[ 1, MAX ]"
    )
);

/* Declare a global pointer to our element base class */
static GstElementClass *parent_class = NULL;

/* Static Function Declarations */
static void
 gst_tividenc1_base_init(gpointer g_class);
static void
 gst_tividenc1_class_init(GstTIVidenc1Class *g_class);
static void
 gst_tividenc1_init(GstTIVidenc1 *object, GstTIVidenc1Class *g_class);
static void
 gst_tividenc1_set_property (GObject *object, guint prop_id,
     const GValue *value, GParamSpec *pspec);
static void
 gst_tividenc1_get_property (GObject *object, guint prop_id, GValue *value,
     GParamSpec *pspec);
static gboolean
 gst_tividenc1_set_sink_caps(GstPad *pad, GstCaps *caps);
static gboolean
 gst_tividenc1_set_source_caps(GstTIVidenc1 *videnc1, Buffer_Handle hBuf);
static gboolean
 gst_tividenc1_sink_event(GstPad *pad, GstEvent *event);
static GstFlowReturn
 gst_tividenc1_chain(GstPad *pad, GstBuffer *buf);
static gboolean
 gst_tividenc1_init_video(GstTIVidenc1 *videnc1);
static gboolean
 gst_tividenc1_exit_video(GstTIVidenc1 *videnc1);
static GstStateChangeReturn
 gst_tividenc1_change_state(GstElement *element, GstStateChange transition);
static void*
 gst_tividenc1_encode_thread(void *arg);
static void
 gst_tividenc1_wait_on_fifo_threashold(GstTIVidenc1 *videnc1, 
    Int32 waitQueueSize);
static GstClockTime
 gst_tividenc1_frame_duration(GstTIVidenc1 *videnc1);
static ColorSpace_Type 
 gst_tividenc1_find_colorSpace (const gchar *colorSpace);
static gboolean
 gst_tividenc1_codec_start (GstTIVidenc1 *videnc1);
static gboolean
 gst_tividenc1_codec_stop (GstTIVidenc1 *videnc1);
static void 
 gst_tividenc1_unblock_fifo(GstTIVidenc1 *videnc1);

/* This variable is used to flush the fifo.  It is pushed to the
 * fifo when we want to flush it.  When the encode/decode thread
 * receives the address of this variable the fifo is flushed and
 * the thread can exit.  The value of this variable is not used.
 */
static int gst_ti_fifo_exit    = 0;
static int gst_ti_fifo_eos     = 0;

/******************************************************************************
 * gst_tividenc1_class_init_trampoline
 *    Boiler-plate function auto-generated by "make_element" script.
 ******************************************************************************/
static void gst_tividenc1_class_init_trampoline(gpointer g_class, gpointer data)
{
    parent_class = (GstElementClass*) g_type_class_peek_parent(g_class);
    gst_tividenc1_class_init((GstTIVidenc1Class*)g_class);
}

/******************************************************************************
 * gst_tividenc1_get_type
 *    Boiler-plate function auto-generated by "make_element" script.
 *    Defines function pointers for initialization routines for this element.
 ******************************************************************************/
GType gst_tividenc1_get_type(void)
{
    static GType object_type = 0;

    if (G_UNLIKELY(object_type == 0)) {
        static const GTypeInfo object_info = {
            sizeof(GstTIVidenc1Class),
            gst_tividenc1_base_init,
            NULL,
            gst_tividenc1_class_init_trampoline,
            NULL,
            NULL,
            sizeof(GstTIVidenc1),
            0,
            (GInstanceInitFunc) gst_tividenc1_init
        };

        object_type = g_type_register_static((gst_element_get_type()),
                          "GstTIVidenc1", &object_info, (GTypeFlags)0);

        /* Initialize GST_LOG for this object */
        GST_DEBUG_CATEGORY_INIT(gst_tividenc1_debug, "TIVidenc1", 0,
            "TI xDM 1.x Video Encoder");

        GST_LOG("initialized get_type\n");
    }

    return object_type;
};

/******************************************************************************
 * gst_tividenc1_base_init
 *    Boiler-plate function auto-generated by "make_element" script.
 *    Initializes element base class.
 ******************************************************************************/
static void gst_tividenc1_base_init(gpointer gclass)
{
    static GstElementDetails element_details = {
        "TI xDM 1.x Video Encoder",
        "Codec/Encoder/Video",
        "Encodes video using an xDM 1.x-based codec",
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
 * gst_tividenc1_class_init
 *    Boiler-plate function auto-generated by "make_element" script.
 *    Initializes the TIVidenc1 class.
 ******************************************************************************/
static void gst_tividenc1_class_init(GstTIVidenc1Class *klass)
{
    GObjectClass    *gobject_class;
    GstElementClass *gstelement_class;

    gobject_class    = (GObjectClass*)    klass;
    gstelement_class = (GstElementClass*) klass;

    gobject_class->set_property = gst_tividenc1_set_property;
    gobject_class->get_property = gst_tividenc1_get_property;

    gstelement_class->change_state = gst_tividenc1_change_state;

    g_object_class_install_property(gobject_class, PROP_ENGINE_NAME,
        g_param_spec_string("engineName", "Engine Name",
            "Engine name used by Codec Engine", "unspecified",
            G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_CODEC_NAME,
        g_param_spec_string("codecName", "Codec Name", "Name of video codec",
            "unspecified", G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_RESOLUTION,
        g_param_spec_string("resolution", "Input resolution", 
            "Input resolution for the input video ('width''x''height')",
            "720x480", G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_IN_COLORSPACE,
        g_param_spec_string("iColorSpace", "Input colorspace",
            "Input color space (UYVY or Y8C8)",
            "unspecified", G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_BITRATE,
        g_param_spec_int("bitRate",
            "encoder bitrate",
            "Set the video encoder bit rate",
            1, G_MAXINT32, 200000, G_PARAM_WRITABLE));

    /* We allow more than three output buffer because this is the buffer that
     * is sent to the downstream element.  It may be that we need to have
     * more than 3 buffer if the downstream element doesn't give the buffer
     * back in time for the codec to use.
     */
    g_object_class_install_property(gobject_class, PROP_NUM_OUTPUT_BUFS,
        g_param_spec_int("numOutputBufs",
            "Number of Ouput Buffers",
            "Number of output buffers to allocate for codec",
            3, G_MAXINT32, 3, G_PARAM_WRITABLE));

    g_object_class_install_property(gobject_class, PROP_FRAMERATE,
        g_param_spec_int("frameRate",
            "Frame rate to play output",
            "Communicate this framerate to downstream elements.  The frame "
            "rate specified should be an integer.  If 29.97fps is desired, "
            "specify 30 for this setting",
            1, G_MAXINT32, 30, G_PARAM_WRITABLE));

    g_object_class_install_property(gobject_class, PROP_CONTIG_INPUT_BUF,
        g_param_spec_boolean("contiguousInputFrame", "Contiguous Input frame",
            "Set this if elemenet recieved contiguous input frame",
            FALSE, G_PARAM_WRITABLE));

    g_object_class_install_property(gobject_class, PROP_GEN_TIMESTAMPS,
        g_param_spec_boolean("genTimeStamps", "Generate Time Stamps",
            "Set timestamps on output buffers",
            TRUE, G_PARAM_WRITABLE));
}

/******************************************************************************
 * gst_tividenc1_init
 *    Initializes a new element instance, instantiates pads and sets the pad
 *    callback functions.
 ******************************************************************************/
static void gst_tividenc1_init(GstTIVidenc1 *videnc1, GstTIVidenc1Class *gclass)
{
    /* Instantiate encoded video sink pad.
     *
     * Fixate on our static template caps instead of writing a getcaps
     * function, which is overkill for this element.
     */
    videnc1->sinkpad =
        gst_pad_new_from_static_template(&sink_factory, "sink");
    gst_pad_set_setcaps_function(
        videnc1->sinkpad, GST_DEBUG_FUNCPTR(gst_tividenc1_set_sink_caps));
    gst_pad_set_event_function(
        videnc1->sinkpad, GST_DEBUG_FUNCPTR(gst_tividenc1_sink_event));
    gst_pad_set_chain_function(
        videnc1->sinkpad, GST_DEBUG_FUNCPTR(gst_tividenc1_chain));
    gst_pad_fixate_caps(videnc1->sinkpad,
        gst_caps_make_writable(
            gst_caps_copy(gst_pad_get_pad_template_caps(videnc1->sinkpad))));

    /* Instantiate deceoded video source pad.
     *
     * Fixate on our static template caps instead of writing a getcaps
     * function, which is overkill for this element.
     */
    videnc1->srcpad =
        gst_pad_new_from_static_template(&src_factory, "src");
    gst_pad_fixate_caps(videnc1->srcpad,
        gst_caps_make_writable(
            gst_caps_copy(gst_pad_get_pad_template_caps(videnc1->srcpad))));

    /* Add pads to TIVidenc1 element */
    gst_element_add_pad(GST_ELEMENT(videnc1), videnc1->sinkpad);
    gst_element_add_pad(GST_ELEMENT(videnc1), videnc1->srcpad);

    /* Initialize TIVidenc1 state */
    videnc1->engineName             = NULL;
    videnc1->codecName              = NULL;
    videnc1->genTimeStamps          = TRUE;

    videnc1->hEngine                = NULL;
    videnc1->hVe1                   = NULL;
    videnc1->threadStatus           = 0UL;

    videnc1->hInFifo                = NULL;

    videnc1->waitOnFifoThreashold      = NULL;
    videnc1->waitQueueSize          = 0;

    videnc1->waitOnEncodeThread     = NULL;
    videnc1->waitOnBufTab           = NULL;

    videnc1->framerateNum           = 0;
    videnc1->framerateDen           = 0;
    videnc1->hOutBufTab             = NULL;

    videnc1->width                  = 0;
    videnc1->height                 = 0;
    videnc1->bitRate                = -1;
    videnc1->colorSpace             = ColorSpace_NOTSET;

    videnc1->numOutputBufs          = 0;

    videnc1->contiguousInputFrame   = FALSE;
    videnc1->waitOnFreeMmapBuf      = NULL;
}

/******************************************************************************
 * gst_tividenc1_find_colorSpace
 *****************************************************************************/
static ColorSpace_Type gst_tividenc1_find_colorSpace (const gchar *colorSpace)
{
    if (!strcmp(colorSpace, "UYVY"))
        return ColorSpace_UYVY;
    else if (!strcmp(colorSpace, "Y8C8")) 
        return ColorSpace_YUV422PSEMI;
    else 
        return ColorSpace_NOTSET; 
}

/******************************************************************************
 * gst_tividenc1_set_property
 *     Set element properties when requested.
 ******************************************************************************/
static void gst_tividenc1_set_property(GObject *object, guint prop_id,
                const GValue *value, GParamSpec *pspec)
{
    GstTIVidenc1 *videnc1 = GST_TIVIDENC1(object);

    GST_LOG("begin set_property\n");

    switch (prop_id) {
        case PROP_IN_COLORSPACE:
            if (videnc1->iColor) {
                g_free((gpointer)videnc1->iColor);
            }
            videnc1->iColor =
                (gchar*)g_malloc(strlen(g_value_get_string(value)) + 1);
            strcpy((gchar *)videnc1->iColor, g_value_get_string(value));
            videnc1->colorSpace =
                 gst_tividenc1_find_colorSpace(g_value_get_string(value));
            GST_LOG("setting \"iColorSpace\" to \"%d\"\n",videnc1->colorSpace);
            break; 
        case PROP_RESOLUTION:
            if (videnc1->resolution) {
                g_free((gpointer)videnc1->resolution);
            }
            videnc1->resolution =
                (gchar*)g_malloc(strlen(g_value_get_string(value)) + 1);
            strcpy((gchar *)videnc1->resolution, g_value_get_string(value));
            sscanf(g_value_get_string(value), "%dx%d", &videnc1->width,
                        &videnc1->height);
            GST_LOG("setting \"resolution\" to \"%dx%d\"\n", videnc1->width,
                        videnc1->height);
            break;
        case PROP_BITRATE:
            videnc1->bitRate =  g_value_get_int(value);
            GST_LOG("setting \"bitRate\" to \"%d\" \n", videnc1->bitRate);
            break; 
        case PROP_ENGINE_NAME:
            if (videnc1->engineName) {
                g_free((gpointer)videnc1->engineName);
            }
            videnc1->engineName =
                (gchar*)g_malloc(strlen(g_value_get_string(value)) + 1);
            strcpy((gchar *)videnc1->engineName, g_value_get_string(value));
            GST_LOG("setting \"engineName\" to \"%s\"\n", videnc1->engineName);
            break;
        case PROP_CODEC_NAME:
            if (videnc1->codecName) {
                g_free((gpointer)videnc1->codecName);
            }
            videnc1->codecName =
                (gchar*)g_malloc(strlen(g_value_get_string(value)) + 1);
            strcpy((gchar*)videnc1->codecName, g_value_get_string(value));
            GST_LOG("setting \"codecName\" to \"%s\"\n", videnc1->codecName);
            break;
        case PROP_NUM_OUTPUT_BUFS:
            videnc1->numOutputBufs = g_value_get_int(value);
            GST_LOG("setting \"numOutputBufs\" to \"%d\" \n",
                     videnc1->numOutputBufs);
            break;
        case PROP_FRAMERATE:
        {
            videnc1->framerateNum = g_value_get_int(value);
            videnc1->framerateDen = 1;

            /* If 30fps was specified, use 29.97 */
            if (videnc1->framerateNum == 30) {
                videnc1->framerateNum = 30000;
                videnc1->framerateDen = 1001;
            }

            GST_LOG("setting \"frameRate\" to \"%2.2lf\"\n",
               (gdouble)videnc1->framerateNum / (gdouble)videnc1->framerateDen);
            break;
        }
        case PROP_CONTIG_INPUT_BUF:
            videnc1->contiguousInputFrame = g_value_get_boolean(value);
            GST_LOG("setting \"contiguousInputFrame\" to \"%s\"\n",
                videnc1->contiguousInputFrame ? "TRUE" : "FALSE");
            break;
        case PROP_GEN_TIMESTAMPS:
            videnc1->genTimeStamps = g_value_get_boolean(value);
            GST_LOG("setting \"genTimeStamps\" to \"%s\"\n",
                videnc1->genTimeStamps ? "TRUE" : "FALSE");
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }

    GST_LOG("end set_property\n");
}

/******************************************************************************
 * gst_tividenc1_get_property
 *     Return values for requested element property.
 ******************************************************************************/
static void gst_tividenc1_get_property(GObject *object, guint prop_id,
                GValue *value, GParamSpec *pspec)
{
    GstTIVidenc1 *videnc1 = GST_TIVIDENC1(object);

    GST_LOG("begin get_property\n");

    switch (prop_id) {
        case PROP_ENGINE_NAME:
            g_value_set_string(value, videnc1->engineName);
            break;
        case PROP_CODEC_NAME:
            g_value_set_string(value, videnc1->codecName);
            break;
        case PROP_RESOLUTION:
            g_value_set_string(value, videnc1->resolution);
            break;
        case PROP_IN_COLORSPACE:
            g_value_set_string(value, videnc1->iColor);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }

    GST_LOG("end get_property\n");
}


/******************************************************************************
 * gst_tividenc1_set_sink_caps
 *     Negotiate our sink pad capabilities.
 ******************************************************************************/
static gboolean gst_tividenc1_set_sink_caps(GstPad *pad, GstCaps *caps)
{
    GstTIVidenc1  *videnc1;
    GstStructure *capStruct;
    const gchar  *mime;
    char         *string;

    videnc1    = GST_TIVIDENC1(gst_pad_get_parent(pad));
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
            videnc1->framerateNum = framerateNum;
            videnc1->framerateDen = framerateDen;
        }
    }

    /* RAW YUV input */
    if (!strcmp(mime,"video/x-raw-yuv")) {
        guint32 fourcc;

        if (gst_structure_get_fourcc(capStruct, "format", &fourcc)) {

            switch (fourcc) {
                case GST_MAKE_FOURCC('U', 'Y', 'V', 'Y'):
                    videnc1->colorSpace = ColorSpace_UYVY;
                    break;

                case GST_MAKE_FOURCC('Y', '8', 'C', '8'):                    
                    videnc1->colorSpace = ColorSpace_YUV422PSEMI;
                    break;

                default:
                    GST_ERROR("unsupported fourcc in video stream\n");
                    gst_object_unref(videnc1);
                    return FALSE;
            }

            if (!gst_structure_get_int(capStruct, "width", &videnc1->width)) {
                        videnc1->width = 0;
            }

            if (!gst_structure_get_int(capStruct, "height", &videnc1->height)) {
                        videnc1->height = 0;
            }
        }
    }

    /* Mime type not supported */
    else {
        GST_ERROR("stream type not supported");
        gst_object_unref(videnc1);
        return FALSE;
    }

    /* Shut-down any running video encoder */
    if (!gst_tividenc1_exit_video(videnc1)) {
        gst_object_unref(videnc1);
        return FALSE;
    }

    gst_object_unref(videnc1);

    GST_LOG("sink caps negotiation successful\n");
    return TRUE;
}

/******************************************************************************
 * gst_tividenc1_set_source_caps
 *     Negotiate our source pad capabilities.
 ******************************************************************************/
static gboolean gst_tividenc1_set_source_caps(
                    GstTIVidenc1 *videnc1, Buffer_Handle hBuf)
{
    BufferGfx_Dimensions  dim;
    GstCaps              *caps = NULL;
    gboolean              ret;
    GstPad               *pad;
    char                 *string;

    pad = videnc1->srcpad;

    /* Create a video caps object using the dimensions from the given buffer */
    BufferGfx_getDimensions(hBuf, &dim);

    /* Create H.264 source cap */
    if (strstr(videnc1->codecName, "h264enc")) {
        caps =
            gst_caps_new_simple("video/x-h264",
                "framerate",    GST_TYPE_FRACTION,  videnc1->framerateNum,
                                                    videnc1->framerateDen,
                "width",        G_TYPE_INT,         dim.width,
                "height",       G_TYPE_INT,         dim.height,
                NULL);
        
        string =  gst_caps_to_string(caps);
        GST_LOG("setting source caps to x-h264: %s", string);
        g_free(string);
    }
 
    /* Create MPEG4 source cap */
    if (strstr(videnc1->codecName, "mpeg4enc")) {
        gint mpegversion = 4;

        caps =
            gst_caps_new_simple("video/mpeg",
                "mpegversion",  G_TYPE_INT,         mpegversion,
                "framerate",    GST_TYPE_FRACTION,  videnc1->framerateNum,
                                                    videnc1->framerateDen,
                "width",        G_TYPE_INT,         dim.width,
                "height",       G_TYPE_INT,         dim.height,
                NULL);

        string =  gst_caps_to_string(caps); 
        GST_LOG("setting source caps to mpeg4: %s", string);
        g_free(string);
    }

    /* Set the source pad caps */
    ret = gst_pad_set_caps(pad, caps);
    gst_caps_unref(caps);

    return ret;
}

/******************************************************************************
 * gst_tividenc1_sink_event
 *     Perform event processing on the input stream.  At the moment, this
 *     function isn't needed as this element doesn't currently perform any
 *     specialized event processing.  We'll leave it in for now in case we need
 *     it later on.
 ******************************************************************************/
static gboolean gst_tividenc1_sink_event(GstPad *pad, GstEvent *event)
{
    GstTIVidenc1 *videnc1;
    gboolean     ret;

    videnc1 = GST_TIVIDENC1(GST_OBJECT_PARENT(pad));

    GST_DEBUG("pad \"%s\" received:  %s\n", GST_PAD_NAME(pad),
        GST_EVENT_TYPE_NAME(event));

    switch (GST_EVENT_TYPE(event)) {

        case GST_EVENT_NEWSEGMENT:
            /* maybe save and/or update the current segment (e.g. for output
             * clipping) or convert the event into one in a different format
             * (e.g. BYTES to TIME) or drop it and set a flag to send a
             * newsegment event in a different format later
             */
            ret = gst_pad_push_event(videnc1->srcpad, event);
            break;

        case GST_EVENT_EOS:
            /* end-of-stream: process any remaining encoded frame data */
            GST_LOG("no more input; draining remaining encoded video data\n");

            /* Push the gst_ti_fifo_eos buffer to let the encode thread know 
             * that we have recieved EOS and it need to reset temporary 
             * variables and discard the saved data.
             */
            if (Fifo_put(videnc1->hInFifo,&gst_ti_fifo_eos) < 0) {
                GST_ERROR("Could not put flush value to Fifo\n");
            }

            /* Propagate EOS to downstream elements */
            ret = gst_pad_push_event(videnc1->srcpad, event);
            break;

        case GST_EVENT_FLUSH_STOP:
            ret = gst_pad_push_event(videnc1->srcpad, event);
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
 * gst_tividenc1_chain
 *    This is the main processing routine.  This function receives a buffer
 *    from the sink pad, processes it, and pushes the result to the source
 *    pad.
 ******************************************************************************/
static GstFlowReturn gst_tividenc1_chain(GstPad * pad, GstBuffer * buf)
{
    GstTIVidenc1 *videnc1 = GST_TIVIDENC1(GST_OBJECT_PARENT(pad));
    gboolean     checkResult;

    /* If any thread aborted, communicate it to the pipeline */
    if (gst_tithread_check_status(videnc1, TIThread_ANY_ABORTED, checkResult)) {
       gst_buffer_unref(buf);
       return GST_FLOW_UNEXPECTED;
    }

    /* If our engine handle is currently NULL, then either this is our first
     * buffer or the upstream element has re-negotiated our capabilities which
     * resulted in our engine being closed.  In either case, we need to
     * initialize (or re-initialize) our video encoder to handle the new
     * stream.
     */
    if (videnc1->hEngine == NULL) {
        videnc1->upstreamBufSize = GST_BUFFER_SIZE(buf);
        if (!gst_tividenc1_init_video(videnc1)) {
            GST_ERROR("unable to initialize video\n");
            gst_buffer_unref(buf);
            return GST_FLOW_UNEXPECTED;
        }
    }

    /* Don't queue up too many buffers -- if we collect too many input buffers
     * without consuming them we'll run out of memory.  Once we reach a
     * threshold, block until the queue thread removes some buffers.
     */
    Rendezvous_reset(videnc1->waitOnFifoThreashold);
    if (Fifo_getNumEntries(videnc1->hInFifo) > videnc1->queueMaxBuffers) {
        GST_LOG("Blocking upstream and waiting for encoder to process some \
                 buffers\n");
        gst_tividenc1_wait_on_fifo_threashold(videnc1, 1);
    }

    /* Queue up the raw data stream into a fifo */
    if (Fifo_put(videnc1->hInFifo, buf) < 0) {
        GST_ERROR("Failed to send buffer to queue thread\n");
        gst_buffer_unref(buf);
        return GST_FLOW_UNEXPECTED;
    }

    /* If we are configured to recieve contiguousInputFrame (which is driver
     * mmap buffer from v4l2src) then wait for encoder thread to consume this
     * buffer before unrefing. 
     * If we unref this buffer in encoder thread then it results deadlock
     * somewhere in v4l2 driver.  We will use this workaround until we get
     * driver fixes in mainline kernel.
     */
    if (videnc1->contiguousInputFrame) {
        Rendezvous_meet(videnc1->waitOnFreeMmapBuf);
    }
   
    return GST_FLOW_OK;
}

/******************************************************************************
 * gst_tividenc1_init_video
 *     Initialize or re-initializes the video stream
 ******************************************************************************/
static gboolean gst_tividenc1_init_video(GstTIVidenc1 *videnc1)
{
    Rendezvous_Attrs      rzvAttrs   = Rendezvous_Attrs_DEFAULT;
    struct sched_param    schedParam;
    pthread_attr_t        attr;
    Fifo_Attrs            fAttrs     = Fifo_Attrs_DEFAULT;

    GST_LOG("begin init_video\n");

    /* If video has already been initialized, shut down previous encoder */
    if (videnc1->hEngine) {
        if (!gst_tividenc1_exit_video(videnc1)) {
            GST_ERROR("failed to shut down existing video encoder\n");
            return FALSE;
        }
    }

    /* Make sure we know what codec we're using */
    if (!videnc1->engineName) {
        GST_ERROR("engine name not specified\n");
        return FALSE;
    }

    if (!videnc1->codecName) {
        GST_ERROR("codec name not specified\n");
        return FALSE;
    }

    /* Set up the queue fifo */
    videnc1->hInFifo = Fifo_create(&fAttrs);

    /* Initialize thread status management */
    videnc1->threadStatus = 0UL;
    pthread_mutex_init(&videnc1->threadStatusMutex, NULL);

    /* Initialize rendezvous objects for making threads wait on conditions */
    videnc1->waitOnFifoThreashold  = Rendezvous_create(100, &rzvAttrs);
    videnc1->waitOnEncodeThread = Rendezvous_create(2, &rzvAttrs);
    videnc1->waitOnBufTab       = Rendezvous_create(100, &rzvAttrs);
    videnc1->waitOnFreeMmapBuf  = Rendezvous_create(2, &rzvAttrs);

    /* Initialize custom thread attributes */
    if (pthread_attr_init(&attr)) {
        GST_WARNING("failed to initialize thread attrs\n");
        gst_tividenc1_exit_video(videnc1);
        return FALSE;
    }

    /* Force the thread to use the system scope */
    if (pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM)) {
        GST_WARNING("failed to set scope attribute\n");
        gst_tividenc1_exit_video(videnc1);
        return FALSE;
    }

    /* Force the thread to use custom scheduling attributes */
    if (pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED)) {
        GST_WARNING("failed to set schedule inheritance attribute\n");
        gst_tividenc1_exit_video(videnc1);
        return FALSE;
    }

    /* Set the thread to be fifo real time scheduled */
    if (pthread_attr_setschedpolicy(&attr, SCHED_FIFO)) {
        GST_WARNING("failed to set FIFO scheduling policy\n");
        gst_tividenc1_exit_video(videnc1);
        return FALSE;
    }

    /* Set the display thread priority */
    schedParam.sched_priority = GstTIVideoThreadPriority;
    if (pthread_attr_setschedparam(&attr, &schedParam)) {
        GST_WARNING("failed to set scheduler parameters\n");
        return FALSE;
    }

    /* Create encoder thread */
    if (pthread_create(&videnc1->encodeThread, &attr,
            gst_tividenc1_encode_thread, (void*)videnc1)) {
        GST_ERROR("failed to create encode thread\n");
        gst_tividenc1_exit_video(videnc1);
        return FALSE;
    }
    gst_tithread_set_status(videnc1, TIThread_DECODE_CREATED);

    /* Destroy the custom thread attributes */
    if (pthread_attr_destroy(&attr)) {
        GST_WARNING("failed to destroy thread attrs\n");
        gst_tividenc1_exit_video(videnc1);
        return FALSE;
    }

    /* Wait for encoder thread to create the open the engine */
    Rendezvous_meet(videnc1->waitOnEncodeThread);

    GST_LOG("end init_video\n");
    return TRUE;
}

/******************************************************************************
 * gst_tividenc1_unblock_fifo
 *    Broadcast when the queue thread has processed enough buffers from the
 *    fifo to unblock anyone waiting to queue some more.
 ******************************************************************************/
static void gst_tividenc1_unblock_fifo(GstTIVidenc1 *videnc1)
{
    if (videnc1->waitQueueSize < Fifo_getNumEntries(videnc1->hInFifo)) {
          return;
    } 
    Rendezvous_force(videnc1->waitOnFifoThreashold);
}


/******************************************************************************
 * gst_tividenc1_exit_video
 *    Shut down any running video encoder, and reset the element state.
 ******************************************************************************/
static gboolean gst_tividenc1_exit_video(GstTIVidenc1 *videnc1)
{
    void*    thread_ret;
    gboolean checkResult;

    GST_LOG("begin exit_video\n");

    /* Shut down the encode thread */
    if (gst_tithread_check_status(
            videnc1, TIThread_DECODE_CREATED, checkResult)) {
        GST_LOG("shutting down encode thread\n");

        /* Push the gst_ti_fifo_exit buffer to let the encode thread know
         * that it need to release resource and shut down itself.
         */
        if (Fifo_put(videnc1->hInFifo,&gst_ti_fifo_exit) < 0) {
            GST_ERROR("Could not put flush value to Fifo\n");
        }

        if (pthread_join(videnc1->encodeThread, &thread_ret) == 0) {
            if (thread_ret == GstTIThreadFailure) {
                GST_DEBUG("encode thread exited with an error condition\n");
            }
        }
    }

    /* Shut down thread status management */
    videnc1->threadStatus = 0UL;
    pthread_mutex_destroy(&videnc1->threadStatusMutex);

    /* Shut down remaining items */
    if (videnc1->hInFifo) {
        Fifo_delete(videnc1->hInFifo);
        videnc1->hInFifo = NULL;
    }

    if (videnc1->waitOnFreeMmapBuf) {
        Rendezvous_delete(videnc1->waitOnFreeMmapBuf);
        videnc1->waitOnFreeMmapBuf = NULL;
    }

    if (videnc1->waitOnEncodeThread) {
        Rendezvous_delete(videnc1->waitOnEncodeThread);
        videnc1->waitOnEncodeThread = NULL;
    }

    if (videnc1->waitOnFifoThreashold) {
        Rendezvous_delete(videnc1->waitOnFifoThreashold);
        videnc1->waitOnFifoThreashold = NULL;
    }

    if (videnc1->waitOnBufTab) {
        Rendezvous_delete(videnc1->waitOnBufTab);
        videnc1->waitOnBufTab = NULL;
    }

    if (videnc1->hCpu) {
        GST_LOG("freeing cpu device buffer\n");
        Cpu_delete(videnc1->hCpu);
        videnc1->hCpu = NULL;
    }

    GST_LOG("end exit_video\n");
    return TRUE;
}


/******************************************************************************
 * gst_tividenc1_change_state
 *     Manage state changes for the video stream.  The gStreamer documentation
 *     states that state changes must be handled in this manner:
 *        1) Handle ramp-up states
 *        2) Pass state change to base class
 *        3) Handle ramp-down states
 ******************************************************************************/
static GstStateChangeReturn gst_tividenc1_change_state(GstElement *element,
                                GstStateChange transition)
{
    GstStateChangeReturn  ret    = GST_STATE_CHANGE_SUCCESS;
    GstTIVidenc1          *videnc1 = GST_TIVIDENC1(element);

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
            if (!gst_tividenc1_exit_video(videnc1)) {
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
 * gst_tividenc1_codec_stop
 *   stop codec engine
 *****************************************************************************/
static gboolean gst_tividenc1_codec_stop (GstTIVidenc1 *videnc1)
{
    if (videnc1->hOutBufTab) {
        GST_LOG("freeing output buffers\n");
        BufTab_delete(videnc1->hOutBufTab);
        videnc1->hOutBufTab = NULL;
    }

    if (videnc1->hVe1) {
        GST_LOG("closing video encoder\n");
        Venc1_delete(videnc1->hVe1);
        videnc1->hVe1 = NULL;
    }

    if (videnc1->hEngine) {
        GST_LOG("closing codec engine\n");
        Engine_close(videnc1->hEngine);
        videnc1->hEngine = NULL;
    }

    return TRUE;
}

/******************************************************************************
 * gst_tividenc1_codec_start
 *   start codec engine
 *****************************************************************************/
static gboolean gst_tividenc1_codec_start (GstTIVidenc1 *videnc1)
{
    VIDENC1_Params        params     = Venc1_Params_DEFAULT;
    VIDENC1_DynamicParams dynParams  = Venc1_DynamicParams_DEFAULT;
    BufferGfx_Attrs       gfxAttrs   = BufferGfx_Attrs_DEFAULT;
    Cpu_Attrs             cpuAttrs   = Cpu_Attrs_DEFAULT;

    /* Open the codec engine */
    GST_LOG("opening codec engine \"%s\"\n", videnc1->engineName);
    videnc1->hEngine = Engine_open((Char *) videnc1->engineName, NULL, NULL);

    if (videnc1->hEngine == NULL) {
        GST_ERROR("failed to open codec engine \"%s\"\n", videnc1->engineName);
        return FALSE;
    }

    /* Determine target board type */
    videnc1->hCpu = Cpu_create(&cpuAttrs);
    
    if (videnc1->hCpu == NULL) {
        GST_ERROR("Failed to create cpu module\n");
        return FALSE;
    }

    if (Cpu_getDevice(NULL, &videnc1->device) < 0) {
        GST_ERROR("Failed to determine target board\n");
        return FALSE;
    }

    /* Set up codec input and reconstruction chroma format depending on device.
     * Refer codec datasheet for supported input and reconstruction chroma 
     * format.
     */
    if (videnc1->device == Cpu_Device_DM6467) {
       params.inputChromaFormat = XDM_YUV_420P;
       params.reconChromaFormat = XDM_CHROMA_NA;
    }
    else {
        params.inputChromaFormat = XDM_YUV_422ILE;

        if (videnc1->device == Cpu_Device_DM355) {
            params.reconChromaFormat = XDM_YUV_420P;
        }
    }

    /* Set up codec parameters depending on bit rate */
    if (videnc1->bitRate < 0) {
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
        params.maxBitRate = videnc1->bitRate;
    }

    params.maxWidth         = videnc1->width;
    params.maxHeight        = videnc1->height;
    dynParams.targetBitRate = params.maxBitRate;
    dynParams.inputWidth    = videnc1->width;
    dynParams.inputHeight   = videnc1->height;

    GST_LOG("configuring video encode width=%ld, height=%ld, colorSpace=%d "
            "bitrate=%ld\n", params.maxWidth, params.maxHeight, 
            videnc1->colorSpace, params.maxBitRate);

    GST_LOG("opening video encoder \"%s\"\n", videnc1->codecName);
    videnc1->hVe1 = Venc1_create(videnc1->hEngine, (Char*)videnc1->codecName,
                      &params, &dynParams);

    if (videnc1->hVe1 == NULL) {
        GST_ERROR("failed to create video encoder: %s\n", videnc1->codecName);
        GST_LOG("closing codec engine\n");
        gst_tividenc1_exit_video(videnc1);
        return FALSE;
    }

    /* Calculate input frame size */
    videnc1->frameSize = BufferGfx_calcLineLength(videnc1->width, 
                            videnc1->colorSpace) * videnc1->height;
   
    /* On DM6467, set the framesize equal to upstream buffer size. Its because
     * the upstream buffer size is different than expected YUV422PSEMI buffer
     * size. This is bug in v4l2 capture driver.
     * NOTE: Because of this bug, on DM6467 plugin does not support encoding 
     * via filesrc.
     */
    if (videnc1->device == Cpu_Device_DM6467) {
        videnc1->frameSize = videnc1->upstreamBufSize;
    }

    GST_LOG("configuring to recieve input frame size %d\n", videnc1->frameSize);

    /* Calculate the maximum number of buffers allowed in queue before
     * blocking upstream.
     */
    videnc1->queueMaxBuffers =(videnc1->frameSize/videnc1->upstreamBufSize)+3;
    GST_LOG("setting max queue threadshold to %d\n", videnc1->queueMaxBuffers);

    /* Create codec output buffers */
    GST_LOG("creating output buffer table\n");
    gfxAttrs.colorSpace     = videnc1->colorSpace;
    gfxAttrs.dim.width      = videnc1->width;
    gfxAttrs.dim.height     = videnc1->height;
    gfxAttrs.dim.lineLength = BufferGfx_calcLineLength(
                                  gfxAttrs.dim.width, gfxAttrs.colorSpace);

    gfxAttrs.bAttrs.memParams.align = 128;
    gfxAttrs.bAttrs.useMask = gst_tidmaibuffertransport_GST_FREE;

    /* If user has passed 0, then we will default to 3 buffers */
    if (videnc1->numOutputBufs == 0) {
        videnc1->numOutputBufs = 3;
    }
    videnc1->hOutBufTab =
        BufTab_create(videnc1->numOutputBufs,
                Venc1_getOutBufSize(videnc1->hVe1),
                BufferGfx_getBufferAttrs(&gfxAttrs));

    if (videnc1->hOutBufTab == NULL) {
        GST_ERROR("failed to create output buffers size %ld\n",
            Venc1_getOutBufSize(videnc1->hVe1)*videnc1->numOutputBufs);
        return FALSE;
    }

    return TRUE;
}

/******************************************************************************
 * gst_tividenc1_encode_thread
 *     Call the video codec to process a full input buffer
 ******************************************************************************/
static void* gst_tividenc1_encode_thread(void *arg)
{
    GstTIVidenc1        *videnc1        = GST_TIVIDENC1(gst_object_ref(arg));
    GstBuffer           *decDataWindow  = NULL;
    BufferGfx_Attrs     gfxAttrs        = BufferGfx_Attrs_DEFAULT;
    void                *threadRet      = GstTIThreadSuccess;
    Buffer_Handle       hDstBuf         = NULL;
    Buffer_Handle       hInBuf          = NULL;
    Buffer_Handle       hCcvBuf         = NULL;
    Buffer_Handle       tmpBuf          = NULL;
    Buffer_Handle       gstBuf          = NULL;
    GstClockTime        frameDuration;
    GstBuffer           *outBuf;
    GstBuffer           *metaData       = gst_buffer_new();
    Int                 ret;
    Ccv_Handle          hCcv            = NULL;
    Ccv_Attrs           ccvAttrs        = Ccv_Attrs_DEFAULT;
    gint                bufSize;
    gint                decDataSize;
    gint                bufOffset       = 0;
    Int                 fifoRet;
    Int                 tmpBufSize;
    Buffer_Attrs        bAttrs          = Buffer_Attrs_DEFAULT;
    Framecopy_Attrs     fcAttrs         = Framecopy_Attrs_DEFAULT;
    Framecopy_Handle    hFc             = NULL;
    Int                 decDataConsumed;
    gint64              totalDuration   = 0;

    /* Calculate the duration of a single frame in this stream */
    frameDuration = gst_tividenc1_frame_duration(videnc1);

    /* Initialize codec engine */
    ret = gst_tividenc1_codec_start(videnc1);

    /* Notify main thread if it is waiting to start codec */
    Rendezvous_meet(videnc1->waitOnEncodeThread);

    if (ret == FALSE) {
        GST_ERROR("failed to start codec.\n");
        goto thread_exit;
    }

    /* On DM6467: upstream will send YUV422PSEMI buffer but codec needs 
     * YUV420PSEMI and we will use DMAI Ccv module to covert from 422 -> 420.
     */
    if (videnc1->device == Cpu_Device_DM6467) {
        GST_LOG("creating ccv job.\n");
        /* create a color conversion job from 422 to 420 */
        ccvAttrs.accel = TRUE;
        hCcv = Ccv_create(&ccvAttrs);
        
        if (hCcv == NULL) {
            GST_ERROR("Failed to create Ccv module\n");
            goto thread_failure;
        }

        gfxAttrs.dim.width          = videnc1->width;
        gfxAttrs.dim.height         = videnc1->height;
        gfxAttrs.colorSpace         = ColorSpace_YUV420PSEMI;
        gfxAttrs.dim.lineLength     = BufferGfx_calcLineLength(videnc1->width, 
                                        ColorSpace_YUV420PSEMI);
        bufSize = gfxAttrs.dim.lineLength * videnc1->height * 3 / 2;

        GST_LOG("creating ccv output buffer size %d\n", bufSize);
        hCcvBuf = Buffer_create(bufSize, BufferGfx_getBufferAttrs(&gfxAttrs));

        if (hCcvBuf == NULL) {
            GST_ERROR("Failed to allocate buffer for ccv module\n");
            goto thread_failure;
        }
    }

    /* If we are configured to recieve driver buffer then create framecopy
     * handle to copy the input buffer.
     */  
    if (videnc1->contiguousInputFrame) {
        /* NOTE: On DM6467, ccv will do color conversion and framecopy in 
         * single-shot.
         */
        if (videnc1->device != Cpu_Device_DM6467) {
            GST_LOG("creating framecopy job\n");

            fcAttrs.accel = TRUE;
            hFc = Framecopy_create(&fcAttrs);

            if (hFc == NULL) {
                GST_ERROR("failed to create framecopy module\n");
                goto thread_failure;
            }
        }

        /* Create a refer graphics buffer to hold the gstreamer buffer */
        GST_LOG("creating reference buffer\n");
        gfxAttrs.bAttrs.reference   = TRUE;
        gfxAttrs.dim.width          = videnc1->width;
        gfxAttrs.dim.height         = videnc1->height;
        gfxAttrs.colorSpace         = videnc1->colorSpace;
        gfxAttrs.dim.lineLength     = BufferGfx_calcLineLength(videnc1->width,
                                        videnc1->colorSpace);
        gstBuf = Buffer_create(videnc1->frameSize, 
                    BufferGfx_getBufferAttrs(&gfxAttrs));

        if (gstBuf == NULL) {
            GST_ERROR("Failed to allocate reference buffer.\n");
            goto thread_failure;
        }
    }
   
    /* create input buffer. This buffer will contain the data from upstream.
     *
     * NOTE: on DM6467, when contiguous input frame is used then hCcvBuf will
     * contain the data hence no need to create input buffer.
     */
    if (videnc1->contiguousInputFrame && videnc1->device == Cpu_Device_DM6467){
        GST_LOG(" do not create input buffer.\n");
    }
    else {
        GST_LOG("creating input buffer size %d\n", videnc1->frameSize);
        gfxAttrs.bAttrs.reference   = FALSE;
        gfxAttrs.colorSpace     = videnc1->colorSpace;
        gfxAttrs.dim.width      = videnc1->width;
        gfxAttrs.dim.height     = videnc1->height;
        gfxAttrs.dim.lineLength = BufferGfx_calcLineLength(
                                  gfxAttrs.dim.width, gfxAttrs.colorSpace);

        hInBuf = Buffer_create(videnc1->frameSize,
                 BufferGfx_getBufferAttrs(&gfxAttrs));        

        if (hInBuf == NULL) {
            GST_ERROR("failed to allocate input buf size %d\n", 
                videnc1->frameSize);
            goto thread_failure;
        }
    }

    /* Main thread loop */
    while (TRUE) {

        /* Get the next input buffer (or block until one is ready) */
        fifoRet = Fifo_get(videnc1->hInFifo, &decDataWindow);

        if (fifoRet < 0) {
            GST_LOG("Failed to get buffer from input fifo %d\n", fifoRet);
            goto thread_failure;
        }

        /* If we have recieved exit buffer on fifo then free the resource
         * and shut down encoder thread.
         */
        if (decDataWindow == (GstBuffer *)(&gst_ti_fifo_exit)) {
            GST_LOG("Processed last input buffer from Fifo; exiting.\n");
            goto thread_exit;
        }

        /* If we have recieved eos buffer on fifo then discard the saved buffer
         * and reset the bufOffset.
         */
        if (decDataWindow == (GstBuffer *)(&gst_ti_fifo_eos)) {
            GST_LOG("Discard the saved buffer and continue to wait..\n");
            
            /* If we have anything saved in temporary buffer then free it. */
            if (tmpBuf) {
                Buffer_delete(tmpBuf);
                tmpBuf = NULL;
            }

            /* Reset the buffer offset and continue to wait on fifo */
            bufOffset = 0;

            continue;
        }

        /* If we received a data frame of zero size, there is no more data to
         * process -- exit the thread.
         */
        if (GST_BUFFER_SIZE(decDataWindow) == 0) {
            GST_LOG("no video data remains, exiting video thread.\n");
            goto thread_exit;
        }

        /* Copy the gstreamer buffer meta data */
        gst_buffer_copy_metadata(metaData, decDataWindow,GST_BUFFER_COPY_ALL);

        /* If we are configured to recieve driver buffer then use framecopy 
         * module to copy the buffers from upstream to codec input buffer.
         */
        if (videnc1->contiguousInputFrame) {
            /* Set the user pointer and number of bytes used on gstBuf */
            Buffer_setUserPtr(gstBuf, (Int8*)GST_BUFFER_DATA(decDataWindow));
            Buffer_setNumBytesUsed(gstBuf,videnc1->frameSize);

            /* Perform CCV on DM6467 */
            if (videnc1->device == Cpu_Device_DM6467) { 
                BufferGfx_resetDimensions(hCcvBuf);

                if (Ccv_config(hCcv, gstBuf, hCcvBuf) < 0) {
                    GST_ERROR("Failed to configure CCV.\n");
                    gst_buffer_unref(decDataWindow);
                    goto thread_failure;
                }

                if (Ccv_execute(hCcv, gstBuf, hCcvBuf) < 0) {
                    GST_ERROR("Failed to execute CCV.\n");
                    gst_buffer_unref(decDataWindow);
                    goto thread_failure;
                }
            }
            else {
                BufferGfx_resetDimensions(hInBuf);

                if (Framecopy_config(hFc, gstBuf, hInBuf) < 0) {
                    GST_ERROR("Failed to configure framecopy.\n");
                    gst_buffer_unref(decDataWindow);
                    goto thread_failure;
                }

                if (Framecopy_execute(hFc, gstBuf, hInBuf) < 0) {
                    GST_ERROR("Failed to execute framecopy.\n");
                    gst_buffer_unref(decDataWindow);
                    goto thread_failure;
                }
            }

            /* unref the buffer */ 
            gst_buffer_unref(decDataWindow);

            /* We are done with the driver buffer, inform chain to unblock main
             * pipeline.
             */            
            Rendezvous_meet(videnc1->waitOnFreeMmapBuf);
        }
        else {
            /* If tempBuf contains data from previous buffer then prepend it in
             * input buffer and release tmpBuf.
             */
            if (tmpBuf) {             
                memcpy(Buffer_getUserPtr(hInBuf), Buffer_getUserPtr(tmpBuf), 
                        Buffer_getSize(tmpBuf));
                Buffer_delete(tmpBuf);
                tmpBuf = NULL;
            }
            decDataSize  = GST_BUFFER_SIZE(decDataWindow);

            /* If accumulated buffer size plus received buffer size is equal
             * to a full frame then copy the received buffer in input buffer.
             */
            if ((bufOffset + decDataSize) == videnc1->frameSize) {
                memcpy(Buffer_getUserPtr(hInBuf)+bufOffset, 
                    GST_BUFFER_DATA(decDataWindow), decDataSize);

                /* Reset the buffer offset */
                bufOffset = 0;
            }
            /* If accumulated buffer size plus received buffer size does not
             * completes a full frame then save the buffer and wait for more.
             */
            else if ((bufOffset + decDataSize) < videnc1->frameSize) {
                memcpy(Buffer_getUserPtr(hInBuf)+bufOffset, 
                    GST_BUFFER_DATA(decDataWindow), decDataSize);
                
                /* Update the bufOffset */
                bufOffset += decDataSize;
                
                /* unref the gstreamer buffer */
                gst_buffer_unref(decDataWindow);
                
                /* continue to wait on fifo_get */
                continue;
            }
            /* If accumulated buffer size plus received buffer size is greater
             * than a full frame then split the recieved buffer, used the first 
             * buffer to create full frame and save the second buffer.
             */
            else {
                /* copy the part of input buffer to construct full frame. */
                memcpy(Buffer_getUserPtr(hInBuf)+bufOffset, 
                    GST_BUFFER_DATA(decDataWindow), 
                    videnc1->frameSize - bufOffset);

                /* Save the remaining data in temporary buffer */
                tmpBufSize = decDataSize - (videnc1->frameSize - bufOffset);
                tmpBuf = Buffer_create(tmpBufSize, &bAttrs);

                if (tmpBuf == NULL) {
                    goto thread_failure;
                }

                memcpy(Buffer_getUserPtr(tmpBuf), 
                    GST_BUFFER_DATA(decDataWindow) + tmpBufSize, tmpBufSize);

                /* Update bufOffset */
                bufOffset = tmpBufSize; 
            }
          
            /* unref the buffer */ 
            gst_buffer_unref(decDataWindow);

            Buffer_setNumBytesUsed(hInBuf, videnc1->frameSize);

            /* Perform ccv on DM6467 */
            if (videnc1->device == Cpu_Device_DM6467) {
                BufferGfx_resetDimensions(hCcvBuf);

                if (Ccv_config(hCcv, hInBuf, hCcvBuf) < 0) {
                    GST_ERROR("failed to configure ccv module\n");
                    goto thread_failure;
                }

                if (Ccv_execute(hCcv, hInBuf, hCcvBuf) < 0) {
                    GST_ERROR("failed to execute ccv module\n");
                    goto thread_failure;
                }
            }
        }
    
        /* Obtain a free output buffer for the encoded data */
        /* If we are not able to find free buffer from BufTab then encoder
         * thread will be blocked on waitOnBufTab rendezvous. And this will be 
         * woke-up by dmaitransportbuffer finalize method.
         */
        hDstBuf = BufTab_getFreeBuf(videnc1->hOutBufTab);
        if (hDstBuf == NULL) {
            Rendezvous_meet(videnc1->waitOnBufTab);
            hDstBuf = BufTab_getFreeBuf(videnc1->hOutBufTab);

            if (hDstBuf == NULL) {
                GST_ERROR("failed to get a free contiguous buffer from"
                            " BufTab\n");
                goto thread_failure;
            }
        }

        /* Reset waitOnBufTab rendezvous handle to its orignal state */
        Rendezvous_reset(videnc1->waitOnBufTab);

        /* Make sure the whole buffer is used for output */
        BufferGfx_resetDimensions(hDstBuf);

        GST_LOG("invoking the video encoder\n");
        if (videnc1->device ==  Cpu_Device_DM6467) {
            ret   = Venc1_process(videnc1->hVe1, hCcvBuf, hDstBuf);
            decDataConsumed = Buffer_getNumBytesUsed(hCcvBuf);
        }
        else {
            ret   = Venc1_process(videnc1->hVe1, hInBuf, hDstBuf);
            decDataConsumed = Buffer_getNumBytesUsed(hInBuf);
        }

        /* unblock anything waiting on fifo. */ 
        gst_tividenc1_unblock_fifo(videnc1);

        if (ret < 0) {
            GST_ERROR("failed to encode video buffer\n");
            goto thread_failure;
        }

        /* If no encoded data was used we cannot find the next frame */
        if (ret == Dmai_EBITERROR && decDataConsumed == 0) {
            GST_ERROR("fatal bit error\n");
            goto thread_failure;
        }

        if (ret > 0) {
            GST_LOG("Venc1_process returned success code %d\n", ret); 
        }

        /* Set the source pad capabilities based on the encoded frame
         * properties.
         */
        gst_tividenc1_set_source_caps(videnc1, hDstBuf);

        /* Create a DMAI transport buffer object to carry a DMAI buffer to
         * the source pad.  The transport buffer knows how to release the
         * buffer for re-use in this element when the source pad calls
         * gst_buffer_unref().
         */
        outBuf = gst_tidmaibuffertransport_new(hDstBuf, videnc1->waitOnBufTab);
        gst_buffer_set_data(outBuf, GST_BUFFER_DATA(outBuf),
            Buffer_getNumBytesUsed(hDstBuf));
        gst_buffer_set_caps(outBuf, GST_PAD_CAPS(videnc1->srcpad));
        
        if (videnc1->genTimeStamps) {
            gst_buffer_copy_metadata(outBuf, metaData, 
                GST_BUFFER_COPY_TIMESTAMPS);
            /* If we do not have valid time stamp then create one */
            if (!GST_CLOCK_TIME_IS_VALID(GST_BUFFER_TIMESTAMP(outBuf))) {
                GST_BUFFER_TIMESTAMP(outBuf) = totalDuration;
                GST_BUFFER_DURATION(outBuf)  = frameDuration;
                totalDuration += frameDuration;
            }
        }
        
        /* Push the transport buffer to the source pad */
        GST_LOG("pushing buffer to source pad with timestamp : %"
                GST_TIME_FORMAT ", duration: %" GST_TIME_FORMAT,
                GST_TIME_ARGS (GST_BUFFER_TIMESTAMP(outBuf)),
                GST_TIME_ARGS (GST_BUFFER_DURATION(outBuf)));

        if (gst_pad_push(videnc1->srcpad, outBuf) != GST_FLOW_OK) {
            GST_WARNING("push to source pad failed\n");
        }
    }

thread_failure:
    gst_tithread_set_status(videnc1, TIThread_DECODE_ABORTED);
    threadRet = GstTIThreadFailure;

thread_exit: 
    if (hCcvBuf) {
        Buffer_delete(hCcvBuf);
    }

    if (hCcv) {
        Ccv_delete(hCcv);
    }

    if (tmpBuf) {
        Buffer_delete(tmpBuf);
    }

    if (hInBuf) {
        Buffer_delete(hInBuf);
    }

    if (gstBuf) {
        Buffer_delete(gstBuf);
    }

    gst_buffer_unref(metaData);

    /* Stop codec engine */
    if (gst_tividenc1_codec_stop(videnc1) < 0) {
        GST_ERROR("failed to stop codec\n");
    }

    gst_object_unref(videnc1);

    GST_LOG("exit video encode_thread (%d)\n", (int)threadRet);
    return threadRet;
}

/******************************************************************************
 * gst_tividenc1_wait_on_fifo_threashold
 *    Wait for the queue thread to consume buffers from the fifo until
 *    there are only "waitQueueSize" items remaining.
 ******************************************************************************/
static void gst_tividenc1_wait_on_fifo_threashold(GstTIVidenc1 *videnc1,
                Int32 waitQueueSize)
{
    videnc1->waitQueueSize = waitQueueSize;
    Rendezvous_meet(videnc1->waitOnFifoThreashold);
}

/******************************************************************************
 * gst_tividenc1_frame_duration
 *    Return the duration of a single frame in nanoseconds.
 ******************************************************************************/
static GstClockTime gst_tividenc1_frame_duration(GstTIVidenc1 *videnc1)
{
    /* Default to 29.97 if the frame rate was not specified */
    if (videnc1->framerateNum == 0 && videnc1->framerateDen == 0) {
        GST_WARNING("framerate not specified; using 29.97fps");
        videnc1->framerateNum = 30000;
        videnc1->framerateDen = 1001;
    }

    return (GstClockTime)
        ((1 / ((gdouble)videnc1->framerateNum/(gdouble)videnc1->framerateDen))
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
