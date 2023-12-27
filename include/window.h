#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "vertices.h"
#include "spline.h"

#ifndef WINDOW_H
#define WINDOW_H

#define WIDTH  1280
#define HEIGHT 720

typedef struct angles_t
{
    double theta;
    double phi;
    double psi;
} angles_t;

typedef struct view_mod_t
{
    double x_offset;
    double y_offset;
    double scale_factor;
} view_mod_t;

typedef struct window_t
{
    int            width;
    int            height;
    SDL_Window *   p_window;
    SDL_Renderer * p_renderer;
    spline_arr_t * p_spline_arr;
    spline_arr_t * p_mirror_spline_arr;
    angles_t       angles;
    view_mod_t     view_mod;
    int            present_points;
    int            show_mirror;
    TTF_Font *     p_font;
    SDL_Color      font_color;
} window_t;

window_t * window_create (void);
void       window_destroy (window_t * p_window);
void       window_prepare (window_t * p_window);
void       window_present (window_t * p_window);
void       window_input (window_t * p_window);
void       window_add_point (window_t * p_window, SDL_Point new_point);
void       translate_vertices (window_t * p_window);
void       window_render_text (
          window_t * p_window, int x, int y, const char * text);
void window_multiline_render_text (window_t * p_window,
                                   int        x,
                                   int        y,
                                   char *     text);
#endif // WINDOW_H