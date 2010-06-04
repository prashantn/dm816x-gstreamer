/*
 * gsttiimgenc.c
 *
 * This file defines the "TIImgenc" element, which encodes a JPEG image
 *
 * Example usage:
 *     gst-launch videotestsrc num-buffers=1 ! 'video/x-raw-yuv, format=(fourcc)I420, width=720, height=480' ! TIImgenc filesink location="<output file>"
 *
 * Notes:
 *
 * Original Author:
 *     Chase Maupin, Texas Instruments, Inc.
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

#include <stdio.h>
#include <string.h>
#include <gst/gst.h>
#include <gst/video/video.h>
#include <ctype.h>

#include <ti/sdo/dmai/Dmai.h>
#include <ti/sdo/dmai/Cpu.h>
#include <ti/sdo/dmai/Buffer.h>
#include <ti/sdo/dmai/BufferGfx.h>
#include <ti/sdo/dmai/BufTab.h>
#include <ti/sdo/dmai/ce/Ienc.h>

#include "gsttiimgenc.h"
#include "gsttidmaibuffertransport.h"
#include "gstticodecs.h"
#include "gsttithreadprops.h"
#include "gstticommonutils.h"

/* Declare variable used to categorize GST_LOG output */
GST_DEBUG_CATEGORY_STATIC (gst_tiimgenc_debug);
#define GST_CAT_DEFAULT gst_tiimgenc_debug

/* Element property identifiers */
enum
{
  PROP_0,
  PROP_ENGINE_NAME,     /* engineName     (string)  */
  PROP_CODEC_NAME,      /* codecName      (string)  */
  PROP_NUM_OUTPUT_BUFS, /* numOutputBufs  (int)     */
  PROP_FRAMERATE,       /* frameRate      (int)     */
  PROP_RESOLUTION,      /* resolution     (string)  */
  PROP_QVALUE,          /* qValue         (int)     */
  PROP_ICOLORSPACE,     /* iColorSpace    (string)  */
  PROP_OCOLORSPACE,     /* oColorSpace    (string)  */
  PROP_DISPLAY_BUFFER,  /* displayBuffer  (boolean) */
  PROP_GEN_TIMESTAMPS   /* genTimeStamps  (boolean) */
};

/* Codec Attributes for conversion function */
enum
{
    VAR_ICOLORSPACE,
    VAR_OCOLORSPACE
};

/* Define sink (input) pad capabilities.  Supported types are:
 *   - UYVY
 *   - 420P
 *   - 422P
 */
static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE(
    "sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS
    (GST_VIDEO_CAPS_YUV("UYVY")";"
     GST_VIDEO_CAPS_YUV("I420")";"
     GST_VIDEO_CAPS_YUV("Y42B")
    )
);

/* NOTE: There is some issue with the static caps on the output pad.
 *       If they are not defined to any and there are capabilities
 *       on the input buffer then the following error occurs:
 *       erroneous pipeline: could not link tiimgenc0 to filesink0
 *
 *       This needs to be fixed.
 *
 *       The original static caps were defined as:
 *
        static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE(
            "encimage",
            GST_PAD_SRC,
            GST_PAD_ALWAYS,
            GST_STATIC_CAPS
            ("video/x-jpeg, "
                "width=(int)[ 1, MAX ], "
                "height=(int)[ 1, MAX ], "
                "framerate=(fraction)[ 0, MAX ]"
            )
        );
 */
/* Define source (output) pad capabilities */
static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE(
    "src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY
);

/* Constants */
#define gst_tiimgenc_CODEC_FREE 0x2

/* Declare a global pointer to our element base class */
static GstElementClass *parent_class = NULL;

/* Static Function Declarations */
static void
 gst_tiimgenc_base_init(gpointer g_class);
static void
 gst_tiimgenc_class_init(GstTIImgencClass *g_class);
static void
 gst_tiimgenc_init(GstTIImgenc *object, GstTIImgencClass *g_class);
static void
 gst_tiimgenc_set_property (GObject *object, guint prop_id,
     const GValue *value, GParamSpec *pspec);
static void
 gst_tiimgenc_get_property (GObject *object, guint prop_id, GValue *value,
     GParamSpec *pspec);
static gboolean
 gst_tiimgenc_set_sink_caps(GstPad *pad, GstCaps *caps);
static gboolean
 gst_tiimgenc_set_source_caps(GstTIImgenc *imgenc, Buffer_Handle hBuf);
static gboolean
 gst_tiimgenc_sink_event(GstPad *pad, GstEvent *event);
static GstFlowReturn
 gst_tiimgenc_chain(GstPad *pad, GstBuffer *buf);
static gboolean
 gst_tiimgenc_init_image(GstTIImgenc *imgenc);
static gboolean
 gst_tiimgenc_exit_image(GstTIImgenc *imgenc);
static GstStateChangeReturn
 gst_tiimgenc_change_state(GstElement *element, GstStateChange transition);
static void*
 gst_tiimgenc_encode_thread(void *arg);
static void
 gst_tiimgenc_drain_pipeline(GstTIImgenc *imgenc);
static GstClockTime
 gst_tiimgenc_frame_duration(GstTIImgenc *imgenc);
static int 
 gst_tiimgenc_convert_fourcc(guint32 fourcc);
static int 
 gst_tiimgenc_convert_attrs(int attr, GstTIImgenc *imgenc);
static char *
 gst_tiimgenc_codec_color_space_to_str(int cspace);
static int 
 gst_tiimgenc_codec_color_space_to_fourcc(int cspace);
static int 
 gst_tiimgenc_convert_color_space(int cspace);
static gboolean 
    gst_tiimgenc_codec_start (GstTIImgenc  *imgenc);
static gboolean 
    gst_tiimgenc_codec_stop (GstTIImgenc  *imgenc);
static void 
    gst_tiimgenc_string_cap(gchar *str);
static void 
    gst_tiimgenc_init_env(GstTIImgenc *imgenc);

/******************************************************************************
 * gst_tiimgenc_class_init_trampoline
 *    Boiler-plate function auto-generated by "make_element" script.
 ******************************************************************************/
static void gst_tiimgenc_class_init_trampoline(gpointer g_class,
                gpointer data)
{
    GST_LOG("Begin\n");
    parent_class = (GstElementClass*) g_type_class_peek_parent(g_class);
    gst_tiimgenc_class_init((GstTIImgencClass*)g_class);
    GST_LOG("Finish\n");
}


/******************************************************************************
 * gst_tiimgenc_get_type
 *    Boiler-plate function auto-generated by "make_element" script.
 *    Defines function pointers for initialization routines for this element.
 ******************************************************************************/
GType gst_tiimgenc_get_type(void)
{
    static GType object_type = 0;

    if (G_UNLIKELY(object_type == 0)) {
        static const GTypeInfo object_info = {
            sizeof(GstTIImgencClass),
            gst_tiimgenc_base_init,
            NULL,
            gst_tiimgenc_class_init_trampoline,
            NULL,
            NULL,
            sizeof(GstTIImgenc),
            0,
            (GInstanceInitFunc) gst_tiimgenc_init
        };

        object_type = g_type_register_static((gst_element_get_type()),
                          "GstTIImgenc", &object_info, (GTypeFlags)0);

        /* Initialize GST_LOG for this object */
        GST_DEBUG_CATEGORY_INIT(gst_tiimgenc_debug, "TIImgenc", 0,
            "TI xDM 0.9 Image Encoder");

        GST_LOG("initialized get_type\n");
    }

    return object_type;
};


/******************************************************************************
 * gst_tiimgenc_base_init
 *    Boiler-plate function auto-generated by "make_element" script.
 *    Initializes element base class.
 ******************************************************************************/
static void gst_tiimgenc_base_init(gpointer gclass)
{
    static GstElementDetails element_details = {
        "TI xDM 0.9 Image Encoder",
        "Codec/Encoder/Image",
        "Encodes an image using an xDM 0.9-based codec",
        "Chase Maupin; Texas Instruments, Inc."
    };
    GST_LOG("Begin\n");

    GstElementClass *element_class = GST_ELEMENT_CLASS(gclass);

    gst_element_class_add_pad_template(element_class,
        gst_static_pad_template_get (&src_factory));
    gst_element_class_add_pad_template(element_class,
        gst_static_pad_template_get (&sink_factory));
    gst_element_class_set_details(element_class, &element_details);
    GST_LOG("Finish\n");
}


/******************************************************************************
 * gst_tiimgenc_class_init
 *    Boiler-plate function auto-generated by "make_element" script.
 *    Initializes the TIImgenc class.
 ******************************************************************************/
