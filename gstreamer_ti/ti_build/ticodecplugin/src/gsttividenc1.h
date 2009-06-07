/*
 * gsttividenc1.h
 *
 * This file enclares the "TIVidenc1" element, which encodes an xDM 1.x video
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

#ifndef __GST_TIVIDENC1_H__
#define __GST_TIVIDENC1_H__

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
#include <ti/sdo/dmai/ce/Venc1.h>
#include <ti/sdo/dmai/Cpu.h>
#include <ti/sdo/dmai/ColorSpace.h>

G_BEGIN_DECLS

/* Standard macros for maniuplating TIVidenc1 objects */
#define GST_TYPE_TIVIDENC1 \
  (gst_tividenc1_get_type())
#define GST_TIVIDENC1(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_TIVIDENC1,GstTIVidenc1))
#define GST_TIVIDENC1_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_TIVIDENC1,GstTIVidenc1Class))
#define GST_IS_TIVIDENC1(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_TIVIDENC1))
#define GST_IS_TIVIDENC1_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_TIVIDENC1))

typedef struct _GstTIVidenc1      GstTIVidenc1;
typedef struct _GstTIVidenc1Class GstTIVidenc1Class;

/* _GstTIVidenc1 object */
struct _GstTIVidenc1
{
  /* gStreamer infrastructure */
  GstElement     element;
  GstPad        *sinkpad;
  GstPad        *srcpad;

  /* Element properties */
  const gchar*   engineName;
  const gchar*   codecName;
  const gchar*   resolution;
  const gchar*   iColor;
  gboolean       displayBuffer;
  gboolean       genTimeStamps;
  gboolean       contiguousInputFrame;
  gint           numInputBufs;
  gint           numOutputBufs;
  gint32         bitRate;

  /* Element state */
  Engine_Handle    hEngine;
  Venc1_Handle     hVe1;
  gboolean         drainingEOS;
  pthread_mutex_t  threadStatusMutex;
  UInt32           threadStatus;
  Cpu_Device       device;
  Cpu_Handle       hCpu;
  gint             queueMaxBuffers;
  gint             upstreamBufSize;

  /* Encode thread */
  pthread_t          encodeThread;
  gboolean           encodeDrained;
  Rendezvous_Handle  waitOnEncodeDrain;

  /* Queue thread */
  pthread_t          queueThread;
  Fifo_Handle        hInFifo;

  /* Blocking Conditions to Throttle I/O */
  Rendezvous_Handle  waitOnQueueThread;
  Int32              waitQueueSize;

  /* Blocking Condition for encode thread */
  Rendezvous_Handle  waitOnEncodeThread;
  Rendezvous_Handle  waitOnBufTab;

  /* Framerate (Num/Den) */
  gint               framerateNum;
  gint               framerateDen;

  /* Frame resolution */
  gint           width;
  gint           height;

  /* Frame color space */
  ColorSpace_Type colorSpace;

  /* Buffer management */
  BufTab_Handle    hOutBufTab;
  GstTICircBuffer *circBuf;

};

/* _GstTIVidenc1Class object */
struct _GstTIVidenc1Class 
{
  GstElementClass parent_class;
};

/* External function enclarations */
GType gst_tividenc1_get_type(void);

G_END_DECLS

#endif /* __GST_TIVIDENC1_H__ */


/******************************************************************************
 * Custom ViM Settings for editing this file
 ******************************************************************************/
#if 0
 Tabs (use 4 spaces for indentation)
 vim:set tabstop=4:      /* Use 4 spaces for tabs          */
 vim:set shiftwidth=4:   /* Use 4 spaces for >> operations */
 vim:set expandtab:      /* Expand tabs into white spaces  */
#endif
