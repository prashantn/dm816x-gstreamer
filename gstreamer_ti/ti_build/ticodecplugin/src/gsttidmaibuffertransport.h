/*
 * gsttidmaibuffertransport.h
 * 
 * This file declares the "TIDmaiBufferTransport" buffer object, which is used
 * to encapsulate an existing DMAI buffer object inside of a gStreamer
 * buffer so it can be passed along the gStreamer pipeline.
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

#ifndef __GST_TIDMAIBUFFERTRANSPORT_H__
#define __GST_TIDMAIBUFFERTRANSPORT_H__

#include <gst/gst.h>

#include <ti/sdo/dmai/Dmai.h>
#include <ti/sdo/dmai/Buffer.h>
#include <ti/sdo/dmai/Rendezvous.h>

#include "gsttidmaibuftab.h"

G_BEGIN_DECLS

/* Type macros for GST_TYPE_TIDMAIBUFFERTRANSPORT */
#define GST_TYPE_TIDMAIBUFFERTRANSPORT \
    (gst_tidmaibuffertransport_get_type())
#define GST_TIDMAIBUFFERTRANSPORT(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_TIDMAIBUFFERTRANSPORT, \
    GstTIDmaiBufferTransport))
#define GST_IS_TIDMAIBUFFERTRANSPORT(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_TIDMAIBUFFERTRANSPORT))
#define GST_TIDMAIBUFFERTRANSPORT_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_TIDMAIBUFFERTRANSPORT, \
    GstTIDmaiBufferTransportClass))
#define GST_IS_TIDMAIBUFFERTRANSPORT_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_TIDMAIBUFFERTRANSPORT))
#define GST_TIDMAIBUFFERTRANSPORT_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_TIDMAIBUFFERTRANSPORT, \
    GstTIDmaiBufferTransportClass))

/* Use mask flags that keep track of where buffer is in use */
#define gst_tidmaibuffer_GST_FREE        0x1
#define gst_tidmaibuffer_CODEC_FREE      0x2
#define gst_tidmaibuffer_VIDEOSINK_FREE  0x4
#define gst_tidmaibuffer_DISPLAY_FREE    0x8

typedef struct _GstTIDmaiBufferTransport      GstTIDmaiBufferTransport;
typedef struct _GstTIDmaiBufferTransportClass GstTIDmaiBufferTransportClass;

/* Utility macros for GST_TYPE_TIDMAIBUFFERTRANSPORT */
#define GST_TIDMAIBUFFERTRANSPORT_DMAIBUF(obj) \
    ((obj) ? GST_TIDMAIBUFFERTRANSPORT(obj)->dmaiBuffer : NULL)

/* _GstTIDmaiBufferTransport object */
struct _GstTIDmaiBufferTransport {
    GstBuffer          parent_instance;
    Buffer_Handle      dmaiBuffer;
    GstTIDmaiBufTab   *owner;
};

struct _GstTIDmaiBufferTransportClass {
    GstBufferClass    derived_methods;
};

/* External function declarations */
GType      gst_tidmaibuffertransport_get_type(void);
GstBuffer* gst_tidmaibuffertransport_new(Buffer_Handle, GstTIDmaiBufTab*);

G_END_DECLS 

#endif /* __GST_TIDMAIBUFFERTRANSPORT_H__ */


/******************************************************************************
 * Custom ViM Settings for editing this file
 ******************************************************************************/
#if 0
 Tabs (use 4 spaces for indentation)
 vim:set tabstop=4:      /* Use 4 spaces for tabs          */
 vim:set shiftwidth=4:   /* Use 4 spaces for >> operations */
 vim:set expandtab:      /* Expand tabs into white spaces  */
#endif