static void gst_tiimgenc_class_init(GstTIImgencClass *klass)
{
    GObjectClass    *gobject_class;
    GstElementClass *gstelement_class;

    gobject_class    = (GObjectClass*)    klass;
    gstelement_class = (GstElementClass*) klass;

    GST_LOG("Begin\n");

    gobject_class->set_property = gst_tiimgenc_set_property;
    gobject_class->get_property = gst_tiimgenc_get_property;

    gstelement_class->change_state = gst_tiimgenc_change_state;

    g_object_class_install_property(gobject_class, PROP_ENGINE_NAME,
        g_param_spec_string("engineName", "Engine Name",
            "Engine name used by Codec Engine", "unspecified",
            G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_CODEC_NAME,
        g_param_spec_string("codecName", "Codec Name", "Name of image codec",
            "unspecified", G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_QVALUE,
        g_param_spec_int("qValue",
            "qValue for encoder",
            "Q compression factor, from 1 (lowest quality)\n"
            "to 97 (highest quality). [default: 75]\n",
            1, 97, 75, G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_ICOLORSPACE,
        g_param_spec_string("iColorSpace", "Input Color Space",
            "Colorspace of the input image\n"
            "\tYUV422P, YUV420P, UYVY" ,
            "UYVY", G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_OCOLORSPACE,
        g_param_spec_string("oColorSpace", "Output Color Space",
            "Colorspace of the output image\n"
            "\tYUV422P, YUV420P, UYVY",
            "YUV422P", G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class, PROP_RESOLUTION,
        g_param_spec_string("resolution", "Image Resolution",
            "The resolution of the input image ('width'x'height')\n",
            "720x480", G_PARAM_WRITABLE));

    /* We allow more that one output buffer because this is the buffer that
     * is sent to the downstream element.  It may be that we need to have
     * more than 1 buffer for the mjpeg case if the downstream element
     * doesn't give the buffer back in time for the codec to use.
     */
    g_object_class_install_property(gobject_class, PROP_NUM_OUTPUT_BUFS,
        g_param_spec_int("numOutputBufs",
            "Number of Ouput Buffers",
            "Number of output buffers to allocate for codec",
            1, G_MAXINT32, 1, G_PARAM_WRITABLE));

    /* For MJPEG we will communicate a frame rate to the down stream
     * element.  This can be ignored normally.
     */
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

    GST_LOG("Finish\n");
}

/******************************************************************************
 * gst_tiimgenc_init_env
 *  Initialize element property default by reading environment variables.
 *****************************************************************************/
static void gst_tiimgenc_init_env(GstTIImgenc *imgenc)
{
    GST_LOG("gst_tiimgenc_init_env - begin");

    if (gst_ti_env_is_defined("GST_TI_TIImgenc_engineName")) {
        imgenc->engineName = gst_ti_env_get_string("GST_TI_TIImgenc_engineName");
        GST_LOG("Setting engineName=%s\n", imgenc->engineName);
    }

    if (gst_ti_env_is_defined("GST_TI_TIImgenc_codecName")) {
        imgenc->codecName = gst_ti_env_get_string("GST_TI_TIImgenc_codecName");
        GST_LOG("Setting codecName=%s\n", imgenc->codecName);
    }
    
    if (gst_ti_env_is_defined("GST_TI_TIImgenc_numOutputBufs")) {
        imgenc->numOutputBufs = 
                            gst_ti_env_get_int("GST_TI_TIImgenc_numOutputBufs");
        GST_LOG("Setting numOutputBufs=%ld\n", imgenc->numOutputBufs);
    }

    if (gst_ti_env_is_defined("GST_TI_TIImgenc_displayBuffer")) {
        imgenc->displayBuffer = 
                gst_ti_env_get_boolean("GST_TI_TIImgenc_displayBuffer");
        GST_LOG("Setting displayBuffer=%s\n",
                 imgenc->displayBuffer  ? "TRUE" : "FALSE");
    }
 
    if (gst_ti_env_is_defined("GST_TI_TIImgenc_genTimeStamps")) {
        imgenc->genTimeStamps = 
                gst_ti_env_get_boolean("GST_TI_TIImgenc_genTimeStamps");
        GST_LOG("Setting genTimeStamps =%s\n", 
                    imgenc->genTimeStamps ? "TRUE" : "FALSE");
    }
    
    if (gst_ti_env_is_defined("GST_TI_TIImgenc_framerate")) {
        imgenc->framerateNum = gst_ti_env_get_int("GST_TI_TIImgenc_framerate");
        imgenc->framerateDen = 1;
        
        /* If 30fps was specified, use 29.97 */        
        if (imgenc->framerateNum == 30) {
            imgenc->framerateNum = 30000;
            imgenc->framerateDen = 1001;
        }
    }

    if (gst_ti_env_is_defined("GST_TI_TIImgenc_qValue")) {
        imgenc->qValue =  gst_ti_env_get_int("GST_TI_TIImgenc_qValue");
        GST_LOG("Setting qValue=%d\n", imgenc->qValue);
    }

    if (gst_ti_env_is_defined("GST_TI_TIImgenc_iColorSpace")) {
        imgenc->iColor = gst_ti_env_get_string("GST_TI_TIImgenc_iColorSpace") ;
        gst_tiimgenc_string_cap(imgenc->iColor);
        GST_LOG("Setting engineName=%s\n", imgenc->iColor);
    }

    if (gst_ti_env_is_defined("GST_TI_TIImgenc_oColorSpace")) {
        imgenc->oColor = gst_ti_env_get_string("GST_TI_TIImgenc_oColorSpace");
        gst_tiimgenc_string_cap(imgenc->oColor);
        GST_LOG("Setting engineName=%s\n", imgenc->oColor);
    }

    if (gst_ti_env_is_defined("GST_TI_TIImgenc_resolution")) {
        sscanf(gst_ti_env_get_string("GST_TI_TIImgenc_resolution"), "%dx%d", 
                &imgenc->width,&imgenc->height);
        GST_LOG("Setting resolution=%dx%d\n", imgenc->width, imgenc->height);
    }
    
    GST_LOG("gst_tiimgenc_init_env - end");
}


/******************************************************************************
 * gst_tiimgenc_init
 *    Initializes a new element instance, instantiates pads and sets the pad
 *    callback functions.
 ******************************************************************************/
static void gst_tiimgenc_init(GstTIImgenc *imgenc, GstTIImgencClass *gclass)
{
    GST_LOG("Begin\n");

    /* Instantiate encoded image sink pad.
     *
     * Fixate on our static template caps instead of writing a getcaps
     * function, which is overkill for this element.
     */
    imgenc->sinkpad =
        gst_pad_new_from_static_template(&sink_factory, "sink");
    gst_pad_set_setcaps_function(
        imgenc->sinkpad, GST_DEBUG_FUNCPTR(gst_tiimgenc_set_sink_caps));
    gst_pad_set_event_function(
        imgenc->sinkpad, GST_DEBUG_FUNCPTR(gst_tiimgenc_sink_event));
    gst_pad_set_chain_function(
        imgenc->sinkpad, GST_DEBUG_FUNCPTR(gst_tiimgenc_chain));
    gst_pad_fixate_caps(imgenc->sinkpad,
        gst_caps_make_writable(
            gst_caps_copy(gst_pad_get_pad_template_caps(imgenc->sinkpad))));

    /* Instantiate deceoded image source pad.
     *
     * Fixate on our static template caps instead of writing a getcaps
     * function, which is overkill for this element.
     */
    imgenc->srcpad =
        gst_pad_new_from_static_template(&src_factory, "src");
    gst_pad_fixate_caps(imgenc->srcpad,
        gst_caps_make_writable(
            gst_caps_copy(gst_pad_get_pad_template_caps(imgenc->srcpad))));

    /* Add pads to TIImgenc element */
    gst_element_add_pad(GST_ELEMENT(imgenc), imgenc->sinkpad);
    gst_element_add_pad(GST_ELEMENT(imgenc), imgenc->srcpad);

    /* Initialize TIImgenc state */
    imgenc->engineName         = NULL;
    imgenc->codecName          = NULL;
    imgenc->displayBuffer      = FALSE;
    imgenc->genTimeStamps      = FALSE;
    imgenc->iColor             = NULL;
    imgenc->oColor             = NULL;
    imgenc->qValue             = 0;
    imgenc->width              = 0;
    imgenc->height             = 0;

    imgenc->hEngine            = NULL;
    imgenc->hIe                = NULL;
    imgenc->drainingEOS        = FALSE;
    imgenc->threadStatus       = 0UL;
    imgenc->capsSet            = FALSE;

    imgenc->waitOnEncodeThread = NULL;
    imgenc->waitOnEncodeDrain  = NULL;
    imgenc->waitOnBufTab       = NULL;

    imgenc->framerateNum       = 0;
    imgenc->framerateDen       = 0;

    imgenc->numOutputBufs      = 0UL;
    imgenc->hOutBufTab         = NULL;
    imgenc->circBuf            = NULL;

    gst_tiimgenc_init_env(imgenc);

    GST_LOG("Finish\n");
}

/*******************************************************************************
 * gst_tiimgenc_string_cap
 *    This function will capitalize the given string.  This makes it easier
 *    to compare the strings later.
*******************************************************************************/
static void gst_tiimgenc_string_cap(gchar *str) {
    int i;
    int len = strlen(str);

    GST_LOG("Begin\n");
    for (i=0; i<len; i++) {
        str[i] = (char)toupper(str[i]);
    }
    GST_LOG("Finish\n");
    return;
}

/******************************************************************************
 * gst_tiimgenc_set_property
 *     Set element properties when requested.
 ******************************************************************************/
static void gst_tiimgenc_set_property(GObject *object, guint prop_id,
                const GValue *value, GParamSpec *pspec)
{
    GstTIImgenc *imgenc = GST_TIIMGENC(object);

    GST_LOG("Begin\n");

    switch (prop_id) {
        case PROP_ENGINE_NAME:
            if (imgenc->engineName) {
                g_free((gpointer)imgenc->engineName);
            }
            imgenc->engineName =
                (gchar*)g_malloc(strlen(g_value_get_string(value)) + 1);
            strcpy((gchar *)imgenc->engineName, g_value_get_string(value));
            GST_LOG("setting \"engineName\" to \"%s\"\n", imgenc->engineName);
            break;
        case PROP_CODEC_NAME:
            if (imgenc->codecName) {
                g_free((gpointer)imgenc->codecName);
            }
            imgenc->codecName =
                (gchar*)g_malloc(strlen(g_value_get_string(value)) + 1);
            strcpy((gchar*)imgenc->codecName, g_value_get_string(value));
            GST_LOG("setting \"codecName\" to \"%s\"\n", imgenc->codecName);
            break;
        case PROP_RESOLUTION:
            sscanf(g_value_get_string(value), "%dx%d", &imgenc->width,
                    &imgenc->height);
            GST_LOG("setting \"resolution\" to \"%dx%d\"\n",
                imgenc->width, imgenc->height);
            break;
        case PROP_QVALUE:
            imgenc->qValue = g_value_get_int(value);
            GST_LOG("setting \"qValue\" to \"%d\"\n",
                imgenc->qValue);
            break;
        case PROP_OCOLORSPACE:
            imgenc->oColor = g_strdup(g_value_get_string(value));
            gst_tiimgenc_string_cap(imgenc->oColor);
            GST_LOG("setting \"oColor\" to \"%s\"\n",
                imgenc->oColor);
            break;
        case PROP_ICOLORSPACE:
            imgenc->iColor = g_strdup(g_value_get_string(value));
            gst_tiimgenc_string_cap(imgenc->iColor);
            GST_LOG("setting \"iColor\" to \"%s\"\n",
                imgenc->iColor);
            break;
        case PROP_NUM_OUTPUT_BUFS:
            imgenc->numOutputBufs = g_value_get_int(value);
            GST_LOG("setting \"numOutputBufs\" to \"%ld\"\n",
                imgenc->numOutputBufs);
            break;
        case PROP_FRAMERATE:
        {
            imgenc->framerateNum = g_value_get_int(value);
            imgenc->framerateDen = 1;

            /* If 30fps was specified, use 29.97 */
            if (imgenc->framerateNum == 30) {
                imgenc->framerateNum = 30000;
                imgenc->framerateDen = 1001;
            }

            GST_LOG("setting \"frameRate\" to \"%2.2lf\"\n",
                (gdouble)imgenc->framerateNum /
                (gdouble)imgenc->framerateDen);
            break;
        }
        case PROP_DISPLAY_BUFFER:
            imgenc->displayBuffer = g_value_get_boolean(value);
            GST_LOG("setting \"displayBuffer\" to \"%s\"\n",
                imgenc->displayBuffer ? "TRUE" : "FALSE");
            break;
        case PROP_GEN_TIMESTAMPS:
            imgenc->genTimeStamps = g_value_get_boolean(value);
            GST_LOG("setting \"genTimeStamps\" to \"%s\"\n",
                imgenc->genTimeStamps ? "TRUE" : "FALSE");
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }

    GST_LOG("Finish\n");
}

/******************************************************************************
 * gst_tiimgenc_get_property
 *     Return values for requested element property.
 ******************************************************************************/
static void gst_tiimgenc_get_property(GObject *object, guint prop_id,
                GValue *value, GParamSpec *pspec)
{
    GstTIImgenc *imgenc = GST_TIIMGENC(object);

    GST_LOG("Begin\n");

    switch (prop_id) {
        case PROP_ENGINE_NAME:
            g_value_set_string(value, imgenc->engineName);
            break;
        case PROP_CODEC_NAME:
            g_value_set_string(value, imgenc->codecName);
            break;
        case PROP_QVALUE:
            g_value_set_int(value, imgenc->qValue);
            break;
        case PROP_OCOLORSPACE:
            g_value_set_string(value, imgenc->oColor);
            break;
        case PROP_ICOLORSPACE:
            g_value_set_string(value, imgenc->iColor);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }

    GST_LOG("Finish\n");
}

/*******************************************************************************
 * gst_tiimgenc_set_sink_caps_helper
 *     This function will look at the capabilities given and set the values
 *     for the encoder if they were not specified on the command line.
 *     It returns TRUE if everything passes and FALSE if there is no
 *     capability in the buffer and the value was not specified on the
 *     command line.
 ******************************************************************************/
static gboolean gst_tiimgenc_set_sink_caps_helper(GstPad *pad, GstCaps *caps)
{
    GstTIImgenc *imgenc;
    GstStructure *capStruct;
    Cpu_Device    device;
    const gchar  *mime;
    GstTICodec   *codec = NULL;

    GST_LOG("Begin\n");

    imgenc   = GST_TIIMGENC(gst_pad_get_parent(pad));

    /* Determine which device the application is running on */
    if (Cpu_getDevice(NULL, &device) < 0) {
        GST_ELEMENT_ERROR(imgenc, RESOURCE, FAILED,
        ("Failed to determine target board\n"), (NULL));
        return FALSE;
    }

    /* Set the default values */
    imgenc->params = Ienc_Params_DEFAULT;
    imgenc->dynParams = Ienc_DynamicParams_DEFAULT;

    /* Set codec to JPEG Encoder */
    codec = gst_ticodec_get_codec("JPEG Image Encoder");

    /* Report if the required codec was not found */
    if (!codec) {
        GST_ELEMENT_ERROR(imgenc, STREAM, CODEC_NOT_FOUND,
        ("unable to find codec needed for stream"), (NULL));
        return FALSE;
    }

    /* Configure the element to use the detected engine name and codec, unless
     * they have been using the set_property function.
     */
    if (!imgenc->engineName) {
        imgenc->engineName = codec->CE_EngineName;
    }
    if (!imgenc->codecName) {
        imgenc->codecName = codec->CE_CodecName;
    }

    if (!caps) {
        GST_INFO("No caps on input.  Using command line values");
        imgenc->capsSet = TRUE;
        return TRUE;
    }

    capStruct = gst_caps_get_structure(caps, 0);
    mime      = gst_structure_get_name(capStruct);

    GST_INFO("requested sink caps:  %s", gst_caps_to_string(caps));

    /* Generic Video Properties */
    if (!strncmp(mime, "video/", 6)) {
        gint  framerateNum;
        gint  framerateDen;
        gint  width;
        gint  height;
   
        if (!imgenc->framerateNum) {        
            if (gst_structure_get_fraction(capStruct, "framerate", 
                                &framerateNum, &framerateDen)) {
                imgenc->framerateNum = framerateNum;
                imgenc->framerateDen = framerateDen;
            }
        }

        /* Get the width and height of the frame if available */
        if (!imgenc->width)
            if (gst_structure_get_int(capStruct, "width", &width)) 
                imgenc->params.maxWidth = 
                imgenc->dynParams.inputWidth = width;
        if (!imgenc->height)
            if (gst_structure_get_int(capStruct, "height", &height))
                imgenc->params.maxHeight = 
                imgenc->dynParams.inputHeight = height;
    }

    /* Get the Chroma Format */
    if (!strcmp(mime, "video/x-raw-yuv")) {
        guint32      format;

        /* Retreive input Color Format from the buffer properties unless
         * a value was set on the command line.
         */
        if (!imgenc->iColor) {
            if (gst_structure_get_fourcc(capStruct, "format", &format)) {
                imgenc->dynParams.inputChromaFormat =
                    gst_tiimgenc_convert_fourcc(format);
            }
            else {
                GST_ELEMENT_ERROR(imgenc, RESOURCE, FAILED,
                ("Input chroma format not specified on either the command line with iColorFormat or in the buffer caps"), (NULL));
                return FALSE;
            }
        }

        /* If an output color format is not specified use the input color
         * format.
         */
        if (!imgenc->oColor) {
            imgenc->params.forceChromaFormat = 
                        imgenc->dynParams.inputChromaFormat;
        }
    } else {
        /* Mime type not supported */
        GST_ELEMENT_ERROR(imgenc, STREAM, NOT_IMPLEMENTED,
        ("stream type not supported"), (NULL));
        return FALSE;
    }

    /* Shut-down any running image encoder */
    if (!gst_tiimgenc_exit_image(imgenc)) {
        GST_ELEMENT_ERROR(imgenc, RESOURCE, FAILED,
        ("unable to shut-down running image encoder"), (NULL));
        return FALSE;
    }

    /* Flag that we have run this code at least once */
    imgenc->capsSet = TRUE;

    GST_LOG("Finish\n");
    return TRUE;
}

/******************************************************************************
 * gst_tiimgenc_set_sink_caps
 *     Negotiate our sink pad capabilities.
 ******************************************************************************/
static gboolean gst_tiimgenc_set_sink_caps(GstPad *pad, GstCaps *caps)
{
    GstTIImgenc *imgenc;
    imgenc   = GST_TIIMGENC(gst_pad_get_parent(pad));

    GST_LOG("Begin\n");

    /* If this call fails then unref the gobject */
    if (!gst_tiimgenc_set_sink_caps_helper(pad, caps)) {
        GST_ELEMENT_ERROR(imgenc, STREAM, NOT_IMPLEMENTED,
        ("stream type not supported"), (NULL));
        gst_object_unref(imgenc);
        return FALSE;
    }

    GST_LOG("sink caps negotiation successful\n");
    GST_LOG("Finish\n");
    return TRUE;
}


/******************************************************************************
 * gst_tiimgenc_set_source_caps
 *     Negotiate our source pad capabilities.
 ******************************************************************************/
static gboolean gst_tiimgenc_set_source_caps(
                    GstTIImgenc *imgenc, Buffer_Handle hBuf)
{
    BufferGfx_Dimensions  dim;
    GstCaps              *caps;
    gboolean              ret;
    GstPad               *pad;
    guint32              format;

    GST_LOG("Begin\n");
    pad = imgenc->srcpad;

    /* Create a UYVY caps object using the dimensions from the given buffer */
    BufferGfx_getDimensions(hBuf, &dim);

    format = gst_tiimgenc_codec_color_space_to_fourcc(imgenc->params.forceChromaFormat);

    if (format == -1) {
        GST_ELEMENT_ERROR(imgenc, RESOURCE, FAILED,
        ("Could not convert codec color space to fourcc"), (NULL));
        return FALSE;
    }
    caps =
        gst_caps_new_simple("video/x-jpeg",
            "framerate", GST_TYPE_FRACTION, imgenc->framerateNum,
                                            imgenc->framerateDen,
            "width",     G_TYPE_INT,        dim.width,
            "height",    G_TYPE_INT,        dim.height,
            NULL);

    /* Set the source pad caps */
    GST_LOG("setting source caps to UYVY:  %s", gst_caps_to_string(caps));
    ret = gst_pad_set_caps(pad, caps);
    gst_caps_unref(caps);

    GST_LOG("Finish\n");
    return ret;
}


/******************************************************************************
 * gst_tiimgenc_sink_event
 *     Perform event processing on the input stream.  At the moment, this
 *     function isn't needed as this element doesn't currently perform any
 *     specialized event processing.  We'll leave it in for now in case we need
 *     it later on.
 ******************************************************************************/
static gboolean gst_tiimgenc_sink_event(GstPad *pad, GstEvent *event)
{
    GstTIImgenc *imgenc;
    gboolean      ret;

    GST_LOG("Begin\n");

    imgenc = GST_TIIMGENC(GST_OBJECT_PARENT(pad));

    GST_DEBUG("pad \"%s\" received:  %s\n", GST_PAD_NAME(pad),
        GST_EVENT_TYPE_NAME(event));

    switch (GST_EVENT_TYPE(event)) {

        case GST_EVENT_NEWSEGMENT:
            /* maybe save and/or update the current segment (e.g. for output
             * clipping) or convert the event into one in a different format
             * (e.g. BYTES to TIME) or drop it and set a flag to send a
             * newsegment event in a different format later
             */
            ret = gst_pad_push_event(imgenc->srcpad, event);
            break;

        case GST_EVENT_EOS:
            /* end-of-stream: process any remaining encoded frame data */
            GST_LOG("no more input; draining remaining encoded image data\n");

            if (!imgenc->drainingEOS) {
               gst_tiimgenc_drain_pipeline(imgenc);
             }

            /* Propagate EOS to downstream elements */
            ret = gst_pad_push_event(imgenc->srcpad, event);
            break;

        case GST_EVENT_FLUSH_STOP:
            ret = gst_pad_push_event(imgenc->srcpad, event);
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

    GST_LOG("Finish\n");
    return ret;

}


/******************************************************************************
 * gst_tiimgenc_chain
 *    This is the main processing routine.  This function receives a buffer
 *    from the sink pad, processes it, and pushes the result to the source
 *    pad.
 ******************************************************************************/
static GstFlowReturn gst_tiimgenc_chain(GstPad * pad, GstBuffer * buf)
{
    GstTIImgenc   *imgenc = GST_TIIMGENC(GST_OBJECT_PARENT(pad));
    GstCaps       *caps   = GST_BUFFER_CAPS(buf);
    GstFlowReturn  flow   = GST_FLOW_OK;
    gboolean       checkResult;

    /* If the encode thread aborted, signal it to let it know it's ok to
     * shut down, and communicate the failure to the pipeline.
     */
    if (gst_tithread_check_status(imgenc, TIThread_CODEC_ABORTED,
            checkResult)) {
        flow = GST_FLOW_UNEXPECTED;
        goto exit;
    }

    /* If we have not negotiated the caps at least once then do so now */
    if (!imgenc->capsSet) {
        if (!gst_tiimgenc_set_sink_caps_helper(pad, caps)) {
            GST_ELEMENT_ERROR(imgenc, RESOURCE, FAILED,
            ("Could not set caps"), (NULL));
            flow = GST_FLOW_UNEXPECTED;
            goto exit;
        }
    }

    /* If our engine handle is currently NULL, then either this is our first
     * buffer or the upstream element has re-negotiated our capabilities which
     * resulted in our engine being closed.  In either case, we need to
     * initialize (or re-initialize) our image encoder to handle the new
     * stream.
     */
    if (imgenc->hEngine == NULL) {
        imgenc->upstreamBufSize = GST_BUFFER_SIZE(buf);
        if (!gst_tiimgenc_init_image(imgenc)) {
            GST_ELEMENT_ERROR(imgenc, RESOURCE, FAILED,
            ("unable to initialize image\n"), (NULL));
            flow = GST_FLOW_UNEXPECTED;
            goto exit;
        }

        GST_TICIRCBUFFER_TIMESTAMP(imgenc->circBuf) =
            GST_CLOCK_TIME_IS_VALID(GST_BUFFER_TIMESTAMP(buf)) ?
            GST_BUFFER_TIMESTAMP(buf) : 0ULL;
    }

    /* Queue up the encoded data stream into a circular buffer */
    if (!gst_ticircbuffer_queue_data(imgenc->circBuf, buf)) {
        GST_ELEMENT_ERROR(imgenc, RESOURCE, WRITE,
        ("Failed to queue input buffer into circular buffer\n"), (NULL));
        flow = GST_FLOW_UNEXPECTED;
        goto exit;
    }

exit:
    gst_buffer_unref(buf);
    return flow;
}

/*******************************************************************************
 * gst_tiimgenc_convert_fourcc
 *      This function will take in a fourcc value (as used in the format
 *      member of the input parameters) and convert it into the color
 *      format for the codec to use.
*******************************************************************************/
static int gst_tiimgenc_convert_fourcc(guint32 fourcc) {
    gchar format[4];

    GST_LOG("Begin\n");
    sprintf(format, "%"GST_FOURCC_FORMAT, GST_FOURCC_ARGS(fourcc));
    GST_DEBUG("format is %s\n", format);

    if (!strcmp(format, "UYVY")) {
        GST_LOG("Finish\n");
        return gst_tiimgenc_convert_color_space(ColorSpace_UYVY);
    } else if (!strcmp(format, "Y42B")) {
        GST_LOG("Finish\n");
        return gst_tiimgenc_convert_color_space(ColorSpace_YUV422P);
    } else if (!strcmp(format, "I420")) {
        GST_LOG("Finish\n");
        return gst_tiimgenc_convert_color_space(ColorSpace_YUV420P);
    } else {
        GST_LOG("Finish\n");
        return -1;
    }
}

/*******************************************************************************
 * gst_tiimgenc_convert_attrs
 *    This function will convert the human readable strings for the
 *    attributes into the proper integer values for the enumerations.
*******************************************************************************/
static int gst_tiimgenc_convert_attrs(int attr, GstTIImgenc *imgenc)
{
  int ret;

  GST_DEBUG("Begin\n");
  switch (attr) {
    case VAR_ICOLORSPACE:
      if (!strcmp(imgenc->iColor, "UYVY"))
          return ColorSpace_UYVY;
      else if (!strcmp(imgenc->iColor, "YUV420P"))
          return ColorSpace_YUV420P;
      else if (!strcmp(imgenc->iColor, "YUV422P"))
          return ColorSpace_YUV422P;
      else {
        GST_ERROR("Invalid iColorSpace entered (%s).  Please choose from:\n"
                "\tUYVY, YUV420P, YUV422P\n", imgenc->iColor);
        return -1;
      }
    break;
    case VAR_OCOLORSPACE:
      if (!strcmp(imgenc->oColor, "UYVY"))
          return ColorSpace_UYVY;
      else if (!strcmp(imgenc->oColor, "YUV420P"))
          return ColorSpace_YUV420P;
      else if (!strcmp(imgenc->oColor, "YUV422P"))
          return ColorSpace_YUV422P;
      else {
        GST_ERROR("Invalid oColorSpace entered (%s).  Please choose from:\n"
                "\tUYVY, YUV420P, YUV422P\n", imgenc->oColor);
        return -1;
      }
    break;
    default:
      GST_ERROR("Unknown Attribute\n");
      ret=-1;
      break;
  }
  GST_DEBUG("Finish\n");
  return ret;
}

/*******************************************************************************
 * gst_tiimgenc_codec_color_space_to_str
 *      Converts the codec color space values to the corresponding
 *      human readable string value
*******************************************************************************/
static char *gst_tiimgenc_codec_color_space_to_str(int cspace) {
    GST_LOG("Begin");
    switch (cspace) {
        case XDM_YUV_422ILE:
            GST_LOG("Finish");
            return "UYVY";
            break;
        case XDM_YUV_420P:
            GST_LOG("Finish");
            return "YUV420P";
            break;
        case XDM_YUV_422P:
            GST_LOG("Finish");
            return "YUV422P";
            break;
        default:
            GST_ERROR("Unknown xDM color space");
            GST_LOG("Finish");
            return "Unknown";
    }
}

/*******************************************************************************
 * gst_tiimgenc_codec_color_space_to_fourcc
 *      Converts the codec color space values to the corresponding
 *      fourcc values
*******************************************************************************/
static int gst_tiimgenc_codec_color_space_to_fourcc(int cspace) {
    GST_LOG("Begin");
    switch (cspace) {
        case XDM_YUV_422ILE:
            GST_LOG("Finish");
            return GST_MAKE_FOURCC('U', 'Y', 'V', 'Y');
            break;
        case XDM_YUV_420P:
            GST_LOG("Finish");
            return GST_MAKE_FOURCC('I', '4', '2', '0');
            break;
        case XDM_YUV_422P:
            GST_LOG("Finish");
            return GST_MAKE_FOURCC('Y', '4', '2', 'B');
            break;
        default:
            GST_ERROR("Unknown xDM color space");
            GST_LOG("Finish");
            return -1;
            break;
    }
}

/*******************************************************************************
 * gst_tiimgenc_codec_color_space_to_dmai
 *      Convert the codec color space type to the corresponding color space
 *      used by DMAI.
*******************************************************************************/
static int gst_tiimgenc_codec_color_space_to_dmai(int cspace) {
    GST_LOG("Begin");
    switch (cspace) {
        case XDM_YUV_422ILE:
            GST_LOG("Finish");
            return ColorSpace_UYVY;
            break;
        case XDM_YUV_420P:
            GST_LOG("Finish");
            return ColorSpace_YUV420P;
            break;
        case XDM_YUV_422P:
            GST_LOG("Finish");
            return ColorSpace_YUV422P;
            break;
        default:
            GST_ERROR("Unsupported Color Space\n");
            GST_LOG("Finish");
            return -1;
    }
}

/*******************************************************************************
 * gst_tiimgenc_convert_color_space
 *      Convert the DMAI color space type to the corresponding color space
 *      used by the codec.
*******************************************************************************/
static int gst_tiimgenc_convert_color_space(int cspace) {
    GST_LOG("Begin");
    switch (cspace) {
        case ColorSpace_UYVY:
            GST_LOG("Finish");
            return XDM_YUV_422ILE;
            break;
        case ColorSpace_YUV420P:
            GST_LOG("Finish");
            return XDM_YUV_420P;
            break;
        case ColorSpace_YUV422P:
            GST_LOG("Finish");
            return XDM_YUV_422P;
            break;
        default:
            GST_ERROR("Unsupported Color Space\n");
            GST_LOG("Finish");
            return -1;
    }
}

/*******************************************************************************
 * gst_tiimgenc_set_codec_attrs
 *     Set the attributes for the encode codec.  The general order is to give
 *     preference to values passed in by the user on the command line,
 *     then check the stream for the information.  If the required information
 *     cannot be determined by either method then error out.
 ******************************************************************************/
static gboolean gst_tiimgenc_set_codec_attrs(GstTIImgenc *imgenc)
{ 
    char *toColor;
    char *tiColor;
    GST_LOG("Begin\n");

    /* Set ColorSpace */
    imgenc->dynParams.inputChromaFormat = imgenc->iColor == NULL ?
                    imgenc->dynParams.inputChromaFormat :
                    gst_tiimgenc_convert_color_space(gst_tiimgenc_convert_attrs(VAR_ICOLORSPACE, imgenc));
    /* Use the input color format if one was not specified on the
     * command line
     */
    imgenc->params.forceChromaFormat = imgenc->oColor == NULL ?
                    imgenc->dynParams.inputChromaFormat :
                    gst_tiimgenc_convert_color_space(gst_tiimgenc_convert_attrs(VAR_OCOLORSPACE, imgenc));

    /* Set Resolution 
     *
     * NOTE:  We assume that there is only 1 resolution.  i.e. there is
     *        no seperate value being used for input resolution and
     *        output resolution
     */
    imgenc->params.maxWidth = imgenc->dynParams.inputWidth = 
                    imgenc->width == 0 ?
                    imgenc->params.maxWidth :
                    imgenc->width;
    imgenc->params.maxHeight = imgenc->dynParams.inputHeight = 
                    imgenc->height == 0 ?
                    imgenc->params.maxHeight :
                    imgenc->height;
    /* Set captureWidth to 0 in order to use encoded image width */
//    imgenc->dynParams.captureWidth = 0;
    imgenc->dynParams.captureWidth = imgenc->params.maxWidth;

    /* Set qValue (default is 75) */
    imgenc->dynParams.qValue = imgenc->qValue == 0 ?
                    imgenc->dynParams.qValue :
                    imgenc->qValue;

    /* Check for valid values (NOTE: minimum width and height are 64) */
    if (imgenc->params.maxWidth < 64) {
        GST_ERROR("The resolution width (%ld) is too small.  Must be at least 64\n", imgenc->params.maxWidth);
        return FALSE;
    }
    if (imgenc->params.maxHeight < 64) {
        GST_ERROR("The resolution height (%ld) is too small.  Must be at least 64\n", imgenc->params.maxHeight);
        return FALSE;
    }
    if ((imgenc->dynParams.qValue < 0) || (imgenc->dynParams.qValue > 100)) {
        GST_ERROR("The qValue (%ld) is not withing the range of 0-100\n", imgenc->dynParams.qValue);
        return FALSE;
    }
    if (imgenc->params.forceChromaFormat == -1) {
        GST_ERROR("Output Color Format (%s) is not supported\n", imgenc->oColor);
        return FALSE;
    }
    if (imgenc->dynParams.inputChromaFormat == -1) {
        GST_ERROR("input Color Format (%s) is not supported\n", imgenc->iColor);
        return FALSE;
    }

    /* Make the color human readable */
    if (!imgenc->oColor)
        toColor = gst_tiimgenc_codec_color_space_to_str(imgenc->params.forceChromaFormat);
    else
        toColor = (char *)imgenc->oColor;
    
    if (!imgenc->iColor)
        tiColor = gst_tiimgenc_codec_color_space_to_str(imgenc->dynParams.inputChromaFormat);
    else
        tiColor = (char *)imgenc->iColor;
    
    GST_DEBUG("\nCodec Parameters:\n"
              "\tparams.maxWidth = %ld\n"
              "\tparams.maxHeight = %ld\n"
              "\tparams.forceChromaFormat = %ld (%s)\n"
              "\nDynamic Parameters:\n"
              "\tdynParams.inputWidth = %ld\n"
              "\tdynParams.inputHeight = %ld\n"
              "\tdynParams.inputChromaFormat = %ld (%s)\n"
              "\tdynParams.qValue = %ld\n",
              imgenc->params.maxWidth, imgenc->params.maxHeight,
              imgenc->params.forceChromaFormat, toColor,
              imgenc->dynParams.inputWidth, imgenc->dynParams.inputHeight,
              imgenc->dynParams.inputChromaFormat, tiColor,
              imgenc->dynParams.qValue);
    GST_LOG("Finish\n");
    return TRUE;
}

/******************************************************************************
 * gst_tiimgenc_init_image
 *     Initialize or re-initializes the image stream
 ******************************************************************************/
static gboolean gst_tiimgenc_init_image(GstTIImgenc *imgenc)
{
    Rendezvous_Attrs    rzvAttrs = Rendezvous_Attrs_DEFAULT;
    struct sched_param  schedParam;
    pthread_attr_t      attr;

    GST_LOG("Begin\n");

    /* If image has already been initialized, shut down previous encoder */
    if (imgenc->hEngine) {
        if (!gst_tiimgenc_exit_image(imgenc)) {
            GST_ELEMENT_ERROR(imgenc, RESOURCE, FAILED,
            ("failed to shut down existing image encoder\n"), (NULL));
            return FALSE;
        }
    }

    /* Make sure we know what codec we're using */
    if (!imgenc->engineName) {
        GST_ELEMENT_ERROR(imgenc, RESOURCE, FAILED,
        ("engine name not specified\n"), (NULL));
        return FALSE;
    }

    if (!imgenc->codecName) {
        GST_ELEMENT_ERROR(imgenc, RESOURCE, FAILED,
        ("codec name not specified\n"), (NULL));
        return FALSE;
    }

    /* Initialize thread status management */
    imgenc->threadStatus = 0UL;
    pthread_mutex_init(&imgenc->threadStatusMutex, NULL);

    /* Initialize rendezvous objects for making threads wait on conditions */
    imgenc->waitOnEncodeThread = Rendezvous_create(2, &rzvAttrs);
    imgenc->waitOnEncodeDrain  = Rendezvous_create(100, &rzvAttrs);
    imgenc->waitOnBufTab       = Rendezvous_create(100, &rzvAttrs);
    imgenc->drainingEOS        = FALSE;

    /* Initialize custom thread attributes */
    if (pthread_attr_init(&attr)) {
        GST_WARNING("failed to initialize thread attrs\n");
        gst_tiimgenc_exit_image(imgenc);
        return FALSE;
    }

    /* Force the thread to use the system scope */
    if (pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM)) {
        GST_WARNING("failed to set scope attribute\n");
        gst_tiimgenc_exit_image(imgenc);
        return FALSE;
    }

    /* Force the thread to use custom scheduling attributes */
    if (pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED)) {
        GST_WARNING("failed to set schedule inheritance attribute\n");
        gst_tiimgenc_exit_image(imgenc);
        return FALSE;
    }

    /* Set the thread to be fifo real time scheduled */
    if (pthread_attr_setschedpolicy(&attr, SCHED_FIFO)) {
        GST_WARNING("failed to set FIFO scheduling policy\n");
        gst_tiimgenc_exit_image(imgenc);
        return FALSE;
    }

    /* Set the display thread priority */
    schedParam.sched_priority = GstTIVideoThreadPriority;
    if (pthread_attr_setschedparam(&attr, &schedParam)) {
        GST_WARNING("failed to set scheduler parameters\n");
        return FALSE;
    }

    /* Create encoder thread */
    if (pthread_create(&imgenc->encodeThread, &attr,
            gst_tiimgenc_encode_thread, (void*)imgenc)) {
        GST_ELEMENT_ERROR(imgenc, RESOURCE, FAILED,
        ("failed to create encode thread\n"), (NULL));
        gst_tiimgenc_exit_image(imgenc);
        return FALSE;
    }
    gst_tithread_set_status(imgenc, TIThread_CODEC_CREATED);

    /* Destroy the custom thread attributes */
    if (pthread_attr_destroy(&attr)) {
        GST_WARNING("failed to destroy thread attrs\n");
        gst_tiimgenc_exit_image(imgenc);
        return FALSE;
    }

    /* Make sure circular buffer and display buffer handles are created by
     * decoder thread.
     */
    Rendezvous_meet(imgenc->waitOnEncodeThread);

    if (imgenc->circBuf == NULL || imgenc->hOutBufTab == NULL) {
        GST_ELEMENT_ERROR(imgenc, RESOURCE, FAILED, 
        ("encode thread failed to create circbuf or display buffer handles\n"),
        (NULL));
        return FALSE;
    }

    GST_LOG("Finish\n");
    return TRUE;
}


/******************************************************************************
 * gst_tiimgenc_exit_image
 *    Shut down any running image encoder, and reset the element state.
 ******************************************************************************/
static gboolean gst_tiimgenc_exit_image(GstTIImgenc *imgenc)
{
    void*    thread_ret;
    gboolean checkResult;

    GST_LOG("Begin\n");

    /* Drain the pipeline if it hasn't already been drained */
    if (!imgenc->drainingEOS) {
       gst_tiimgenc_drain_pipeline(imgenc);
     }

    /* Shut down the encode thread */
    if (gst_tithread_check_status(
            imgenc, TIThread_CODEC_CREATED, checkResult)) {
        GST_LOG("shutting down encode thread\n");

        Rendezvous_force(imgenc->waitOnEncodeThread);
        if (pthread_join(imgenc->encodeThread, &thread_ret) == 0) {
            if (thread_ret == GstTIThreadFailure) {
                GST_DEBUG("encode thread exited with an error condition\n");
            }
        }
    }

    /* Shut down remaining items */
    if (imgenc->waitOnEncodeDrain) {
        Rendezvous_delete(imgenc->waitOnEncodeDrain);
        imgenc->waitOnEncodeDrain = NULL;
    }

    if (imgenc->waitOnEncodeThread) {
        Rendezvous_delete(imgenc->waitOnEncodeThread);
        imgenc->waitOnEncodeThread = NULL;
    }

    if (imgenc->waitOnBufTab) {
        Rendezvous_delete(imgenc->waitOnBufTab);
        imgenc->waitOnBufTab = NULL;
    }

    /* Shut down thread status management */
    imgenc->threadStatus = 0UL;
    pthread_mutex_destroy(&imgenc->threadStatusMutex);

    GST_LOG("Finish\n");
    return TRUE;
}


/******************************************************************************
 * gst_tiimgenc_change_state
 *     Manage state changes for the image stream.  The gStreamer documentation
 *     states that state changes must be handled in this manner:
 *        1) Handle ramp-up states
 *        2) Pass state change to base class
 *        3) Handle ramp-down states
 ******************************************************************************/
static GstStateChangeReturn gst_tiimgenc_change_state(GstElement *element,
                                GstStateChange transition)
{
    GstStateChangeReturn  ret    = GST_STATE_CHANGE_SUCCESS;
    GstTIImgenc          *imgenc = GST_TIIMGENC(element);

    GST_LOG("Begin change_state (%d)\n", transition);

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
            GST_LOG("State changed from READY to NULL.  Shutting down any"
                    "running image encoders\n");
            /* Shut down any running image encoder */
            if (!gst_tiimgenc_exit_image(imgenc)) {
                return GST_STATE_CHANGE_FAILURE;
            }
            break;

        default:
            break;
    }

    GST_LOG("Finish change_state\n");
    return ret;
}
/******************************************************************************
 * gst_tiimgenc_codec_stop
 *     Stop codec engine
 *****************************************************************************/
static gboolean gst_tiimgenc_codec_stop (GstTIImgenc  *imgenc)
{
    /* Shut down remaining items */
    if (imgenc->circBuf) {
        GstTICircBuffer *circBuf;

        GST_LOG("freeing cicrular input buffer\n");

        circBuf              = imgenc->circBuf;
        imgenc->circBuf      = NULL;
        imgenc->framerateNum = 0;
        imgenc->framerateDen = 0;
        gst_ticircbuffer_unref(circBuf);
    }

    if (imgenc->hOutBufTab) {

        /* Re-claim all output buffers that were pushed downstream, and then
         * delete the BufTab.
         */
        gst_ti_reclaim_buffers(imgenc->hOutBufTab);

        GST_LOG("freeing output buffers\n");
        BufTab_delete(imgenc->hOutBufTab);
        imgenc->hOutBufTab = NULL;
    }

    if (imgenc->hIe) {
        GST_LOG("closing image encoder\n");
        Ienc_delete(imgenc->hIe);
        imgenc->hIe = NULL;
    }

    if (imgenc->hEngine) {
        GST_LOG("closing codec engine\n");
        Engine_close(imgenc->hEngine);
        imgenc->hEngine = NULL;
    }

    return TRUE;
}


/******************************************************************************
 * gst_tiimgenc_codec_start
 *     Initialize codec engine
 *****************************************************************************/
static gboolean gst_tiimgenc_codec_start (GstTIImgenc  *imgenc)
{
    BufferGfx_Attrs        gfxAttrs  = BufferGfx_Attrs_DEFAULT;
    Int                    inBufSize;

    /* Open the codec engine */
    GST_LOG("opening codec engine \"%s\"\n", imgenc->engineName);
    imgenc->hEngine = Engine_open((Char *) imgenc->engineName, NULL, NULL);

    if (imgenc->hEngine == NULL) {
        GST_ELEMENT_ERROR(imgenc, RESOURCE, FAILED,
        ("failed to open codec engine \"%s\"\n", imgenc->engineName), (NULL));
        return FALSE;
    }
    GST_LOG("opened codec engine \"%s\" successfully\n", imgenc->engineName);

    if (!gst_tiimgenc_set_codec_attrs(imgenc)) {
        GST_ELEMENT_ERROR(imgenc, RESOURCE, FAILED,
        ("Error while trying to set the codec attrs\n"), (NULL));
        return FALSE;
    }

    GST_LOG("opening image encoder \"%s\"\n", imgenc->codecName);
    imgenc->hIe = Ienc_create(imgenc->hEngine, (Char*)imgenc->codecName,
                      &imgenc->params, &imgenc->dynParams);

    if (imgenc->hIe == NULL) {
        GST_ELEMENT_ERROR(imgenc, STREAM, CODEC_NOT_FOUND,
        ("failed to create image encoder: %s\n", imgenc->codecName), (NULL));
        GST_DEBUG("Verify that the values being used for input and output ColorSpace are supported by your codec.\n");
        GST_LOG("closing codec engine\n");
        return FALSE;
    }

    inBufSize = BufferGfx_calcLineLength(imgenc->width,
                gst_tiimgenc_convert_attrs(VAR_ICOLORSPACE, imgenc))
                * imgenc->height;

    /* Create a circular input buffer */
    imgenc->circBuf = gst_ticircbuffer_new(
                           Ienc_getInBufSize(imgenc->hIe), 2, TRUE);

    if (imgenc->circBuf == NULL) {
        GST_ELEMENT_ERROR(imgenc, RESOURCE, NO_SPACE_LEFT,
        ("failed to create circular input buffer\n"), (NULL));
        return FALSE;
    }

     /* Calculate the maximum number of buffers allowed in queue before
     * blocking upstream.
     */
    imgenc->queueMaxBuffers = (inBufSize / imgenc->upstreamBufSize) + 3;
    GST_LOG("setting max queue threadshold to %d\n", imgenc->queueMaxBuffers);

    /* Display buffer contents if displayBuffer=TRUE was specified */
    gst_ticircbuffer_set_display(imgenc->circBuf, imgenc->displayBuffer);

    /* Define the number of display buffers to allocate.  This number must be
     * at least 1, but should be more if codecs don't return a display buffer
     * after every process call.  If this has not been set via set_property(),
     * default to the value set above based on device type.
     */
    if (imgenc->numOutputBufs == 0) {
        imgenc->numOutputBufs = 1;
    }
    /* Create codec output buffers */
    GST_LOG("creating output buffer table\n");
    gfxAttrs.colorSpace     = gst_tiimgenc_codec_color_space_to_dmai(imgenc->params.forceChromaFormat);
    gfxAttrs.dim.width      = imgenc->params.maxWidth;
    gfxAttrs.dim.height     = imgenc->params.maxHeight;
    gfxAttrs.dim.lineLength = BufferGfx_calcLineLength(
                                  gfxAttrs.dim.width, gfxAttrs.colorSpace);

    gfxAttrs.bAttrs.memParams.align = 128;

    /* By default, new buffers are marked as in-use by the codec */
    gfxAttrs.bAttrs.useMask = gst_tiimgenc_CODEC_FREE;

    imgenc->hOutBufTab =
        BufTab_create(imgenc->numOutputBufs, Ienc_getOutBufSize(imgenc->hIe),
            BufferGfx_getBufferAttrs(&gfxAttrs));

    if (imgenc->hOutBufTab == NULL) {
        GST_ELEMENT_ERROR(imgenc, RESOURCE, NO_SPACE_LEFT,
        ("failed to create output buffers\n"), (NULL));
        return FALSE;
    }

    return TRUE;

}


/******************************************************************************
 * gst_tiimgenc_encode_thread
 *     Call the image codec to process a full input buffer
 ******************************************************************************/
static void* gst_tiimgenc_encode_thread(void *arg)
{
    GstTIImgenc           *imgenc        = GST_TIIMGENC(gst_object_ref(arg));
    GstBuffer              *encDataWindow  = NULL;
    gboolean               codecFlushed    = FALSE;
    void                   *threadRet      = GstTIThreadSuccess;
    BufferGfx_Attrs        gfxAttrs        = BufferGfx_Attrs_DEFAULT;
    Buffer_Handle          hDstBuf;
    Int32                  encDataConsumed;
    GstClockTime           encDataTime;
    GstClockTime           frameDuration;
    Buffer_Handle          hEncDataWindow;
    BufferGfx_Dimensions   dim;
    GstBuffer              *outBuf;
    Int                    bufIdx;
    Int                    ret;

    GST_LOG("Begin\n");
    /* Calculate the duration of a single frame in this stream */
    frameDuration = gst_tiimgenc_frame_duration(imgenc);

    /* Initialize codec engine */
    ret = gst_tiimgenc_codec_start(imgenc);

    /* Notify main thread that is ok to continue initialization */
    Rendezvous_meet(imgenc->waitOnEncodeThread);
    Rendezvous_reset(imgenc->waitOnEncodeThread);

    if (ret == FALSE) {
        GST_ELEMENT_ERROR(imgenc, RESOURCE, FAILED,
        ("failed to start codec\n"), (NULL));
        goto thread_exit;
    }


    /* Main thread loop */
    while (TRUE) {

        /* Obtain an encoded data frame */
        encDataWindow  = gst_ticircbuffer_get_data(imgenc->circBuf);
        encDataTime    = GST_BUFFER_TIMESTAMP(encDataWindow);
        hEncDataWindow = GST_TIDMAIBUFFERTRANSPORT_DMAIBUF(encDataWindow);

        /* If we received a data frame of zero size, there is no more data to
         * process -- exit the thread.  If we weren't told that we are
         * draining the pipeline, something is not right, so exit with an
         * error.
         */
        if (GST_BUFFER_SIZE(encDataWindow) == 0) {
            GST_LOG("no image data remains\n");
            if (!imgenc->drainingEOS) {
                goto thread_failure;
            }

            goto thread_exit;
        }

        /* Obtain a free output buffer for the encoded data */
        /* If we are not able to find free buffer from BufTab then decoder 
         * thread will be blocked on waitOnBufTab rendezvous. And this will be 
         * woke-up by dmaitransportbuffer finalize method.
         */
        hDstBuf = BufTab_getFreeBuf(imgenc->hOutBufTab);
        if (hDstBuf == NULL) {
            Rendezvous_meet(imgenc->waitOnBufTab);
            hDstBuf = BufTab_getFreeBuf(imgenc->hOutBufTab);

            if (hDstBuf == NULL) {
                GST_ELEMENT_ERROR(imgenc, RESOURCE, READ,
                ("failed to get a free contiguous buffer from BufTab\n"),
                (NULL));
                goto thread_failure;
            }
        }

        /* Reset waitOnBufTab rendezvous handle to its orignal state */
        Rendezvous_reset(imgenc->waitOnBufTab);

        /* Make sure the whole buffer is used for output */
        BufferGfx_resetDimensions(hDstBuf);

        /* Create a BufferGfx object that will point to a reference buffer from
         * The circular buffer.  This is needed for the encoder which requires
         * that the input buffer be a BufferGfx object.
         */
        BufferGfx_getBufferAttrs(&gfxAttrs)->reference = TRUE;
        imgenc->hInBuf = Buffer_create(Buffer_getSize(hEncDataWindow),
                                BufferGfx_getBufferAttrs(&gfxAttrs));
        Buffer_setUserPtr(imgenc->hInBuf, Buffer_getUserPtr(hEncDataWindow));
        Buffer_setNumBytesUsed(imgenc->hInBuf, 
                               Buffer_getSize(imgenc->hInBuf));

        /* Set the dimensions of the buffer to the resolution */
        BufferGfx_getDimensions(hDstBuf, &dim);
        dim.width = imgenc->dynParams.inputWidth;
        dim.height = imgenc->dynParams.inputHeight;
        BufferGfx_setDimensions(imgenc->hInBuf, &dim);
        BufferGfx_setColorSpace(imgenc->hInBuf,
                     imgenc->dynParams.inputChromaFormat); 

        /* Invoke the image encoder */
        GST_LOG("invoking the image encoder\n");
        ret             = Ienc_process(imgenc->hIe, imgenc->hInBuf, hDstBuf);
        encDataConsumed = (codecFlushed) ? 0 :
                          Buffer_getNumBytesUsed(hEncDataWindow);

        /* Delete the temporary Graphics buffer used for the encode process */
        Buffer_delete(imgenc->hInBuf);

        if (ret < 0) {
            GST_ELEMENT_ERROR(imgenc, STREAM, ENCODE,
            ("failed to encode image buffer\n"), (NULL));
            goto thread_failure;
        }

        /* If no data was used we must have some kind of encode error */
        if (ret == Dmai_EBITERROR && encDataConsumed == 0 && !codecFlushed) {
            GST_ELEMENT_ERROR(imgenc, STREAM, ENCODE, 
            ("fatal bit error\n"), (NULL));
            goto thread_failure;
        }

        if (ret > 0) {
            GST_LOG("Ienc_process returned success code %d\n", ret); 
        }

        /* Release the reference buffer, and tell the circular buffer how much
         * data was consumed.
         */
        ret = gst_ticircbuffer_data_consumed(imgenc->circBuf, encDataWindow,
                  encDataConsumed);
        encDataWindow = NULL;

        if (!ret) {
            goto thread_failure;
        }


        /* Set the source pad capabilities based on the encoded frame
         * properties.
         */
        gst_tiimgenc_set_source_caps(imgenc, hDstBuf);

        /* Create a DMAI transport buffer object to carry a DMAI buffer to
         * the source pad.  The transport buffer knows how to release the
         * buffer for re-use in this element when the source pad calls
         * gst_buffer_unref().
         */
        outBuf = gst_tidmaibuffertransport_new(hDstBuf, imgenc->waitOnBufTab);
        gst_buffer_set_data(outBuf, GST_BUFFER_DATA(outBuf),
            Buffer_getNumBytesUsed(hDstBuf));
        gst_buffer_set_caps(outBuf, GST_PAD_CAPS(imgenc->srcpad));

        /* If we have a valid time stamp, set it on the buffer */
        if (imgenc->genTimeStamps &&
            GST_CLOCK_TIME_IS_VALID(encDataTime)) {
            GST_LOG("image timestamp value: %llu\n", encDataTime);
            GST_BUFFER_TIMESTAMP(outBuf) = encDataTime;
            GST_BUFFER_DURATION(outBuf)  = frameDuration;
        }
        else {
            GST_BUFFER_TIMESTAMP(outBuf) = GST_CLOCK_TIME_NONE;
        }

        /* Tell circular buffer how much time we consumed */
        gst_ticircbuffer_time_consumed(imgenc->circBuf, frameDuration);

        /* Push the transport buffer to the source pad */
        GST_LOG("pushing display buffer to source pad\n");

        if (gst_pad_push(imgenc->srcpad, outBuf) != GST_FLOW_OK) {
            GST_DEBUG("push to source pad failed\n");
            goto thread_failure;
        }

        /* Release buffers no longer in use by the codec */
        Buffer_freeUseMask(hDstBuf, gst_tiimgenc_CODEC_FREE);
    }

thread_failure:

    gst_tithread_set_status(imgenc, TIThread_CODEC_ABORTED);
    gst_ticircbuffer_consumer_aborted(imgenc->circBuf);
    threadRet = GstTIThreadFailure;

thread_exit:
 
    /* Re-claim any buffers owned by the codec */
    bufIdx = BufTab_getNumBufs(imgenc->hOutBufTab);

    while (bufIdx-- > 0) {
        Buffer_Handle hBuf = BufTab_getBuf(imgenc->hOutBufTab, bufIdx);
        Buffer_freeUseMask(hBuf, gst_tiimgenc_CODEC_FREE);
    }

    /* Release the last buffer we retrieved from the circular buffer */
    if (encDataWindow) {
        gst_ticircbuffer_data_consumed(imgenc->circBuf, encDataWindow, 0);
    }

    /* We have to wait to shut down this thread until we can guarantee that
     * no more input buffers will be queued into the circular buffer
     * (we're about to delete it).  
     */
    Rendezvous_meet(imgenc->waitOnEncodeThread);
    Rendezvous_reset(imgenc->waitOnEncodeThread);

    /* Notify main thread that we are done draining before we shutdown the
     * codec, or we will hang.  We proceed in this order so the EOS event gets
     * propagated downstream before we attempt to shut down the codec.  The
     * codec-shutdown process will block until all BufTab buffers have been
     * released, and downstream-elements may hang on to buffers until
     * they get the EOS.
     */
    Rendezvous_force(imgenc->waitOnEncodeDrain);

    /* Stop codec engine */
    if (gst_tiimgenc_codec_stop(imgenc) < 0) {
        GST_ERROR("failed to stop codec\n");
    }

    gst_object_unref(imgenc);

    GST_LOG("Finish image encode_thread (%d)\n", (int)threadRet);
    return threadRet;
}


/******************************************************************************
 * gst_tiimgenc_drain_pipeline
 *    Wait for the encode thread to finish processing queued input data.
 ******************************************************************************/
static void gst_tiimgenc_drain_pipeline(GstTIImgenc *imgenc)
{
    gboolean checkResult;

    /* If the encode thread hasn't been created, there is nothing to drain. */
    if (!gst_tithread_check_status(
             imgenc, TIThread_CODEC_CREATED, checkResult)) {
        return;
    }

    imgenc->drainingEOS = TRUE;
    gst_ticircbuffer_drain(imgenc->circBuf, TRUE);

    /* Tell the encode thread that it is ok to shut down */
    Rendezvous_force(imgenc->waitOnEncodeThread);

    /* Wait for the encoder to finish draining */
    Rendezvous_meet(imgenc->waitOnEncodeDrain);
}


/******************************************************************************
 * gst_tiimgenc_frame_duration
 *    Return the duration of a single frame in nanoseconds.
 ******************************************************************************/
static GstClockTime gst_tiimgenc_frame_duration(GstTIImgenc *imgenc)
{
    GST_LOG("Begin\n");
    /* Default to 29.97 if the frame rate was not specified */
    if (imgenc->framerateNum == 0 && imgenc->framerateDen == 0) {
        GST_WARNING("framerate not specified; using 29.97fps");
        imgenc->framerateNum = 30000;
        imgenc->framerateDen = 1001;
    }

    GST_LOG("Finish\n");
    return (GstClockTime)
        ((1 / ((gdouble)imgenc->framerateNum/(gdouble)imgenc->framerateDen))
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
