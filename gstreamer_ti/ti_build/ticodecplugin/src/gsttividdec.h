/*
 * gsttividdec.h
 *
 * This file declares the "TIViddec" element, which decodes an xDM 0.9 video
 * stream.
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

#ifndef __GST_TIVIDDEC_H__
#define __GST_TIVIDDEC_H__

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
#include <ti/sdo/dmai/ce/Vdec.h>

G_BEGIN_DECLS

/* Standard macros for maniuplating TIViddec objects */
#define GST_TYPE_TIVIDDEC \
  (gst_tividdec_get_type())
#define GST_TIVIDDEC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_TIVIDDEC,GstTIViddec))
#define GST_TIVIDDEC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_TIVIDDEC,GstTIViddecClass))
#define GST_IS_TIVIDDEC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_TIVIDDEC))
#define GST_IS_TIVIDDEC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_TIVIDDEC))

typedef struct _GstTIViddec      GstTIViddec;
typedef struct _GstTIViddecClass GstTIViddecClass;

/* _GstTIViddec object */
struct _GstTIViddec
{
  /* gStreamer infrastructure */
  GstElement     element;
  GstPad        *sinkpad;
  GstPad        *srcpad;

  /* Element properties */
  const gchar*   engineName;
  const gchar*   codecName;
  gboolean       displayBuffer;
  gboolean       genTimeStamps;

  /* Element state */
  Engine_Handle    hEngine;
  Vdec_Handle      hVd;
  gboolean         drainingEOS;
  pthread_mutex_t  threadStatusMutex;
  UInt32           threadStatus;

  /* Decode thread */
  pthread_t          decodeThread;
  gboolean           decodeDrained;
  Rendezvous_Handle  waitOnDecodeDrain;

  /* Queue thread */
  pthread_t          queueThread;
  Fifo_Handle        hInFifo;

  /* Blocking Conditions to Throttle I/O */
  Rendezvous_Handle  waitOnQueueThread;
  Int32              waitQueueSize;

  /* Blocking Conditions for queue thread */
  Rendezvous_Handle  waitOnDecodeThread;
  
  /* Framerate (Num/Den) */
  gint               framerateNum;
  gint               framerateDen;

  /* Buffer management */
  UInt32           numOutputBufs;
  BufTab_Handle    hOutBufTab;
  GstTICircBuffer *circBuf;

  /* Quicktime h264 header  */
  GstBuffer       *sps_pps_data;
  GstBuffer       *nal_code_prefix;
  guint           nal_length;
};

/* _GstTIViddecClass object */
struct _GstTIViddecClass 
{
  GstElementClass parent_class;
};

/* External function declarations */
GType gst_tividdec_get_type(void);

G_END_DECLS

#endif /* __GST_TIVIDDEC_H__ */


/******************************************************************************
 * Custom ViM Settings for editing this file
 ******************************************************************************/
#if 0
 Tabs (use 4 spaces for indentation)
 vim:set tabstop=4:      /* Use 4 spaces for tabs          */
 vim:set shiftwidth=4:   /* Use 4 spaces for >> operations */
 vim:set expandtab:      /* Expand tabs into white spaces  */
#endif
