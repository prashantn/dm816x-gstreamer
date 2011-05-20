
#ifndef __GST_PERF_H__
#define __GST_PERF_H__

#include <pthread.h>

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>


G_BEGIN_DECLS

/* Standard macros for maniuplating perf objects */
#define GST_TYPE_PERF \
  (gst_perf_get_type())
#define GST_PERF(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_PERF,Gstperf))
#define GST_PERF_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_PERF,GstperfClass))
#define GST_IS_PERF(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_PERF))
#define GST_IS_PERF_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_PERF))

typedef struct _Gstperf      Gstperf;
typedef struct _GstperfClass GstperfClass;

/* _Gstperf object */
struct _Gstperf
{
  /* gStreamer infrastructure */
  GstBaseTransform  element;
  GstPad            *sinkpad;
  GstPad            *srcpad;

  /* statistics */
  guint64 frames_count, last_frames_count, total_size;

  GstClockTime start_ts;
  GstClockTime last_ts;
  GstClockTime interval_ts;
  guint data_probe_id;

  gboolean print_fps, print_arm_load, fps_update_interval;
  gboolean  print_throughput;

/*
  gboolean sync;
  gboolean use_text_overlay;
  gboolean signal_measurements;
  GstClockTime fps_update_interval;
  gdouble max_fps;
  gdouble min_fps;
*/
};

/* _GstperfClass object */
struct _GstperfClass
{
  GstBaseTransformClass  parent_class;
};

/* External function enclarations */
GType gst_perf_get_type(void);

G_END_DECLS

#endif /* __GST_PERF_H__ */


