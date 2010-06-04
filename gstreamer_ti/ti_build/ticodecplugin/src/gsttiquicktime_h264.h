/*
 * gsttiquicktime_h264.h
 *
 * This file declares structure and macro used for creating byte-stream syntax
 * needed for decoding H.264 stream demuxed via qtdemuxer and creating
 * codec_data field for the H.264 encoder.
 *
 * Original Author:
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

#ifndef __GST_QUICKTIME_H264_H__
#define __GST_QUICKTIME_H264_H__

#include <gst/gst.h>
#include "gstticircbuffer.h"

/* Get version number from avcC atom  */
#define AVCC_ATOM_GET_VERSION(header,pos) \
    GST_BUFFER_DATA(header)[pos]

/* Get stream profile from avcC atom  */
#define AVCC_ATOM_GET_STREAM_PROFILE(header,pos) \
    GST_BUFFER_DATA(header)[pos]

/* Get compatible profiles from avcC atom */
#define AVCC_ATOM_GET_COMPATIBLE_PROFILES(header,pos) \
    GST_BUFFER_DATA(header)[pos]

/* Get stream level from avcC atom */
#define AVCC_ATOM_GET_STREAM_LEVEL(header,pos) \
    GST_BUFFER_DATA(header)[pos]

/* Get NAL length from avcC atom  */
#define AVCC_ATOM_GET_NAL_LENGTH(header,pos) \
    (GST_BUFFER_DATA(header)[pos] & 0x03) + 1

/* Get number of SPS from avcC atom */
#define AVCC_ATOM_GET_NUM_SPS(header,pos) \
    GST_BUFFER_DATA(header)[pos] & 0x1f

/* Get SPS length from avcC atom */
#define AVCC_ATOM_GET_SPS_NAL_LENGTH(header, pos) \
    GST_BUFFER_DATA(header)[pos] << 8 | GST_BUFFER_DATA(header)[pos+1]

#define AVCC_ATOM_GET_PPS_NAL_LENGTH(header, pos) \
    GST_BUFFER_DATA(header)[pos] << 8 | GST_BUFFER_DATA(header)[pos+1]

/* Get number of PPS from avcC atom */
#define AVCC_ATOM_GET_NUM_PPS(header,pos) \
    GST_BUFFER_DATA(header)[pos]

/* Get PPS length from avcC atom */
#define AVCC_ATOM_GET_PPS_LENGTH(header,pos) \
    GST_BUFFER_DATA(header)[pos] << 8 | GST_BUFFER_DATA(header)[pos+1]

/* Function to check if we have valid avcC header */
int gst_h264_valid_quicktime_header (GstBuffer *buf);

/* Function to read sps and pps data field from avcc header */
GstBuffer* gst_h264_get_sps_pps_data (GstBuffer *buf);

/* Function to read NAL length field from avcc header */
guint8 gst_h264_get_nal_length (GstBuffer *buf);

/* Function to get predefind NAL prefix code */
GstBuffer* gst_h264_get_nal_prefix_code (void);

/* Function to parse input stream and put in circular buffer */
int gst_h264_parse_and_queue (GstTICircBuffer *circBuf, GstBuffer *buf, 
    GstBuffer *sps_pps_data, GstBuffer *nal_code_prefix, guint8 nal_length );

/* Function to check if we are using h264 decoder */
gboolean gst_is_h264_decoder (const gchar *name);

/* Function to check if we are using h264 encoder */
gboolean gst_is_h264_encoder (const gchar *name);

/* Function to create codec_data (avcC atom) from h264 stream */
GstBuffer* gst_h264_create_codec_data(Buffer_Handle hBuf);

#endif /* __GST_TIQUICKTIME_H264_H__ */


/******************************************************************************
 * Custom ViM Settings for editing this file
 ******************************************************************************/
#if 0
 Tabs (use 4 spaces for indentation)
 vim:set tabstop=4:      /* Use 4 spaces for tabs          */
 vim:set shiftwidth=4:   /* Use 4 spaces for >> operations */
 vim:set expandtab:      /* Expand tabs into white spaces  */
#endif
