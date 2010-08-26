/*
 * gsttiauddec1.h
 *
 * This file declares the "TIAuddec1" element, which decodes an xDM 1.x audio
 * stream.
 *
 * Original Author:
 *     Brijesh Singh, Texas Instruments, Inc.
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
 */

#ifndef __GST_TIAUDDEC1_H__
#define __GST_TIAUDDEC1_H__

#include <pthread.h>

#include <gst/gst.h>

#include <xdc/std.h>
#include <ti/sdo/ce/Engine.h>

#include <ti/sdo/dmai/Dmai.h>
#include <ti/sdo/dmai/Buffer.h>
#include <ti/sdo/dmai/Fifo.h>
#include <ti/sdo/dmai/Rendezvous.h>
#include <ti/sdo/dmai/ce/Adec1.h>

#include "gstticircbuffer.h"
#include "gsttidmaibuftab.h"

G_BEGIN_DECLS

/* Standard macros for maniuplating TIAuddec1 objects */
#define GST_TYPE_TIAUDDEC1 \
  (gst_tiauddec1_get_type())
#define GST_TIAUDDEC1(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_TIAUDDEC1,GstTIAuddec1))
#define GST_TIAUDDEC1_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_TIAUDDEC1,GstTIAuddec1Class))
#define GST_IS_TIAUDDEC1(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_TIAUDDEC1))
#define GST_IS_TIAUDDEC1_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_TIAUDDEC1))

typedef struct _GstTIAuddec1      GstTIAuddec1;
typedef struct _GstTIAuddec1Class GstTIAuddec1Class;

/* _GstTIAuddec1 object */
struct _GstTIAuddec1
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
  gint           sampleRate;
  gboolean       rtCodecThread;

  /* Element state */
  Engine_Handle    hEngine;
  Adec1_Handle     hAd;
  gint             channels;
  gboolean         drainingEOS;
  pthread_mutex_t  threadStatusMutex;
  UInt32           threadStatus;

  /* Decode thread */
  pthread_t          decodeThread;
  Rendezvous_Handle  waitOnDecodeThread;
  Rendezvous_Handle  waitOnDecodeDrain;

  /* Buffer management */
  UInt32           numOutputBufs;
  GstTIDmaiBufTab *hOutBufTab;
  GstTICircBuffer *circBuf;

  /* AAC header (qtdemuxer) */
  GstBuffer       *aac_header_data;

  /* Segment handling */
  GstSegment      *segment;

  /* Buffer timestamp */
  gint64          totalDuration;
  guint64         totalBytes;
};

/* _GstTIAuddec1Class object */
struct _GstTIAuddec1Class 
{
  GstElementClass parent_class;
};

/* External function declarations */
GType gst_tiauddec1_get_type(void);

G_END_DECLS

#endif /* __GST_TIAUDDEC1_H__ */


/******************************************************************************
 * Custom ViM Settings for editing this file
 ******************************************************************************/
#if 0
 Tabs (use 4 spaces for indentation)
 vim:set tabstop=4:      /* Use 4 spaces for tabs          */
 vim:set shiftwidth=4:   /* Use 4 spaces for >> operations */
 vim:set expandtab:      /* Expand tabs into white spaces  */
#endif
