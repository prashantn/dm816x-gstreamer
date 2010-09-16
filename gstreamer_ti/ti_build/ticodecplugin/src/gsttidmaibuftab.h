/*
 * gsttidmaibuftab.h
 * 
 * The "GstTIDmaiBufTab" object is used encapsulate DMAI BufTab objects inside
 * of a GstMiniObjects.  The allows us to postpone deleting a BufTab until all
 * of its buffers have been unrefed, even if the element that created the
 * BufTab no longer needs it.
 * 
 * Original Author:
 *     Don Darling, Texas Instruments, Inc.
 *
 * Copyright (C) 2010 Texas Instruments Incorporated - http://www.ti.com/
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

#ifndef __GST_TIDMAIBUFTAB_H__
#define __GST_TIDMAIBUFTAB_H__

#include <pthread.h>

#include <gst/gst.h>

#include <ti/sdo/dmai/Dmai.h>
#include <ti/sdo/dmai/BufTab.h>
#include <ti/sdo/dmai/Buffer.h>
#include <ti/sdo/dmai/Rendezvous.h>

G_BEGIN_DECLS

/* Type macros for GST_TYPE_TIDMAIBUFTAB */
#define GST_TYPE_TIDMAIBUFTAB \
    (gst_tidmaibuftab_get_type())
#define GST_TIDMAIBUFTAB(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_TIDMAIBUFTAB, \
    GstTIDmaiBufTab))
#define GST_IS_TIDMAIBUFTAB(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_TIDMAIBUFTAB))
#define GST_TIDMAIBUFTAB_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_TIDMAIBUFTAB, \
    GstTIDmaiBufTabClass))
#define GST_IS_TIDMAIBUFTAB_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_TIDMAIBUFTAB))
#define GST_TIDMAIBUFTAB_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_TIDMAIBUFTAB, \
    GstTIDmaiBufTabClass))

/* Utility macros */
#define GST_TIDMAIBUFTAB_BUFTAB(obj) \
    ((obj) ? GST_TIDMAIBUFTAB(obj)->hBufTab : NULL)
#define GST_TIDMAIBUFTAB_BUFAVAIL_RV(obj) \
    ((obj) ? GST_TIDMAIBUFTAB(obj)->hBufAvailRv : NULL)
#define GST_TIDMAIBUFTAB_GETBUF_MUTEX(obj) \
    &(GST_TIDMAIBUFTAB(obj)->hGetBufMutex)

typedef struct _GstTIDmaiBufTab      GstTIDmaiBufTab;
typedef struct _GstTIDmaiBufTabClass GstTIDmaiBufTabClass;

/* _GstTIDmaiBufTab object */
struct _GstTIDmaiBufTab {
    GstMiniObject     parent_instance;
    BufTab_Handle     hBufTab;
    Rendezvous_Handle hBufAvailRv;
    pthread_mutex_t   hGetBufMutex;
    gboolean          blocking;
};

struct _GstTIDmaiBufTabClass {
    GstMiniObjectClass    derived_methods;
};

/* External function declarations */
GType            gst_tidmaibuftab_get_type(void);
GstTIDmaiBufTab* gst_tidmaibuftab_new(gint num_bufs, gint32 size,
                     Buffer_Attrs *attrs);
Buffer_Handle    gst_tidmaibuftab_get_buf(GstTIDmaiBufTab *self);
void             gst_tidmaibuftab_set_blocking(GstTIDmaiBufTab *self,
                     gboolean blocking);
void             gst_tidmaibuftab_ref(GstTIDmaiBufTab *self);
void             gst_tidmaibuftab_unref(GstTIDmaiBufTab *self);

G_END_DECLS 

#endif /* __GST_TIDMAIBUFTAB_H__ */


/******************************************************************************
 * Custom ViM Settings for editing this file
 ******************************************************************************/
#if 0
 Tabs (use 4 spaces for indentation)
 vim:set tabstop=4:      /* Use 4 spaces for tabs          */
 vim:set shiftwidth=4:   /* Use 4 spaces for >> operations */
 vim:set expandtab:      /* Expand tabs into white spaces  */
#endif
