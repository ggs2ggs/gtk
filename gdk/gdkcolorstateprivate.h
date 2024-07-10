#pragma once

#include "gdkcolorstate.h"

#include "gdkdebugprivate.h"
#include "gdkmemoryformatprivate.h"
#include "gdkrgba.h"

typedef enum
{
  GDK_COLOR_STATE_ID_SRGB,
  GDK_COLOR_STATE_ID_SRGB_LINEAR,
  GDK_COLOR_STATE_ID_XYZ,
  GDK_COLOR_STATE_ID_OKLAB,
  GDK_COLOR_STATE_ID_OKLCH,

  GDK_COLOR_STATE_N_IDS
} GdkColorStateId;

typedef struct _GdkColorStateClass GdkColorStateClass;

struct _GdkColorState
{
  const GdkColorStateClass *klass;
  gatomicrefcount ref_count;

  GdkMemoryDepth depth;
  GdkColorState *rendering_color_state;
};

typedef void            (* GdkFloatColorConvert)(GdkColorState  *self,
                                                 float         (*values)[4],
                                                 gsize           n_values);

struct _GdkColorStateClass
{
  void                  (* free)                (GdkColorState  *self);
  gboolean              (* equal)               (GdkColorState  *self,
                                                 GdkColorState  *other);
  const char *          (* get_name)            (GdkColorState  *self);
  gboolean              (* has_srgb_tf)         (GdkColorState  *self,
                                                 GdkColorState **out_no_srgb);
  GdkFloatColorConvert  (* get_convert_to)      (GdkColorState  *self,
                                                 GdkColorState  *target);
};

typedef struct _GdkDefaultColorState GdkDefaultColorState;

struct _GdkDefaultColorState
{
  GdkColorState parent;

  const char *name;
  GdkColorState *no_srgb;
  GdkFloatColorConvert convert_to[GDK_COLOR_STATE_N_IDS];
};

extern GdkDefaultColorState gdk_default_color_states[GDK_COLOR_STATE_N_IDS];

#define GDK_COLOR_STATE_SRGB        ((GdkColorState *) &gdk_default_color_states[GDK_COLOR_STATE_ID_SRGB])
#define GDK_COLOR_STATE_SRGB_LINEAR ((GdkColorState *) &gdk_default_color_states[GDK_COLOR_STATE_ID_SRGB_LINEAR])
#define GDK_COLOR_STATE_XYZ         ((GdkColorState *) &gdk_default_color_states[GDK_COLOR_STATE_ID_XYZ])
#define GDK_COLOR_STATE_OKLAB       ((GdkColorState *) &gdk_default_color_states[GDK_COLOR_STATE_ID_OKLAB])
#define GDK_COLOR_STATE_OKLCH       ((GdkColorState *) &gdk_default_color_states[GDK_COLOR_STATE_ID_OKLCH])

#define GDK_IS_DEFAULT_COLOR_STATE(c) ((GdkDefaultColorState *) (c) >= &gdk_default_color_states[0] && \
                                       (GdkDefaultColorState *) (c) < &gdk_default_color_states[GDK_COLOR_STATE_N_IDS])
#define GDK_DEFAULT_COLOR_STATE_ID(c) ((GdkColorStateId) (((GdkDefaultColorState *) c) - gdk_default_color_states))

const char *    gdk_color_state_get_name                (GdkColorState          *color_state);
gboolean        gdk_color_state_has_srgb_tf             (GdkColorState          *self,
                                                         GdkColorState         **out_no_srgb);

static inline GdkColorState *
gdk_color_state_get_rendering_color_state (GdkColorState *self)
{
  if (!GDK_DEBUG_CHECK (LINEAR))
    return self;

  return self->rendering_color_state;
}

static inline GdkMemoryDepth
gdk_color_state_get_depth (GdkColorState *self)
{
  if (!GDK_DEBUG_CHECK (LINEAR) && self->depth == GDK_MEMORY_U8_SRGB)
    return GDK_MEMORY_U8;

  return self->depth;
}

static inline GdkColorState *
gdk_color_state_get_by_id (GdkColorStateId id)
{
  return (GdkColorState *) &gdk_default_color_states[id];
}

#define gdk_color_state_ref(self) _gdk_color_state_ref (self)
static inline GdkColorState *
_gdk_color_state_ref (GdkColorState *self)
{
  if (GDK_IS_DEFAULT_COLOR_STATE (self))
    return self;

  g_atomic_ref_count_inc (&self->ref_count);

  return self;
}

#define gdk_color_state_unref(self) _gdk_color_state_unref (self)
static inline void
_gdk_color_state_unref (GdkColorState *self)
{
  if (GDK_IS_DEFAULT_COLOR_STATE (self))
    return;

  if (g_atomic_ref_count_dec (&self->ref_count))
    self->klass->free (self);
}

#define gdk_color_state_equal(a,b) _gdk_color_state_equal ((a), (b))
static inline gboolean
_gdk_color_state_equal (GdkColorState *self,
                        GdkColorState *other)
{
  if (self == other)
    return TRUE;

  if (self->klass != other->klass)
    return FALSE;

  return self->klass->equal (self, other);
}

static inline GdkFloatColorConvert
gdk_color_state_get_convert_to (GdkColorState *self,
                                GdkColorState *target)
{
  return self->klass->get_convert_to (self, target);
}

static inline void
gdk_color_state_from_rgba (GdkColorState *self,
                           const GdkRGBA *rgba,
                           float          out_color[4])
{
  GdkFloatColorConvert convert_to;

  out_color[0] = rgba->red;
  out_color[1] = rgba->green;
  out_color[2] = rgba->blue;
  out_color[3] = rgba->alpha;

  if (gdk_color_state_equal (GDK_COLOR_STATE_SRGB, self))
    return;

  convert_to = gdk_color_state_get_convert_to (GDK_COLOR_STATE_SRGB, self);
  convert_to (GDK_COLOR_STATE_SRGB, (float(*)[4]) out_color, 1);
}
