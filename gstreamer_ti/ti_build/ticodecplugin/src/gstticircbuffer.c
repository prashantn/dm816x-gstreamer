/*
 * gstticircbuffer.c
 * 
 * This file defines the "TICircBuffer" buffer object, which implements a
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
#include <string.h>
#include <stdlib.h>

#include <ti/sdo/dmai/Dmai.h>
#include <ti/sdo/dmai/Buffer.h>
#include <ti/sdo/dmai/BufferGfx.h>
#include <ti/sdo/dmai/Framecopy.h>

#include "gstticircbuffer.h"
#include "gsttidmaibuffertransport.h"

/* Declare variable used to categorize GST_LOG output */
GST_DEBUG_CATEGORY_STATIC(gst_ticircbuffer_debug);
#define GST_CAT_DEFAULT gst_ticircbuffer_debug

/* Declare a global pointer to our buffer base class */
static GstMiniObjectClass *parent_class;

/* Static Function Declarations */
static void      gst_ticircbuffer_init(GTypeInstance *instance,
                     gpointer g_class);
static void      gst_ticircbuffer_class_init(gpointer g_class,
                     gpointer class_data);
static void      gst_ticircbuffer_finalize(GstTICircBuffer* circBuf);
static void      gst_ticircbuffer_wait_on_producer(GstTICircBuffer *circBuf);
static void      gst_ticircbuffer_broadcast_producer(GstTICircBuffer *circBuf);
static void      gst_ticircbuffer_wait_on_consumer(GstTICircBuffer *circBuf,
                                                   Int32 bytesNeeded);
static void      gst_ticircbuffer_broadcast_consumer(GstTICircBuffer *circBuf);
static gboolean  gst_ticircbuffer_shift_data(GstTICircBuffer *circBuf);
static Int32     gst_ticircbuffer_reset_read_pointer(GstTICircBuffer *circBuf);
static gboolean  gst_ticircbuffer_window_available(GstTICircBuffer *circBuf);
static Int32     gst_ticircbuffer_data_available(GstTICircBuffer *circBuf);
static Int32     gst_ticircbuffer_data_size(GstTICircBuffer *circBuf);
static Int32     gst_ticircbuffer_write_space(GstTICircBuffer *circBuf);
static Int32     gst_ticircbuffer_is_empty(GstTICircBuffer *circBuf);
static void      gst_ticircbuffer_display(GstTICircBuffer *circBuf);

/* Useful macros */
#define gst_ticircbuffer_first_window_free(circBuf) \
            ((circBuf)->readPtr - Buffer_getUserPtr((circBuf)->hBuf) >= \
             ((circBuf)->windowSize + (circBuf)->readAheadSize))

/* Constants */
#define DISP_SIZE 77

/******************************************************************************
 * gst_ticircbuffer_get_type
 *    Defines function pointers for initialization routines for this object.
 ******************************************************************************/
GType gst_ticircbuffer_get_type(void)
{
    static GType object_type = 0;

    if (G_UNLIKELY(object_type == 0)) {
        static const GTypeInfo object_info = {
            sizeof(GstMiniObjectClass),
            NULL,
            NULL,
            gst_ticircbuffer_class_init,
            NULL,
            NULL,
            sizeof(GstTICircBuffer),
            0,
            gst_ticircbuffer_init,
            NULL
        };

        object_type =
            g_type_register_static(GST_TYPE_MINI_OBJECT,
                "GstTICircBuffer", &object_info, (GTypeFlags) 0);

        /* Initialize GST_LOG for this object */
        GST_DEBUG_CATEGORY_INIT(gst_ticircbuffer_debug,
                                "TICircBuffer", 0,
                                "TI DMAI Circular Buffer");

        GST_LOG("initialized get_type");
    }

    return object_type;
}


/******************************************************************************
 * gst_ticircbuffer_class_init
 *    Initializes the GstTICircBuffer class.
 ******************************************************************************/
static void gst_ticircbuffer_class_init(gpointer g_class,
                gpointer class_data)
{
    GstMiniObjectClass *mo_class = GST_MINI_OBJECT_CLASS(g_class);

    GST_LOG("begin class_init");
    parent_class = (GstMiniObjectClass*)g_type_class_peek_parent(g_class);

    mo_class->finalize =
        (GstMiniObjectFinalizeFunction) gst_ticircbuffer_finalize;

    GST_LOG("end class_init");
}


/******************************************************************************
 * gst_ticircbuffer_finalize
 ******************************************************************************/
