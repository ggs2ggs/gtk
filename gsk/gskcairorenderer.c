/*
 * Copyright © 2016  Endless 
 *             2018  Benjamin Otte
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
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include "config.h"

#include "gskcairorenderer.h"

#include "gskdebugprivate.h"
#include "gskrendererprivate.h"
#include "gskrendernodeprivate.h"

#include "gdk/gdkcolorprofileprivate.h"
#include "gdk/gdkmemorytextureprivate.h"
#include "gdk/gdktextureprivate.h"

#ifdef G_ENABLE_DEBUG
typedef struct {
  GQuark cpu_time;
  GQuark gpu_time;
} ProfileTimers;
#endif

struct _GskCairoRenderer
{
  GskRenderer parent_instance;

  GdkCairoContext *cairo_context;

  gboolean color_managed;
#ifdef G_ENABLE_DEBUG
  ProfileTimers profile_timers;
#endif
};

struct _GskCairoRendererClass
{
  GskRendererClass parent_class;
};

G_DEFINE_TYPE (GskCairoRenderer, gsk_cairo_renderer, GSK_TYPE_RENDERER)

static gboolean
gsk_cairo_renderer_realize (GskRenderer  *renderer,
                            GdkSurface   *surface,
                            GError      **error)
{
  GskCairoRenderer *self = GSK_CAIRO_RENDERER (renderer);

  self->cairo_context = gdk_surface_create_cairo_context (surface);

  return TRUE;
}

static void
gsk_cairo_renderer_unrealize (GskRenderer *renderer)
{
  GskCairoRenderer *self = GSK_CAIRO_RENDERER (renderer);

  g_clear_object (&self->cairo_context);
}

static void
gsk_cairo_renderer_do_render (GskRenderer   *renderer,
                              cairo_t       *cr,
                              GskRenderNode *root)
{
  GskCairoRenderer *self = GSK_CAIRO_RENDERER (renderer);
#ifdef G_ENABLE_DEBUG
  GskProfiler *profiler;
  gint64 cpu_time;
#endif

#ifdef G_ENABLE_DEBUG
  profiler = gsk_renderer_get_profiler (renderer);
  gsk_profiler_timer_begin (profiler, self->profile_timers.cpu_time);
#endif

  gsk_render_node_draw (root, cr);

#ifdef G_ENABLE_DEBUG
  cpu_time = gsk_profiler_timer_end (profiler, self->profile_timers.cpu_time);
  gsk_profiler_timer_set (profiler, self->profile_timers.cpu_time, cpu_time);

  gsk_profiler_push_samples (profiler);
#endif
}

static GdkTexture *
gsk_cairo_renderer_render_texture (GskRenderer           *renderer,
                                   GskRenderNode         *root,
                                   const graphene_rect_t *viewport)
{
  GskCairoRenderer *self = GSK_CAIRO_RENDERER (renderer);
  GdkTexture *texture;
  cairo_surface_t *surface;
  cairo_t *cr;

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, ceil (viewport->size.width), ceil (viewport->size.height));
  if (self->color_managed)
      gdk_cairo_surface_set_color_profile (surface, gdk_color_profile_get_srgb_linear ());

  cr = cairo_create (surface);

  cairo_translate (cr, - viewport->origin.x, - viewport->origin.y);

  gsk_cairo_renderer_do_render (renderer, cr, root);

  cairo_destroy (cr);

  texture = gdk_texture_new_for_surface (surface);
  cairo_surface_destroy (surface);

  return texture;
}

static void
gsk_cairo_renderer_render (GskRenderer          *renderer,
                           GskRenderNode        *root,
                           const cairo_region_t *region)
{
  GskCairoRenderer *self = GSK_CAIRO_RENDERER (renderer);
  cairo_t *cr;

  gdk_draw_context_begin_frame (GDK_DRAW_CONTEXT (self->cairo_context),
                                region);
  cr = gdk_cairo_context_cairo_create (self->cairo_context);

  g_return_if_fail (cr != NULL);

#ifdef G_ENABLE_DEBUG
  if (GSK_RENDERER_DEBUG_CHECK (renderer, GEOMETRY))
    {
      GdkSurface *surface = gsk_renderer_get_surface (renderer);

      cairo_save (cr);
      cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
      cairo_rectangle (cr,
                       0, 0,
                       gdk_surface_get_width (surface), gdk_surface_get_height (surface));
      cairo_set_source_rgba (cr, 0, 0, 0.85, 0.5);
      cairo_stroke (cr);
      cairo_restore (cr);
    }
#endif

  if (!self->color_managed ||
      gdk_color_profile_is_linear (gdk_cairo_get_color_profile (cr)))
    {
      gsk_cairo_renderer_do_render (renderer, cr, root);
    }
  else
    {
      GdkSurface *surface = gsk_renderer_get_surface (renderer);
      GdkColorProfile *target_profile = gdk_cairo_get_color_profile (cr);
      cairo_surface_t *cairo_surface;
      cairo_t *cr2;
      GdkTexture *color_correct;
      const cairo_region_t *frame_region;
      cairo_rectangle_int_t extents;
      guint i, n;

      frame_region = gdk_draw_context_get_frame_region (GDK_DRAW_CONTEXT (self->cairo_context));
      cairo_region_get_extents (frame_region, &extents);
      /* We can't use cairo_push_group() here, because we'd lose the
       * color profile information. */
      cairo_surface = gdk_surface_create_similar_surface (surface,
                                                          CAIRO_CONTENT_COLOR_ALPHA,
                                                          extents.width,
                                                          extents.height);
      gdk_cairo_surface_set_color_profile (cairo_surface,
                                           gdk_color_profile_get_srgb_linear ());

      cr2 = cairo_create (cairo_surface);
      cairo_translate (cr2, -extents.x, -extents.y);
      gdk_cairo_region (cr2, frame_region);
      cairo_clip (cr2);
      gsk_cairo_renderer_do_render (renderer, cr2, root);
      cairo_destroy (cr2);

      color_correct = gdk_texture_new_for_surface (cairo_surface);
      cairo_surface_destroy (cairo_surface);
      n = cairo_region_num_rectangles (frame_region);
      for (i = 0; i < n; i++)
        {
          cairo_rectangle_int_t rect;
          GdkMemoryTexture *sub;

          cairo_region_get_rectangle (frame_region, i, &rect);
          rect.x -= extents.x;
          rect.y -= extents.y;

          sub = gdk_memory_texture_convert (g_object_ref (GDK_MEMORY_TEXTURE (color_correct)),
                                            GDK_MEMORY_DEFAULT,
                                            target_profile,
                                            &rect);
          cairo_surface = gdk_texture_download_surface (GDK_TEXTURE (sub), target_profile);
          cairo_set_source_surface (cr, cairo_surface, rect.x + extents.x, rect.y + extents.y);
          cairo_paint (cr);
          cairo_surface_destroy (cairo_surface);
          g_object_unref (sub);
        }
      g_object_unref (color_correct);

    }

  cairo_destroy (cr);

  gdk_draw_context_end_frame (GDK_DRAW_CONTEXT (self->cairo_context));
}

