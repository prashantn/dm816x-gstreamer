/*
 * gsttidmaibuffertransport.c
 * 
 * This file defines the "TIDmaiBufferTransport" buffer object, which is used
 * to encapsulate an existing DMAI buffer object inside of a gStreamer
 * buffer so it can be passed along the gStreamer pipeline.
 * 
 * Specifically, this object provides a finalize function that will release
 * a DMAI buffer properly when gst_buffer_unref() is called.  If the specified
 * DMAI buffer is part of a BufTab, it will be released for re-use.
 * DMAI buffers no part of a BufTab will be deleted when no longer referenced.
 *
 * Downstream elements may use the GST_IS_TIDMAIBUFFERTRANSPORT() macro to
 * check to see if a gStreamer buffer encapsulates a DMAI buffer.  When passed
 * an element of this type, elements can take advantage of the fact that the
 * buffer is contiguously allocated in memory.  Also, if the element is using
 * DMAI it can access the DMAI buffer directly via the
 * GST_TIDMAIBUFFERTRANSPORT_DMAIBUF() macro.
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

#include <stdlib.h>
#include <pthread.h>

#include <ti/sdo/dmai/Dmai.h>
#include <ti/sdo/dmai/Buffer.h>
#include <ti/sdo/dmai/BufTab.h>
#include <ti/sdo/dmai/Rendezvous.h>

#include "gsttidmaibuffertransport.h"

/* Declare variable used to categorize GST_LOG output */
GST_DEBUG_CATEGORY_STATIC (gst_tidmaibuffertransport_debug);
#define GST_CAT_DEFAULT gst_tidmaibuffertransport_debug

/* Declare a global pointer to our buffer base class */
static GstBufferClass *parent_class;

/* Static Function Declarations */
static void
    gst_tidmaibuffertransport_init(GstTIDmaiBufferTransport *self);
static void
    gst_tidmaibuffertransport_log_init(void);
static void
    gst_tidmaibuffertransport_class_init(GstTIDmaiBufferTransportClass *klass);
static void
    gst_tidmaibuffertransport_finalize(GstBuffer *gstbuffer);

/* Define GST_TYPE_TIDMAIBUFFERTRANSPORT */
G_DEFINE_TYPE_WITH_CODE (GstTIDmaiBufferTransport, gst_tidmaibuffertransport, \
    GST_TYPE_BUFFER, gst_tidmaibuffertransport_log_init());


/******************************************************************************
 * gst_tidmaibuffertransport_log_init
 *    Initialize the GST_LOG for this type
 ******************************************************************************/
static void gst_tidmaibuffertransport_log_init(void)
{
    GST_DEBUG_CATEGORY_INIT(gst_tidmaibuffertransport_debug,
        "TIDmaiBufferTransport", 0, "TI DMAI Buffer Transport");
}


/******************************************************************************
 * gst_tidmaibuffertransport_init
 *    Initializes a new transport buffer instance.
 ******************************************************************************/
static void gst_tidmaibuffertransport_init(GstTIDmaiBufferTransport *self)
{
    GST_LOG("begin init\n");

    self->dmaiBuffer = NULL;
    self->owner      = NULL;

    GST_LOG("end init\n");
}


/******************************************************************************
 * gst_tidmaibuffertransport_class_init
 *    Initializes the GstTIDmaiBufferTransport class.
 ******************************************************************************/
static void gst_tidmaibuffertransport_class_init(
                GstTIDmaiBufferTransportClass *klass)
{
    GST_LOG("begin class_init\n");

    parent_class = g_type_class_peek_parent(klass);

    /* Override the mini-object's finalize routine so we can do cleanup when
     * a GstTIDmaiBufferTransport is unref'd.
     */
    klass->derived_methods.mini_object_class.finalize =
        (GstMiniObjectFinalizeFunction) gst_tidmaibuffertransport_finalize;

    GST_LOG("end class_init\n");
}


/******************************************************************************
 * gst_tidmaibuffertransport_finalize
 *    Dispose a DMAI buffer transport object
 ******************************************************************************/