static void gst_ticircbuffer_finalize(GstTICircBuffer* circBuf)
{
    if (circBuf == NULL) {
        return;
    }

    GST_LOG("Maximum bytes consumed:  %lu\n", circBuf->maxConsumed);

    if (circBuf->hBuf) {
        Buffer_delete(circBuf->hBuf);
    }

    if (circBuf->waitOnProducer) {
        Rendezvous_delete(circBuf->waitOnProducer);
    }

    if (circBuf->waitOnConsumer) {
        Rendezvous_delete(circBuf->waitOnConsumer);
    }
}

/******************************************************************************
 * gst_ticircbuffer_init
 *    Initializes a new transport buffer instance.
 ******************************************************************************/
static void gst_ticircbuffer_init(GTypeInstance *instance,
                gpointer g_class)
{
    GstTICircBuffer   *circBuf  = GST_TICIRCBUFFER(instance);
    Rendezvous_Attrs   rzvAttrs = Rendezvous_Attrs_DEFAULT;

    GST_LOG("begin init");

    circBuf->hBuf            = NULL;
    circBuf->readPtr         = NULL;
    circBuf->writePtr        = NULL;
    circBuf->dataTimeStamp   = 0ULL;
    circBuf->dataDuration    = 0ULL;
    circBuf->windowSize      = 0UL;
    circBuf->readAheadSize   = 0UL;
    circBuf->waitOnProducer  = Rendezvous_create(100, &rzvAttrs);
    circBuf->waitOnConsumer  = Rendezvous_create(100, &rzvAttrs);
    circBuf->drain           = FALSE;
    circBuf->bytesNeeded     = 0UL;
    circBuf->maxConsumed     = 0UL;
    circBuf->displayBuffer   = FALSE;
    circBuf->contiguousData  = TRUE;
    circBuf->fixedBlockSize  = FALSE;
    circBuf->consumerAborted = FALSE;
    circBuf->userCopy       = NULL;

    GST_LOG("end init");
}


/******************************************************************************
 * gst_ticircbuffer_new
 *     Create a circular buffer to store an encoded input stream.  Increasing
 *     the number of windows stored in the buffer can help performance if
 *     adequate memory is available.
 ******************************************************************************/
GstTICircBuffer* gst_ticircbuffer_new(Int32 windowSize, Int32 numWindows,
                     Bool fixedBlockSize)
{
    GstTICircBuffer *circBuf;
    Buffer_Attrs     bAttrs  = Buffer_Attrs_DEFAULT;
    Int32            bufSize;

    circBuf = (GstTICircBuffer*)gst_mini_object_new(GST_TYPE_TICIRCBUFFER);

    g_return_val_if_fail(circBuf != NULL, NULL);

    GST_INFO("requested windowSize:  %ld\n", windowSize);
    circBuf->windowSize = windowSize;

    GST_INFO("fixed block size is %s\n", fixedBlockSize ? "ON" : "OFF");
    circBuf->fixedBlockSize = fixedBlockSize;

    /* We need to have at least 2 windows allocated for us to be able
     * to copy buffer data while the consumer is running.
     */
    if (numWindows < 3 && circBuf->fixedBlockSize != TRUE) {
        GST_ERROR("numWindows must be at least 3 when fixedBlockSize=FALSE");
        return NULL;
    }

    /* Set the read ahead size to be 1/4 of a window */
    circBuf->readAheadSize = (fixedBlockSize) ? 0 : windowSize >> 2;

    /* Allocate the circular buffer */
    bufSize = (numWindows * windowSize) + (circBuf->readAheadSize << 1);

    GST_LOG("creating circular input buffer of size %lu\n", bufSize);
    circBuf->hBuf = Buffer_create(bufSize, &bAttrs);

    if (circBuf->hBuf == NULL) {
        GST_ERROR("failed to create buffer");
        gst_object_unref(circBuf);
        return NULL;
    }

    circBuf->readPtr = circBuf->writePtr = Buffer_getUserPtr(circBuf->hBuf);

    return circBuf;
}

/******************************************************************************
 * gst_ticircbuffer_copy_config
 *  This function configures circular buffer to use user defined copy routine.
 *  Args:
 *     @circBuf  -  circular buffer object
 *     @fxn      -  function pointer
 *           {
 *             Int8*     - destination buffer pointer
 *            GstBuffer* - src gstreamer buffer pointer
 *              void*    - function argument.
 *            } 
 *     void*      -  function argument 
 *****************************************************************************/
