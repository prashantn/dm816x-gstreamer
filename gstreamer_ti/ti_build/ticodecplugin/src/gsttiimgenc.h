/*
 * gsttiimgenc.h
 *
 * This file declares the "TIImgenc" element, which encodes an image in
 * jpeg format
 *
 * Original Author:
 *     Chase Maupin, Texas Instruments, Inc.
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

#ifndef __GST_TIIMGENC_H__
#define __GST_TIIMGENC_H__

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
#include <ti/sdo/dmai/BufferGfx.h>
#include <ti/sdo/dmai/ce/Ienc.h>

G_BEGIN_DECLS

/* Standard macros for maniuplating TIImgenc objects */
#define GST_TYPE_TIIMGENC \
  (gst_tiimgenc_get_type())
#define GST_TIIMGENC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_TIIMGENC,GstTIImgenc))
#define GST_TIIMGENC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_TIIMGENC,GstTIImgencClass))
#define GST_IS_TIIMGENC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_TIIMGENC))
#define GST_IS_TIIMGENC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_TIIMGENC))

typedef struct _GstTIImgenc      GstTIImgenc;
typedef struct _GstTIImgencClass GstTIImgencClass;

/* _GstTIImgenc object */
struct _GstTIImgenc
{
  /* gStreamer infrastructure */
  GstElement                element;
  GstPad                    *sinkpad;
  GstPad                    *srcpad;

  /* Element properties */
  const gchar*              engineName;
  const gchar*              codecName;
  gboolean                  displayBuffer;
  gboolean                  genTimeStamps;
  gchar*                    iColor;
  gchar*                    oColor;
  gint                      qValue;
  /* Resolution input */
  gint                      width;
  gint                      height;


  /* Element state */
  Engine_Handle             hEngine;
  Ienc_Handle              hIe;
  gboolean                  drainingEOS;
  pthread_mutex_t           threadStatusMutex;
  UInt32                    threadStatus;
  gboolean                  capsSet;
  gint                      upstreamBufSize;
  gint                      queueMaxBuffers;

  /* Codec Parameters */
  IMGENC_Params            params;
  IMGENC_DynamicParams     dynParams;

  /* Encode thread */
  pthread_t                 encodeThread;
  gboolean                  encodeDrained;
  Rendezvous_Handle         waitOnEncodeDrain;

  /* Queue thread */
  pthread_t                 queueThread;
  Fifo_Handle               hInFifo;

  /* Blocking Conditions to Throttle I/O */
  Rendezvous_Handle         waitOnQueueThread;
  Rendezvous_Handle         waitOnBufTab;
  Int32                     waitQueueSize;

  /* Blocking conditions for encode thread */
  Rendezvous_Handle         waitOnEncodeThread;

  /* Framerate (Num/Den) */
  gint                      framerateNum;
  gint                      framerateDen;

  /* Buffer management */
  UInt32                    numOutputBufs;
  BufTab_Handle             hOutBufTab;
  GstTICircBuffer           *circBuf;
  Buffer_Handle             hInBuf;
};

/* _GstTIImgencClass object */
struct _GstTIImgencClass 
{
  GstElementClass parent_class;
};

/* External function declarations */
GType gst_tiimgenc_get_type(void);

G_END_DECLS

#endif /* __GST_TIIMGENC_H__ */


/******************************************************************************
 * Custom ViM Settings for editing this file
 ******************************************************************************/
#if 0
 Tabs (use 4 spaces for indentation)
 vim:set tabstop=4:      /* Use 4 spaces for tabs          */
 vim:set shiftwidth=4:   /* Use 4 spaces for >> operations */
 vim:set expandtab:      /* Expand tabs into white spaces  */
#endif
