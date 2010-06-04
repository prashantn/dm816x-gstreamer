/*
 * gsttiquicktime_h264.c
 *
 * This file defines functions for :
 *  - converting quicktime packetized stream in NAL byte-stream format.
 *  - extracting SPS, PPS from H.264 stream needed to construct codec_data 
 *    field for encoder.
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

#include "gsttiquicktime_h264.h"
#include "gstticodecs.h"

/* NAL start code length (in byte) */
#define NAL_START_CODE_LENGTH 4

/* NAL start code */
static unsigned int NAL_START_CODE=0x1000000;

/* Local function declaration */
static int gst_h264_sps_pps_calBufSize(GstBuffer *codec_data);
static GstBuffer* gst_h264_get_avcc_header (GstBuffer *buf);

/******************************************************************************
 * gst_is_h264_decoder
 *****************************************************************************/
gboolean gst_is_h264_decoder (const gchar *name)
{
    GstTICodec *h264Codec;

    h264Codec  =  gst_ticodec_get_codec("H.264 Video Decoder");

    if (h264Codec && !strcmp(h264Codec->CE_CodecName, name)) {
        return TRUE;
    }

    return FALSE;
}

/******************************************************************************
 * gst_is_h264_encoder
 *****************************************************************************/
gboolean gst_is_h264_encoder (const gchar *name)
{
    GstTICodec *h264Codec;

    h264Codec  =  gst_ticodec_get_codec("H.264 Video Encoder");

    if (h264Codec && !strcmp(h264Codec->CE_CodecName, name)) {
        return TRUE;
    }

    return FALSE;
}

/******************************************************************************
 * gst_h264_get_sps_pps_data - This function returns SPS and PPS NAL unit
 * syntax by parsing the codec_data field. This is used to construct 
 * byte-stream NAL unit syntax.
 * Each byte-stream NAL unit syntax structure contains one start
 * code prefix of size four bytes and value 0x0000001, followed by one NAL
 * unit syntax.
 *****************************************************************************/
GstBuffer* gst_h264_get_sps_pps_data (GstBuffer *buf)
{
    int i, sps_pps_pos = 0, len;
    guint8 numSps, numPps;
    guint8 byte_pos;
    gint sps_pps_size = 0;
    GstBuffer *g_sps_pps_data = NULL, *codec_data = NULL;
    guint8 *sps_pps_data = NULL;

    /* Get codec_data from the cap */
    codec_data = gst_h264_get_avcc_header(buf);

    if (codec_data == NULL) {
        return NULL;
    }

    /* Get the total sps and pps length after adding NAL code prefix. */
    sps_pps_size = gst_h264_sps_pps_calBufSize(codec_data);

    /* Allocate sps_pps_data buffer */
    g_sps_pps_data =  gst_buffer_new_and_alloc(sps_pps_size);
    if (g_sps_pps_data == NULL) {
        GST_ERROR("Failed to allocate sps_pps_data buffer\n");
        return NULL;
    }
    sps_pps_data = GST_BUFFER_DATA(g_sps_pps_data);

    /* Set byte_pos to 5. Indicates sps byte location in avcC header. */
    byte_pos = 5;

    /* Copy sps unit dump. */
    numSps = AVCC_ATOM_GET_NUM_SPS(codec_data, byte_pos);
    byte_pos++;
    for (i=0; i < numSps; i++) {
        memcpy(sps_pps_data+sps_pps_pos, (unsigned char*) &NAL_START_CODE, 
            NAL_START_CODE_LENGTH);
        sps_pps_pos += NAL_START_CODE_LENGTH;
        len = AVCC_ATOM_GET_SPS_NAL_LENGTH(codec_data,byte_pos);
        GST_LOG("  - sps[%d]=%d\n", i, len);
        byte_pos +=2;

        memcpy(sps_pps_data+sps_pps_pos,
                GST_BUFFER_DATA(codec_data)+byte_pos, len);
        sps_pps_pos += len;
        byte_pos += len;
    }

    /* Copy pps unit dump. */
    numPps = AVCC_ATOM_GET_NUM_PPS(codec_data,byte_pos); 
    byte_pos++;
    for (i=0; i < numPps; i++) {
        memcpy(sps_pps_data+sps_pps_pos, (unsigned char*) &NAL_START_CODE,
                 NAL_START_CODE_LENGTH);
        sps_pps_pos += NAL_START_CODE_LENGTH;
        len = AVCC_ATOM_GET_PPS_NAL_LENGTH(codec_data,byte_pos);
        GST_LOG("  - pps[%d]=%d\n", i, len);
        byte_pos +=2;
        memcpy(sps_pps_data+sps_pps_pos,
                GST_BUFFER_DATA(codec_data)+byte_pos, len);
        sps_pps_pos += len;
        byte_pos += len;
    }

    return g_sps_pps_data;
}