gboolean gst_ticircbuffer_copy_config (GstTICircBuffer *circBuf, 
    Int (*fxn) (Int8* src, GstBuffer *dst, void *args), void *data)
{
    circBuf->userCopyData = data;
    circBuf->userCopy = fxn;

    return TRUE;    
}

/******************************************************************************
 * gst_ticircbuffer_queue_data
 *     Append received encoded data to end of circular buffer
 ******************************************************************************/
gboolean gst_ticircbuffer_queue_data(GstTICircBuffer *circBuf, GstBuffer *buf)
{
    gboolean result = TRUE;
    Int32    writeSpace;

    /* If the circular buffer doesn't exist, do nothing */
    if (circBuf == NULL) {
        goto exit_fail;
    }

    /* Reset our mutex condition so a call to wait_on_consumer will block */
    Rendezvous_reset(circBuf->waitOnConsumer);

    /* If the consumer aborted, abort the buffer queuing.  We don't want to
     * queue buffers that no one will read.
     */
    if (circBuf->consumerAborted) {
        goto exit_fail;
    }

    /* If we run out of space, we need to move the data from the last buffer
     * window to the first window and continue queuing new data in the second
     * window.  If the consumer isn't done with the first window yet, we need
     * to block until it is.
     */
    while ((writeSpace = gst_ticircbuffer_write_space(circBuf)) <
            GST_BUFFER_SIZE(buf)) {

        /* If the write pointer is ahead of the read pointer, check to see if
         * the first window is free.  If it is, we may be able to shift the
         * data without blocking.
         */
        if (circBuf->contiguousData &&
            gst_ticircbuffer_first_window_free(circBuf)) {

            if (gst_ticircbuffer_shift_data(circBuf)) {
                continue;
            }
        }

        /* If there is some space available in the circular buffer, process as
         * much of the input buffer as we can before we block.  The ability to
         * do this is critical for both encode and decode operations.  For
         * encode, this guarantees that we will always provide a full window
         * to encode and never starve the codec.  For decode, this guarantees
         * the write pointer will always reach the last window of the circular
         * buffer before blocking, which is critical for the write pointer to
         * be reset properly.
         */
        if (writeSpace > 0) {

            GstBuffer* subBuf;
            gboolean   tmpResult;

            GST_LOG("splitting input buffer of size %u into two pieces of "
                "sizes %lu and %lu\n",  GST_BUFFER_SIZE(buf), writeSpace,
                GST_BUFFER_SIZE(buf) - writeSpace);

            subBuf = gst_buffer_create_sub(buf, 0, writeSpace);
            tmpResult = gst_ticircbuffer_queue_data(circBuf, subBuf);
            gst_buffer_unref(subBuf);

            if (!tmpResult) { goto exit_fail; }

            subBuf = gst_buffer_create_sub(buf, writeSpace,
                         GST_BUFFER_SIZE(buf) - writeSpace);
            tmpResult = gst_ticircbuffer_queue_data(circBuf, subBuf);
            gst_buffer_unref(subBuf);

            if (!tmpResult) { goto exit_fail; }

            goto exit;
        }

        /* Block until either the first window is free, or there is enough
         * free space available to put our buffer.
         */
        GST_LOG("blocking input until processing thread catches up\n");
        gst_ticircbuffer_wait_on_consumer(circBuf, GST_BUFFER_SIZE(buf));
        GST_LOG("unblocking input\n");

        /* Reset our mutex condition so calling wait_on_consumer will block */
        Rendezvous_reset(circBuf->waitOnConsumer);

        /* If the consumer aborted, abort the buffer queuing.  We don't want to
         * queue buffers that no one will read.
         */
        if (circBuf->consumerAborted) {
            goto exit_fail;
        }

        gst_ticircbuffer_shift_data(circBuf);
    }

    /* Log the buffer timestamp if available */
    if (GST_CLOCK_TIME_IS_VALID(GST_BUFFER_TIMESTAMP(buf))) {
        GST_LOG("buffer received:  timestamp:  %llu, duration:  "
            "%llu\n", GST_BUFFER_TIMESTAMP(buf), GST_BUFFER_DURATION(buf));
    }
    else {
        GST_LOG("buffer received:  no timestamp available\n");
    }

    /* Copy the buffer using user defined function */
    if (circBuf->userCopy) {
        GST_LOG("copying input buffer using user provided copy fxn\n");
        if (circBuf->userCopy(circBuf->writePtr, buf, 
              circBuf->userCopyData) < 0) {
            GST_ERROR("failed to copy input buffer.\n");
            goto exit_fail;
        }
    }
    else {        
        memcpy(circBuf->writePtr, GST_BUFFER_DATA(buf), GST_BUFFER_SIZE(buf));
    }
    circBuf->writePtr += GST_BUFFER_SIZE(buf);

    /* Copy new data to the end of the buffer */
    GST_LOG("queued %u bytes of data\n", GST_BUFFER_SIZE(buf));

    /* Output the buffer status to stdout if buffer debug is enabled */
    if (circBuf->displayBuffer) {
        gst_ticircbuffer_display(circBuf);
    }

    /* If the upstream elements are providing time information, update the
     * duration of time stored by the encoded data.
     */
    if (!GST_CLOCK_TIME_IS_VALID(GST_BUFFER_DURATION(buf))) {
        circBuf->dataDuration = GST_CLOCK_TIME_NONE;
    }
    else if (GST_CLOCK_TIME_IS_VALID(circBuf->dataDuration)) {
        circBuf->dataDuration += GST_BUFFER_DURATION(buf);
    }


    /* If our buffer got low, some consuming threads may have blocked waiting
     * for more data.  If there is at least a window and our specified read
     * ahead available in the buffer, unblock any threads.
     */
    if (gst_ticircbuffer_data_size(circBuf) >=
        circBuf->windowSize + circBuf->readAheadSize) {
        gst_ticircbuffer_broadcast_producer(circBuf);
    }

    goto exit;

exit_fail:
    result = FALSE;
exit:
    return result;
}


