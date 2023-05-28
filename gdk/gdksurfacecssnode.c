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

#include "config.h"

#include "gdksurfacecssnodeprivate.h"

#include "gtk/gtkcssanimatedstyleprivate.h"
#include "gtk/gtksettingsprivate.h"

G_DEFINE_TYPE (GdkSurfaceCssNode, gdk_surface_css_node, GTK_TYPE_CSS_NODE)

static GdkFrameClock *
gdk_surface_css_node_get_frame_clock (GtkCssNode *node)
{
  GdkSurfaceCssNode *surface_node = GDK_SURFACE_CSS_NODE (node);

  if (surface_node->surface == NULL)
    return NULL;

  if (!gtk_settings_get_enable_animations (gtk_settings_get_for_display (gdk_surface_get_display (surface_node->surface))))
    return NULL;

  return gdk_surface_get_frame_clock (surface_node->surface);
}


static void
gdk_surface_css_node_queue_callback (GdkFrameClock  *frame_clock,
                                     gpointer       user_data)
{
  GtkCssNode *node = user_data;
  gtk_css_node_invalidate_frame_clock (node, TRUE);
  gtk_css_node_validate (node);
}

static void
gdk_surface_css_node_queue_validate (GtkCssNode *node)
{
  GdkSurfaceCssNode *surface_node = GDK_SURFACE_CSS_NODE (node);
  GdkFrameClock *frame_clock = gdk_surface_css_node_get_frame_clock (node);

  if (frame_clock)
    {
      surface_node->validate_cb_id = g_signal_connect (frame_clock, "update",
                                                       G_CALLBACK (gdk_surface_css_node_queue_callback),
                                                       node);
      gdk_frame_clock_begin_updating (frame_clock);
    }
  else
    {
      gtk_css_node_validate (node);
    }
}

static void
gdk_surface_css_node_dequeue_validate (GtkCssNode *node)
{
  GdkSurfaceCssNode *surface_node = GDK_SURFACE_CSS_NODE (node);
  GdkFrameClock *frame_clock = gdk_surface_css_node_get_frame_clock (node);

  if (frame_clock && surface_node->validate_cb_id != 0)
    {
      g_signal_handler_disconnect (frame_clock, surface_node->validate_cb_id);
      surface_node->validate_cb_id = 0;
    }
}


static GtkStyleProvider *
gdk_surface_css_node_get_style_provider (GtkCssNode *node)
{
  GdkSurfaceCssNode *surface_node = GDK_SURFACE_CSS_NODE (node);
  GtkStyleCascade *cascade;

  if (surface_node->surface == NULL)
    return NULL;

  cascade = _gtk_settings_get_style_cascade (gtk_settings_get_for_display (gdk_surface_get_display (surface_node->surface)),
                                             gdk_surface_get_scale_factor (surface_node->surface));
  return GTK_STYLE_PROVIDER (cascade);
}

static void
gdk_surface_css_node_class_init (GdkSurfaceCssNodeClass *klass)
{
  GtkCssNodeClass *node_class = GTK_CSS_NODE_CLASS (klass);

  node_class->queue_validate = gdk_surface_css_node_queue_validate;
  node_class->dequeue_validate = gdk_surface_css_node_dequeue_validate;
  node_class->get_style_provider = gdk_surface_css_node_get_style_provider;
  node_class->get_frame_clock = gdk_surface_css_node_get_frame_clock;
}

GtkCssNode *
gdk_surface_css_node_new (GdkSurface *surface)
{
  GdkSurfaceCssNode *result;

  g_return_val_if_fail (GDK_IS_SURFACE (surface), NULL);

  result = g_object_new (GDK_TYPE_SURFACE_CSS_NODE, NULL);
  result->surface = surface;

  return GTK_CSS_NODE (result);
}

static void
gdk_surface_css_node_init (GdkSurfaceCssNode *node)
{
  node->validate_cb_id = 0;
}

void
gdk_surface_css_node_surface_destroyed (GdkSurfaceCssNode *node)
{
  g_return_if_fail (GDK_IS_SURFACE_CSS_NODE (node));
  g_return_if_fail (node->surface != NULL);

  node->surface = NULL;
  /* Contents of this node are now undefined.
   * So we don't clear the style or anything.
   */
}

GdkSurface *
gdk_surface_css_node_get_surface (GdkSurfaceCssNode *node)
{
  g_return_val_if_fail (GDK_IS_SURFACE_CSS_NODE (node), NULL);

  return node->surface;
}

