/* gdkcolorstate.c
 *
 * Copyright 2024 Matthias Clasen
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

#include "gdkcolorstateprivate.h"

#include <math.h>

/**
 * GdkColorState:
 *
 * A `GdkColorState` object provides the information to interpret
 * colors and pixels in a variety of ways.
 *
 * They are also known as
 * [*color spaces*](https://en.wikipedia.org/wiki/Color_space).
 *
 * Crucially, GTK knows how to convert colors from one color
 * state to another.
 *
 * `GdkColorState objects are immutable and therefore threadsafe.
 *
 * Since 4.16
 */

G_DEFINE_BOXED_TYPE (GdkColorState, gdk_color_state,
                     gdk_color_state_ref, gdk_color_state_unref);

/* {{{ Public API */

/**
 * gdk_color_state_ref:
 * @self: a `GdkColorState`
 *
 * Increase the reference count of @self.
 *
 * Returns: the object that was passed in
 * 
 * Since: 4.16
 */
GdkColorState *
(gdk_color_state_ref) (GdkColorState *self)
{
  return _gdk_color_state_ref (self);
}

/**
 * gdk_color_state_unref:
 * @self:a `GdkColorState`
 *
 * Decrease the reference count of @self.
 *
 * Unless @self is static, it will be freed
 * when the reference count reaches zero.
 *
 * Since: 4.16
 */
void
(gdk_color_state_unref) (GdkColorState *self)
{
  _gdk_color_state_unref (self);
}

/**
 * gdk_color_state_get_srgb:
 *
 * Returns the color state object representing the sRGB color space.
 *
 * Since: 4.16
 */
GdkColorState *
gdk_color_state_get_srgb (void)
{
  return GDK_COLOR_STATE_SRGB;
}

/**
 * gdk_color_state_get_srgb_linear:
 *
 * Returns the color state object representing the linearized sRGB color space.
 *
 * Since: 4.16
 */
GdkColorState *
gdk_color_state_get_srgb_linear (void)
{
  return GDK_COLOR_STATE_SRGB_LINEAR;
}

/**
 * gdk_color_state_get_xyz:
 *
 * Returns the color state object representing the XYZ color space.
 *
 * Since: 4.16
 */
GdkColorState *
gdk_color_state_get_xyz (void)
{
  return GDK_COLOR_STATE_XYZ;
}

/**
 * gdk_color_state_equal:
 * @self: a `GdkColorState`
 * @other: another `GdkColorStatee`
 *
 * Compares two `GdkColorStates` for equality.
 *
 * Note that this function is not guaranteed to be perfect and two objects
 * describing the same color state may compare not equal. However, different
 * color states will never compare equal.
 *
 * Returns: %TRUE if the two color states compare equal
 *
 * Since: 4.16
 */
gboolean
(gdk_color_state_equal) (GdkColorState *self,
                         GdkColorState *other)
{
  return _gdk_color_state_equal (self, other);
}

/* }}} */
/* {{{ Default implementation */
/* {{{ Vfuncs */

static gboolean
gdk_default_color_state_equal (GdkColorState *self,
                               GdkColorState *other)
{
  return self == other;
}

static const char *
gdk_default_color_state_get_name (GdkColorState *color_state)
{
  GdkDefaultColorState *self = (GdkDefaultColorState *) color_state;

  return self->name;
}

static gboolean
gdk_default_color_state_has_srgb_tf (GdkColorState  *color_state,
                                     GdkColorState **out_no_srgb)
{
  GdkDefaultColorState *self = (GdkDefaultColorState *) color_state;

  if (self->no_srgb == NULL)
    return FALSE;

  if (out_no_srgb)
    *out_no_srgb = gdk_color_state_ref (self->no_srgb);

  return TRUE;
}

static GdkFloatColorConvert
gdk_default_color_state_get_convert_to (GdkColorState  *color_state,
                                        GdkColorState  *target)
{
  GdkDefaultColorState *self = (GdkDefaultColorState *) color_state;

  if (!GDK_IS_DEFAULT_COLOR_STATE (target))
    return NULL;

  return self->convert_to[GDK_DEFAULT_COLOR_STATE_ID (target)];
}

/* }}} */
/* {{{ Conversion functions */

#define COORDINATE_TRANSFORM(name, tf) \
static void \
name(GdkColorState  *self, \
     float         (*values)[4], \
     gsize           n_values) \
{ \
  for (gsize i = 0; i < n_values; i++) \
    { \
      values[i][0] = tf (values[i][0]); \
      values[i][1] = tf (values[i][1]); \
      values[i][2] = tf (values[i][2]); \
    } \
}

static inline float
srgb_oetf (float v)
{
  if (v > 0.0031308)
    return 1.055 * pow (v, 1 / 2.4) - 0.055;
  else
    return 12.92 * v;
}

static inline float
srgb_eotf (float v)
{
  if (v >= 0.04045)
    return powf (((v + 0.055) / (1 + 0.055)), 2.4);
  else
    return v / 12.92;
}

COORDINATE_TRANSFORM(gdk_default_srgb_to_srgb_linear, srgb_eotf)
COORDINATE_TRANSFORM(gdk_default_srgb_linear_to_srgb, srgb_oetf)

static inline void
vec3_multiply (const float matrix[3][3],
               const float vec[3],
               float       res[3])
{
  res[0] = matrix[0][0] * vec[0] + matrix[0][1] * vec[1] + matrix[0][2] * vec[2];
  res[1] = matrix[1][0] * vec[0] + matrix[1][1] * vec[1] + matrix[1][2] * vec[2];
  res[2] = matrix[2][0] * vec[0] + matrix[2][1] * vec[1] + matrix[2][2] * vec[2];
}