/******************************************************************************
 * gst_h264_get_nal_code_prefix - This function return the pre-define NAL 
 * prefix code.
 *****************************************************************************/
GstBuffer* gst_h264_get_nal_prefix_code (void)
{
    GstBuffer *nal_code_prefix;

    nal_code_prefix = gst_buffer_new_and_alloc(NAL_START_CODE_LENGTH);
    if (nal_code_prefix == NULL) {
        GST_ERROR("Failed to allocate memory\n");
        return NULL;
    }

    memcpy(GST_BUFFER_DATA(nal_code_prefix), (unsigned char*) &NAL_START_CODE,
                NAL_START_CODE_LENGTH);

    return nal_code_prefix;

}

/******************************************************************************
 * gst_h264_parse_and_queue  - This function adds sps and pps header data
 * before pushing input buffer in queue.
 *
 * H264 in quicktime is what we call in gstreamer 'packtized' h264.
 * A codec_data is exchanged in the caps that contains, among other things,
 * the nal_length_size field and SPS, PPS.

 * The data consists of a nal_length_size header containing the length of
 * the NAL unit that immediatly follows the size header. 

 * Inserting the SPS,PPS (after prefixing them with nal prefix codes) and
 * exchanging the size header with nal prefix codes is a valid way to transform
 * a packetized stream into a byte stream.
 *****************************************************************************/
int gst_h264_parse_and_queue (GstTICircBuffer *circBuf, GstBuffer *buf, 
    GstBuffer *sps_pps_data, GstBuffer *nal_code_prefix, guint8 nal_length)
{
    int i, nal_size=0, avail = GST_BUFFER_SIZE(buf);
    GstBuffer *subBuf;
    guint8 *inBuf = GST_BUFFER_DATA(buf);
    int offset = 0;

    /* Put SPS and PPS data (prefixed with NAL code) in fifo */
    if (!gst_ticircbuffer_queue_data(circBuf, sps_pps_data)) {
        GST_ERROR("Failed to put SPS, PPS data in Fifo \n");
        return FALSE;
    }

    do {
        nal_size = 0;
        for (i=0; i < nal_length; i++) {
            nal_size = (nal_size << 8) | inBuf[i];
        }
        inBuf += nal_length;
        offset += nal_length;
            
        /* Put NAL prefix code in fifo */
        if (!gst_ticircbuffer_queue_data(circBuf, nal_code_prefix)) {
            GST_ERROR("Failed to put NAL code prefix\n");
            return FALSE;
        }

        /* Get a child buffer starting with offset and end at nal_size */
        subBuf = gst_buffer_create_sub(buf, offset, nal_size);
        if (subBuf == NULL) {
            GST_ERROR("Failed to get nal_size=%d buffer from adapater\n", 
                    nal_size);
            return FALSE;
        }

        /* Put nal_size data from input buffer in fifo */
        if (!gst_ticircbuffer_queue_data(circBuf, subBuf)) {
            GST_ERROR("Failed to put NAL code prefix\n");
            gst_buffer_unref(subBuf);
            return FALSE;
        }

        /* Unref the sub buffer */
        gst_buffer_unref(subBuf);

        offset += nal_size;
        inBuf += nal_size;
        avail -= (nal_size + nal_length);
    }while (avail > 0);

    return TRUE;
}

/******************************************************************************
 * gst_h264_get_nal_length - This function return the NAL length in avcC
 * header.
 *****************************************************************************/
guint8 gst_h264_get_nal_length (GstBuffer *buf)
{
    guint8 nal_length;

    /* Get codec_data */
    GstBuffer *codec_data = gst_h264_get_avcc_header(buf);

    /* Get nal length from avcC header */
    nal_length = AVCC_ATOM_GET_NAL_LENGTH(codec_data, 4);

    GST_LOG("NAL length=%d ",nal_length);
            
    return nal_length;
}

/******************************************************************************
 * gst_h264_get_avcc_header - This function gets codec_data field from the cap
 ******************************************************************************/