/******************************************************************************
 * gst_ticircbuffer_data_consumed
 ******************************************************************************/
gboolean gst_ticircbuffer_data_consumed(
             GstTICircBuffer *circBuf, GstBuffer *buf, Int32 bytesConsumed)
{
    if (circBuf == NULL) {
        return FALSE;
    }

    /* Make sure the client didn't consume more data than we gave out */
    if (GST_BUFFER_SIZE(buf) < bytesConsumed) {
        GST_WARNING("%ld bytes reported consumed from the circular buffer "
            "when only %d were available", bytesConsumed,
            GST_BUFFER_SIZE(buf));
        bytesConsumed = GST_BUFFER_SIZE(buf);
    }

    /* In fixedBlockSize mode, ensure that we always consume exactly one
     *  window.
     */
    if (circBuf->fixedBlockSize && bytesConsumed != GST_BUFFER_SIZE(buf)) {
        GST_ERROR("when circular buffer is created with fixedBlockSize=TRUE "
           "consumers must always consume an entire window at a time");
        return FALSE;
    }

    /* Release the reference buffer */
    gst_buffer_unref(buf);

    /* Update the read pointer */
    GST_LOG("%ld bytes consumed\n", bytesConsumed);
    circBuf->readPtr  += bytesConsumed;

    /* Update the max bytes consumed statistic */
    if (bytesConsumed > circBuf->maxConsumed) {
        circBuf->maxConsumed = bytesConsumed;
    }

    /* Output the buffer status to stdout if buffer debug is enabled */
    if (circBuf->displayBuffer) {
        gst_ticircbuffer_display(circBuf);
    }

    /* Unblock the input buffer queue if there is room for more buffers. */
    gst_ticircbuffer_broadcast_consumer(circBuf);

    return TRUE;
}


/*****************************************************************************
 * gst_ticircbuffer_time_consumed
 *****************************************************************************/
gboolean gst_ticircbuffer_time_consumed(GstTICircBuffer *circBuf,
             GstClockTime timeConsumed)
{
    if (circBuf == NULL) {
        return FALSE;
    }

    if (!GST_CLOCK_TIME_IS_VALID(timeConsumed)) {
        circBuf->dataDuration = GST_CLOCK_TIME_NONE;
    }
    else {
        circBuf->dataTimeStamp += timeConsumed;
    }

    if (GST_CLOCK_TIME_IS_VALID(circBuf->dataDuration)) {
        circBuf->dataDuration  -= timeConsumed;
    }

    return TRUE;
}


/******************************************************************************
 * gst_ticircbuffer_get_data
 ******************************************************************************/
