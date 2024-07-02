#pragma once

#include "gdk/gdkcolorstateprivate.h"

#include <cairo.h>

static inline void
gsk_cairo_set_source_rgba (cairo_t       *cr,
                           GdkColorState *ccs,
                           const GdkRGBA *rgba)
{
  float color[4];

  gdk_color_state_from_rgba (ccs, rgba, color);
  cairo_set_source_rgba (cr, color[0], color[1], color[2], color[3]);
}

static inline void
gsk_cairo_pattern_add_color_stop_rgba (cairo_pattern_t *pattern,
                                       GdkColorState   *ccs,
                                       double           offset,
                                       const GdkRGBA   *rgba)
{
  float color[4];

  gdk_color_state_from_rgba (ccs, rgba, color);
  cairo_pattern_add_color_stop_rgba (pattern, offset, color[0], color[1], color[2], color[3]);
}

static inline void
gsk_cairo_rectangle (cairo_t               *cr,
                     const graphene_rect_t *rect)
{
  cairo_rectangle (cr,
                   rect->origin.x, rect->origin.y,
                   rect->size.width, rect->size.height);
}

static inline void
gsk_cairo_surface_convert_color_state (cairo_surface_t *surface,
                                       GdkColorState   *source,
                                       GdkColorState   *target)
{
  cairo_surface_t *image_surface;

  image_surface = cairo_surface_map_to_image (surface, NULL);

  gdk_memory_convert_color_state (cairo_image_surface_get_data (image_surface),
                                  cairo_image_surface_get_stride (image_surface),
                                  GDK_MEMORY_DEFAULT,
                                  source,
                                  target,
                                  cairo_image_surface_get_width (image_surface),
                                  cairo_image_surface_get_height (image_surface));

  cairo_surface_mark_dirty (image_surface);
  cairo_surface_unmap_image (surface, image_surface);
  /* https://gitlab.freedesktop.org/cairo/cairo/-/merge_requests/487 */
  cairo_surface_mark_dirty (surface);
}

