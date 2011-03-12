/*
 * gsttidmaiperf.c
 *
 * This file defines the "dmaiperf" element, which can be used to get
 * the perfance of your pipeline.
 *
 * Original Author:
 *     Lissandro Mendez - RidgeRun.
 *
 * Copyright (C) 2009 RidgeRun - http://www.ridgerun.com/
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

/**
 * SECTION:element-dmaiperf
 *
 * DmaiPerf can be used to capture pipeline performance data.  Each 
 * second dmaiperf sends a frames per second, bytes per second, and
 * timestamp data using gst_element_post_message.
 *
 * The performance messages can be used by your application.  However,
 * the dmaiperf element is EXPERIMENTAL and intended for engineers 
 * optimizing dmaiperf.  Message data format, properties, and other
 * aspects of the dmaiperf element will change.  Applications that
 * intend to use the dmaiperf element need to realize these changes 
 * will likely break your application.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch filesrc location=/movie.mp4 ! qtdemux ! TIViddec2 engineName=decode codecName=mpeg4dec ! dmaiperf engine-name=decode print-arm-load=TRUE ! TIDmaiVideoSink displayStd=v4l2 displayDevice=/dev/video2 videoStd=D1_NTSC videoOutput=COMPOSITE resizer=FALSE accelFrameCopy=TRUE 
 * ]| This pipeline decodes the mpeg4 video in movie.mp4 and renders it on /dev/video2.
 * </refsect2>
 *
 * <refsect2>
 * <title>Performance data format</title>
 *
 * The data format is EXPERIMENTAL and will be changed without regard
 * to backwards compatibility.
 * |[
 * Timestamp: 0:32:55.208388628; bps: 76032000; fps: 110; CPU: 46; DSP: 73; 
 * mem_seg: DDR2; base: 0x8fb86cde; size: 0x20000; maxblocklen: 0x15f28; used: 0x9de8; 
 * mem_seg: DDRALGHEAP; base: 0x88000000; size: 0x7a00000; maxblocklen: 0x74ec080; used: 0x513f18; 
 * mem_seg: L1DSRAM; base: 0x11f04000; size: 0x10000; maxblocklen: 0x800; used: 0xf800;  
 * ]| This is an example string of performance data passed via
 * gst_element_post_message.  If dmaiperf is used with gst-launch,
 * the something similar is put to stdout, tagged with INFO.
 *
 * The general format is key: value;  The currently used keys
 * include Timestamp, bps, fps, CPU, and DSP.  For each memory
 * segment, the following keys are used: mem_seg, base, size, 
 * maxblocklen, and used.
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <gst/gst.h>
#include <gst/video/video.h>
#include <ti/sdo/dmai/Dmai.h>
#include "gsttidmaiperf.h"

/* Declare variable used to categorize GST_LOG output */
GST_DEBUG_CATEGORY_STATIC (gst_dmaiperf_debug);
#define GST_CAT_DEFAULT gst_dmaiperf_debug

/* The message is variable length depending on configuration */
#define GST_TIME_FORMAT_MAX_SIZE 4096


/* Element property identifier */
enum
{
  PROP_0,
  PROP_ENGINE_NAME,
  PROP_PRINT_ARM_LOAD
};

static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("ANY")
    );

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("ANY")
    );

/* Declare a global pointer to our element base class */
static GstElementClass *parent_class = NULL;

/* Static Function Declarations */
static void gst_dmaiperf_base_init (gpointer g_class);
static void gst_dmaiperf_class_init (GstDmaiperfClass * g_class);
static void gst_dmaiperf_init (GstDmaiperf * object, GstDmaiperfClass * g_class );
static GstFlowReturn gst_dmaiperf_transform_ip (GstBaseTransform * trans,
    GstBuffer * buf);
static gboolean gst_dmaiperf_start (GstBaseTransform * trans);
static gboolean gst_dmaiperf_stop (GstBaseTransform * trans);
static void gst_dmaiperf_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_dmaiperf_cpu_perf (GstClockTime factor_d, GstClockTime factor_n, GstDmaiperf *dmaiperf ,Int * load);

/******************************************************************************
 * gst_dmaiperf_init
 *    Sets up element specific performance capture data structure.
 *****************************************************************************/