GstBuffer* gst_ticircbuffer_get_data(GstTICircBuffer *circBuf)
{
    Buffer_Handle  hCircBufWindow;
    Buffer_Attrs   bAttrs;
    GstBuffer     *result;
    Int32          bufSize;

    if (circBuf == NULL) {
        return NULL;
    }

    /* Reset our mutex condition so calling wait_on_consumer will block */
    Rendezvous_reset(circBuf->waitOnProducer);

    /* Reset the read pointer to the beginning of the buffer when we're
     * approaching the buffer's end (see function definition for reset
     * conditions).
     */
    gst_ticircbuffer_reset_read_pointer(circBuf);

    /* Don't return any data util we have a full window available */
    while (!circBuf->drain && !gst_ticircbuffer_window_available(circBuf)) {

        GST_LOG("blocking output until a full window is available\n");
        gst_ticircbuffer_wait_on_producer(circBuf);
        GST_LOG("unblocking output\n");
        gst_ticircbuffer_reset_read_pointer(circBuf);

        /* Reset our mutex condition so calling wait_on_consumer will block */
        Rendezvous_reset(circBuf->waitOnProducer);
    }

    /* Set the size of the buffer to be no larger than the window size.  Some
     * audio codecs have an issue when you pass a buffer larger than 64K.
     * We need to pass it smaller buffer sizes though, as the EOS is detected
     * when we return a 0 size buffer.
     */
    bufSize = gst_ticircbuffer_data_available(circBuf);
    if (bufSize > circBuf->windowSize) {
        bufSize = circBuf->windowSize;
    }

    /* Return a reference buffer that points to the area of the circular
     * buffer we want to decode.
     */
    Buffer_getAttrs(circBuf->hBuf, &bAttrs);
    bAttrs.reference = TRUE;

    hCircBufWindow = Buffer_create(bufSize, &bAttrs);

    Buffer_setUserPtr(hCircBufWindow, circBuf->readPtr);
    Buffer_setNumBytesUsed(hCircBufWindow, bufSize);

    GST_LOG("returning data at offset %u\n", circBuf->readPtr - 
        Buffer_getUserPtr(circBuf->hBuf));

    result = (GstBuffer*)(gst_tidmaibuffertransport_new(hCircBufWindow, NULL));
    GST_BUFFER_TIMESTAMP(result) = circBuf->dataTimeStamp;
    GST_BUFFER_DURATION(result)  = GST_CLOCK_TIME_NONE;
    return result;
}


/******************************************************************************
 * gst_ticircbuffer_wait_on_producer
 *    Wait for a producer to process data
 ******************************************************************************/
static void gst_ticircbuffer_wait_on_producer(GstTICircBuffer *circBuf)
{
    Rendezvous_meet(circBuf->waitOnProducer);
}


/******************************************************************************
 * gst_ticircbuffer_broadcast_producer
 *    Broadcast when producer has processed some data
 ******************************************************************************/
static void gst_ticircbuffer_broadcast_producer(GstTICircBuffer *circBuf)
{
    GST_LOG("broadcast_producer: output unblocked\n");
    Rendezvous_force(circBuf->waitOnProducer);
}


/******************************************************************************
 * gst_ticircbuffer_wait_on_consumer
 *    Wait for a consumer to process data
 ******************************************************************************/
static void gst_ticircbuffer_wait_on_consumer(GstTICircBuffer *circBuf,
                                              Int32 bytesNeeded)
{
    circBuf->bytesNeeded = bytesNeeded;
    Rendezvous_meet(circBuf->waitOnConsumer);
}

/******************************************************************************
 * gst_ticircbuffer_broadcast_consumer
 *    Broadcast when consumer has processed some data
 ******************************************************************************/
static void gst_ticircbuffer_broadcast_consumer(GstTICircBuffer *circBuf)
{
    gboolean canUnblock = FALSE;

    /* If the write pointer is at the end of the buffer and the first window
     * is free, unblock so the queue thread can shift data to the beginning
     * and continue.
     */
    if (circBuf->contiguousData &&
        gst_ticircbuffer_first_window_free(circBuf)) {
            canUnblock = TRUE;
    }

    /* Otherwise, we can unblock if there is now enough space to queue the
     * next input buffer.
     */
    else if (gst_ticircbuffer_write_space(circBuf) >= circBuf->bytesNeeded) {
        canUnblock = TRUE;
    }

    if (canUnblock) {
        GST_LOG("broadcast_consumer: input unblocked\n");
        Rendezvous_force(circBuf->waitOnConsumer);
    }
}


/******************************************************************************
 * gst_ticircbuffer_consumer_aborted
 *    Consumer aborted - no longer block waiting on the consumer, and throw
 *    away all input buffers.
 ******************************************************************************/
void gst_ticircbuffer_consumer_aborted(GstTICircBuffer *circBuf)
{
    if (circBuf == NULL) {
        return;
    }

    circBuf->consumerAborted = TRUE;
    Rendezvous_force(circBuf->waitOnConsumer);
}


