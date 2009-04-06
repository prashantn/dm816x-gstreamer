/*
 * gstticommonutils.h
 *
 * This file declares common routine used by all elements. 
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

#ifndef __GST_TICOMMONUTILS_H__
#define __GST_TICOMMONUTILS_H__

#include <pthread.h>

#include <gst/gst.h>

#include <xdc/std.h>

#include <ti/sdo/dmai/Dmai.h>
#include <ti/sdo/dmai/Buffer.h>

/* This variable is used to flush the fifo.  It is pushed to the
 * fifo when we want to flush it.  When the encode/decode thread
 * receives the address of this variable the fifo is flushed and
 * the thread can exit.  The value of this variable is not used.
 */
extern int gst_ti_flush_fifo;

/* Function to calculate the display buffer size */
gint gst_ti_calculate_display_bufSize (Buffer_Handle hDstBuf);

/* Function to read environment variable and return its boolean value */
gboolean gst_ti_env_get_boolean (gchar *env);

/* Function to read environment variable and return string value */
gchar* gst_ti_env_get_string (gchar *env);

/* Function to read environment variable and return integer value */
gint gst_ti_env_get_int (gchar *env);

/* Function to check if the environment variable is defined */
gboolean gst_ti_env_is_defined (gchar *env);

#endif 

/******************************************************************************
 * Custom ViM Settings for editing this file
 ******************************************************************************/
#if 0
 Tabs (use 4 spaces for indentation)
 vim:set tabstop=4:      /* Use 4 spaces for tabs          */
 vim:set shiftwidth=4:   /* Use 4 spaces for >> operations */
 vim:set expandtab:      /* Expand tabs into white spaces  */
#endif

