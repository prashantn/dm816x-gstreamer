/*
 * gstticommonutils.c
 *
 * This file implements common routine used by all elements.
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

#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <xdc/std.h>

#include <ti/sdo/dmai/Dmai.h>
#include <ti/sdo/dmai/Buffer.h>
#include <ti/sdo/dmai/BufferGfx.h>

#include <gst/gst.h>

#include "gsttidmaibuffertransport.h"

/* This variable is used to flush the fifo.  It is pushed to the
 * fifo when we want to flush it.  When the encode/decode thread
 * receives the address of this variable the fifo is flushed and
 * the thread can exit.  The value of this variable is not used.
 */
int gst_ti_flush_fifo = 0;

/* Declare variable used to categorize GST_LOG output */
GST_DEBUG_CATEGORY_STATIC(gst_ticommonutils_debug);
#define GST_CAT_DEFAULT gst_ticommonutils_debug

/******************************************************************************
 * gst_ti_commonutils_debug_init
 *****************************************************************************/
static void gst_ti_commonutils_debug_init(void)
{
    /* Initialize GST_LOG for this object */
    GST_DEBUG_CATEGORY_INIT(gst_ticommonutils_debug, "TICommonUtils", 0,
                                "TI plugin common utils");

}

/******************************************************************************
 * gst_ti_correct_display_bufSize 
 *    Function to calculate video output buffer size.
 *
 *    In some cases codec does not return the correct output buffer size. But
 *    downstream elements like "ffmpegcolorspace" expect the correct output
 *    buffer.  Return the corrected numBytesUsed by the buffer, if needed.
 *****************************************************************************/
gint gst_ti_correct_display_bufSize (Buffer_Handle hDstBuf)
{
    BufferGfx_Dimensions    dim;

    BufferGfx_getDimensions(hDstBuf, &dim);

    /* If colorspace is YUV422 set the buffer size to width * 2 * height */
    if (BufferGfx_getColorSpace(hDstBuf) == ColorSpace_UYVY) {
        return dim.width * 2 * dim.height;
    }
   
    /* Return numBytesUsed values for other colorspace like 
     * YUV420PSEMI and YUV422PSEMI because we may need to perform ccv opertion
     * on codec output data before display the video.
     */
    return Buffer_getNumBytesUsed(hDstBuf);
}


/******************************************************************************
 * gst_ti_calc_buffer_size
 *
 *     Calculate the size of a buffer based on its dimensions and colorspace.
 *     This function is based on the DMAI function called BufferGfx_calcSize,
 *     but it takes a video standard instead of dimensions.  You don't really
 *     need to know the video standard to calculate the size (some of which
 *     only vary in framerates), and in some cases the video standard is not
 *     known.
 *
 *     It would be good if this function could be added to DMAI and the current
 *     "BufferGfx_calcSize" function just calls it using the dimensions from
 *     the video standard.
 ******************************************************************************/
gint gst_ti_calc_buffer_size(gint width, gint height, gint bytesPerLine,
                             ColorSpace_Type colorSpace)
{
    gint bufSize;

    if (bytesPerLine == 0) {
        bytesPerLine = (gint)BufferGfx_calcLineLength(width, colorSpace);
        if (bytesPerLine < 0) {
            return -1;
        }
    }

    if (colorSpace == ColorSpace_YUV422PSEMI) {
        bufSize = (bytesPerLine * height) << 1;
    }
    else if (colorSpace == ColorSpace_YUV420PSEMI) {
        bufSize = (bytesPerLine * height * 3) >> 1;
    }
    else {
        bufSize = bytesPerLine * height;
    }

    return bufSize;
}

/******************************************************************************
 * gst_ti_get_env_boolean 
 *   Function will return environment boolean. 
 *****************************************************************************/
gboolean gst_ti_env_get_boolean (gchar *env)
{
    Char  *env_value;

    gst_ti_commonutils_debug_init();

    env_value = getenv(env);

    /* If string in set to TRUE then return TRUE else FALSE */
    if (env_value && !strcmp(env_value,"TRUE")) {
        return TRUE;  
    }
    else if (env_value && !strcmp(env_value,"FALSE")) {
        return FALSE;
    }
    else {
        GST_WARNING("Failed to get boolean value of env '%s'"  
                    " - setting FALSE\n", env);
        return FALSE;
    }
}     

