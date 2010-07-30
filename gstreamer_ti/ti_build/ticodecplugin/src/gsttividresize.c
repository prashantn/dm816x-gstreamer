/*
 * gsttividresize.c
 *
 * This file defines the "TIVidResize" element, which resizes the video frames
 * using hardware resizer (if available).
 *
 * Example usage:
 *     gst-launch videotestsrc ! 'video/x-raw-yuv,width=160,height=120' !
 *       TIVidResize ! 'video/x-raw-yuv,width=320,height=240' ! 
 *        fakesink silent=TRUE
 *
 * Original Author:
 *     Brijesh Singh, Texas Instruments, Inc.
 *
 * Note: Element pushes dmai transport buffer to the src. The downstream can 
 * use Dmai transport macro to get the DMAI buffer handle.
 *
 * This element supports only YUV422, Y8C8 and NV12 color space.
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
#include <gst/base/gstbasetransform.h>

#include <ti/sdo/dmai/Dmai.h>
#include <ti/sdo/dmai/Buffer.h>
#include <ti/sdo/dmai/BufferGfx.h>
#include <ti/sdo/dmai/BufTab.h>
#include <ti/sdo/dmai/Resize.h>

#include "gsttividresize.h"
#include "gsttidmaibuffertransport.h"
#include "gstticommonutils.h"

/* Declare variable used to categorize GST_LOG output */
GST_DEBUG_CATEGORY_STATIC (gst_tividresize_debug);
#define GST_CAT_DEFAULT gst_tividresize_debug

/* Element property identifier */
enum {
  PROP_0,
  PROP_CONTIG_INPUT_FRAME,       /*  contiguousInputFrame    (boolean)   */
  PROP_NUM_OUTPUT_BUFS,          /*  numOutputBufs           (gint)      */
  PROP_HORZ_WINDOW_TYPE,         /*  hWindowType             (gint)      */
  PROP_VERT_WINDOW_TYPE,         /*  vWindowType             (gint)      */
  PROP_HORZ_FILTER_TYPE,         /*  hFilterType             (gint)      */
  PROP_VERT_FILTER_TYPE          /*  vFilterType             (gint)      */
};

/* Define property default */
#define DEFAULT_HORZ_WINDOW_TYPE        Resize_WindowType_BLACKMAN
#define DEFAULT_VERT_WINDOW_TYPE        Resize_WindowType_BLACKMAN
#define DEFAULT_HORZ_FILTER_TYPE        Resize_FilterType_LOWPASS
#define DEFAULT_VERT_FILTER_TYPE        Resize_FilterType_LOWPASS
#define DEFAULT_NUM_OUTPUT_BUFS         2
#define DEFAULT_CONTIGUOUS_INPUT_FRAME  FALSE

/* Define sink and src pad capabilities.  Currently, UYVY and Y8C8
 * supported.
 *
 * UYVY - YUV 422 interleaved corresponding to V4L2_PIX_FMT_UYVY in v4l2
 * Y8C8 - YUV 422 semi planar. The dm6467 VDCE outputs this format after a
 *        color conversion.The format consists of two planes: one with the
 *        Y component and one with the CbCr components interleaved (hence semi)  *        See the LSP VDCE documentation for a thorough description of this
 *        format.
 * NV12 - 8-bit Y plane followed by an interleaved U/V plane with 2x2 
 *        subsampling
 */
static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE(
    "sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS
    ( GST_VIDEO_CAPS_YUV("UYVY")";"
      GST_VIDEO_CAPS_YUV("NV16")";"
      GST_VIDEO_CAPS_YUV("Y8C8")";"
      GST_VIDEO_CAPS_YUV("NV12")
    )
);

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE(
    "src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS
    ( GST_VIDEO_CAPS_YUV("UYVY")";"
      GST_VIDEO_CAPS_YUV("NV16")";"
      GST_VIDEO_CAPS_YUV("Y8C8")";"
      GST_VIDEO_CAPS_YUV("NV12")
    )
);