static GstBuffer * gst_h264_get_avcc_header (GstBuffer *buf)
{
    const GValue *value;
    GstStructure *capStruct;
    GstCaps      *caps = GST_BUFFER_CAPS(buf);
    GstBuffer    *codec_data = NULL;

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
 * gst_h264_valid_avcc_header - This function checks if codec_data has a
 * valid avcC header in h264 stream.
 * To do this, it reads codec_data field passed via demuxer and if the
 * codec_data buffer size is greater than 7, then we have a valid quicktime 
 * avcC atom header.
 *
 *      -: avcC atom header :-
 *  -----------------------------------
 *  1 byte  - version
 *  1 byte  - h.264 stream profile
 *  1 byte  - h.264 compatible profiles
 *  1 byte  - h.264 stream level
 *  6 bits  - reserved set to 63
 *  2 bits  - NAL length 
 *            ( 0 - 1 byte; 1 - 2 bytes; 3 - 4 bytes)
 *  3 bit   - reserved
 *  5 bits  - number of SPS 
 *  for (i=0; i < number of SPS; i++) {
 *      2 bytes - SPS length
 *      SPS length bytes - SPS NAL unit
 *  }
 *  1 byte  - number of PPS
 *  for (i=0; i < number of PPS; i++) {
 *      2 bytes - PPS length 
 *      PPS length bytes - PPS NAL unit 
 *  }
 * ------------------------------------------
 *****************************************************************************/
gboolean gst_h264_valid_quicktime_header (GstBuffer *buf)
{
    GstBuffer *codec_data;

    /* Read code_data field passed from the demuxer. */
    codec_data = gst_h264_get_avcc_header(buf);

    if (codec_data == NULL) {
        GST_LOG("demuxer does not have codec_data field\n");
        return FALSE;
    }

    /* Check the buffer size. */
    if (GST_BUFFER_SIZE(codec_data) < 7) {
        GST_LOG("codec_data field does not have a valid quicktime header\n");
        return FALSE;
    }

    /* print some debugging */
    GST_LOG("avcC version=%d, profile=%#x , level=%#x ",
            AVCC_ATOM_GET_VERSION(codec_data,0),
            AVCC_ATOM_GET_STREAM_PROFILE(codec_data,1),
            AVCC_ATOM_GET_STREAM_LEVEL(codec_data, 3));

    return TRUE;
}

/******************************************************************************
 * gst_h264_sps_pps_calBuffer_Size  - Function to calculate total buffer size
 * needed for copying SPS(Sequence parameter set) and PPS 
 * (Picture parameter set) data.
 *****************************************************************************/
static int gst_h264_sps_pps_calBufSize (GstBuffer *codec_data)
{
    int i, byte_pos, sps_nal_length=0, pps_nal_length=0;  
    int numSps=0, numPps=0, sps_pps_size=0;
   
    /* Set byte_pos to 5. Indicates sps byte location in avcC header. */
    byte_pos = 5; 

    /* Get number of SPS in avcC header */
    numSps = AVCC_ATOM_GET_NUM_SPS(codec_data,byte_pos);
    GST_LOG("sps=%d ", numSps);

    /* number of SPS is 1-byte long, increment byte_pos counter by 1 byte */
    byte_pos++;

    /* If number of SPS is non-zero, then copy NAL unit dump in buffer */
    if (numSps) {

        for (i=0; i < numSps; i++) {
            sps_nal_length = AVCC_ATOM_GET_SPS_NAL_LENGTH(codec_data, 
                                byte_pos);
            /* NAL length is 2-byte long, increment the byte_pos by NAL length
             * plus 2.
             */ 
            byte_pos = byte_pos + sps_nal_length + 2;

            /* add NAL start code prefix length in total sps_pps_size, this is 
             * because we need to add code prefix on every NAL unit. */
            sps_pps_size += sps_nal_length + NAL_START_CODE_LENGTH;
        }
    }

    /* Get the number of PPS in avcC header */
    numPps = AVCC_ATOM_GET_NUM_PPS(codec_data, byte_pos);
    GST_LOG("pps=%d \n", numPps);

    /* number of PPS is 1-byte long, increment byte_pos counter  by 1 byte */
    byte_pos++;


    /* If number of PPS is non-zero, then copy NAL unit dump in buffer */
    if (numPps) {

        for (i=0; i < numPps; i++) {
            pps_nal_length = AVCC_ATOM_GET_PPS_NAL_LENGTH(codec_data,
                                 byte_pos);
            /* NAL length is 2-byte long, increment the byte_pos by NAL length
             * plus 2.
             */ 
            byte_pos = byte_pos + pps_nal_length + 2;

            /* add NAL start code prefix length in total sps_pps_size, this is 
             * because we need to add code prefix on every NAL unit. */
            sps_pps_size += pps_nal_length + NAL_START_CODE_LENGTH;
        }
    }
    
    return sps_pps_size;
}

/******************************************************************************
 * gst_h264_find_next_nal_code
 *  Use Boyer-Moore string matching algorithm to find NAL start code.
 *****************************************************************************/
static guint gst_h264_find_next_nal_code (Int8 *data, gint size)
{
    guint offset = 3;
    unsigned int shift;
    
    while (offset < size) {
        if (1 == data[offset]) {
            shift = offset;
            if (0 == data[--shift]) {
                if (0 == data[--shift]) {
                    if (0 == data[--shift]) {
                        return shift;
                    }
                }
            } 
        offset += 4;
    } 
    else if (0 == data[offset]) {
        /* maybe next byte is 1? */
        offset++;
    } 
    else {
        /* can jump 4 bytes forward */
        offset += 4;
    }
  }

  GST_LOG ("Cannot find next NAL start code. returning %u\n", size);

  return size;
}

/******************************************************************************
 * gst_h264_create_sps_pps
 *  This function parses H.264 stream and returns sps and pps data.
 *
 *  Note: function does not prefix NAL code on SPS and PPS unit.
 *****************************************************************************/
static gboolean gst_h264_create_sps_pps (Buffer_Handle hBuf, GstBuffer **sps, 
    GstBuffer **pps)
{
    guint         next, nal_len, size;
    Int8          *data;
    guint8        type, header;

    data = Buffer_getUserPtr(hBuf);
    size = Buffer_getNumBytesUsed(hBuf);

    next = gst_h264_find_next_nal_code(data, size);
    
    data += next;
    size -= next;

    GST_LOG("Found first start at %u\n", next);

    while (size > 4) {
        data += 4;
        size -= 4;

        next = gst_h264_find_next_nal_code(data, size);
        nal_len = next;

        GST_LOG("Found next start at %u\n", next);

        header = data[0];
        type = header & 0x1f;

        if (type == 0x7) {
            *sps =  gst_buffer_new_and_alloc(nal_len);

            if (*sps == NULL) {
                GST_ERROR("Failed to allocate memory for sps buffer\n");
                return FALSE;
            }

            memcpy(GST_BUFFER_DATA(*sps), data, nal_len);
        }
        else if (type == 0x8) {
            *pps =  gst_buffer_new_and_alloc(nal_len);

            if (*pps == NULL) {
                GST_ERROR("Failed to allocate memory for pps buffer\n");
                return FALSE;
            }
            memcpy(GST_BUFFER_DATA(*pps), data, nal_len);
        }

        data += nal_len;
        size -= nal_len;
    }

    return TRUE;
}

/******************************************************************************
 * gst_h264_create_codec_data
 *****************************************************************************/
GstBuffer* gst_h264_create_codec_data (Buffer_Handle hBuf)
{
    GstBuffer *sps_data, *pps_data, *codec_data;
    guint8     *sps, *pps, *buf;
    guint      sps_len, pps_len, numSps, numPps, offset, size;

    sps_data = pps_data = codec_data = NULL;
    offset = 0;

    if (!gst_h264_create_sps_pps(hBuf, &sps_data, &pps_data)) {
        goto exit;
    }

    sps = GST_BUFFER_DATA(sps_data);
    pps = GST_BUFFER_DATA(pps_data);
    sps_len = GST_BUFFER_SIZE(sps_data);
    pps_len = GST_BUFFER_SIZE(pps_data);

    /* we have only one sps, pps */
    numSps = numPps = 1;

    /* Calculate the size for avcC atom */
    size = 5 + 1 + ((2 + sps_len) * numSps) + 1 + ((2 + pps_len) * numPps);
    
    /* allocate codec data buffer */
    codec_data = gst_buffer_new_and_alloc(size);

    if (codec_data == NULL)  {
        GST_ERROR("failed to allocate buffer size %d for codec_data", size);
        goto exit;
    }

    buf = GST_BUFFER_DATA(codec_data);

    /* Fill the avcC header */
    buf[offset++] = 0x01;
    buf[offset++] = sps[1];
    buf[offset++] = sps[2];
    buf[offset++] = sps[3];
    buf[offset++] = 0xff;
    buf[offset++] = 0xe1;
    buf[offset++] = (sps_len >> 8) & 0xff;
    buf[offset++] = (sps_len) & 0xff;

    memcpy(buf + offset, sps, sps_len);
    offset += sps_len;

    buf[offset++] = 0x1;
    buf[offset++] = (pps_len >> 8) & 0xff;
    buf[offset++] = (pps_len) & 0xff;

    memcpy(buf + offset, pps, pps_len);

exit:
    if (sps_data) {
        gst_buffer_unref(sps_data);
    }

    if (pps_data) {
        gst_buffer_unref(pps_data);
    }

    return codec_data;
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

