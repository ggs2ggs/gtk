/* GTK - The GIMP Toolkit
 * Copyright (C) 2023 Sasha Hale <dgsasha04@gmail.com>
 * Copyright (C) 2014 Benjamin Otte <otte@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "gtk/gtkcssnodeprivate.h"
#include "gdk/gdksurface.h"

G_BEGIN_DECLS

#define GDK_TYPE_SURFACE_CSS_NODE           (gdk_surface_css_node_get_type ())
#define GDK_SURFACE_CSS_NODE(obj)           (G_TYPE_CHECK_INSTANCE_CAST (obj, GDK_TYPE_SURFACE_CSS_NODE, GdkSurfaceCssNode))
#define GDK_SURFACE_CSS_NODE_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST (cls, GDK_TYPE_SURFACE_CSS_NODE, GdkSurfaceCssNodeClass))
#define GDK_IS_SURFACE_CSS_NODE(obj)        (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDK_TYPE_SURFACE_CSS_NODE))
#define GDK_IS_SURFACE_CSS_NODE_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE (obj, GDK_TYPE_SURFACE_CSS_NODE))
#define GDK_SURFACE_CSS_NODE_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_SURFACE_CSS_NODE, GdkSurfaceCssNodeClass))

typedef struct _GdkSurfaceCssNode           GdkSurfaceCssNode;
typedef struct _GdkSurfaceCssNodeClass      GdkSurfaceCssNodeClass;

struct _GdkSurfaceCssNode
{
  GtkCssNode node;

  guint validate_cb_id;
  GdkSurface *surface;
};

struct _GdkSurfaceCssNodeClass
{
  GtkCssNodeClass node_class;
};

GType                   gdk_surface_css_node_get_type           (void) G_GNUC_CONST;

GtkCssNode *            gdk_surface_css_node_new                (GdkSurface           *surface);

void                    gdk_surface_css_node_surface_destroyed  (GdkSurfaceCssNode    *node);

GdkSurface *            gdk_surface_css_node_get_surface        (GdkSurfaceCssNode    *node);

G_END_DECLS

