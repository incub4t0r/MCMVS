#include "../include/spline.h"
#include "../include/vertices.h"
#include <stdlib.h>

// need to add a spline to the array, separated by start and end
spline_t * spline_create (void)
{
    spline_t * p_spline = malloc(sizeof(spline_t));
    p_spline->size      = 0;
    p_spline->capacity  = 4;
    p_spline->pp_raw_vertices
        = malloc(p_spline->capacity * sizeof(double_vertex_t *));

    return p_spline;
}

void spline_add_vertex (spline_t * p_spline, double_vertex_t * vertex)
{
    double_vertex_t * p_new_vertex = malloc(sizeof(double_vertex_t));
    p_new_vertex->x                = vertex->x;
    p_new_vertex->y                = vertex->y;
    p_new_vertex->z                = vertex->z;

    if (p_spline->size == p_spline->capacity)
    {
        p_spline->capacity *= 2;
        p_spline->pp_raw_vertices
            = realloc(p_spline->pp_raw_vertices,
                      p_spline->capacity * sizeof(double_vertex_t *));
    }

    p_spline->pp_raw_vertices[p_spline->size] = p_new_vertex;
    p_spline->size++;
}

static void vertex_destroy(double_vertex_t * p_vertex)
{
    free(p_vertex);
}

void spline_destroy (spline_t * p_spline)
{
    if (NULL == p_spline)
    {
        return;
    }

    for (int idx = 0; idx < p_spline->size; idx++)
    {
        vertex_destroy(p_spline->pp_raw_vertices[idx]);
        // free(p_spline->pp_raw_vertices[idx]); // Free each vertex
    }
    free(p_spline->pp_raw_vertices);
    free(p_spline);
    p_spline = NULL;
}

spline_arr_t * spline_arr_create (void)
{
    spline_arr_t * p_spline_arr = malloc(sizeof(spline_arr_t));
    p_spline_arr->size          = 0;
    p_spline_arr->capacity      = 4;
    p_spline_arr->p_spline_arr
        = malloc(p_spline_arr->capacity * sizeof(spline_t *));
    return p_spline_arr;
}

void spline_arr_add (spline_arr_t * p_spline_arr, spline_t * p_spline)
{
    if (p_spline_arr->size == p_spline_arr->capacity)
    {
        p_spline_arr->capacity *= 2;
        p_spline_arr->p_spline_arr
            = realloc(p_spline_arr->p_spline_arr,
                      p_spline_arr->capacity * sizeof(spline_t *));
    }

    p_spline_arr->p_spline_arr[p_spline_arr->size] = p_spline;
    p_spline_arr->size++;
}

void spline_arr_destroy (spline_arr_t * p_spline_arr)
{
    if (NULL == p_spline_arr)
    {
        return;
    }

    printf("Spline arr size: %d\n", p_spline_arr->size);
    for (int idx = 0; idx < p_spline_arr->size; idx++)
    {
        if (NULL != p_spline_arr->p_spline_arr[idx])
        {
            spline_destroy((spline_t *)(p_spline_arr->p_spline_arr[idx]));
        }
    }

    free(p_spline_arr->p_spline_arr);
    free(p_spline_arr);
    p_spline_arr = NULL;
}