static void
gst_dmaiperf_init (GstDmaiperf * dmaiperf, GstDmaiperfClass * gclass)
{
  gst_base_transform_set_qos_enabled (GST_BASE_TRANSFORM (dmaiperf), TRUE);
  dmaiperf->hDsp = NULL;
  dmaiperf->hEngine = NULL;
  dmaiperf->lastLoadstamp = GST_CLOCK_TIME_NONE;
  dmaiperf->lastWorkload = 0;
  dmaiperf->fps = 0;
  dmaiperf->bps = 0;
  dmaiperf->engineName = NULL;
  dmaiperf->hCpu = NULL;
  dmaiperf->printArmLoad = FALSE;
  dmaiperf->error = NULL;
}

/******************************************************************************
 * gst_dmaiperf_get_type
 *    Defines function pointers for initialization routines for this element.
 *****************************************************************************/
GType
gst_dmaiperf_get_type (void)
{
  static GType object_type = 0;

  if (G_UNLIKELY (object_type == 0)) {
    static const GTypeInfo object_info = {
      sizeof (GstDmaiperfClass),
      gst_dmaiperf_base_init,
      NULL,
      (GClassInitFunc) gst_dmaiperf_class_init,
      NULL,
      NULL,
      sizeof (GstDmaiperf),
      0,
      (GInstanceInitFunc) gst_dmaiperf_init
    };

    object_type = g_type_register_static (GST_TYPE_BASE_TRANSFORM,
        "GstDmaiperf", &object_info, (GTypeFlags) 0);

    /* Initialize GST_LOG for this object */
    GST_DEBUG_CATEGORY_INIT (gst_dmaiperf_debug, "dmaiperf", 0,
        "Dmai performance tool");

    GST_LOG ("initialized get_type\n");
  }

  return object_type;
};


/******************************************************************************
 * gst_dmaiperf_base_init
 *    Initializes element base class.
 ******************************************************************************/
static void
gst_dmaiperf_base_init (gpointer gclass)
{
  static GstElementDetails element_details = {
    "Dmai Performance Identity element",
    "Filter/Perf",
    "EXPERMINTAL Identity element used get pipeline performance data",
    "Lissandro Mendez,www.RidgeRun.com"
  };

  GstElementClass *element_class = GST_ELEMENT_CLASS (gclass);

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&src_factory));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&sink_factory));
  gst_element_class_set_details (element_class, &element_details);
}

static GstFlowReturn
gst_dmaiperf_prepare_output_buffer (GstBaseTransform * trans,
  GstBuffer * in_buf, gint out_size, GstCaps * out_caps, GstBuffer ** out_buf)
{
   *out_buf = gst_buffer_ref (in_buf);
   return GST_FLOW_OK;
}

/******************************************************************************
 * gst_dmaiperf_class_init
 *    Initializes the Dmaiperf class.
 ******************************************************************************/
static void
gst_dmaiperf_class_init (GstDmaiperfClass * klass)
{
  GObjectClass *gobject_class;
  GstBaseTransformClass *trans_class;


  gobject_class = (GObjectClass *) klass;

  gobject_class->set_property = gst_dmaiperf_set_property;
  gobject_class = (GObjectClass *) klass;
  trans_class = (GstBaseTransformClass *) klass;

  trans_class->transform_ip = GST_DEBUG_FUNCPTR (gst_dmaiperf_transform_ip);
  trans_class->prepare_output_buffer = GST_DEBUG_FUNCPTR (gst_dmaiperf_prepare_output_buffer);
  trans_class->start = GST_DEBUG_FUNCPTR (gst_dmaiperf_start);
  trans_class->stop = GST_DEBUG_FUNCPTR (gst_dmaiperf_stop);

  trans_class->passthrough_on_same_caps = FALSE;

  parent_class = g_type_class_peek_parent (klass);

  g_object_class_install_property (gobject_class, PROP_ENGINE_NAME,
      g_param_spec_string ("engine-name", "engine-name",
          "Engine Name", NULL, G_PARAM_WRITABLE));

  g_object_class_install_property (gobject_class, PROP_PRINT_ARM_LOAD,
      g_param_spec_boolean ("print-arm-load", "print-arm-load",
          "Print the CPU load info", FALSE, G_PARAM_WRITABLE));

  
  GST_LOG ("initialized class init\n");
}

