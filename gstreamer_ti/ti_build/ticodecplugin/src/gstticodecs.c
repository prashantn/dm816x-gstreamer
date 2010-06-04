/*
 * gstticodecs.c
 *
 * This file provides an oracle for looking-up codec information for available
 * codecs on a platform.
 *
 * Original Author:
 *     Don Darling, Texas Instruments, Inc.
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

#include <string.h>
#include <xdc/std.h>
#include "gstticodecs.h"

extern GstTICodec gst_ticodec_codecs[];

/******************************************************************************
 * gst_ticodec_get_codec
 *     For the given codec name known to the plugin, return the string
 *     Codec Engine expects for that codec.
 ******************************************************************************/
GstTICodec *gst_ticodec_get_codec(Char *codecName)
{
    Int i;

    for (i = 0; gst_ticodec_codecs[i].codecName; i++) {
        if (!strcmp(codecName, gst_ticodec_codecs[i].codecName)) {
            return &gst_ticodec_codecs[i];
        }
    }

    return NULL;
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
