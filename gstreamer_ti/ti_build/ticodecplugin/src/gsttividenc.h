/*
 * gsttividenc.h
 *
 * This file enclares the "TIVidenc" element, which encodes an xDM 0.9 video
 * stream.
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

#ifndef __GST_TIVIDENC_H__
#define __GST_TIVIDENC_H__

#include <pthread.h>

#include <gst/gst.h>
#include "gstticircbuffer.h"

#include <xdc/std.h>
#include <ti/sdo/ce/Engine.h>

#include <ti/sdo/dmai/Dmai.h>
#include <ti/sdo/dmai/Buffer.h>
#include <ti/sdo/dmai/Fifo.h>
#include <ti/sdo/dmai/BufTab.h>
#include <ti/sdo/dmai/Rendezvous.h>
#include <ti/sdo/dmai/ce/Venc.h>
#include <ti/sdo/dmai/ColorSpace.h>

G_BEGIN_DECLS

/* Standard macros for maniuplating TIVidenc objects */
#define GST_TYPE_TIVIDENC \
  (gst_tividenc_get_type())
#define GST_TIVIDENC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_TIVIDENC,GstTIVidenc))
#define GST_TIVIDENC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_TIVIDENC,GstTIVidencClass))
#define GST_IS_TIVIDENC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_TIVIDENC))
#define GST_IS_TIVIDENC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_TIVIDENC))

typedef struct _GstTIVidenc      GstTIVidenc;
typedef struct _GstTIVidencClass GstTIVidencClass;

/* _GstTIVidenc object */
struct _GstTIVidenc
{
  /* gStreamer infrastructure */
  GstElement        element;
  GstPad            *sinkpad;
  GstPad            *srcpad;

  /* Element properties */
  const gchar*      engineName;
  const gchar*      codecName;
  const gchar*      resolution;
  const gchar*      iColor;
  gboolean          displayBuffer;
  gboolean          genTimeStamps;
  gint              numInputBufs;
  gint              numOutputBufs;
  glong             bitRate;
  gboolean          contiguousInputFrame;
            
  /* Element state */
  Engine_Handle     hEngine;
  Venc_Handle       hVe;
  gboolean          drainingEOS;
  pthread_mutex_t   threadStatusMutex;
  UInt32            threadStatus;
  gint              queueMaxBuffers;
  gint              upstreamBufSize;
              
  /* Encode thread */
  pthread_t         encodeThread;
  gboolean          encodeDrained;
  Rendezvous_Handle waitOnEncodeDrain;
  Rendezvous_Handle waitOnBufTab;

  /* Queue thread */
  pthread_t         queueThread;
  Fifo_Handle       hInFifo;

  /* Blocking Conditions to Throttle I/O */
  Rendezvous_Handle waitOnQueueThread;
  Int32             waitQueueSize;

  /* Blocking Condition for encode thread */
  Rendezvous_Handle waitOnEncodeThread;

  /* Framerate (Num/Den) */
  gint              framerateNum;
  gint              framerateDen;

  /* Frame resolution */
  gint              width;
  gint              height;

  /* Frame color space */
  ColorSpace_Type   colorSpace;

  /* Buffer management */
  BufTab_Handle     hOutBufTab;
  GstTICircBuffer   *circBuf;

};

/* _GstTIVidencClass object */
struct _GstTIVidencClass 
{
  GstElementClass parent_class;
};

/* External function enclarations */
GType gst_tividenc_get_type(void);

G_END_DECLS

#endif /* __GST_TIVIDENC_H__ */


/******************************************************************************
 * Custom ViM Settings for editing this file
 ******************************************************************************/
#if 0
 Tabs (use 4 spaces for indentation)
 vim:set tabstop=4:      /* Use 4 spaces for tabs          */
 vim:set shiftwidth=4:   /* Use 4 spaces for >> operations */
 vim:set expandtab:      /* Expand tabs into white spaces  */
#endif