#define LINEAR_TRANSFORM(name, matrix) \
static void \
name (GdkColorState  *self, \
      float         (*values)[4], \
      gsize           n_values) \
{ \
  for (gsize i = 0; i < n_values; i++) \
    { \
      float res[3]; \
\
      vec3_multiply (matrix, values[i], res); \
\
      values[i][0] = res[0]; \
      values[i][1] = res[1]; \
      values[i][2] = res[2]; \
    } \
}

static const float srgb_linear_to_xyz[3][3] = {
  { (506752.0 / 1228815.0),  (87881.0 / 245763.0),   (12673.0 /   70218.0) },
  {  (87098.0 /  409605.0), (175762.0 / 245763.0),   (12673.0 /  175545.0) },
  {  ( 7918.0 /  409605.0),  (87881.0 / 737289.0), (1001167.0 / 1053270.0) },
};

static const float xyz_to_srgb_linear[3][3] = {
  {    (12831.0 /   3959.0),     - (329.0 /    214.0),  - (1974.0 /   3959.0) },
  { - (851781.0 / 878810.0),   (1648619.0 / 878810.0),   (36519.0 / 878810.0) },
  {      (705.0 /  12673.0),    - (2585.0 /  12673.0),     (705.0 /    667.0) },
};

LINEAR_TRANSFORM(gdk_default_xyz_to_srgb_linear, xyz_to_srgb_linear)
LINEAR_TRANSFORM(gdk_default_srgb_linear_to_xyz, srgb_linear_to_xyz)

#define CONCAT(name, f1, f2) \
static void \
name (GdkColorState  *self, \
      float         (*values)[4], \
      gsize           n_values) \
{ \
  f1 (self, values, n_values); \
  f2 (self, values, n_values); \
}

CONCAT(gdk_default_xyz_to_srgb, gdk_default_xyz_to_srgb_linear, gdk_default_srgb_linear_to_srgb);
CONCAT(gdk_default_srgb_to_xyz, gdk_default_srgb_to_srgb_linear, gdk_default_srgb_linear_to_xyz);

/* }}} */

static const
GdkColorStateClass GDK_DEFAULT_COLOR_STATE_CLASS = {
  .free = NULL, /* crash here if this ever happens */
  .equal = gdk_default_color_state_equal,
  .get_name = gdk_default_color_state_get_name,
  .has_srgb_tf = gdk_default_color_state_has_srgb_tf,
  .get_convert_to = gdk_default_color_state_get_convert_to,
};

GdkDefaultColorState gdk_default_color_states[] = {
  [GDK_COLOR_STATE_ID_SRGB] = {
    .parent = {
      .klass = &GDK_DEFAULT_COLOR_STATE_CLASS,
      .ref_count = 0,
      .depth = GDK_MEMORY_U8_SRGB,
      .rendering_color_state = GDK_COLOR_STATE_SRGB_LINEAR,
    },
    .name = "srgb",
    .no_srgb = GDK_COLOR_STATE_SRGB_LINEAR,
    .convert_to = {
      [GDK_COLOR_STATE_ID_SRGB_LINEAR] = gdk_default_srgb_to_srgb_linear,
      [GDK_COLOR_STATE_ID_XYZ]  = gdk_default_srgb_to_xyz,
    },
  },
  [GDK_COLOR_STATE_ID_SRGB_LINEAR] = {
    .parent = {
      .klass = &GDK_DEFAULT_COLOR_STATE_CLASS,
      .ref_count = 0,
      .depth = GDK_MEMORY_U8,
      .rendering_color_state = GDK_COLOR_STATE_SRGB_LINEAR,
    },
    .name = "srgb-linear",
    .no_srgb = NULL,
    .convert_to = {
      [GDK_COLOR_STATE_ID_SRGB] = gdk_default_srgb_linear_to_srgb,
      [GDK_COLOR_STATE_ID_XYZ]  = gdk_default_srgb_linear_to_xyz,
    },
  },
  [GDK_COLOR_STATE_ID_XYZ] = {
    .parent = {
      .klass = &GDK_DEFAULT_COLOR_STATE_CLASS,
      .ref_count = 0,
      .depth = GDK_MEMORY_FLOAT16,
      .rendering_color_state = GDK_COLOR_STATE_XYZ,
    },
    .name = "xyz",
    .no_srgb = NULL,
    .convert_to = {
      [GDK_COLOR_STATE_ID_SRGB] = gdk_default_xyz_to_srgb,
      [GDK_COLOR_STATE_ID_SRGB_LINEAR] = gdk_default_xyz_to_srgb_linear,
    },
  },
};

/* }}} */
/* {{{ Private API */

const char *
gdk_color_state_get_name (GdkColorState *self)
{
  return self->klass->get_name (self);
}

/*<private>
 * gdk_color_state_has_srgb_tf:
 * @self: a colorstate
 * @out_no_srgb: (out) (optional) (transfer full): return location for 
 *   non-srgb version of colorstate
 *
 * This function checks if the colorstate uses an SRGB transfer function
 * as final operation. In that case, it is suitable for use with GL_SRGB
 * (and the Vulkan equivalents).
 *
 * Returns: TRUE if a non-srgb version of this colorspace exists.
 **/
gboolean
gdk_color_state_has_srgb_tf (GdkColorState  *self,
                             GdkColorState **out_no_srgb)
{
  if (!GDK_DEBUG_CHECK (LINEAR))
    return FALSE;

  return self->klass->has_srgb_tf (self, out_no_srgb);
}

/* }}} */

/* vim:set foldmethod=marker expandtab: */
