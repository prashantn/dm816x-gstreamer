/*
 * gsttidmaibuftab.c
 * 
 * The "GstTIDmaiBufTab" object is used encapsulate DMAI BufTab objects inside
 * of a GstMiniObjects.  The allows us to postpone deleting a BufTab until all
 * of its buffers have been unrefed, even if the element that created the
 * BufTab no longer needs it.
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

#include <ti/sdo/dmai/Dmai.h>
#include <ti/sdo/dmai/BufTab.h>
#include <ti/sdo/dmai/Buffer.h>
#include <ti/sdo/dmai/Rendezvous.h>

#include "gsttidmaibuftab.h"

/* Declare variable used to categorize GST_LOG output */
GST_DEBUG_CATEGORY_STATIC (gst_tidmaibuftab_debug);
#define GST_CAT_DEFAULT gst_tidmaibuftab_debug

/* Declare a global pointer to our buffer base class */
static GstMiniObjectClass *parent_class;

/* Static Function Declarations */
static void
    gst_tidmaibuftab_init(GstTIDmaiBufTab *self);
static void
    gst_tidmaibuftab_log_init(void);
static void
    gst_tidmaibuftab_class_init(GstTIDmaiBufTabClass *klass);
static void
    gst_tidmaibuftab_finalize(GstTIDmaiBufTab *self);

/* Define GST_TYPE_TIDMAIBUFTAB */
G_DEFINE_TYPE_WITH_CODE (GstTIDmaiBufTab, gst_tidmaibuftab, \
    GST_TYPE_MINI_OBJECT, gst_tidmaibuftab_log_init());


/******************************************************************************
 * gst_tidmaibuftab_log_init
 *    Initialize the GST_LOG for this type
 ******************************************************************************/
static void gst_tidmaibuftab_log_init(void)
{
    GST_DEBUG_CATEGORY_INIT(gst_tidmaibuftab_debug,
        "GstTIDmaiBufTab", 0, "TI DMAI Buffer Table (BufTab)");
}


/******************************************************************************
 * gst_tidmaibuftab_init
 *    Initializes a new transport buffer instance.
 ******************************************************************************/
static void gst_tidmaibuftab_init(GstTIDmaiBufTab *self)
{
    GST_LOG("begin init\n");

    self->hBufTab     = NULL;
    self->hBufAvailRv = NULL;
    self->blocking    = TRUE;

    GST_LOG("end init\n");
}


/******************************************************************************
 * gst_tidmaibuftab_class_init
 *    Initializes the GstTIDmaiBufTab class.
 ******************************************************************************/
static void gst_tidmaibuftab_class_init(
                GstTIDmaiBufTabClass *klass)
{
    GstMiniObjectClass *mo_class = GST_MINI_OBJECT_CLASS(klass);

    GST_LOG("begin class_init\n");

    parent_class = g_type_class_peek_parent(klass);

    /* Override the mini-object's finalize routine so we can do cleanup when
     * a GstTIDmaiBufTab is unref'd.
     */
    mo_class->finalize =
        (GstMiniObjectFinalizeFunction) gst_tidmaibuftab_finalize;

    GST_LOG("end class_init\n");
}


/******************************************************************************
 * gst_tidmaibuftab_finalize
 *    Dispose a DMAI BufTab object.
 ******************************************************************************/
static void gst_tidmaibuftab_finalize(GstTIDmaiBufTab *self)
{
    GST_LOG("begin finalize\n");

    if (self->hBufTab) {
        BufTab_delete(self->hBufTab);
        self->hBufTab = NULL;
    }

    if (self->hBufAvailRv) {
        Rendezvous_delete(self->hBufAvailRv);
        self->hBufAvailRv = NULL;
    }

    pthread_mutex_destroy(&self->hGetBufMutex);

    /* Call GstMiniObject's finalize routine, so our base class can do its
     * cleanup as well.  If we don't do this, we could end up with a memory
     * leak that is very difficult to track down.
     */
    GST_MINI_OBJECT_CLASS(parent_class)->finalize(GST_MINI_OBJECT(self));

    GST_LOG("end finalize\n");
}


/******************************************************************************
 * gst_tidmaibuftab_get_buf
 *    Return a free buffer from the DMAI BufTab object.
 ******************************************************************************/
Buffer_Handle gst_tidmaibuftab_get_buf(GstTIDmaiBufTab *self)
{
    Buffer_Handle hFreeBuf = NULL;

    /* Get a free buffer from the BufTab */
    pthread_mutex_lock(&self->hGetBufMutex);
    hFreeBuf = BufTab_getFreeBuf(self->hBufTab);

    /* If we are configured to block until we have a buffer, wait until a
     * buffer is available
     */
    if (self->blocking && !hFreeBuf) {
        Rendezvous_reset(self->hBufAvailRv);

        pthread_mutex_unlock(&self->hGetBufMutex);
        Rendezvous_meet(self->hBufAvailRv);
        pthread_mutex_lock(&self->hGetBufMutex);

        hFreeBuf = BufTab_getFreeBuf(self->hBufTab);
    }
    pthread_mutex_unlock(&self->hGetBufMutex);

    if (self->blocking && !hFreeBuf) {
        GST_ERROR("Failed to get a buffer from the GstTIDmaiBufTab object");
    }

    return hFreeBuf;
}


/******************************************************************************
 * gst_tidmaibuftab_set_blocking
 ******************************************************************************/
void gst_tidmaibuftab_set_blocking(GstTIDmaiBufTab *self, gboolean blocking)
{
   self->blocking = blocking;
}


/******************************************************************************
 * gst_tidmaibuftab_new
 *    Create a new DMAI BufTab object.
 ******************************************************************************/
GstTIDmaiBufTab* gst_tidmaibuftab_new(gint num_bufs, gint32 size,
                     Buffer_Attrs *attrs)
{
    Rendezvous_Attrs  rzvAttrs = Rendezvous_Attrs_DEFAULT;
    GstTIDmaiBufTab  *self;

    GST_LOG("begin new\n");

    self = (GstTIDmaiBufTab*)gst_mini_object_new(GST_TYPE_TIDMAIBUFTAB);
    g_return_val_if_fail(self != NULL, NULL);

    self->hBufTab     = BufTab_create(num_bufs, size, attrs);
    self->hBufAvailRv = Rendezvous_create(Rendezvous_INFINITE, &rzvAttrs);

    pthread_mutex_init(&self->hGetBufMutex, NULL);

    if (!self->hBufTab || !self->hBufAvailRv) {
        GST_ERROR("Failed to create a new GstTIDmaiBufTab object");
        gst_mini_object_unref(GST_MINI_OBJECT(self));
        return NULL;
    }

    return self;
}


/******************************************************************************
 * gst_tidmaibuftab_ref
 *    Add a reference to a DMAI BufTab object.
 ******************************************************************************/
void gst_tidmaibuftab_ref(GstTIDmaiBufTab *self)
{
    gst_mini_object_ref(GST_MINI_OBJECT(self));
}


/******************************************************************************
 * gst_tidmaibuftab_unref
 *    Un-ref a DMAI BufTab object.
 ******************************************************************************/
void gst_tidmaibuftab_unref(GstTIDmaiBufTab *self)
{
    gst_mini_object_unref(GST_MINI_OBJECT(self));
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