/******************************************************************************
 * gst_tividenc_set_property
  *     Set element properties when requested.
   ******************************************************************************/
static void
gst_dmaiperf_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstDmaiperf *dmaiperf = GST_DMAIPERF (object);

  GST_LOG ("begin set_property\n");

  switch (prop_id) {
    case PROP_ENGINE_NAME:
      dmaiperf->engineName = g_strdup(g_value_get_string(value));
      break;

    case PROP_PRINT_ARM_LOAD:
      dmaiperf->printArmLoad = g_value_get_boolean(value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  GST_LOG ("end set_property\n");
}

/******************************************************************************
 * gst_dmaiperf_start
 *    Start measuring pipeline performance
 ******************************************************************************/
static gboolean
gst_dmaiperf_start (GstBaseTransform * trans)
{
  GstDmaiperf *dmaiperf = (GstDmaiperf *) trans;

  /* Make sure we know what codec we're using */
  if (!dmaiperf->engineName) {
    GST_ELEMENT_WARNING (dmaiperf, STREAM, CODEC_NOT_FOUND, (NULL),
        ("Engine name not specified, not printing DSP information"));
  } else {
      dmaiperf->hEngine = Engine_open ((Char *) dmaiperf->engineName, NULL, NULL);

      if (dmaiperf->hEngine == NULL) {
        GST_ELEMENT_ERROR (dmaiperf, STREAM, CODEC_NOT_FOUND, (NULL),
            ("failed to open codec engine \"%s\"", dmaiperf->engineName));
        return FALSE;
      }

      if (!dmaiperf->hDsp) {
        dmaiperf->hDsp = Engine_getServer (dmaiperf->hEngine);
        if (!dmaiperf->hDsp) {
          GST_ELEMENT_WARNING (dmaiperf, STREAM, ENCODE, (NULL),
              ("Failed to open the DSP Server handler, unable to report DSP load"));
        } else {
          GST_ELEMENT_INFO (dmaiperf, STREAM, ENCODE, (NULL),
              ("Printing DSP load every 1 second..."));
        }
      }
  }

  if (dmaiperf->printArmLoad){
    Cpu_Attrs cpuAttrs = Cpu_Attrs_DEFAULT;
    dmaiperf->hCpu = Cpu_create(&cpuAttrs);
  }

  dmaiperf->error = g_error_new(GST_CORE_ERROR,GST_CORE_ERROR_TAG,"Performance Information");

  return TRUE;
}

/******************************************************************************
 * gst_dmaiperf_stop
 *    Stop measuring pipeline performance
 ******************************************************************************/

static gboolean
gst_dmaiperf_stop (GstBaseTransform * trans)
{
  GstDmaiperf *dmaiperf = (GstDmaiperf *) trans;
  if(dmaiperf->engineName){
    g_free((gpointer)dmaiperf->engineName);
  }

  if (dmaiperf->error)
    g_error_free(dmaiperf->error);

  if (dmaiperf->hEngine) {
    GST_DEBUG ("closing the engine\n");
    Engine_close (dmaiperf->hEngine);
    dmaiperf->hEngine = NULL;
  }

  if (dmaiperf->hDsp) {
    dmaiperf->hDsp = NULL;
  }

  if (dmaiperf->hCpu) {
    Cpu_delete (dmaiperf->hCpu);
    dmaiperf->hCpu = NULL;
  }

  return TRUE;
}


/******************************************************************************
 * gst_tividresize_transform
  *    Transforms one incoming buffer to one outgoing buffer.
   *****************************************************************************/
static GstFlowReturn
gst_dmaiperf_transform_ip (GstBaseTransform * trans, GstBuffer * buf)
{
  GstDmaiperf *dmaiperf = GST_DMAIPERF (trans);
  GST_LOG ("Transform function\n");

  GstClockTime time = gst_util_get_timestamp ();
  if (!GST_CLOCK_TIME_IS_VALID (dmaiperf->lastLoadstamp) ||
        (GST_CLOCK_TIME_IS_VALID (time) &&
            GST_CLOCK_DIFF (dmaiperf->lastLoadstamp, time) > GST_SECOND)) {
      gchar info[GST_TIME_FORMAT_MAX_SIZE];
      gint idx;
      /*Real data per second: Time spent / unit (1000msec)*/
      
      GstClockTime factor_n = GST_TIME_AS_MSECONDS(GST_CLOCK_DIFF (dmaiperf->lastLoadstamp, time));
      GstClockTime factor_d = GST_TIME_AS_MSECONDS(GST_SECOND);
      
      idx = g_snprintf (info, GST_TIME_FORMAT_MAX_SIZE, "Timestamp: %" GST_TIME_FORMAT"; "
          "bps: %llu; "
          "fps: %llu; ",
          GST_TIME_ARGS (time), (dmaiperf->bps * factor_d / factor_n), (dmaiperf->fps * factor_d / factor_n));

      dmaiperf->fps = 0;
      dmaiperf->bps = 0;

      if (dmaiperf->hCpu){
          Int load;
          gst_dmaiperf_cpu_perf(factor_d, factor_n, dmaiperf, &load);
          idx += g_snprintf (&info[idx], GST_TIME_FORMAT_MAX_SIZE - idx,
              "CPU: %d; ",load);
      }

      if (dmaiperf->hDsp) {
          gint32 nsegs, i;
          guint32 load = Server_getCpuLoad (dmaiperf->hDsp);
          idx += g_snprintf (&info[idx], GST_TIME_FORMAT_MAX_SIZE-idx,
              "DSP: %d; ", load);

          Server_getNumMemSegs (dmaiperf->hDsp, &nsegs);
          for (i = 0; i < nsegs; i++) {
            Server_MemStat ms;
            Server_getMemStat (dmaiperf->hDsp, i, &ms);
            idx += g_snprintf (&info[idx], GST_TIME_FORMAT_MAX_SIZE - idx,
                "mem_seg: %s; base: 0x%x; size: 0x%x; maxblocklen: 0x%x; used: 0x%x; ",
                ms.name, (unsigned int) ms.base, (unsigned int) ms.size,
                (unsigned int) ms.maxBlockLen, (unsigned int) ms.used);
          }
      }

      if (idx ==  GST_TIME_FORMAT_MAX_SIZE){
	/* more info than buffer can hold, make sure it has reasonable termination. */
	info[GST_TIME_FORMAT_MAX_SIZE - 2] = '*';
	info[GST_TIME_FORMAT_MAX_SIZE - 1] = '\0';
      }

      gst_element_post_message(
        (GstElement *)dmaiperf,
        gst_message_new_info((GstObject *)dmaiperf, dmaiperf->error, 
          (const gchar *)info));
      dmaiperf->lastLoadstamp = time;
  }

  dmaiperf->fps++;
  dmaiperf->bps+= GST_BUFFER_SIZE(buf);

  return GST_FLOW_OK;;
}

/****************************************************************************
* gst_dmaiperf_cpu_perf
*    Returns the cpu's workload
*****************************************************************************/
static void
gst_dmaiperf_cpu_perf (GstClockTime factor_d, GstClockTime factor_n,
                        GstDmaiperf *dmaiperf, Int * load)
{
    FILE * pStat;

    char str[4];
    int workload[7];
    int totalWorkload;

    pStat = fopen ("/proc/stat", "r");
    if ( fscanf (pStat,"%s%d%d%d%d%d%d%d", &str[0], &workload[0], &workload[1],
                 &workload[2], &workload[3], &workload[4], &workload[5],
                 &workload[6]) != 8){
        GST_ELEMENT_WARNING (dmaiperf, STREAM, ENCODE, (NULL),
              ("can't read /proc/stat\n"));
        dmaiperf->lastWorkload = 0;
        *load = 0;
    } else {
        totalWorkload =  workload[0] + workload[1] + workload[2] + workload[5] + workload[6];
        if (dmaiperf->lastWorkload != 0){
            *load = (totalWorkload - dmaiperf->lastWorkload) * factor_d / factor_n;
        } else {
            *load = 0;
        }
        dmaiperf->lastWorkload = totalWorkload;
    }
    fclose (pStat);
}
