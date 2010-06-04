/* gsttiquicktime_mpeg4.h
 *
 * This file declares structure and macro used for creating byte-stream syntax
 * needed for decoding MPEG4 stream demuxed via qtdemuxer.
 *
 * Original Author:
 *     Pratheesh Gangadhar, Texas Instruments, Inc.
 *     Brijesh Singh, Texas Instruments, Inc.
 *
 * Copyright (C) 2008-2010 Texas Instruments Incorporated - http://www.ti.com/
 *
 * This program is free software; you can redistribute it and/or modify 
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation version 2.1 of the License.
 * whether express or implied; without even the implied warranty of * 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 */

#ifndef __GST_QUICKTIME_MPEG4_H__
#define __GST_QUICKTIME_MPEG4_H__

#include <gst/gst.h>
#include "gstticircbuffer.h"

/* Function to check if we have codec_data field */
gboolean gst_mpeg4_valid_quicktime_header (GstBuffer *buf);

/* Function to get codec data field from demuxer */
GstBuffer * gst_mpeg4_get_header (GstBuffer *buf);

/* Function to prefix quicktime codec_data in gstreamer buffer  */
int gst_mpeg4_parse_and_queue (GstTICircBuffer *circBuf, GstBuffer *buf, 
        GstBuffer *mpeg4_header);

/* Function to check if we are using mpeg4 decoder */
gboolean gst_is_mpeg4_decoder (const gchar *name);

#endif /* __GST_TIQUICKTIME_MPEG4_H__ */


/******************************************************************************
 * Custom ViM Settings for editing this file
 ******************************************************************************/
#if 0
 Tabs (use 4 spaces for indentation)
 vim:set tabstop=4:      /* Use 4 spaces for tabs          */
 vim:set shiftwidth=4:   /* Use 4 spaces for >> operations */
 vim:set expandtab:      /* Expand tabs into white spaces  */
#endif