/******************************************************************************
 * gst_ti_get_env_string 
 *   Function will return environment string. 
 *****************************************************************************/
gchar* gst_ti_env_get_string (gchar *env)
{
    Char  *env_value;

    gst_ti_commonutils_debug_init();
    
    env_value = getenv(env);
    
    if (env_value) {
        return env_value;
    }

    GST_WARNING("Failed to get value of env '%s' - setting NULL\n", env);    
    return NULL;
}
     
/******************************************************************************
 * gst_ti_get_env_int
 *   Function will return environment integer. 
 *****************************************************************************/
gint gst_ti_env_get_int (gchar *env)
{
    Char  *env_value;
    
    gst_ti_commonutils_debug_init();

    env_value = getenv(env);
    
    /* Covert string into interger */
    if (env_value) {
        return atoi(env_value);  
    }
    
    GST_WARNING("Failed to get int value of env '%s' - setting 0\n", env);    
    return 0;
}     

/******************************************************************************
 * gst_ti_env_is_defined
 *  Function will check if environment variable is defined. 
 *****************************************************************************/
gboolean gst_ti_env_is_defined (gchar *env)
{
    Char  *env_value;
    
    env_value = getenv(env);
    
    if (env_value) {
        return TRUE;
    }  
    
    return FALSE;
}

/******************************************************************************
 * gst_ti_src_convert_format
 *  Function to convert source byte format to time format.
 *****************************************************************************/
gboolean gst_ti_src_convert_format(GstFormat src_format, gint64 src_val,
    GstFormat dest_format, gint64 * dest_val, gint64 totalDuration,
    guint64 totalBytes)
{
    guint64 val;

    if (src_format == dest_format) {
        if (dest_val) {
            *dest_val = src_val;
        }
        return TRUE;
    }

    if (totalBytes == 0 || totalDuration == 0) {
        return FALSE;
    }

    /* convert based on the average bitrate so far */
    if (src_format == GST_FORMAT_BYTES && dest_format == GST_FORMAT_TIME) {
        val = gst_util_uint64_scale (src_val, totalDuration, totalBytes);
    }
    else if (src_format == GST_FORMAT_TIME && dest_format == GST_FORMAT_BYTES){
        val = gst_util_uint64_scale (src_val, totalBytes, totalDuration);
    }
    else {
        return FALSE;
    }

    if (dest_val) {
        *dest_val = (gint64) val;
    }

    return TRUE;
}

/******************************************************************************
 * gst_ti_query_srcpad
 *  Function implements quering playback position and duration in stream. 
 *****************************************************************************/
gboolean gst_ti_query_srcpad(GstPad * pad, GstQuery * query, 
    GstPad *sinkpad, gint64 totalDuration, guint64 totalBytes)
{
    gboolean    res     = FALSE;
    GstPad      *peer   = NULL;
    GstFormat   format;
    gint64      len_bytes, duration;
    gint64      pos_bytes, pos;

    GST_LOG("processing %s query", GST_QUERY_TYPE_NAME(query));
    
    switch (GST_QUERY_TYPE(query)) {
        
        case GST_QUERY_DURATION:
            /* Invoke the default query handler, if demuxer is present in
             * pipeline then we don't need to provide duration information.
             */ 
            if ((res = gst_pad_query_default(pad, query))) {
                break;
            }

            gst_query_parse_duration(query, &format, NULL);
            if (format != GST_FORMAT_TIME) {
                GST_LOG("query failed: can't handle format %s", 
                    gst_format_get_name (format));
                break;
            }

            /* Get the sinkpad information */
            peer = gst_pad_get_peer(sinkpad);
            if (peer == NULL) {
                break;
            }

            /* We have reached here then it means we playing elementry
             * pipeline. Query duration in byte format and convert 
             * in time format.
             */
            format = GST_FORMAT_BYTES;
            if (!gst_pad_query_duration(peer, &format, &len_bytes)) {
                GST_LOG("query failed: failed to get upstream length");
                break;
            }

            /* Convert byte to time format */
            res = gst_ti_src_convert_format(GST_FORMAT_BYTES, len_bytes,
                    GST_FORMAT_TIME, &duration, totalDuration, totalBytes);

            if (res) {
                /* Set the query duration */
                gst_query_set_duration(query, GST_FORMAT_TIME, duration);
                GST_LOG("duration estimate: %" GST_TIME_FORMAT, 
                            GST_TIME_ARGS (duration));
            }

            break;

        case GST_QUERY_POSITION:
            /* Invoke the default query handler, if demuxer is present in
             * pipeline then we don't need to provide position information.
             */ 
            if ((res = gst_pad_query_default(pad, query))) {
                break;
            }

            /* Get the current query format */
            gst_query_parse_position (query, &format, NULL);
            if (format != GST_FORMAT_TIME) {
                GST_LOG("query failed: can't handle format %s",
                    gst_format_get_name (format));
                break;
            }

            /* Get the sinkpad information */
            peer = gst_pad_get_peer(sinkpad);
            if (peer == NULL) {
                break;
            }

            /* We have reached here then it means we playing elementry
             * pipeline. Query duration in byte format and convert 
             * in time format.
             */
            format = GST_FORMAT_BYTES;
            if (!gst_pad_query_position (peer, &format, &pos_bytes)) {
                pos = totalDuration;
                res = TRUE;
            } 
            
            /* Convert byte to time format */
            res = gst_ti_src_convert_format(GST_FORMAT_BYTES, pos_bytes,
                        GST_FORMAT_TIME, &pos, totalDuration,
                        totalBytes);

            if (res) {
                /* Set the query position */
                gst_query_set_position(query, GST_FORMAT_TIME, pos);
                GST_LOG("current position: %" GST_TIME_FORMAT, 
                            GST_TIME_ARGS(pos));
            }

            break;

        default:
            gst_pad_query_default(pad, query);

            break;
    }

    if (peer) {
        gst_object_unref (peer);
    }

    return res;
}

