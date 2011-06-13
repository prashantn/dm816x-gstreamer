/*
 * Copyright (C) 2010-2011 Texas Instrument Inc.
 *
 * Author: Brijesh Singh <bksingh@ti.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */
#ifndef __GST_OMXCTRL_H__
#define __GST_OMXCTRL_H__

#include <gst/gst.h>
#include "gstomx.h"

#include "gstomx_util.h"

G_BEGIN_DECLS

#define GST_OMX_BASE_CTRL(obj) (GstOmxBaseCtrlDc *) (obj)
#define GST_OMX_BASE_CTRL_TYPE (gst_omx_base_ctrldc_get_type ())
#define GST_OMX_BASE_CTRL_CLASS(obj) (GstOmxBaseCtrlDcClass *) (obj)

/* Type macros for GST_TYPE_OMXCTRL */
#define GST_TYPE_OMXCTRL \
    (gstomx_ctrl_get_type())
#define GST_OMXCTRL(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_OMXCTRL, \
    GstOmxCtrl))
#define GST_OMXCTRL_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_OMXCTRL, \
    GstOmxCtrlClass))
#define GST_IS_OMXCTRL_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_OMXCTRL))
#define GST_OMXCTRL_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_OMXCTRL, \
    GstOmxCtrlClass))

typedef struct _GstOmxCtrl      GstOmxCtrl;
typedef struct _GstOmxCtrlClass GstOmxCtrlClass;

/* _GstOmxCtrl object */
struct _GstOmxCtrl {
    GstObject parent;

    GOmxCore *gomx;
    GOmxPort *in_port;

    char *omx_role;
    char *omx_component;
    char *omx_library;

    gboolean port_initialized;
};

struct _GstOmxCtrlClass {
    GstObjectClass gobject_class;
};

/* External function declarations */
GType  gstomx_ctrl_get_type(void);
gboolean gstomx_ctrl_set_dc_mode (GstOmxCtrl *self, char *mode_string);

G_END_DECLS 

#endif /* GSTOMX_BASE_CTRL_H */