static void gst_tidmaibuffertransport_finalize(GstBuffer *gstbuffer)
{
    GstTIDmaiBufferTransport *self = GST_TIDMAIBUFFERTRANSPORT(gstbuffer);

    GST_LOG("begin finalize\n");

    /* If we're part of a a GstTIDmaiBufTab object, we need to make sure that
     * changing the useMask and calling Rendezvous_force is done as a single
     * step, or the condition mutex that waits for an available buffer may
     * not work properly.
     */
    if (self->owner) {
        pthread_mutex_lock(GST_TIDMAIBUFTAB_GETBUF_MUTEX(self->owner));
    }

    /* If the DMAI buffer is part of a BufTab, free it for re-use.  Otherwise,
     * destroy the buffer.
     */
    if (Buffer_getBufTab(self->dmaiBuffer) != NULL) {
        GST_LOG("clearing GStreamer useMask bit\n");
        Buffer_freeUseMask(self->dmaiBuffer, gst_tidmaibuffer_GST_FREE);
        Buffer_freeUseMask(self->dmaiBuffer, gst_tidmaibuffer_VIDEOSINK_FREE);
    } else {
        GST_LOG("calling Buffer_delete()\n");
        Buffer_delete(self->dmaiBuffer);
    }

    /* If a GstTIDmaiBufTab object is blocked waiting for a buffer to be freed,
     * wake it up.
     */
    if (self->owner) {
        Rendezvous_force(GST_TIDMAIBUFTAB_BUFAVAIL_RV(self->owner));
        pthread_mutex_unlock(GST_TIDMAIBUFTAB_GETBUF_MUTEX(self->owner));
    }

    /* Remove reference to the GstTIDmaiBufTab object that owns us, if any */
    if (self->owner) {
        gst_tidmaibuftab_unref(self->owner);
    }

    self->dmaiBuffer = NULL;
    self->owner      = NULL;

    /* Call GstBuffer's finalize routine, so our base class can do it's cleanup
     * as well.  If we don't do this, we'll have a memory leak that is very
     * difficult to track down.
     */
    GST_BUFFER_CLASS(parent_class)->
        mini_object_class.finalize(GST_MINI_OBJECT(gstbuffer));

    GST_LOG("end finalize\n");
}


/******************************************************************************
 * gst_tidmaibuffertransport_new
 *    Create a new DMAI buffer transport object.
 ******************************************************************************/
GstBuffer* gst_tidmaibuffertransport_new(
               Buffer_Handle dmaiBuffer, GstTIDmaiBufTab *owner)
{
    GstTIDmaiBufferTransport *tdt_buf;

    tdt_buf = (GstTIDmaiBufferTransport*)
              gst_mini_object_new(GST_TYPE_TIDMAIBUFFERTRANSPORT);

    g_return_val_if_fail(tdt_buf != NULL, NULL);

    GST_BUFFER_SIZE(tdt_buf) = Buffer_getSize(dmaiBuffer);
    GST_BUFFER_DATA(tdt_buf) = (Void*)Buffer_getUserPtr(dmaiBuffer);

    if (GST_BUFFER_DATA(tdt_buf) == NULL) {
        gst_mini_object_unref(GST_MINI_OBJECT(tdt_buf));
        return NULL;
    }

    tdt_buf->dmaiBuffer = dmaiBuffer;
    tdt_buf->owner      = owner;

    /* If the owner was specified, create a reference to it.  This keeps the
     * owning GstTIDmaiBufTab object alive while there is a transport buffer
     * that is managing one of its buffers.
     */ 
    if (tdt_buf->owner) {
        gst_tidmaibuftab_ref(tdt_buf->owner);
    }

    /* If the DMAI buffer is part of a BufTab, mark it as being in use by the
     * GStreamer pipeline.
     */
    if (Buffer_getBufTab(tdt_buf->dmaiBuffer) != NULL) {
        Buffer_setUseMask(tdt_buf->dmaiBuffer,
            Buffer_getUseMask(tdt_buf->dmaiBuffer) |
            gst_tidmaibuffer_GST_FREE);
    }

    GST_LOG("end new\n");

    return GST_BUFFER(tdt_buf);
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
