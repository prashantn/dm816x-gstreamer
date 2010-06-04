/*
 * gsttiquicktime_aac.c
 *
 * This file defines functions used to create temporary ADIF headers needed for
 * decoding aac streams demuxed via qtdemuxer.
 * 
 * For more information on ADIF header refer ISO/IEC 13818-7 Part7: Advanced
 * Audio Coding (AAC)
 *
 * Original Author:
 *     Brijesh Singh, Texas Instruments, Inc.
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

#include "gsttiquicktime_aac.h"

/* local function declaration */    
static guint gst_get_aac_rateIdx (guint rate);

/******************************************************************************
 * gst_aac_valid_header - This function checks if we have  valid ADIF or
 * ADTS header in input stream. 
 *****************************************************************************/
int gst_aac_valid_header (guint8 *data)
{
    if (data[0] == 'A' && data[1] == 'D' && data[2] == 'I'
         && data[3] == 'F') {
        return TRUE;
    }

    if ((data[0] == 0xff) && ((data[1] >> 4) == 0xf)) {
        return TRUE;
    }
    
    return FALSE;
}

/******************************************************************************
 *  gst_ADIF_header_create - This function creates ADIF header using 
 *  sampling frequency rate and number of channel passed during cap 
 *  negotiation.
 *  Note: The function is needed only when qtdemuxer is used to play quicktime 
 *  files (.mp4 or .mov).
 *****************************************************************************/
GstBuffer* gst_aac_header_create(guint rate, gint channels)
{
    GstBuffer *aac_header_buf;

    /* Allocate buffer to store AAC ADIF header */
    aac_header_buf = gst_buffer_new_and_alloc(MAX_AAC_HEADER_LENGTH);
    if (aac_header_buf == NULL) {
        GST_ERROR("Failed to allocate memory for aac header\n");
        return NULL;
    }

    memset(GST_BUFFER_DATA(aac_header_buf), 0, MAX_AAC_HEADER_LENGTH);

    /* Set adif_id field in ADIF header  - Always "ADIF"  (32-bit long) */
    ADIF_SET_ID(aac_header_buf, "ADIF");

    /* Disable copyright id  field in ADIF header - (1-bit long) */
    ADIF_CLEAR_COPYRIGHT_ID_PRESENT(aac_header_buf);

    /* Set profile field in ADIF header - (2-bit long)
     * 0 - MAIN, 1 - LC,  2 - SCR  3 - LTR (2-bit long) 
     */
    ADIF_SET_PROFILE(aac_header_buf, 0x1);

    /* Set sampling rate index field in ADIF header - (4-bit long) */ 
    ADIF_SET_SAMPLING_FREQUENCY_INDEX(aac_header_buf, 
                                    gst_get_aac_rateIdx(rate));

    /* Set front_channel_element field in ADIF header - (4-bit long) */
    ADIF_SET_FRONT_CHANNEL_ELEMENT(aac_header_buf, channels);
   
    /* Set comment field in ADIF header (8-bit long) */
    ADIF_SET_COMMENT_FIELD(aac_header_buf, 0x3);
    
    return aac_header_buf;
}

/******************************************************************************
 * gst_get_acc_rateIdx - This function calculate sampling index rate using
 * the lookup table defined in ISO/IEC 13818-7 Part7: Advanced Audio Coding.
 *****************************************************************************/
static guint gst_get_aac_rateIdx (guint rate)
{
    if (rate >= 96000)
        return 0;
    else if (rate >= 88200)
        return 1;
    else if (rate >= 64000)
        return 2;
    else if (rate >= 48000)
        return 3;
    else if (rate >= 44100)
        return 4;
    else if (rate >= 32000)
        return 5;
    else if (rate >= 24000)
        return 6;
    else if (rate >= 22050)
        return 7;
    else if (rate >= 16000)
        return 8;
    else if (rate >= 12000)
        return 9;
    else if (rate >= 11025)
        return 10;
    else if (rate >= 8000)
        return 11;
    else if (rate >= 7350)
        return 12;
    else
        return 15;              
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

