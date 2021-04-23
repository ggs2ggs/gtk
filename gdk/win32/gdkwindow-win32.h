/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#ifndef __GDK_WINDOW_WIN32_H__
#define __GDK_WINDOW_WIN32_H__

#include "gdk/win32/gdkprivate-win32.h"
#include "gdk/gdkwindowimpl.h"
#include "gdk/gdkcursor.h"

#include <windows.h>

#ifdef GDK_WIN32_ENABLE_EGL
#include <epoxy/egl.h>
#endif

G_BEGIN_DECLS

/* Window implementation for Win32
 */

typedef struct _GdkWindowImplWin32 GdkWindowImplWin32;
typedef struct _GdkWindowImplWin32Class GdkWindowImplWin32Class;

#define GDK_TYPE_WINDOW_IMPL_WIN32              (_gdk_window_impl_win32_get_type ())
#define GDK_WINDOW_IMPL_WIN32(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_WINDOW_IMPL_WIN32, GdkWindowImplWin32))
#define GDK_WINDOW_IMPL_WIN32_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_WINDOW_IMPL_WIN32, GdkWindowImplWin32Class))
#define GDK_IS_WINDOW_IMPL_WIN32(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_WINDOW_IMPL_WIN32))
#define GDK_IS_WINDOW_IMPL_WIN32_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_WINDOW_IMPL_WIN32))
#define GDK_WINDOW_IMPL_WIN32_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_WINDOW_IMPL_WIN32, GdkWindowImplWin32Class))

typedef enum _GdkWin32MonitorDpiType
{
  MDT_EFFECTIVE_DPI  = 0,
  MDT_ANGULAR_DPI    = 1,
  MDT_RAW_DPI        = 2,
  MDT_DEFAULT        = MDT_EFFECTIVE_DPI
} GdkWin32MonitorDpiType;

struct _GdkWindowImplWin32
{
  GdkWindowImpl parent_instance;

  GdkWindow *wrapper;
  HANDLE handle;

  gint8 toplevel_window_type;

  GdkCursor *cursor;
  HICON   hicon_big;
  HICON   hicon_small;

  /* Window size hints */
  gint hint_flags;
  GdkGeometry hints;

  GdkEventMask native_event_mask;

  GdkWindowTypeHint type_hint;

  GdkWindow *transient_owner;
  GSList    *transient_children;
  gint       num_transients;
  gboolean   changing_state;

  gint initial_x;
  gint initial_y;

  /* left/right/top/bottom width of the shadow/resize-grip around the window */
  RECT margins;

  /* left+right and top+bottom from @margins */
  gint margins_x;
  gint margins_y;

  /* Set to TRUE when GTK tells us that margins are 0 everywhere.
   * We don't actually set margins to 0, we just set this bit.
   */
  guint zero_margins : 1;
  guint no_bg : 1;
  guint inhibit_configure : 1;
  guint override_redirect : 1;

  /* If TRUE, the @temp_styles is set to the styles that were temporarily
   * added to this window.
   */
  guint have_temp_styles : 1;

  /* If TRUE, the window is in the process of being maximized.
   * This is set by WM_SYSCOMMAND and by gdk_win32_window_maximize (),
   * and is unset when WM_WINDOWPOSCHANGING is handled.
   */
  guint maximizing : 1;

  cairo_surface_t *cairo_surface;

  /* Unlike window-backed surfaces, DIB-backed surface
   * does not provide a way to query its size,
   * so we have to remember it ourselves.
   */
  gint             dib_width;
  gint             dib_height;

  HDC              repaint_hdc; /* only valid during WM_PAINT */
  cairo_surface_t *repaint_cairo_surface;
  HDC              hdc;
  int              hdc_count;
  HBITMAP          saved_dc_bitmap; /* Original bitmap for dc */

  /* Decorations set by gdk_window_set_decorations() or NULL if unset */
  GdkWMDecoration* decorations;

  /* Temporary styles that this window got for the purpose of
   * handling WM_SYSMENU.
   * They are removed at the first opportunity (usually WM_INITMENU).
   */
  LONG_PTR temp_styles;

  /* Last window rect that we gave the OS for WM_SIZING.
   * Note that this is the window rect, not the client rect.
   */
  RECT last_sizing_rect;

  /* scale of window on HiDPI */
  gint window_scale;
  gint unscaled_width;
  gint unscaled_height;

#ifdef GDK_WIN32_ENABLE_EGL
  EGLSurface egl_surface;
  EGLSurface egl_dummy_surface;
  guint egl_force_redraw_all : 1;
#endif
};

struct _GdkWindowImplWin32Class
{
  GdkWindowImplClass parent_class;
};

GType _gdk_window_impl_win32_get_type (void);

void  _gdk_win32_window_tmp_unset_bg  (GdkWindow *window,
				       gboolean   recurse);
void  _gdk_win32_window_tmp_reset_bg  (GdkWindow *window,
				       gboolean   recurse);

void  _gdk_win32_window_tmp_unset_parent_bg (GdkWindow *window);
void  _gdk_win32_window_tmp_reset_parent_bg (GdkWindow *window);

void  _gdk_win32_window_update_style_bits   (GdkWindow *window);

gint  _gdk_win32_window_get_scale_factor    (GdkWindow *window);

#ifdef GDK_WIN32_ENABLE_EGL
EGLSurface _gdk_win32_window_get_egl_surface (GdkWindow *window,
                                              EGLConfig  config,
                                              gboolean   is_dummy);
#endif

G_END_DECLS

#endif /* __GDK_WINDOW_WIN32_H__ */
