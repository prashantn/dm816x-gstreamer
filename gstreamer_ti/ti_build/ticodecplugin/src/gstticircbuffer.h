/*
 * gstticircbuffer.h
 * 
 * This file declares the "TICircBuffer" buffer object, which implements a
 * circular buffer using a DMAI buffer object.
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

#ifndef __GST_CIRCBUFFER_H__
#define __GST_CIRCBUFFER_H__

#include <gst/gst.h>

#include <ti/sdo/dmai/Dmai.h>
#include <ti/sdo/dmai/Buffer.h>
#include <ti/sdo/dmai/BufferGfx.h>
#include <ti/sdo/dmai/Rendezvous.h>
#include <ti/sdo/dmai/Framecopy.h>

G_BEGIN_DECLS

typedef struct _GstTICircBuffer GstTICircBuffer;

/* Standard macros for manipulating transport objects */
#define GST_TYPE_TICIRCBUFFER (gst_ticircbuffer_get_type())
#define GST_IS_TICIRCBUFFER(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_TICIRCBUFFER))
#define GST_TICIRCBUFFER(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_TICIRCBUFFER, \
     GstTICircBuffer))
#define GST_TICIRCBUFFER_TIMESTAMP(obj)  (GST_TICIRCBUFFER(obj)->dataTimeStamp)
#define GST_TICIRCBUFFER_DURATION(obj)   (GST_TICIRCBUFFER(obj)->dataDuration)
#define GST_TICIRCBUFFER_WINDOWSIZE(obj) (GST_TICIRCBUFFER(obj)->windowSize)

#define gst_ticircbuffer_unref(buf) \
            gst_mini_object_unref(GST_MINI_OBJECT_CAST(buf))

/* _GstTICircBuffer object */
struct _GstTICircBuffer {

    /* Parent Class */
    GstMiniObject      mini_object;

    /* Circular Buffer */
    Buffer_Handle      hBuf;
    Int8              *readPtr;
    Int8              *writePtr;
    Int32              readAheadSize;
    gboolean           fixedBlockSize;
    gboolean           contiguousData;
    gboolean           consumerAborted;

    /* Timestamp Management */
    GstClockTime       dataTimeStamp;
    GstClockTime       dataDuration;

    /* Input Thresholds */
    Int32              windowSize;
    gboolean           drain;
    Int32              bytesNeeded;

    /* Blocking Conditions to Throttle I/O */
    Rendezvous_Handle  waitOnConsumer;
    Rendezvous_Handle  waitOnProducer;

    /* Debug / Stats */
    gboolean           displayBuffer;
    Int32              maxConsumed;

    /* Define user copy function */
    void               *userCopyData;
    gboolean          (*userCopy) (Int8 *dst, GstBuffer *src, void *data);
};

/* External function declarations */
GType            gst_ticircbuffer_get_type(void);
GstTICircBuffer* gst_ticircbuffer_new(Int32 windowSize, Int32 numWindows,
                     Bool fixedBlockSize);
gboolean         gst_ticircbuffer_queue_data(GstTICircBuffer *circBuf,
                     GstBuffer *buf);
gboolean         gst_ticircbuffer_data_consumed(GstTICircBuffer *circBuf,
                     GstBuffer* buf, Int32 bytesConsumed);
gboolean         gst_ticircbuffer_time_consumed(
                     GstTICircBuffer *circBuf, GstClockTime timeConsumed);
GstBuffer*       gst_ticircbuffer_get_data(GstTICircBuffer *circBuf);
void             gst_ticircbuffer_drain(GstTICircBuffer *circBuf,
                     gboolean status);
void             gst_ticircbuffer_set_display(GstTICircBuffer *circBuf,
                     gboolean disp);
void             gst_ticircbuffer_consumer_aborted(GstTICircBuffer *circBuf);
gboolean         gst_ticircbuffer_copy_config (GstTICircBuffer *circBuf,
                  Int (*userCopy) (Int8* dst, GstBuffer* src, void *data), 
                    void *data);

G_END_DECLS

#endif /* __GST_CIRCBUFFER_H__ */


/******************************************************************************
 * Custom ViM Settings for editing this file
 ******************************************************************************/
#if 0
 Tabs (use 4 spaces for indentation)
 vim:set tabstop=4:      /* Use 4 spaces for tabs          */
 vim:set shiftwidth=4:   /* Use 4 spaces for >> operations */
 vim:set expandtab:      /* Expand tabs into white spaces  */
#endif
