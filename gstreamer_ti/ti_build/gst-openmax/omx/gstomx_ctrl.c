/*
 * Copyright (C) 2011-2012 Texas Instrument Inc.
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

#include "gstomx_ctrl.h"

#include <xdc/std.h>
#include <OMX_TI_Index.h>
#include <OMX_TI_Common.h>
#include <omx_vfdc.h>
#include <omx_ctrl.h>

enum
{
    ARG_0,
    ARG_COMPONENT_ROLE,
    ARG_COMPONENT_NAME,
    ARG_LIBRARY_NAME
};

G_DEFINE_TYPE (GstOmxCtrl, gstomx_ctrl, G_TYPE_OBJECT)

static gint
gstomx_ctrl_convert_string_to_dc_mode (gchar *str)
{
    if (!strcmp (str, "OMX_DC_MODE_1080P_60"))
        return OMX_DC_MODE_1080P_60;

    return 0;
}

gboolean 
gstomx_ctrl_set_dc_mode (GstOmxCtrl *self, char *mode)
{
    OMX_PARAM_VFDC_DRIVERINSTID driverId;
    OMX_ERRORTYPE err;
    GOmxCore *gomx;

    if (!self->port_initialized)
        g_omx_core_init (self->gomx);

    gomx = (GOmxCore*) self->gomx;

    _G_OMX_INIT_PARAM (&driverId);
    driverId.nDrvInstID = 0; /* on chip HDMI */
    driverId.eDispVencMode = gstomx_ctrl_convert_string_to_dc_mode (mode);

    err = OMX_SetParameter (gomx->omx_handle, (OMX_INDEXTYPE) OMX_TI_IndexParamVFDCDriverInstId, 
        &driverId);
    if(err != OMX_ErrorNone)
        return FALSE;

    self->port_initialized = TRUE;

    return TRUE;
}

static void 
gstomx_ctrl_init (GstOmxCtrl * foo)
{
    /* nothing to do */
}

static void
get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
    GstOmxCtrl *self;

    self = GST_OMXCTRL (object);

    switch (prop_id)
    {
        case ARG_COMPONENT_ROLE:
            g_value_set_string (value, self->omx_role);
            break;
        case ARG_COMPONENT_NAME:
            g_value_set_string (value, self->omx_component);
            break;
        case ARG_LIBRARY_NAME:
            g_value_set_string (value, self->omx_library);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
set_property (GObject * object, guint prop_id, const GValue * value,
    GParamSpec * pspec)
{
    GstOmxCtrl *self;

    self = GST_OMXCTRL (object);

    switch (prop_id)
    {
        case ARG_COMPONENT_ROLE:
            g_free (self->omx_role);
            self->omx_role = g_value_dup_string (value);
            break;
        case ARG_COMPONENT_NAME:
            g_free (self->omx_component);
            self->omx_component = g_value_dup_string (value);
            break;
        case ARG_LIBRARY_NAME:
            g_free (self->omx_library);
            self->omx_library = g_value_dup_string (value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
finalize (GObject *obj)
{
    GstOmxCtrl *self;

    self = GST_OMXCTRL (obj);

    if (self->port_initialized)
        g_omx_core_free (self->gomx);

    g_free (self->omx_role);
    g_free (self->omx_component);
    g_free (self->omx_library);
}

static void
gstomx_ctrl_class_init (GstOmxCtrlClass * klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    gobject_class->finalize = finalize;
    gobject_class->set_property = set_property;
    gobject_class->get_property = get_property;

    g_object_class_install_property (gobject_class, ARG_COMPONENT_ROLE,
                                     g_param_spec_string ("component-role", "Component role",
                                                          "Role of the OpenMAX IL component",
                                                           NULL, G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class, ARG_COMPONENT_NAME,
                                     g_param_spec_string ("component-name", "Component name",
                                                          "Name of the OpenMAX IL component to use",
                                                           NULL, G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class, ARG_LIBRARY_NAME,
                                     g_param_spec_string ("library-name", "Library name",
                                                         "Name of the OpenMAX IL implementation library to use",
                                                          NULL, G_PARAM_READWRITE));
}

