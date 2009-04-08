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

#ifndef __GST_DMAIBUFFERTRANSPORT_H__
#define __GST_DMAIBUFFERTRANSPORT_H__

#include <gst/gst.h>

#include <ti/sdo/dmai/Dmai.h>
#include <ti/sdo/dmai/Buffer.h>
#include <ti/sdo/dmai/Rendezvous.h>

G_BEGIN_DECLS

typedef struct _GstTIDmaiBufferTransport GstTIDmaiBufferTransport;

/* Standard macros for manipulating transport objects */
#define GST_TYPE_TIDMAIBUFFERTRANSPORT \
    (gst_tidmaibuffertransport_get_type())
#define GST_IS_TIDMAIBUFFERTRANSPORT(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_TIDMAIBUFFERTRANSPORT))
#define GST_TIDMAIBUFFERTRANSPORT(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_TIDMAIBUFFERTRANSPORT, \
     GstTIDmaiBufferTransport))
#define GST_TIDMAIBUFFERTRANSPORT_DMAIBUF(obj) \
    (GST_TIDMAIBUFFERTRANSPORT(obj)->dmaiBuffer)

#define gst_tidmaibuffertransport_GST_FREE  0x1

/* _GstTIDmaiBufferTransport object */
struct _GstTIDmaiBufferTransport {
  GstBuffer         buffer;
  Buffer_Handle     dmaiBuffer;
  Rendezvous_Handle hRv;
};

/* External function declarations */
GType      gst_tidmaibuffertransport_get_type(void);
GstBuffer* gst_tidmaibuffertransport_new(Buffer_Handle hBuf,Rendezvous_Handle);

G_END_DECLS 

#endif /* __GST_DMAIBUFFERTRANSPORT_H__ */


/******************************************************************************
 * Custom ViM Settings for editing this file
 ******************************************************************************/
#if 0
 Tabs (use 4 spaces for indentation)
 vim:set tabstop=4:      /* Use 4 spaces for tabs          */
 vim:set shiftwidth=4:   /* Use 4 spaces for >> operations */
 vim:set expandtab:      /* Expand tabs into white spaces  */
#endif
