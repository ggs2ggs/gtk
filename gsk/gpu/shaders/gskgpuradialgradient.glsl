#include "common.glsl"

#define VARIATION_SUPERSAMPLING ((GSK_VARIATION & (1u << 0)) == (1u << 0))
#define VARIATION_REPEATING     ((GSK_VARIATION & (1u << 1)) == (1u << 1))

PASS(0) vec2 _pos;
PASS_FLAT(1) Rect _rect;
PASS_FLAT(2) vec4 _color0;
PASS_FLAT(3) vec4 _color1;
PASS_FLAT(4) vec4 _color2;
PASS_FLAT(5) vec4 _color3;
PASS_FLAT(6) vec4 _color4;
PASS_FLAT(7) vec4 _color5;
PASS_FLAT(8) vec4 _color6;
PASS_FLAT(9) vec4 _offsets0;
PASS_FLAT(10) vec3 _offsets1;
PASS_FLAT(11) vec4 _center_radius;
PASS_FLAT(12) vec2 _startend;


#ifdef GSK_VERTEX_SHADER

IN(0) vec4 in_rect;
IN(1) vec4 in_color0;
IN(2) vec4 in_color1;
IN(3) vec4 in_color2;
IN(4) vec4 in_color3;
IN(5) vec4 in_color4;
IN(6) vec4 in_color5;
IN(7) vec4 in_color6;
IN(8) vec4 in_offsets0;
IN(9) vec3 in_offsets1;
IN(10) vec4 in_center_radius;
IN(11) vec2 in_startend;

void
run (out vec2 pos)
{
  Rect r = rect_from_gsk (in_rect);
  
  pos = rect_get_position (r);

  _pos = pos;
  _rect = r;

  _center_radius = in_center_radius;
  _startend = in_startend;

  _color0 = color_premultiply (in_color0);
  _color1 = color_premultiply (in_color1);
  _color2 = color_premultiply (in_color2);
  _color3 = color_premultiply (in_color3);
  _color4 = color_premultiply (in_color4);
  _color5 = color_premultiply (in_color5);
  _color6 = color_premultiply (in_color6);
  _offsets0 = in_offsets0;
  _offsets1 = in_offsets1;
}

#endif



#ifdef GSK_FRAGMENT_SHADER

vec4
get_gradient_color (float offset)
{
  vec4 color;

  if (offset <= _offsets0[3])
    {
      if (offset <= _offsets0[1])
        {
          if (offset <= _offsets0[0])
            color = _color0;
          else
            color = mix (_color0, _color1, (offset - _offsets0[0]) / (_offsets0[1] - _offsets0[0]));
        }
      else
        {
          if (offset <= _offsets0[2])
            color = mix (_color1, _color2, (offset - _offsets0[1]) / (_offsets0[2] - _offsets0[1]));
          else
            color = mix (_color2, _color3, (offset - _offsets0[2]) / (_offsets0[3] - _offsets0[2]));
        }
    }
  else
    {
      if (offset <= _offsets1[1])
        {
          if (offset <= _offsets1[0])
            color = mix (_color3, _color4, (offset - _offsets0[3]) / (_offsets1[0] - _offsets0[3]));
          else
            color = mix (_color4, _color5, (offset - _offsets1[0]) / (_offsets1[1] - _offsets1[0]));
        }
      else
        {
          if (offset <= _offsets1[2])
            color = mix (_color5, _color6, (offset - _offsets1[1]) / (_offsets1[2] - _offsets1[1]));
          else
            color = _color6;
        }
    }
  return color;
}

vec4
get_gradient_color_at (vec2 pos)
{
  float offset = length (pos / _center_radius.zw);
  offset = (offset - _startend.x) / (_startend.y - _startend.x);
  if (VARIATION_REPEATING)
    offset = fract (offset);
  else
    offset = clamp (offset, 0.0, 1.0);

  return output_color_from_alt (get_gradient_color (offset));
}

void
run (out vec4 color,
     out vec2 position)
{
  float alpha = rect_coverage (_rect, _pos);

  vec2 pos = _pos / GSK_GLOBAL_SCALE - _center_radius.xy;
  if (VARIATION_SUPERSAMPLING)
    {
      vec2 dpos = 0.25 * fwidth (pos);
      color = output_color_alpha (get_gradient_color_at (pos + vec2(- dpos.x, - dpos.y)) +
                                  get_gradient_color_at (pos + vec2(- dpos.x,   dpos.y)) +
                                  get_gradient_color_at (pos + vec2(  dpos.x, - dpos.y)) +
                                  get_gradient_color_at (pos + vec2(  dpos.x,   dpos.y)),
                                  0.25 * alpha);
    }
  else
    {
      color = output_color_alpha (get_gradient_color_at (pos), alpha);;
    }

  position = _pos;
}

#endif