/******************************************************************************
 * gst_ticircbuffer_drain
 *    When set to TRUE, we no longer block waiting for a window -- all data
 *    gets consumed.
 ******************************************************************************/
void gst_ticircbuffer_drain(GstTICircBuffer *circBuf, gboolean status)
{
    if (circBuf == NULL) {
        return;
    }

    circBuf->drain = status;

    if (status == TRUE) {
        gst_ticircbuffer_broadcast_producer(circBuf);
    }
}

/******************************************************************************
 * gst_ticircbuffer_shift_data
 *    Look for uncopied data in the last window and move it to the first one.
 ******************************************************************************/
static gboolean gst_ticircbuffer_shift_data(GstTICircBuffer *circBuf)
{
    Int8*     firstWindow   = Buffer_getUserPtr(circBuf->hBuf);
    Int32     lastWinOffset = Buffer_getSize(circBuf->hBuf) -
                              (circBuf->windowSize + circBuf->readAheadSize);
    Int8*     lastWindow    = firstWindow + lastWinOffset;
    Int32     bytesToCopy   = 0;
    gboolean  writePtrReset = FALSE;

    /* In fixedBlockSize mode, just wait until the write poitner reaches the
     * end of the buffer and then reset it to the beginning (no copying).
     */
    if (circBuf->fixedBlockSize && circBuf->writePtr >= circBuf->readPtr) {
        if (circBuf->writePtr == firstWindow + Buffer_getSize(circBuf->hBuf)) {
            GST_LOG("resetting write pointer (%lu->0)\n",
                (UInt32)(circBuf->writePtr - firstWindow));
            circBuf->writePtr       = Buffer_getUserPtr(circBuf->hBuf);
            circBuf->contiguousData = FALSE;
        }
        return TRUE;
    }

    /* Otherwise copy unconsumed data from the last window to the first one
     * and reset the write pointer back to the first window.
     */
    if (gst_ticircbuffer_first_window_free(circBuf) &&
        circBuf->writePtr >= circBuf->readPtr       &&
        circBuf->writePtr >= lastWindow + circBuf->windowSize)
    {

        bytesToCopy = circBuf->writePtr - lastWindow;

        GST_LOG("shifting %lu bytes of data from %lu to 0\n", bytesToCopy,
            (UInt32)(lastWindow - firstWindow));

        if (bytesToCopy > 0) {
            memcpy(firstWindow, lastWindow, bytesToCopy);
        }

        GST_LOG("resetting write pointer (%lu->%lu)\n",
            (UInt32)(circBuf->writePtr - firstWindow),
            (UInt32)(circBuf->writePtr - (lastWindow - firstWindow) -
                     firstWindow));
        circBuf->writePtr       -= (lastWindow - firstWindow);
        circBuf->contiguousData  = FALSE;
        writePtrReset            = TRUE;

        /* The queue function will not unblock the consumer until there is
         * at least windowSize + readAhead available, but if the read pointer
         * is toward the end of the buffer, we may never get more than just
         * windowSize available and will deadlock.  To avoid this situation,
         * wake the consumer after shifting data so it has an opportunity to
         * process the last window in the buffer and reset itself to the
         * beginning of the buffer.
         */
        gst_ticircbuffer_broadcast_producer(circBuf);
    }
    return writePtrReset;
}


/******************************************************************************
 * gst_ticircbuffer_reset_read_pointer
 *    Reset the read pointer back to the beginning of the buffer.
 ******************************************************************************/
static Int32 gst_ticircbuffer_reset_read_pointer(GstTICircBuffer *circBuf)
{
    Int8  *circBufStart  = Buffer_getUserPtr(circBuf->hBuf);
    Int32  lastWinOffset = Buffer_getSize(circBuf->hBuf) -
                            (circBuf->windowSize + circBuf->readAheadSize);
    Int8  *lastWindow    = circBufStart + lastWinOffset;
    Int32  resetDelta    = lastWindow - circBufStart;

    /* In fixedBlockSize mode, just wait until the read poitner reaches the
     * end of the buffer and then reset it to the beginning.
     */
    if (circBuf->fixedBlockSize && !circBuf->contiguousData) {
        if (circBuf->readPtr == circBufStart + Buffer_getSize(circBuf->hBuf)) {
            GST_LOG("resetting read pointer (%lu->0)\n",
                (UInt32)(circBuf->readPtr - circBufStart));
            circBuf->readPtr        = Buffer_getUserPtr(circBuf->hBuf);
            circBuf->contiguousData = TRUE;
        }
        return 0;
    }

    /* Otherwise, reset it when the read pointer reaches the last window and
     * the last window has already been copied back to the first window.
     */
    if (!circBuf->contiguousData                           &&
         circBuf->readPtr              >  lastWindow       &&
         circBuf->readPtr - resetDelta <= circBuf->writePtr) {

        GST_LOG("resetting read pointer (%lu->%lu)\n",
            (UInt32)(circBuf->readPtr - circBufStart),
            (UInt32)(circBuf->readPtr - resetDelta - circBufStart));
        circBuf->readPtr        -= resetDelta;
        circBuf->contiguousData  = TRUE;
        return TRUE;
    }

    return FALSE;
}