/* Declare a global pointer to our element base class */
static GstElementClass *parent_class = NULL;

/* Static Function Declarations */
static void
 gst_tividresize_base_init(gpointer g_class);
static void
 gst_tividresize_class_init(GstTIVidresizeClass *g_class);
static void
 gst_tividresize_init(GstTIVidresize *object);
static gboolean gst_tividresize_exit_resize(GstTIVidresize *vidresize);
static void gst_tividresize_fixate_caps (GstBaseTransform *trans,
 GstPadDirection direction, GstCaps *caps, GstCaps *othercaps);
static gboolean gst_tividresize_set_caps (GstBaseTransform *trans, 
 GstCaps *in, GstCaps *out);
static gboolean gst_tividresize_parse_caps (GstCaps *cap, gint *width,
 gint *height, guint32 *fourcc);
static GstCaps * gst_tividresize_transform_caps (GstBaseTransform *trans,
 GstPadDirection direction, GstCaps *caps);
static GstFlowReturn gst_tividresize_transform (GstBaseTransform *trans,
 GstBuffer *inBuf, GstBuffer *outBuf);
static gboolean gst_tividresize_get_unit_size (GstBaseTransform *trans,
 GstCaps *caps, guint *size);
static ColorSpace_Type gst_tividresize_get_colorSpace (guint32 fourcc);
static void gst_tividresize_set_property(GObject *object, guint prop_id,
 const GValue *value, GParamSpec *pspec);
static GstFlowReturn gst_tividresize_prepare_output_buffer (GstBaseTransform
 *trans, GstBuffer *inBuf, gint size, GstCaps *caps, GstBuffer **outBuf);
static Buffer_Handle gst_tividresize_gfx_buffer_create (gint width, 
 gint height, ColorSpace_Type colorSpace, gint size, gboolean is_reference);

/******************************************************************************
 * gst_tividresize_init
 *****************************************************************************/
static void gst_tividresize_init (GstTIVidresize *vidresize)
{
    gst_base_transform_set_qos_enabled (GST_BASE_TRANSFORM (vidresize), TRUE);

    vidresize->hWindowType              =  Resize_WindowType_BLACKMAN;
    vidresize->vWindowType              =  Resize_WindowType_BLACKMAN;
    vidresize->hFilterType              =  Resize_FilterType_LOWPASS;
    vidresize->vFilterType              =  Resize_FilterType_LOWPASS;
    vidresize->contiguousInputFrame     =  DEFAULT_CONTIGUOUS_INPUT_FRAME;
    vidresize->numOutputBufs            =  DEFAULT_NUM_OUTPUT_BUFS;
    vidresize->hResize                  =  NULL;
}

/******************************************************************************
 * gst_tividresize_get_type
 *    Boiler-plate function auto-generated by "make_element" script.
 *    Defines function pointers for initialization routines for this element.
 ******************************************************************************/
GType gst_tividresize_get_type(void)
{
    static GType object_type = 0;

    if (G_UNLIKELY(object_type == 0)) {
        static const GTypeInfo object_info = {
            sizeof(GstTIVidresizeClass),
            gst_tividresize_base_init,
            NULL,
            (GClassInitFunc) gst_tividresize_class_init,
            NULL,
            NULL,
            sizeof(GstTIVidresize),
            0,
            (GInstanceInitFunc) gst_tividresize_init
        };

        object_type = g_type_register_static(GST_TYPE_BASE_TRANSFORM,
                          "GstTIVidResize", &object_info, (GTypeFlags)0);

        /* Initialize GST_LOG for this object */
        GST_DEBUG_CATEGORY_INIT(gst_tividresize_debug, "TIVidResize", 0,
            "TI Video Resize");

        GST_LOG("initialized get_type\n");
    }

    return object_type;
};

/******************************************************************************
 * gst_tividresize_base_init
 *    Boiler-plate function auto-generated by "make_element" script.
 *    Initializes element base class.
 ******************************************************************************/
