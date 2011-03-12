/*
 * gsttividdec2.h
 *
 * This file declares the "TIViddec2" element, which decodes an xDM 1.2 video
 * stream.
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

#ifndef __GST_TIVIDDEC2_H__
#define __GST_TIVIDDEC2_H__

#include <pthread.h>

#include <gst/gst.h>
#include "gstticircbuffer.h"
#include "gsttidmaibuftab.h"

#include <xdc/std.h>
#include <ti/sdo/ce/Engine.h>

#include <ti/sdo/dmai/Dmai.h>
#include <ti/sdo/dmai/Buffer.h>
#include <ti/sdo/dmai/Fifo.h>
#include <ti/sdo/dmai/BufTab.h>
#include <ti/sdo/dmai/Rendezvous.h>
#include <ti/sdo/dmai/ce/Vdec2.h>

G_BEGIN_DECLS

/* Standard macros for maniuplating TIViddec2 objects */
#define GST_TYPE_TIVIDDEC2 \
  (gst_tividdec2_get_type())
#define GST_TIVIDDEC2(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_TIVIDDEC2,GstTIViddec2))
#define GST_TIVIDDEC2_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_TIVIDDEC2,GstTIViddec2Class))
#define GST_IS_TIVIDDEC2(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_TIVIDDEC2))
#define GST_IS_TIVIDDEC2_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_TIVIDDEC2))

typedef struct _GstTIViddec2      GstTIViddec2;
typedef struct _GstTIViddec2Class GstTIViddec2Class;

/* _GstTIViddec2 object */
struct _GstTIViddec2
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
  gboolean       rtCodecThread;

  /* Element state */
  Engine_Handle    hEngine;
  Vdec2_Handle     hVd;
  gboolean         drainingEOS;
  pthread_mutex_t  threadStatusMutex;
  UInt32           threadStatus;
  gboolean         firstFrame;
  gint             width, height;

  /* Decode thread */
  pthread_t          decodeThread;
  Rendezvous_Handle  waitOnDecodeThread;
  Rendezvous_Handle  waitOnDecodeDrain;

  /* Framerate */
  GValue             framerate;

  /* Buffer management */
  UInt32           numOutputBufs;
  GstTIDmaiBufTab *hOutBufTab;
  GstTICircBuffer *circBuf;
  gboolean         padAllocOutbufs;

  /* Quicktime h264 header  */
  GstBuffer       *sps_pps_data;
  GstBuffer       *nal_code_prefix;
  guint           nal_length;

  /* Segment handling */
  GstSegment      *segment;

  /* Buffer timestamp */
  gint64          totalDuration;
  guint64         totalBytes;

  /* Quicktime MPEG4 header */
  GstBuffer       *mpeg4_quicktime_header;
};

/* _GstTIViddec2Class object */
struct _GstTIViddec2Class 
{
  GstElementClass parent_class;
};

/* External function declarations */
GType gst_tividdec2_get_type(void);

G_END_DECLS

#endif /* __GST_TIVIDDEC2_H__ */


/******************************************************************************
 * Custom ViM Settings for editing this file
 ******************************************************************************/
#if 0
 Tabs (use 4 spaces for indentation)
 vim:set tabstop=4:      /* Use 4 spaces for tabs          */
 vim:set shiftwidth=4:   /* Use 4 spaces for >> operations */
 vim:set expandtab:      /* Expand tabs into white spaces  */
#endif