/******************************************************************************
 * gst_ticircbuffer_window_available
 *    Determine if there is enough data in the buffer to satisfy the consumer
 ******************************************************************************/
static gboolean gst_ticircbuffer_window_available(GstTICircBuffer *circBuf)
{
    gboolean result;

    /* Otherwise, return TRUE if the data available satisifies the specified
     * window size.
     */
    result = gst_ticircbuffer_data_available(circBuf) >= circBuf->windowSize;

    GST_LOG("data is %savailable: %lu %s %lu\n",
        (result) ? ""   : "not ", gst_ticircbuffer_data_available(circBuf),
        (result) ? ">=" : "<",    circBuf->windowSize);

    return result;
}


/******************************************************************************
 * gst_ticircbuffer_data_available
 *    Return how much contiguous data is available at the read pointer.
 ******************************************************************************/
static Int32 gst_ticircbuffer_data_available(GstTICircBuffer *circBuf)
{
    /* First check if the buffer is empty, in which case return 0. */
    if (gst_ticircbuffer_is_empty(circBuf)) {
        return 0;
    }

    /* If the write pointer is now ahead of the read pointer, make sure there
     * is a window available.
     */
    if (circBuf->contiguousData) {
        return (circBuf->writePtr - circBuf->readPtr);
    }

    /* Otherwise, there needs to be enough data between the read pointer and
     * the end of the buffer.
     */
    else {
       return (Buffer_getUserPtr(circBuf->hBuf) +
               Buffer_getSize(circBuf->hBuf)) - circBuf->readPtr;
    }

    return 0;
}


/******************************************************************************
 * gst_ticircbuffer_data_size
 *    Return the total amount of data currently in the buffer (may not all be
 *    contiguous in memory).
 ******************************************************************************/
static Int32 gst_ticircbuffer_data_size(GstTICircBuffer *circBuf)
{
    if (gst_ticircbuffer_is_empty(circBuf)) {
        return 0;
    }

    return (circBuf->contiguousData) ?
        (circBuf->writePtr - circBuf->readPtr) :
        Buffer_getSize(circBuf->hBuf) - (circBuf->readPtr - circBuf->writePtr);
}


/******************************************************************************
 * gst_ticircbuffer_write_space
 *    Return the free space available in the buffer for CONTIGUOUS WRITING at
 *    the writePtr location.
 ******************************************************************************/
static Int32 gst_ticircbuffer_write_space(GstTICircBuffer *circBuf)
{
    if (circBuf->contiguousData) {
        return (Buffer_getUserPtr(circBuf->hBuf) +
                Buffer_getSize(circBuf->hBuf)) - circBuf->writePtr;
    }

    return circBuf->readPtr - circBuf->writePtr;
}


/******************************************************************************
 * gst_ticircbuffer_set_display
 *     Enable or disable the output of the visual buffer representation.
 ******************************************************************************/
void gst_ticircbuffer_set_display(GstTICircBuffer *circBuf, gboolean disp)
{
    circBuf->displayBuffer = disp;
}


/******************************************************************************
 * gst_ticircbuffer_is_empty
 ******************************************************************************/
static Int32 gst_ticircbuffer_is_empty(GstTICircBuffer *circBuf)
{
    return (circBuf->contiguousData && circBuf->readPtr == circBuf->writePtr);
}


/******************************************************************************
 * gst_ticircbuffer_display
 *     Print out a nice visual of the buffer.  Legend:
 *         W = write pointer
 *         R = read pointer
 *         B = write pointer and read pointer when they are at the same place
 *         | = end of window
 *         = = active data when there is at least one window in the buffer
 *         - = active data when there is less than a window in the buffer
 *
 *     Examples:
 *        Write pointer after read pointer (contiguousData == TRUE):
 *            [  R=====================|==================================W   ]
 *        Read pointer after write pointer (contiguousData == FALSE):
 *            [==============================W  R=====================|=======]
 *
 ******************************************************************************/