static void
gsk_cairo_renderer_class_init (GskCairoRendererClass *klass)
{
  GskRendererClass *renderer_class = GSK_RENDERER_CLASS (klass);

  renderer_class->realize = gsk_cairo_renderer_realize;
  renderer_class->unrealize = gsk_cairo_renderer_unrealize;
  renderer_class->render = gsk_cairo_renderer_render;
  renderer_class->render_texture = gsk_cairo_renderer_render_texture;
}

static void
gsk_cairo_renderer_init (GskCairoRenderer *self)
{
  self->color_managed = TRUE;

#ifdef G_ENABLE_DEBUG
  GskProfiler *profiler = gsk_renderer_get_profiler (GSK_RENDERER (self));

  self->profile_timers.cpu_time = gsk_profiler_add_timer (profiler, "cpu-time", "CPU time", FALSE, TRUE);
#endif
}

/**
 * gsk_cairo_renderer_new:
 *
 * Creates a new Cairo renderer.
 *
 * The Cairo renderer is the fallback renderer drawing in ways similar
 * to how GTK 3 drew its content. Its primary use is as comparison tool.
 *
 * The Cairo renderer is incomplete. It cannot render 3D transformed
 * content and will instead render an error marker. Its usage should be
 * avoided.
 *
 * Returns: a new Cairo renderer.
 **/
GskRenderer *
gsk_cairo_renderer_new (void)
{
  return g_object_new (GSK_TYPE_CAIRO_RENDERER, NULL);
}
