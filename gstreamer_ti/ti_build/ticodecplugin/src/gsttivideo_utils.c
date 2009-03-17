/*
 * gsttivideo_utils.c
 *
 * This file implements common functions needed by video elements.
 *
 * Original Author:
 *     Brijesh Singh, Texas Instruments, Inc.
 *
 * Copyright (C) $year Texas Instruments Incorporated - http://www.ti.com/
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

#include <gst/gst.h>

#include <xdc/std.h>

#include <ti/sdo/dmai/Dmai.h>
#include <ti/sdo/dmai/Buffer.h>
#include <ti/sdo/dmai/BufferGfx.h>

/******************************************************************************
 * gst_calculate_display_bufSize 
 *    This function is used to calculate output buffer size.
 *    We need this routine for  ffmpegcolorspace or xvimagesink.
 *
 *    Downstream elements like "ffmpegcolorspace" verifies buffer size with
 *    resolution before performing  color conversion. 
 *****************************************************************************/
gint gst_calculate_display_bufSize (Buffer_Handle hDstBuf)
{
    BufferGfx_Dimensions    dim;

    BufferGfx_getDimensions(hDstBuf, &dim);

    /* If colorspace is YUV422 set the buffer size to width * 2 * height */
    if (BufferGfx_getColorSpace(hDstBuf) == ColorSpace_UYVY) {
        return dim.width * 2 * dim.height;
    }
   
    /* Return numBytesUsed values for other colorspace like 
     * YUV420PSEMI and YUV422PSEMI. 
     */
    return Buffer_getNumBytesUsed(hDstBuf);
}
 