static void gst_tividresize_base_init(gpointer gclass)
{
    static GstElementDetails element_details = {
        "TI video scale",
        "Filter/Resize",
        "Resize video using harware resizer",
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
 * gst_tividresize_class_init
 *    Boiler-plate function auto-generated by "make_element" script.
 *    Initializes the TIVidresize class.
 ******************************************************************************/
static void gst_tividresize_class_init(GstTIVidresizeClass *klass)
{
    GObjectClass    *gobject_class;
    GstBaseTransformClass   *trans_class;

    gobject_class    = (GObjectClass*)    klass;
    trans_class      = (GstBaseTransformClass *) klass;

    gobject_class->set_property = gst_tividresize_set_property;

    gobject_class->finalize = (GObjectFinalizeFunc)gst_tividresize_exit_resize;

    trans_class->transform_caps = 
        GST_DEBUG_FUNCPTR(gst_tividresize_transform_caps);
    trans_class->set_caps  = GST_DEBUG_FUNCPTR(gst_tividresize_set_caps);
    trans_class->transform = GST_DEBUG_FUNCPTR(gst_tividresize_transform);
    trans_class->fixate_caps = GST_DEBUG_FUNCPTR(gst_tividresize_fixate_caps);
    trans_class->get_unit_size = 
            GST_DEBUG_FUNCPTR(gst_tividresize_get_unit_size);
    trans_class->passthrough_on_same_caps = TRUE;
    trans_class->prepare_output_buffer = 
        GST_DEBUG_FUNCPTR(gst_tividresize_prepare_output_buffer);
    parent_class = g_type_class_peek_parent (klass);

    g_object_class_install_property(gobject_class, PROP_CONTIG_INPUT_FRAME,
        g_param_spec_boolean("contiguousInputFrame", "Contiguous input buffer",
            "Set this if element recieves contiguous input frame from"
            " upstream.", DEFAULT_CONTIGUOUS_INPUT_FRAME, G_PARAM_WRITABLE));

    g_object_class_install_property(gobject_class, PROP_NUM_OUTPUT_BUFS,
        g_param_spec_int("numOutputBufs",
            "Number of Output buffers",
            "Number of output buffers to allocate for resizer",
            1, G_MAXINT32, DEFAULT_NUM_OUTPUT_BUFS, G_PARAM_WRITABLE));

    g_object_class_install_property(gobject_class, PROP_HORZ_WINDOW_TYPE,
        g_param_spec_int("hWindowType",
            "Horizontal  video type ",
            "Horizontal window types supported for coefficient calculation\n"
            "\t\t\t 0 - HANN \n"
            "\t\t\t 1 - BLACKMAN \n"
            "\t\t\t 2 - TRIANGULAR \n"
            "\t\t\t 3 - RECTANGULAR \n",
            1, G_MAXINT32, DEFAULT_HORZ_WINDOW_TYPE, G_PARAM_WRITABLE));

    g_object_class_install_property(gobject_class, PROP_VERT_WINDOW_TYPE,
        g_param_spec_int("vWindowType",
            "Vertical video type ",
            "Vertical window types supported for coefficient calculation\n"
            "\t\t\t 0 - HANN \n"
            "\t\t\t 1 - BLACKMAN \n"
            "\t\t\t 2 - TRIANGULAR \n"
            "\t\t\t 3 - RECTANGULAR \n",
            1, G_MAXINT32, DEFAULT_VERT_WINDOW_TYPE, G_PARAM_WRITABLE));

    g_object_class_install_property(gobject_class, PROP_HORZ_FILTER_TYPE,
        g_param_spec_int("hFilterType",
            "Horizontal filter type ",
            "Horizontal filter types supported for coefficient calculation\n"
            "\t\t\t 0 - BILINEAR \n"
            "\t\t\t 1 - BICUBIC \n"
            "\t\t\t 2 - LOWPASS \n",
            1, G_MAXINT32, DEFAULT_HORZ_FILTER_TYPE, G_PARAM_WRITABLE));

    g_object_class_install_property(gobject_class, PROP_VERT_FILTER_TYPE,
        g_param_spec_int("vFilterType",
            "Vertical filter type ",
            "Vertical filter types supported for coefficient calculation\n"
            "\t\t\t 0 - BILINEAR \n"
            "\t\t\t 1 - BICUBIC \n"
            "\t\t\t 2 - LOWPASS \n",
            1, G_MAXINT32, DEFAULT_VERT_FILTER_TYPE, G_PARAM_WRITABLE));

    GST_LOG("initialized class init\n");
}

/******************************************************************************
 * gst_tividresize_gfx_buffer_create
 *  Helper function to create dmai graphics buffer
 *****************************************************************************/
static Buffer_Handle gst_tividresize_gfx_buffer_create (gint width, 
    gint height, ColorSpace_Type colorSpace, gint size, gboolean is_reference)
{
    BufferGfx_Attrs gfxAttrs   = BufferGfx_Attrs_DEFAULT;
    Buffer_Handle   buf        = NULL;
                
    gfxAttrs.bAttrs.reference  = is_reference;
    gfxAttrs.colorSpace     = colorSpace;
    gfxAttrs.dim.width      = width;
    gfxAttrs.dim.height     = height;
    gfxAttrs.dim.lineLength = BufferGfx_calcLineLength(gfxAttrs.dim.width, 
                                gfxAttrs.colorSpace);
    buf = Buffer_create(size, BufferGfx_getBufferAttrs(&gfxAttrs));

    if (buf == NULL) {
        return NULL;
    }

    return buf;
}

/*****************************************************************************
 * gst_tividresize_prepare_output_buffer
 *    Function is used to allocate output buffer
 *****************************************************************************/
static GstFlowReturn gst_tividresize_prepare_output_buffer (GstBaseTransform
    *trans, GstBuffer *inBuf, gint size, GstCaps *caps, GstBuffer **outBuf)
{
    GstTIVidresize *vidresize = GST_TIVIDRESIZE(trans);
    Buffer_Handle   hOutBuf;

    GST_LOG("begin prepare output buffer\n");

    /* Get free buffer from buftab */
    if (!(hOutBuf = gst_tidmaibuftab_get_buf(vidresize->hOutBufTab))) {
        GST_ELEMENT_ERROR(vidresize, RESOURCE, READ,
            ("failed to get free buffer\n"), (NULL));
        return GST_FLOW_ERROR;
    }

    /* Create a DMAI transport buffer object to carry a DMAI buffer to
     * the source pad.  The transport buffer knows how to release the
     * buffer for re-use in this element when the source pad calls
     * gst_buffer_unref().
     */
    GST_LOG("creating dmai transport buffer\n");
    *outBuf = gst_tidmaibuffertransport_new(hOutBuf, vidresize->hOutBufTab);
    gst_buffer_set_data(*outBuf, (guint8*) Buffer_getUserPtr(hOutBuf), 
                        Buffer_getSize(hOutBuf));
    gst_buffer_set_caps(*outBuf, GST_PAD_CAPS(trans->srcpad));

    GST_LOG("end prepare output buffer\n");   

    return GST_FLOW_OK;
}

/******************************************************************************
 * gst_tividenc_set_property
 *     Set element properties when requested.
 ******************************************************************************/
static void gst_tividresize_set_property(GObject *object, guint prop_id,
                const GValue *value, GParamSpec *pspec)
{
    GstTIVidresize *vidresize = GST_TIVIDRESIZE(object);

    GST_LOG("begin set_property\n");

    switch (prop_id) {
        case PROP_CONTIG_INPUT_FRAME:
            vidresize->contiguousInputFrame = g_value_get_boolean(value);
            GST_LOG("setting \"contiguousInputFrame\" to \"%s\"\n",
                vidresize->contiguousInputFrame ? "TRUE" : "FALSE");
            break;
        case PROP_NUM_OUTPUT_BUFS:
            vidresize->numOutputBufs = g_value_get_int(value);
            GST_LOG("setting \"numOutputBufs\" to \"%d\"\n",
                vidresize->numOutputBufs);
            break;
        case PROP_HORZ_WINDOW_TYPE:
            vidresize->hWindowType = g_value_get_int(value);
            GST_LOG("setting \"hWindowType\" to \"%d\"\n",
                vidresize->hWindowType);
            break;
        case PROP_VERT_WINDOW_TYPE:
            vidresize->vWindowType = g_value_get_int(value);
            GST_LOG("setting \"vWindowType\" to \"%d\"\n",
                vidresize->vWindowType);
            break;
        case PROP_HORZ_FILTER_TYPE:
            vidresize->hFilterType = g_value_get_int(value);
            GST_LOG("setting \"hFilterType\" to \"%d\"\n",
                vidresize->hFilterType);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }

    GST_LOG("end set_property\n");
}
       
/******************************************************************************
 * gst_tividresize_get_unit_size
 *   get the size in bytes of one unit for the given caps
 *****************************************************************************/ 
static gboolean gst_tividresize_get_unit_size (GstBaseTransform *trans,
    GstCaps *caps, guint *size)
{
    gint           height, width;
    guint32         fourcc;
    ColorSpace_Type colorSpace;

    GST_LOG("begin get_unit_size\n");
 
    if (!gst_tividresize_parse_caps(caps, &width, &height, &fourcc)) {
        GST_ERROR("Failed to get resolution\n");
        return FALSE;
    }

    colorSpace = gst_tividresize_get_colorSpace(fourcc);

    *size = gst_ti_calc_buffer_size(width, height, 0, colorSpace);

    GST_LOG("setting unit_size = %d\n", *size);
 
    GST_LOG("end get_unit_size\n"); 
    return TRUE; 
}

/******************************************************************************
 * gst_tividresize_transform 
 *    Transforms one incoming buffer to one outgoing buffer.
 *****************************************************************************/
static GstFlowReturn gst_tividresize_transform (GstBaseTransform *trans,
    GstBuffer *src, GstBuffer *dst)
{
    GstTIVidresize      *vidresize  = GST_TIVIDRESIZE(trans);
    Buffer_Handle       hInBuf      = NULL, hOutBuf = NULL;
    GstFlowReturn       ret         = GST_FLOW_ERROR;
    Resize_Attrs        rszAttrs    = Resize_Attrs_DEFAULT;

    GST_LOG("begin transform\n");

    /* Get the output buffer handle */ 
    hOutBuf = GST_TIDMAIBUFFERTRANSPORT_DMAIBUF(dst);

    /* Get the input buffer handle */
    if (GST_IS_TIDMAIBUFFERTRANSPORT(src)) {
        /* if we are recieving dmai transport buffer then get the handle */
        hInBuf = GST_TIDMAIBUFFERTRANSPORT_DMAIBUF(src);
    }
    else if (vidresize->contiguousInputFrame) {
        /* if we are configured to recieve continguous input buffer then 
         * create reference dmai buffer.
         */
        hInBuf = gst_tividresize_gfx_buffer_create(vidresize->srcWidth, 
                    vidresize->srcHeight, vidresize->srcColorSpace, 
                    GST_BUFFER_SIZE(src), TRUE);
        if (hInBuf == NULL) {
            GST_ELEMENT_ERROR(vidresize, RESOURCE, NO_SPACE_LEFT,
            ("failed to create input dmai reference buffer \n"), (NULL));
            goto exit;
        }
        Buffer_setUserPtr(hInBuf, (Int8*) GST_BUFFER_DATA(src));
        Buffer_setNumBytesUsed(hInBuf, GST_BUFFER_SIZE(src));
    }
    else {
        /* If we are recieving non-contiguous buffer then create dmai 
         * contiguous input buffer and copy the data in this buffer.
         */
        hInBuf = gst_tividresize_gfx_buffer_create(vidresize->srcWidth,
                    vidresize->srcHeight, vidresize->srcColorSpace,
                     GST_BUFFER_SIZE(src), FALSE);
        if (hInBuf == NULL) {
            GST_ELEMENT_ERROR(vidresize, RESOURCE, NO_SPACE_LEFT,
            ("failed to create input dmai buffer \n"), (NULL));
            goto exit;
        }

        memcpy(Buffer_getUserPtr(hInBuf), GST_BUFFER_DATA(src), 
                GST_BUFFER_SIZE(src));
        Buffer_setNumBytesUsed(hInBuf, GST_BUFFER_SIZE(src));
    }

    /* Create resize handle */
    if (vidresize->hResize == NULL) {
        GST_LOG("scaling from=%dx%d, size=%ld -> to=%dx%d, size=%ld\n", 
            vidresize->srcWidth, vidresize->srcHeight, Buffer_getSize(hInBuf),
            vidresize->dstWidth, vidresize->dstHeight, Buffer_getSize(hOutBuf));

        rszAttrs.hWindowType = vidresize->hWindowType;
        rszAttrs.vWindowType = vidresize->vWindowType;
        rszAttrs.hFilterType = vidresize->hFilterType;
        rszAttrs.vFilterType = vidresize->vFilterType;

        vidresize->hResize = Resize_create(&rszAttrs);

        if (vidresize->hResize == NULL) {
            GST_ELEMENT_ERROR(vidresize, RESOURCE, FAILED,
            ("failed to create resize handle\n"), (NULL));
            goto exit;
        }

        if (Resize_config(vidresize->hResize, hInBuf, hOutBuf) < 0) {
            GST_ELEMENT_ERROR(vidresize, RESOURCE, FAILED,
            ("failed to configure resize\n"), (NULL));
            goto exit;
        }
    }

    /* Execute resizer */
    if (Resize_execute(vidresize->hResize, hInBuf, hOutBuf) < 0) {
        GST_ELEMENT_ERROR(vidresize, RESOURCE, FAILED,
        ("failed to execute resize\n"), (NULL));
        goto exit;
    }

    /* TODO: 
     * DM355 resizer module in DMAI 1_20_00_06 does not sets correct
     * numBytesUsed field. This could potentially issue when downstream is
     * using this field. To workaround this, we will compute and set correct
     * numBytesUsed.
     */
    Buffer_setNumBytesUsed(hOutBuf,  
        gst_ti_calc_buffer_size(vidresize->dstWidth, vidresize->dstHeight, 
        0, vidresize->dstColorSpace));

    ret = GST_FLOW_OK;

exit:
    if (hInBuf && !GST_IS_TIDMAIBUFFERTRANSPORT(src)) {
        Buffer_delete(hInBuf);
    }

    GST_LOG("end transform\n");
    return ret;
}

/******************************************************************************
 * gst_tividresize_make_caps
 *  this function will create simple caps based on the input format.
 *****************************************************************************/
GstCaps * gst_tividresize_make_caps(GstCaps *incap, guint32 fourcc)
{
    GstCaps *caps;
    GstStructure        *structure;

    caps = gst_caps_copy(incap);
    structure = gst_structure_copy (gst_caps_get_structure (caps, 0));

    gst_structure_set (structure,
      "format", GST_TYPE_FOURCC, fourcc,
      "width", GST_TYPE_INT_RANGE, 1, G_MAXINT,
      "height", GST_TYPE_INT_RANGE, 1, G_MAXINT, (gchar*) NULL);

    gst_caps_merge_structure(caps, gst_structure_copy (structure));
    gst_structure_free(structure);

    return caps;
}

/******************************************************************************
 * gst_tividresize_transform_caps
 *   Given the pad in this direction and the given caps, what caps are allowed 
 *   on the other pad in this element
 *****************************************************************************/
static GstCaps * gst_tividresize_transform_caps (GstBaseTransform *trans,
    GstPadDirection direction, GstCaps *caps)
{
    GstTIVidresize      *vidresize;
    GstStructure        *capStruct;
    GstCaps             *ret, *tmp;
    int                 i;
    static const guint32 supported_fmt[] = {
                                            GST_MAKE_FOURCC('U','Y','V','Y'),
                                            GST_MAKE_FOURCC('N','V','1','6'),
                                            GST_MAKE_FOURCC('Y','8','C','8'),
                                            GST_MAKE_FOURCC('N','V','1','2'),
                                           };

    capStruct = gst_caps_get_structure(caps, 0);
    g_return_val_if_fail(GST_CAPS_IS_SIMPLE(caps), NULL);

    vidresize   = GST_TIVIDRESIZE(trans);

    GST_LOG("begin transform caps (%s)\n",
        direction==GST_PAD_SRC ? "src" : "sink");

    ret = gst_caps_new_empty();

    for(i=0; i < G_N_ELEMENTS(supported_fmt); i++) {
        tmp = gst_tividresize_make_caps(caps, supported_fmt[i]);
        if (tmp) { 
            gst_caps_append(ret, tmp);
        }
    }

    GST_LOG("returing cap %" GST_PTR_FORMAT, ret);
    GST_LOG("end transform caps\n");

    return ret;
}

/******************************************************************************
 * gst_tividresize_parse_caps
 *****************************************************************************/
static gboolean gst_tividresize_parse_caps (GstCaps *cap, gint *width,
    gint *height, guint32 *format)
{
    GstStructure    *structure;
    structure = gst_caps_get_structure(cap, 0);
    
    GST_LOG("begin parse caps\n");

    if (!gst_structure_get_int(structure, "width", width)) {
        GST_ERROR("Failed to get width \n");
        return FALSE;
    }

    if (!gst_structure_get_int(structure, "height", height)) {
        GST_ERROR("Failed to get height \n");
        return FALSE;
    }
    
    if (!gst_structure_get_fourcc(structure, "format", format)) {
        GST_ERROR("failed to get fourcc from cap\n");
        return FALSE;
    }

    GST_LOG("end parse caps\n");
   
    return TRUE; 
}

/*****************************************************************************
 * gst_tividresize_get_colorSpace
 ****************************************************************************/
ColorSpace_Type gst_tividresize_get_colorSpace (guint32 fourcc)
{
    switch (fourcc) {
        case GST_MAKE_FOURCC('U', 'Y', 'V', 'Y'):            
            return ColorSpace_UYVY;
        case GST_MAKE_FOURCC('N', 'V', '1', '6'):
        case GST_MAKE_FOURCC('Y', '8', 'C', '8'):
            return ColorSpace_YUV422PSEMI;
        case GST_MAKE_FOURCC('N', 'V', '1', '2'):
            return ColorSpace_YUV420PSEMI;
        default:
            GST_ERROR("failed to get colorspace\n");
            return ColorSpace_NOTSET;
    }
}

/******************************************************************************
 * gst_tividresize_set_caps
 *****************************************************************************/
static gboolean gst_tividresize_set_caps (GstBaseTransform *trans, 
    GstCaps *in, GstCaps *out)
{
    GstTIVidresize      *vidresize  = GST_TIVIDRESIZE(trans);
    BufferGfx_Attrs     gfxAttrs    = BufferGfx_Attrs_DEFAULT;
    gboolean            ret         = FALSE;
    guint32             fourcc;
    guint               outBufSize;

    GST_LOG("begin set caps\n");

    /* parse input cap */
    if (!gst_tividresize_parse_caps(in, &vidresize->srcWidth,
             &vidresize->srcHeight, &fourcc)) {
        GST_ELEMENT_ERROR(vidresize, RESOURCE, FAILED,
        ("failed to get input resolution\n"), (NULL));
        goto exit;
    }

    GST_LOG("input fourcc %" GST_FOURCC_FORMAT, GST_FOURCC_ARGS(fourcc));

    /* map fourcc with its corresponding dmai colorspace type */ 
    vidresize->srcColorSpace = gst_tividresize_get_colorSpace(fourcc);

    /* parse output cap */
    if (!gst_tividresize_parse_caps(out, &vidresize->dstWidth,
             &vidresize->dstHeight, &fourcc)) {
        GST_ELEMENT_ERROR(vidresize, RESOURCE, FAILED,
        ("failed to get output resolution\n"), (NULL));
        goto exit;
    }

    GST_LOG("output fourcc %" GST_FOURCC_FORMAT, GST_FOURCC_ARGS(fourcc));

    /* map fourcc with its corresponding dmai colorspace type */
    vidresize->dstColorSpace = gst_tividresize_get_colorSpace(fourcc);

    /* calculate output buffer size */
    outBufSize = gst_ti_calc_buffer_size(vidresize->dstWidth,
        vidresize->dstHeight, 0, vidresize->dstColorSpace);

    /* allocate output buffer */
    gfxAttrs.bAttrs.useMask = gst_tidmaibuffer_GST_FREE;
    gfxAttrs.colorSpace = vidresize->dstColorSpace;
    gfxAttrs.dim.width = vidresize->dstWidth;
    gfxAttrs.dim.height = vidresize->dstHeight;
    gfxAttrs.dim.lineLength =
        BufferGfx_calcLineLength (gfxAttrs.dim.width, gfxAttrs.colorSpace);

    if (vidresize->numOutputBufs == 0) {
        vidresize->numOutputBufs = 2;
    }
 
   vidresize->hOutBufTab = gst_tidmaibuftab_new(vidresize->numOutputBufs,
       outBufSize, BufferGfx_getBufferAttrs (&gfxAttrs));

    if (vidresize->hOutBufTab == NULL) {
        GST_ELEMENT_ERROR(vidresize, RESOURCE, NO_SPACE_LEFT,
        ("failed to create output bufTab\n"), (NULL));
        goto exit;
    }

    ret = TRUE;

exit:
    GST_LOG("end set caps\n");
    return ret;
}

/******************************************************************************
 * gst_video_scale_fixate_caps 
 *****************************************************************************/
static void gst_tividresize_fixate_caps (GstBaseTransform *trans,
     GstPadDirection direction, GstCaps *caps, GstCaps *othercaps)
{
    GstStructure    *ins, *outs;
    gint            srcWidth, srcHeight;
    gint            dstWidth, dstHeight;

    g_return_if_fail(gst_caps_is_fixed(caps));

    GST_LOG("begin fixating cap\n");

    ins = gst_caps_get_structure(caps, 0);
    outs = gst_caps_get_structure(othercaps, 0);

    /* get the sink dimension */
    if (!gst_structure_get_int(ins, "width", &srcWidth)) {
        if (gst_structure_get_int(ins, "height", &srcHeight)) {
            GST_LOG("dimensions already set to %dx%d, not fixating", 
                    srcWidth, srcHeight);
        }
        return;
    }

    /* get the source dimension */
    if (gst_structure_get_int(outs, "width", &dstWidth)) {
        if (gst_structure_get_int(outs, "height", &dstHeight)) {
            GST_LOG("dimensions already set to %dx%d, not fixating", 
                    srcWidth, dstHeight);
        }
        return;
    }

    GST_LOG("end fixating cap\n");
}

/******************************************************************************
 * gst_tividresize_exit_resize
 *    Shut down any running video resize, and reset the element state.
 ******************************************************************************/
static gboolean gst_tividresize_exit_resize(GstTIVidresize *vidresize)
{
    GST_LOG("begin exit_video\n");

    /* Shut down remaining items */
    if (vidresize->hResize) {
        GST_LOG("deleting resizer device\n");
        Resize_delete(vidresize->hResize);
        vidresize->hResize = NULL;
    }

    if (vidresize->hOutBufTab) {
        GST_LOG("freeing output buffers\n");
        gst_tidmaibuftab_unref(vidresize->hOutBufTab);
        vidresize->hOutBufTab = NULL;
    }

    GST_LOG("end exit_video\n");
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
