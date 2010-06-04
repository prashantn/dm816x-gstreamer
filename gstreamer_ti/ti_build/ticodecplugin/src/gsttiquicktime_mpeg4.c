/*
 * gsttiquicktime_mpeg4.c
 *
 * This file defines functions used for extracting codec_data field from 
 * demuxer header and also adding this header in front of every encoded frame.
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
 *
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <gst/gst.h>

#include <ti/sdo/dmai/Fifo.h>

#include "gsttiquicktime_mpeg4.h"
#include "gstticodecs.h"

/******************************************************************************
 * gst_is_mpeg4_decoder
 *****************************************************************************/
gboolean gst_is_mpeg4_decoder (const gchar *name)
{
    GstTICodec *mpeg4Codec;

    mpeg4Codec  =  gst_ticodec_get_codec("MPEG4 Video Decoder");

    if (mpeg4Codec && !strcmp(mpeg4Codec->CE_CodecName, name)) {
        return TRUE;
    }

    return FALSE;
}

/******************************************************************************
 * gst_mpeg4_parse_and_queue  - This function adds MPEG4 header before putting 
 * input in circular buffer queue.
 *****************************************************************************/
int gst_mpeg4_parse_and_queue (GstTICircBuffer *circBuf, GstBuffer *buf, 
        GstBuffer *mpeg4_header)
{
    /* Put quicktime MPEG4 header in queue */
    if (gst_ticircbuffer_queue_data(circBuf, mpeg4_header) < 0) {
        GST_ERROR("Failed to put MPEG4 header data in Fifo \n");
        return FALSE;
    }

    /* Put encoded buffer in queue */
    if (gst_ticircbuffer_queue_data(circBuf, buf)<0) {
        GST_ERROR("Failed to queue input buffer\n");
        return FALSE;
    }

    return TRUE;
}

/******************************************************************************
 * gst_mpeg4_get_header - This function gets codec_data field from 
 * the cap
 ******************************************************************************/
GstBuffer * gst_mpeg4_get_header (GstBuffer *buf)
{
    const GValue *value;
    GstStructure *capStruct;
    GstCaps      *caps = GST_BUFFER_CAPS(buf);
    GstBuffer    *codec_data = NULL;

    if (!caps) {
        return NULL;
    }

    capStruct = gst_caps_get_structure(caps,0);

    /* Check if codec_data field exist */
    if (!gst_structure_has_field(capStruct, "codec_data")) {
        return NULL;
    }

    /* Read extra data passed via demuxer. */
    value = gst_structure_get_value(capStruct, "codec_data");
    if (value < 0) {
        GST_ERROR("demuxer does not have codec_data field\n");
        return NULL;
    }

    codec_data = gst_value_get_buffer(value); 

    return codec_data;
}

/******************************************************************************
 * gst_mpeg4_valid_quicktime_header - This function checks if buffer has 
 * codec_data field. 
 *****************************************************************************/
gboolean gst_mpeg4_valid_quicktime_header (GstBuffer *buf)
{
    GstBuffer   *codec_data = gst_mpeg4_get_header(buf);

    if (codec_data == NULL) {
        GST_LOG("demuxer does not have codec_data field\n");
        return FALSE;
    }

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

