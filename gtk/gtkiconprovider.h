
/*
 * Copyright © 2024 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Matthias Clasen <mclasen@redhat.com>
 */

#pragma once

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtktypes.h>
#include <gdk/gdkpaintable.h>
#include <gdk/gdkdisplay.h>

G_BEGIN_DECLS

#define GTK_TYPE_ICON_PROVIDER (gtk_icon_provider_get_type ())

GDK_AVAILABLE_IN_4_16
G_DECLARE_INTERFACE (GtkIconProvider, gtk_icon_provider, GTK, ICON_PROVIDER, GObject)

struct _GtkIconProviderInterface
{
  GTypeInterface g_iface;

  GdkPaintable * (* lookup_icon) (GtkIconProvider  *provider,
                                  const char       *icon_name,
                                  int               size,
                                  float             scale);
};

GDK_AVAILABLE_IN_4_16
void              gtk_icon_provider_set_for_display (GdkDisplay      *display,
                                                     GtkIconProvider *provider);

GDK_AVAILABLE_IN_4_16
GtkIconProvider * gtk_icon_provider_get_for_display (GdkDisplay      *display);

GDK_AVAILABLE_IN_4_16
GdkPaintable *    gtk_lookup_icon                   (GdkDisplay      *display,
                                                     const char      *icon_name,
                                                     int              size,
                                                     float            scale);

G_END_DECLS

