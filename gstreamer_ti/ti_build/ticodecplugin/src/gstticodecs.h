/*
 * gstticodecs.h
 *
 * This file declares an oracle for looking up platform-specific codec
 * information.
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

#include <xdc/std.h>

/* Data structures to hold codec information */
typedef struct _GstTICodec {

    /* String name of codec used by plugin */
    Char *codecName;

    /* String name of codec used by Codec Engine */
    Char *CE_CodecName;

    /* Engine the contains this codec */
    Char *CE_EngineName;

} GstTICodec;

/* Function declarations */
GstTICodec *gst_ticodec_get_codec(Char *codecName);


/******************************************************************************
 * Custom ViM Settings for editing this file
 ******************************************************************************/
#if 0
 Tabs (use 4 spaces for indentation)
 vim:set tabstop=4:      /* Use 4 spaces for tabs          */
 vim:set shiftwidth=4:   /* Use 4 spaces for >> operations */
 vim:set expandtab:      /* Expand tabs into white spaces  */
#endif
