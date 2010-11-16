/*
 * gsttiprepencbuf.h
 *
 * This file declares the "TIPrepEncBuf" element, which prepares a GstBuffer
 * for use with the encoders used by the TIVidenc1 element.  Typically this
 * means copying the buffer into a physically contiguous buffer, and performing
 * a color conversion on platforms like DM6467.  Hardware acceleration is used
 * for a copy.
 *
 * Platforms where TIVidenc1 supports zero-copy encode (such as DM365) should
 * not use this element when performing capture+encode.  In these cases, the
 * encoder can process buffers directly from MMAP capture buffers.
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

#ifndef __GST_TIPREPENCBUF_H__
#define __GST_TIPREPENCBUF_H__

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>

#include <ti/sdo/dmai/Framecopy.h>
#include <ti/sdo/dmai/Ccv.h>
#include <ti/sdo/dmai/Cpu.h>

#include "gsttidmaibuftab.h"

G_BEGIN_DECLS

/* Standard macros for maniuplating TIPrepEncBuf objects */
#define GST_TYPE_TIPREPENCBUF \
  (gst_tiprepencbuf_get_type())
#define GST_TIPREPENCBUF(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_TIPREPENCBUF,GstTIPrepEncBuf))
#define GST_TIPREPENCBUF_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_TIPREPENCBUF,GstTIPrepEncBufClass))
#define GST_IS_TIPREPENCBUF(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_TIPREPENCBUF))
#define GST_IS_TIPREPENCBUF_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_TIPREPENCBUF))

typedef struct _GstTIPrepEncBuf      GstTIPrepEncBuf;
typedef struct _GstTIPrepEncBufClass GstTIPrepEncBufClass;

/* _GstTIPrepEncBuf object */
struct _GstTIPrepEncBuf
{
  /* gStreamer infrastructure */
  GstBaseTransform  element;
  GstPad            *sinkpad;
  GstPad            *srcpad;

  /* Element property */
  gboolean          contiguousInputFrame;
  gint              numOutputBufs;

  /* Element state */
  gint              srcWidth;
  gint              srcHeight;
  ColorSpace_Type   srcColorSpace;
  gint              dstWidth;
  gint              dstHeight;
  ColorSpace_Type   dstColorSpace;
  Framecopy_Handle  hFc;
  Ccv_Handle        hCcv;
  GstTIDmaiBufTab  *hOutBufTab;
  Cpu_Device        device;
};

/* _GstTIPrepEncBufClass object */
struct _GstTIPrepEncBufClass 
{
  GstBaseTransformClass parent_class;
};

/* External function enclarations */
GType gst_tiprepencbuf_get_type(void);

G_END_DECLS

#endif /* __GST_TIPREPENCBUF_H__ */


/******************************************************************************
 * Custom ViM Settings for editing this file
 ******************************************************************************/
#if 0
 Tabs (use 4 spaces for indentation)
 vim:set tabstop=4:      /* Use 4 spaces for tabs          */
 vim:set shiftwidth=4:   /* Use 4 spaces for >> operations */
 vim:set expandtab:      /* Expand tabs into white spaces  */
#endif
