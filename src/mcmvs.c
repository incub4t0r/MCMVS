#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "../include/window.h"
#include "../include/vertices.h"
#include "../include/spline.h"
#include <SDL2/SDL.h>
#include <limits.h>

int main (int argc, char * argv[])
{
    if (2 != argc)
    {
        printf("Usage: %s <filename>\n", argv[0]);
        return 1;
    }

    FILE * p_file = fopen(argv[1], "r");

    if (NULL == p_file)
    {
        printf("Error opening file %s\n", argv[1]);
        return 1;
    }

    window_t * p_window = window_create();

    if (NULL == p_window)
    {
        printf("[-] Window is null\n");
        return 1;
    }
    int             number   = -1;
    char            junk[10] = { 0 };
    double_vertex_t vertex   = { 0 };

    // read the file and create the splines
    while (0 == feof(p_file))
    {
        spline_t * p_spline = spline_create();
        fscanf(p_file, "%3s%d\n%1s\n", junk, &number, junk);
        while (3
               == fscanf(p_file,
                         "%lf%*[^0-9]%lf%*[^0-9]%lf\n",
                         &vertex.x,
                         &vertex.y,
                         &vertex.z))
        {
            // printf("%lf %lf %lf\n", vertex.x, vertex.y, vertex.z);
            spline_add_vertex(p_spline, &vertex);
        }
        spline_arr_add(p_window->p_spline_arr, p_spline);
        fscanf(p_file, "%1s\n", junk);
    }

    double min_x = DBL_MAX;
    double min_y = DBL_MAX;
    double min_z = DBL_MAX;
    double max_x = DBL_MIN;
    double max_y = DBL_MIN;
    double max_z = DBL_MIN;

    for (int i = 0; i < p_window->p_spline_arr->size; i++)
    {
        spline_t * p_spline = p_window->p_spline_arr->p_spline_arr[i];
        for (int j = 0; j < p_spline->size; j++)
        {
            double_vertex_t * p_vertex = p_spline->pp_raw_vertices[j];
            min_x                      = fmin(min_x, p_vertex->x);
            min_y                      = fmin(min_y, p_vertex->y);
            min_z                      = fmin(min_z, p_vertex->z);
            max_x                      = fmax(max_x, p_vertex->x);
            max_y                      = fmax(max_y, p_vertex->y);
            max_z                      = fmax(max_z, p_vertex->z);
        }
    }

    // double sum_x        = 0;
    // double sum_y        = 0;
    // double sum_z        = 0;
    // int    total_points = 0;
    // for (int i = 0; i < p_window->p_spline_arr->size; i++)
    // {
    //     spline_t * p_spline = p_window->p_spline_arr->p_spline_arr[i];
    //     for (int j = 0; j < p_spline->size; j++)
    //     {
    //         double_vertex_t * p_vertex = p_spline->pp_raw_vertices[j];
    //         sum_x += p_vertex->x;
    //         sum_y += p_vertex->y;
    //         sum_z += p_vertex->z;
    //         total_points++;
    //     }
    // }
    // double center_x = sum_x / total_points;
    // double center_y = sum_y / total_points;
    // double center_z = sum_z / total_points;

    double center_x = (max_x + min_x) / 2;
    double center_y = (max_y + min_y) / 2;
    double center_z = (max_z + min_z) / 2;

    printf("Center: %lf %lf %lf\n", center_x, center_y, center_z);
    // translate all vertices to center
    for (int i = 0; i < p_window->p_spline_arr->size; i++)
    {
        spline_t * p_spline = p_window->p_spline_arr->p_spline_arr[i];
        for (int j = 0; j < p_spline->size; j++)
        {
            double_vertex_t * p_vertex = p_spline->pp_raw_vertices[j];
            p_vertex->x -= center_x;
            // p_vertex->y -= center_y;
            p_vertex->z -= center_z;
        }
    }

    // calculate the mirror splines
    for (int i = 0; i < p_window->p_spline_arr->size; i++)
    {
        spline_t * p_spline = p_window->p_spline_arr->p_spline_arr[i];
        spline_t * p_mirror = spline_create();
        for (int j = 0; j < p_spline->size; j++)
        {
            double_vertex_t * p_vertex      = p_spline->pp_raw_vertices[j];
            double_vertex_t   mirror_vertex = { 0 };
            mirror_vertex.x                 = p_vertex->x;
            mirror_vertex.y                 = -p_vertex->y;
            mirror_vertex.z                 = p_vertex->z;
            spline_add_vertex(p_mirror, &mirror_vertex);
        }
        spline_arr_add(p_window->p_mirror_spline_arr, p_mirror);
    }

    double x_range = max_x - min_x;
    double y_range = max_y - min_y;
    double z_range = max_z - min_z;
    double x_scale_factor
        = (double)WIDTH
          / (x_range + (2 * center_x + p_window->view_mod.x_offset));
    double y_scale_factor
        = (double)HEIGHT
          / (y_range + (2 * center_y + p_window->view_mod.y_offset));
    p_window->view_mod.scale_factor
        = x_scale_factor < y_scale_factor ? x_scale_factor : y_scale_factor;

    uint64_t start           = 0;
    uint64_t end             = 0;
    double   frame_time      = 0;
    char     text[100]       = { 0 };
    char     guide_text[200] = { 0 };
    for (;;)
    {
        start = SDL_GetPerformanceCounter();
        // translate_vertices(p_window);
        window_prepare(p_window);
        sprintf(text,
                "FPS   : %lf\n"
                "SF    : %lf\n"
                "Theta : %lf\n"
                "Phi   : %lf\n"
                "Psi   : %lf\n",
                1.0 / frame_time,
                p_window->view_mod.scale_factor,
                p_window->angles.theta,
                p_window->angles.phi,
                p_window->angles.psi);
        window_multiline_render_text(p_window, 10, 10, text);
        sprintf(guide_text,
                "WASD   : Move\n"
                "QE     : Zoom\n"
                "IJKLUO : Rotate\n"
                "R      : Reset\n"
                "M      : Toggle mirror\n"
                "P      : Toggle points\n"
                "Click  : Toggle rotate\n");
        window_multiline_render_text(
            p_window, 10, p_window->height - 200, guide_text);
        window_input(p_window);
        window_present(p_window);
        end        = SDL_GetPerformanceCounter();
        frame_time = (double)(end - start) / SDL_GetPerformanceFrequency();
        // printf("FPS: %f\n", 1.0 / frame_time);
        // printf("Scale factor: %lf\n", p_window->view_mod.scale_factor);
        // printf("Theta: %f\n", p_window->angles.theta);
        // printf("Phi:   %f\n", p_window->angles.phi);
        // printf("Psi:   %f\n", p_window->angles.psi);
    }
    window_destroy(p_window);
    fclose(p_file);
}