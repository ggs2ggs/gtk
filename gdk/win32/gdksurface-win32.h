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

#ifndef __GDK_SURFACE_WIN32_H__
#define __GDK_SURFACE_WIN32_H__

#include "gdk/win32/gdkprivate-win32.h"
#include "gdk/win32/gdkwin32cursor.h"
#include "gdk/win32/gdkwin32surface.h"
#include "gdk/gdksurfaceprivate.h"
#include "gdk/gdkcursor.h"

#include <windows.h>

#ifdef HAVE_EGL
# include <epoxy/egl.h>
#endif

G_BEGIN_DECLS

typedef enum
{
  GDK_DECOR_ALL         = 1 << 0,
  GDK_DECOR_BORDER      = 1 << 1,
  GDK_DECOR_RESIZEH     = 1 << 2,
  GDK_DECOR_TITLE       = 1 << 3,
  GDK_DECOR_MENU        = 1 << 4,
  GDK_DECOR_MINIMIZE    = 1 << 5,
  GDK_DECOR_MAXIMIZE    = 1 << 6
} GdkWMDecoration;

/* defined in gdkdrop-win32.c */
typedef struct _drop_target_context drop_target_context;

struct _GdkWin32Surface
{
  GdkSurface parent_instance;

  HANDLE handle;

  HICON   hicon_big;
  HICON   hicon_small;

  /* The cursor that GDK set for this window via GdkDevice */
  GdkWin32HCursor *cursor;

  /* When VK_PACKET sends us a leading surrogate, it's stashed here.
   * Later, when another VK_PACKET sends a tailing surrogate, we make up
   * a full unicode character from them, or discard the leading surrogate,
   * if the next key is not a tailing surrogate.
   */
  wchar_t leading_surrogate_keydown;
  wchar_t leading_surrogate_keyup;

  /* Window size hints */
  int hint_flags;
  GdkGeometry hints;

  /* Non-NULL for any window that is registered as a drop target.
   * For OLE2 protocol only.
   */
  drop_target_context *drop_target;

  GdkSurface *transient_owner;
  GSList    *transient_children;
  int        num_transients;
  gboolean   changing_state;

  int initial_x;
  int initial_y;

  /* left/right/top/bottom width of the shadow/resize-grip around the window */
  RECT shadow;

  /* left+right and top+bottom from @shadow */
  int shadow_x;
  int shadow_y;

  /* Set to TRUE when GTK tells us that shadow are 0 everywhere.
   * We don't actually set shadow to 0, we just set this bit.
   */
  guint zero_shadow : 1;
  guint inhibit_configure : 1;

  /* If TRUE, the @temp_styles is set to the styles that were temporarily
   * added to this window.
   */
  guint have_temp_styles : 1;

  /* If TRUE, the window is in the process of being maximized.
   * This is set by WM_SYSCOMMAND and by gdk_win32_surface_maximize (),
   * and is unset when WM_WINDOWPOSCHANGING is handled.
   */
  guint maximizing : 1;

  /* WGL requires that we use CS_OWNDC and keep the hdc around */
  HDC              hdc;

  /* Enable all decorations? */
  gboolean decorate_all;

  /* Temporary styles that this window got for the purpose of
   * handling WM_SYSMENU.
   * They are removed at the first opportunity (usually WM_INITMENU).
   */
  LONG_PTR temp_styles;

  /* scale of window on HiDPI */
  int surface_scale;

  GdkToplevelLayout *toplevel_layout;
  struct {
    int configured_width;
    int configured_height;
    RECT configured_rect;
  } next_layout;

#ifdef HAVE_EGL
  guint egl_force_redraw_all : 1;
#endif
};

struct _GdkWin32SurfaceClass
{
  GdkSurfaceClass parent_class;
};

GType _gdk_win32_surface_get_type (void);

void  _gdk_win32_surface_update_style_bits   (GdkSurface *window);

int   _gdk_win32_surface_get_scale_factor    (GdkSurface *window);

void  _gdk_win32_get_window_client_area_rect (GdkSurface *window,
                                              int         scale,
                                              RECT       *rect);

void gdk_win32_surface_move (GdkSurface *surface,
                             int         x,
                             int         y);

void gdk_win32_surface_move_resize (GdkSurface *window,
                                    int         x,
                                    int         y,
                                    int         width,
                                    int         height);

RECT
gdk_win32_surface_handle_queued_move_resize (GdkDrawContext *draw_context);

#ifdef HAVE_EGL
EGLSurface gdk_win32_surface_get_egl_surface (GdkSurface *surface,
                                              EGLConfig   config,
                                              gboolean    is_dummy);
#endif

G_END_DECLS

#endif /* __GDK_SURFACE_WIN32_H__ */
