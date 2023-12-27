#include <SDL2/SDL.h>
#include "vertices.h"

#ifndef SPLINE_H
#define SPLINE_H

typedef struct spline_t
{
    double_vertex_t ** pp_raw_vertices;
    int                size;
    int                capacity;
} spline_t;

typedef struct spline_arr_t
{
    spline_t ** p_spline_arr;
    int         size;
    int         capacity;
} spline_arr_t;

spline_t * spline_create (void);
void       spline_add_vertex (spline_t * p_spline, double_vertex_t * vertex);
void       spline_destroy (spline_t * p_spline);

spline_arr_t * spline_arr_create (void);
void spline_arr_add (spline_arr_t * p_spline_arr, spline_t * p_spline);
void spline_arr_destroy (spline_arr_t * p_spline_arr);

#endif // SPLINE_H
