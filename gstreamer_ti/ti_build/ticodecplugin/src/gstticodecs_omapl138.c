/*
 * gstticodecs_omapl138.c
 *
 * This file provides information for available codecs on the OMAP3530 platform.
 *
 * Original Author:
 *     Don Darling, Texas Instruments, Inc.
 *
 * Copyright (C) 2010 Texas Instruments Incorporated - http://www.ti.com/
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

#include "gstticodecs.h"

/* Declaration of the production engine and decoders shipped with the DVSDK */
static Char codecServer[] = "codecServer";

/* NULL terminated list of speech decoders in the engine to use in the demo */
GstTICodec gst_ticodec_codecs[] = {

    /* Audio Codecs */
    {
        "AAC Audio Decoder",     /* String name of codec used by plugin      */
        "aachedec",              /* String name of codec used by CE          */
        codecServer              /* Engine that contains this codec          */
    },

    /* Video Codecs */
    {
        "H.264 Video Decoder",   /* String name of codec used by plugin      */
        "h264dec",               /* String name of codec used by CE          */
        codecServer              /* Engine that contains this codec          */
    }, {
        "MPEG4 Video Decoder",   /* String name of codec used by plugin      */
        "mpeg4dec",              /* String name of codec used by CE          */
        codecServer              /* Engine that contains this codec          */
    }, {
        "MPEG2 Video Decoder",   /* String name of codec used by plugin      */
        "mpeg2dec",              /* String name of codec used by CE          */
        codecServer              /* Engine that contains this codec          */
    }, {
        "H.264 Video Encoder",   /* String name of codec used by plugin      */
        "h264enc",               /* String name of codec used by CE          */
        codecServer              /* Engine that contains this codec          */
    }, {
        "MPEG4 Video Encoder",   /* String name of codec used by plugin      */
        "mpeg4enc",               /* String name of codec used by CE          */
        codecServer              /* Engine that contains this codec          */
    },

    /* Image Decoders */
    {
        "JPEG Image Decoder",    /* String name of codec used by plugin      */
        "jpegdec",               /* String name of codec used by CE          */
        codecServer              /* Engine that contains this codec          */
    }, {
        "JPEG Image Encoder",    /* String name of codec used by plugin      */
        "jpegenc",               /* String name of codec used by CE          */
        codecServer              /* Engine that contains this codec          */
    },

    { NULL }
};


/******************************************************************************
 * Custom ViM Settings for editing this file
 ******************************************************************************/
#if 0
 Tabs (use 4 spaces for indentation)
 vim:set tabstop=4:      /* Use 4 spaces for tabs          */
 vim:set shiftwidth=4:   /* Use 4 spaces for >> operations */
 vim:set expandtab:      /* Expand tabs into white spaces  */
#endif