/******************************************************************************
 * gst_ti_parse_newsegment
 *  This function parses newsegment event recieved from the upstream.
 *
 *  If recieved event format is in GST_FORMAT_BYTES then convert in
 *  GST_FORMAT_TIME.
 *****************************************************************************/
void gst_ti_parse_newsegment(GstEvent **event, GstSegment *segment, 
    gint64 *totalDuration, guint64 totalBytes)
{
    GstFormat   fmt;
    gboolean    update;
    gint64      start, stop, position;
    gint64      new_start = 0, new_stop = -1;
    gdouble     rate;

    /* Initialize debug category */
    gst_ti_commonutils_debug_init();

    gst_event_parse_new_segment(*event, &update, &rate, &fmt, &start, &stop, 
                                &position);

    /* Recieved event format in time format - we don't need to do anything */
    if (fmt == GST_FORMAT_TIME) {        
        GST_LOG("Got NEWSEGMENT event in GST_FORMAT_TIME (%" 
                GST_TIME_FORMAT " - %" GST_TIME_FORMAT ")", 
                GST_TIME_ARGS(start), GST_TIME_ARGS(stop));

        gst_segment_set_newsegment(segment, update, rate, fmt, start, 
                                    stop, position);
    }

    /* Recieved event format is byte format - we need convert in time format */
    else if (fmt == GST_FORMAT_BYTES) {
        GST_LOG("Got NEWSEGMENT event in GST_FORMAT_BYTES (%"
                G_GUINT64_FORMAT " - %" G_GUINT64_FORMAT ")", 
                start, stop);

        /* convert start position from byte format to time format */
        if (gst_ti_src_convert_format(GST_FORMAT_BYTES, start, GST_FORMAT_TIME, 
                &new_start, *totalDuration, totalBytes)) {
            /* convert stop position from byte format to time format */
            if (gst_ti_src_convert_format(GST_FORMAT_BYTES, stop, 
                GST_FORMAT_TIME, &new_stop, *totalDuration, totalBytes)) {
                        /* Do nothing */    
            }
        }

        /* unref the current event */
        gst_event_unref(*event);

        /* create new segment with new start and stop position */              
        *event = gst_event_new_new_segment(update, rate, GST_FORMAT_TIME, 
                    new_start, new_stop, new_start);

        gst_segment_set_newsegment(segment, update, rate, GST_FORMAT_TIME,
            new_start, new_stop, new_start);

        GST_LOG("Sending new NEWSEGMENT event, time %" 
                GST_TIME_FORMAT " - %" GST_TIME_FORMAT, 
                GST_TIME_ARGS(new_start), GST_TIME_ARGS(new_stop));

        /* re-initialize totalDuration to new_start */ 
        *totalDuration    = new_start;
    }

    return;
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

