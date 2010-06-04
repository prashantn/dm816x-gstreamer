/*
 * gsttithreadprops.h
 *
 * This file manages thread properties for multi-threaded GStreamer elements
 * in the TI Codec plugin.
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

#ifndef __GST_TITHREADPROPS_H__
#define __GST_TITHREADPROPS_H__

#include <pthread.h>

G_BEGIN_DECLS

/* Thread priorities */
#define GstTIVideoThreadPriority sched_get_priority_max(SCHED_FIFO)
#define GstTIAudioThreadPriority sched_get_priority_max(SCHED_FIFO) - 1

/* Thread return values */
#define GstTIThreadSuccess (void*)0
#define GstTIThreadFailure (void*)-1

/* Thread status flags */
#define TIThread_CODEC_CREATED  0x00000001UL
#define TIThread_CODEC_ABORTED  0x80000004UL

/* Thread status utility macros. */
#define gst_tithread_lock_status(x) \
            pthread_mutex_lock(&(x)->threadStatusMutex)
#define gst_tithread_unlock_status(x) \
            pthread_mutex_unlock(&(x)->threadStatusMutex)
        
#define gst_tithread_set_status(x, y)      \
            (gst_tithread_lock_status(x),  \
             ((x)->threadStatus |= (y)),   \
             gst_tithread_unlock_status(x))

#define gst_tithread_check_status(x, y, z)               \
            (gst_tithread_lock_status(x),                \
             ((z) = (((x)->threadStatus & (y)) == (y))), \
             gst_tithread_unlock_status(x),              \
             (z))

#define gst_tithread_clear_status(x, y)    \
            (gst_tithread_lock_status(x),  \
             ((x)->threadStatus &= ~(y)),  \
             gst_tithread_unlock_status(x))

G_END_DECLS

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
