/*
 * gsttidmaiperf.h
 *
 * This file enclares the "dmaiperf" element, which can be used to get
 * the perfance of your pipeline
 *
 * Original Author:
 *     RidgeRun.
 *
 * Copyright (C) $year RidgeRun - http://www.ridgerun.com/
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

#ifndef __GST_DMAIPERF_H__
#define __GST_DMAIPERF_H__

#include <pthread.h>

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>
#include <ti/sdo/ce/Engine.h>
#include <ti/sdo/dmai/Cpu.h>
#include <xdc/std.h>

#include <ti/sdo/dmai/Dmai.h>

G_BEGIN_DECLS

/* Standard macros for maniuplating Dmaiperf objects */
#define GST_TYPE_DMAIPERF \
  (gst_dmaiperf_get_type())
#define GST_DMAIPERF(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_DMAIPERF,GstDmaiperf))
#define GST_DMAIPERF_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_DMAIPERF,GstDmaiperfClass))
#define GST_IS_DMAIPERF(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_DMAIPERF))
#define GST_IS_DMAIPERF_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_DMAIPERF))

typedef struct _GstDmaiperf      GstDmaiperf;
typedef struct _GstDmaiperfClass GstDmaiperfClass;

/* _GstDmaiperf object */
struct _GstDmaiperf
{
  /* gStreamer infrastructure */
  GstBaseTransform  element;
  GstPad            *sinkpad;
  GstPad            *srcpad;

  Server_Handle     hDsp;
  Engine_Handle     hEngine;
  Cpu_Handle        hCpu;
  const gchar       *engineName;
  GError            *error;

  /* Element property */
  GstClockTime      lastLoadstamp;
  int               lastWorkload;
  guint32           fps;
  guint32           bps;
  gboolean          printArmLoad;
};

/* _GstDmaiperfClass object */
struct _GstDmaiperfClass
{
  GstBaseTransformClass  parent_class;
};

/* External function enclarations */
GType gst_dmaiperf_get_type(void);

G_END_DECLS

#endif /* __GST_DMAIPERF_H__ */


/******************************************************************************
 * Custom ViM Settings for editing this file
 ******************************************************************************/
#if 0
 Tabs (use 4 spaces for indentation)
 vim:set tabstop=4:      /* Use 4 spaces for tabs          */
 vim:set shiftwidth=4:   /* Use 4 spaces for >> operations */
 vim:set expandtab:      /* Expand tabs into white spaces  */
#endif
