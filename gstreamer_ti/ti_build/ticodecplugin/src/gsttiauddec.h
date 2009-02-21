/*
 * gsttiauddec.h
 *
 * This file declares the "TIAuddec" element, which decodes an xDM 0.9 audio
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
 * This program is distributed “as is” WITHOUT ANY WARRANTY of any kind,
 * whether express or implied; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 */

#ifndef __GST_TIAUDDEC_H__
#define __GST_TIAUDDEC_H__

#include <pthread.h>

#include <gst/gst.h>

#include <xdc/std.h>
#include <ti/sdo/ce/Engine.h>

#include <ti/sdo/dmai/Dmai.h>
#include <ti/sdo/dmai/Buffer.h>
#include <ti/sdo/dmai/Fifo.h>
#include <ti/sdo/dmai/Rendezvous.h>
#include <ti/sdo/dmai/ce/Adec.h>

#include "gstticircbuffer.h"

G_BEGIN_DECLS

/* Standard macros for maniuplating TIAuddec objects */
#define GST_TYPE_TIAUDDEC \
  (gst_tiauddec_get_type())
#define GST_TIAUDDEC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_TIAUDDEC,GstTIAuddec))
#define GST_TIAUDDEC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_TIAUDDEC,GstTIAuddecClass))
#define GST_IS_TIAUDDEC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_TIAUDDEC))
#define GST_IS_TIAUDDEC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_TIAUDDEC))

typedef struct _GstTIAuddec      GstTIAuddec;
typedef struct _GstTIAuddecClass GstTIAuddecClass;

/* _GstTIAuddec object */
struct _GstTIAuddec
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
  Adec_Handle      hAd;
  gint             channels;
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

  /* Blocking conditions for decode thread */
  Rendezvous_Handle  waitOnDecodeThread;
  
  /* Buffer management */
  UInt32           numOutputBufs;
  BufTab_Handle    hOutBufTab;
  GstTICircBuffer *circBuf;
  GstBuffer       *adif_data;

  /* AAC header (qtdemuxer) */
  GstBuffer       *aac_header_data;
};

/* _GstTIAuddecClass object */
struct _GstTIAuddecClass 
{
  GstElementClass parent_class;
};

/* External function declarations */
GType gst_tiauddec_get_type(void);

G_END_DECLS

#endif /* __GST_TIAUDDEC_H__ */


/******************************************************************************
 * Custom ViM Settings for editing this file
 ******************************************************************************/
#if 0
 Tabs (use 4 spaces for indentation)
 vim:set tabstop=4:      /* Use 4 spaces for tabs          */
 vim:set shiftwidth=4:   /* Use 4 spaces for >> operations */
 vim:set expandtab:      /* Expand tabs into white spaces  */
#endif