static void gst_ticircbuffer_display(GstTICircBuffer *circBuf)
{
    static Char   buffer[DISP_SIZE + 3];
    static Int32  lastReadBufOffset  = -1;
    static Int32  lastWriteBufOffset = -1;
    Int8*         circBufStart       = Buffer_getUserPtr(circBuf->hBuf);
    Int32         readOffset         = circBuf->readPtr  - circBufStart;
    Int32         writeOffset        = circBuf->writePtr - circBufStart;
    Int32         winBufOffset       = ((double)circBuf->windowSize /
                                        (double)Buffer_getSize(circBuf->hBuf) *
                                        DISP_SIZE);
    Int32         readBufOffset     = ((double)readOffset /
                                       (double)Buffer_getSize(circBuf->hBuf) *
                                        DISP_SIZE) + 1;
    Int32         writeBufOffset    = ((double)writeOffset /
                                       (double)Buffer_getSize(circBuf->hBuf) *
                                        DISP_SIZE) + 1;
    Char          dataChar          = '=';
    Int           i;

    /* If the read or write pointers are past the end of the buffer, make sure
     * they get printed in the last position.
     */
    if (readBufOffset == DISP_SIZE+1) {
        readBufOffset--;
    }
    if (writeBufOffset == DISP_SIZE+1) {
        writeBufOffset--;
    }

    /* Don't print anything out if the buffer hasn't changed significantly */
    if (lastReadBufOffset > -1) {
        if (lastReadBufOffset == readBufOffset &&
            lastWriteBufOffset == writeBufOffset) {
            return;
        }
    }
    lastReadBufOffset  = readBufOffset;
    lastWriteBufOffset = writeBufOffset;

    /* Create a border on the ends of the buffer, and terminate the string */
    buffer[0]             = '[';
    buffer[DISP_SIZE + 1] = ']';
    buffer[DISP_SIZE + 2] = '\0';

    /* If less than a full window is available, represent data with "-" instead
     * of "=".
     */
    if (!gst_ticircbuffer_window_available(circBuf)) {
        dataChar = '-';
    }

    /* Fill in each character of the array representing the buffer */
    for (i = 1; i <= DISP_SIZE; i++) {

        /* If we are at a read or write pointer location, generate the
         * character representing the pointer.
         */
        if (i == readBufOffset && i == writeBufOffset) {
           buffer[i] = 'B';
        }
        else if (i == readBufOffset) {
           buffer[i] = 'R';
        }
        else if (i == writeBufOffset) {
           buffer[i] = 'W';
        }

        /* Case 1:
         *
         * [  B                     |                                      ] or
         * [==B=====================|======================================]
         *
         * If the read and write pointers are at the same location, we need
         * to fill all other locations with either a ' ' or an '=', depending
         * on if the buffer is full or empty.  The only exception is the
         * location of the end of the window, represented by '|'.
         */
        else if (readBufOffset == writeBufOffset) {
           if (i == readBufOffset + winBufOffset) {
               buffer[i] = '|';
           }
           else if (!circBuf->contiguousData) {
               buffer[i] = dataChar;
           }
           else {
               buffer[i] = ' ';
           }
        }

        /* Case 2:
         *
         * [  R=====================|==================================W   ]
         *
         * Mark active data when the write pointer is after the read pointer
         * If the current location is between the two pointers, we get a '=',
         * otherwise we get a ' '.
         */
        else if (circBuf->contiguousData) {
           if (i > readBufOffset && i == readBufOffset + winBufOffset) {
               buffer[i] = '|';
           }
           else if (i > readBufOffset && i < writeBufOffset) {
               buffer[i] = dataChar;
           }
           else {
               buffer[i] = ' ';
           }
        }

        /* Case 3:
         *
         * [==============================W  R=====================|=======]
         *
         * Mark active data when the read pointer is after the write pointer
         * If the current location is between the two pointers, we get a ' ',
         * otherwise we get a '='.
         */
        else {
           if (i > readBufOffset && i == readBufOffset + winBufOffset) {
               buffer[i] = '|';
           }
           else if (i > readBufOffset || i < writeBufOffset) {
               buffer[i] = dataChar;
           }
           else {
               buffer[i] = ' ';
           }
        }
    }
    printf("%s\n", buffer);
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
