#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "../include/window.h"
#include "../include/vertices.h"
#include "../include/spline.h"
#include <stdbool.h>
#include <limits.h>
#include <math.h>

#define INTERPOLATE_SIZE 100
#define POINT_SIZE       4
#define CSERP_SIZE       1

/**
 * @brief Creates a window and its renderer on heap.
 *
 * @return window_t* Pointer to the created window.
 */
window_t * window_create (void)
{
    window_t * p_window = malloc(sizeof(window_t));

    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        printf("[-] Error initializing SDL: %s\n", SDL_GetError());
        return NULL;
    }

    p_window->p_window = SDL_CreateWindow("SDL2",
                                          SDL_WINDOWPOS_UNDEFINED,
                                          SDL_WINDOWPOS_UNDEFINED,
                                          WIDTH,
                                          HEIGHT,
                                          SDL_WINDOW_OPENGL);

    if (NULL == p_window->p_window)
    {
        printf("[-] Error creating window: %s\n", SDL_GetError());
        SDL_Quit();
        return NULL;
    }

    p_window->width  = WIDTH;
    p_window->height = HEIGHT;

    // if (SDL_SetWindowFullscreen(p_window->p_window,
    //                             SDL_WINDOW_FULLSCREEN_DESKTOP)
    //     != 0)
    // {
    //     printf("Failed to set fullscreen: %s\n", SDL_GetError());
    //     // handle error
    // }

    // int width, height;
    // SDL_GetWindowSize(p_window->p_window, &width, &height);
    // p_window->width  = width;
    // p_window->height = height;
    // printf("width: %d, height: %d\n", p_window->width, p_window->height);

    p_window->p_renderer = SDL_CreateRenderer(p_window->p_window,
                                              -1,
                                              SDL_RENDERER_ACCELERATED
                                                  | SDL_RENDERER_PRESENTVSYNC);

    if (NULL == p_window->p_renderer)
    {
        printf("error creating p_renderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(p_window->p_window);
        SDL_Quit();
        return NULL;
    }

    // set window width and height
    p_window->width  = WIDTH;
    p_window->height = HEIGHT;
    p_window->angles = (angles_t) { 0, 0, 0 };
    p_window->view_mod
        = (view_mod_t) { p_window->width / 2, p_window->height / 2, 1 };
    p_window->p_spline_arr        = spline_arr_create();
    p_window->p_mirror_spline_arr = spline_arr_create();
    p_window->present_points      = 0;
    p_window->show_mirror         = 0;

    // start ttf
    if (TTF_Init() == -1)
    {
        printf("TTF_Init: %s\n", TTF_GetError());
        exit(2);
    }

    TTF_Font * p_font = TTF_OpenFont("../src/RobotoMono.ttf", 20);
    if (p_font == NULL)
    {
        printf("TTF_OpenFont: %s\n", TTF_GetError());
        exit(2);
    }

    p_window->font_color = (SDL_Color) { 255, 255, 255, 255 };
    p_window->p_font     = p_font;

    return p_window;
}

void window_render_text (window_t * p_window, int x, int y, const char * text)
{
    SDL_Color     color   = p_window->font_color;
    SDL_Surface * surface = TTF_RenderText_Solid(p_window->p_font, text, color);
    SDL_Texture * texture
        = SDL_CreateTextureFromSurface(p_window->p_renderer, surface);
    SDL_Rect dst = { x, y, surface->w, surface->h };

    SDL_FreeSurface(surface);
    SDL_RenderCopy(p_window->p_renderer, texture, NULL, &dst);
    SDL_DestroyTexture(texture);
}

void window_multiline_render_text (window_t * p_window,
                                   int        x,
                                   int        y,
                                   char *     text)
{
    char * line = strtok(text, "\n");
    while (line != NULL)
    {
        window_render_text(p_window, x, y, line);
        y += TTF_FontHeight(p_window->p_font); // Move y-coordinate to next line
        line = strtok(NULL, "\n");
    }
}

/**
 * @brief Adds a double vertex to the window's raw vertices
 *
 * @param p_window Pointer to the window to add the vertex to.
 * @param new_vertex The vertex to add.
 */
// void window_add_vertex (window_t * p_window, double_vertex_t new_vertex)
// {
//     p_window->p_raw_vertices->size++;
//     p_window->p_raw_vertices->p_vertex
//         = realloc(p_window->p_raw_vertices->p_vertex,
//                   p_window->p_raw_vertices->size *
//                   sizeof(double_vertex_t));
//     p_window->p_raw_vertices->p_vertex[p_window->p_raw_vertices->size -
//     1]
//         = new_vertex;
// }

/**
 * @brief Destroys a window and its renderer.
 *
 * @param p_window Pointer to the window to destroy.
 */
void window_destroy (window_t * p_window)
{
    SDL_DestroyRenderer(p_window->p_renderer);
    SDL_DestroyWindow(p_window->p_window);
    spline_arr_destroy(p_window->p_spline_arr);
    spline_arr_destroy(p_window->p_mirror_spline_arr);

    SDL_Quit();
    free(p_window);
}

static double_vertex_t d_apply_rotation (double_vertex_t vertex,
                                         const double    rotation[3][3])
{
    double_vertex_t new_vertex = { 0, 0, 0 };

    new_vertex.x = rotation[0][0] * vertex.x + rotation[0][1] * vertex.y
                   + rotation[0][2] * vertex.z;
    new_vertex.y = rotation[1][0] * vertex.x + rotation[1][1] * vertex.y
                   + rotation[1][2] * vertex.z;
    new_vertex.z = rotation[2][0] * vertex.x + rotation[2][1] * vertex.y
                   + rotation[2][2] * vertex.z;
    return new_vertex;
}

typedef struct cubic_coefficients_t
{
    double a, b, c, d;
} cubic_coefficients_t;

// Define the function to calculate the cubic coefficients
cubic_coefficients_t * calculate_spline_coefficients (
    double_vertex_t ** pp_points, int size)
{
    cubic_coefficients_t * coefficients
        = calloc(size, sizeof(cubic_coefficients_t));

    double * h     = calloc(size, sizeof(double));
    double * alpha = calloc(size, sizeof(double));
    double * l     = calloc(size, sizeof(double));
    double * mu    = calloc(size, sizeof(double));
    double * z     = calloc(size, sizeof(double));

    for (int i = 0; i < size - 1; i++)
    {
        h[i] = pp_points[i + 1]->x - pp_points[i]->x;
    }

    for (int i = 1; i < size - 1; i++)
    {
        alpha[i] = (3.0 / h[i]) * (pp_points[i + 1]->y - pp_points[i]->y)
                   - (3.0 / h[i - 1]) * (pp_points[i]->y - pp_points[i - 1]->y);
    }

    l[0]  = 1.0;
    mu[0] = 0.0;
    z[0]  = 0.0;

    for (int i = 1; i < size - 1; i++)
    {
        l[i] = 2.0 * (pp_points[i + 1]->x - pp_points[i - 1]->x)
               - h[i - 1] * mu[i - 1];
        mu[i] = h[i] / l[i];
        z[i]  = (alpha[i] - h[i - 1] * z[i - 1]) / l[i];
    }

    l[size - 1]              = 1.0;
    z[size - 1]              = 0.0;
    coefficients[size - 1].c = 0.0;

    for (int j = size - 2; j >= 0; j--)
    {
        coefficients[j].c = z[j] - mu[j] * coefficients[j + 1].c;
        coefficients[j].b
            = (pp_points[j + 1]->y - pp_points[j]->y) / h[j]
              - h[j] * (coefficients[j + 1].c + 2.0 * coefficients[j].c) / 3.0;
        coefficients[j].d
            = (coefficients[j + 1].c - coefficients[j].c) / (3.0 * h[j]);
        coefficients[j].a = pp_points[j]->y;
    }

    free(h);
    free(alpha);
    free(l);
    free(mu);
    free(z);

    return coefficients;
}

// Define the function to generate the interpolated points
double_vertex_t * generate_interpolated_points (
    cubic_coefficients_t * coefficients,
    double_vertex_t **     pp_points,
    int                    size,
    int                    num_points)
{
    double_vertex_t * interpolated_points
        = calloc(num_points, sizeof(double_vertex_t));
    double step = (pp_points[size - 1]->x - pp_points[0]->x) / (num_points - 1);

    for (int i = 0; i < num_points; i++)
    {
        double x       = pp_points[0]->x + i * step;
        int    segment = 0;

        // Find the segment that x falls in
        for (int j = 0; j < size - 1; j++)
        {
            if (x >= pp_points[j]->x && x <= pp_points[j + 1]->x)
            {
                segment = j;
                break;
            }
        }

        // Calculate the interpolated y value
        double dx = x - pp_points[segment]->x;
        double y  = coefficients[segment].a + coefficients[segment].b * dx
                   + coefficients[segment].c * dx * dx
                   + coefficients[segment].d * dx * dx * dx;

        interpolated_points[i].x = x;
        interpolated_points[i].y = y;
    }

    return interpolated_points;
}

// qsort function compare_double_vertex_x
int compare_double_vertex_x (const void * a, const void * b)
{
    double_vertex_t * p1 = *(double_vertex_t **)a;
    double_vertex_t * p2 = *(double_vertex_t **)b;
    if (p1->x < p2->x)
        return -1;
    else if (p1->x > p2->x)
        return 1;
    else
        return 0;
}

// qsort function compare_double_vertex_y
int compare_double_vertex_y (const void * a, const void * b)
{
    double_vertex_t * p1 = *(double_vertex_t **)a;
    double_vertex_t * p2 = *(double_vertex_t **)b;
    if (p1->y < p2->y)
        return -1;
    else if (p1->y > p2->y)
        return 1;
    else
        return 0;
}

/**
 *
 * @brief Prepares the window for rendering.
 *
 * @param p_window Pointer to the window to prepare.
 */
void window_prepare (window_t * p_window)
{
    SDL_SetRenderDrawColor(p_window->p_renderer, 0, 0, 0, 255);
    // SDL_SetRenderDrawColor(p_window->p_renderer, 96, 128, 255, 255);
    SDL_RenderClear(p_window->p_renderer);
    SDL_SetRenderDrawColor(p_window->p_renderer, 255, 255, 255, 255);

    double theta_rad = p_window->angles.theta * M_PI / 180.0;
    double phi_rad   = p_window->angles.phi * M_PI / 180.0;
    double psi_rad   = p_window->angles.psi * M_PI / 180.0;

    double rotation_matrix[3][3]
        = { { cos(theta_rad) * cos(psi_rad),
              cos(theta_rad) * sin(psi_rad),
              -sin(theta_rad) },
            { sin(phi_rad) * sin(theta_rad) * cos(psi_rad)
                  - cos(phi_rad) * sin(psi_rad),
              sin(phi_rad) * sin(theta_rad) * sin(psi_rad)
                  + cos(phi_rad) * cos(psi_rad),
              sin(phi_rad) * cos(theta_rad) },
            { cos(phi_rad) * sin(theta_rad) * cos(psi_rad)
                  + sin(phi_rad) * sin(psi_rad),
              cos(phi_rad) * sin(theta_rad) * sin(psi_rad)
                  - sin(phi_rad) * cos(psi_rad),
              cos(phi_rad) * cos(theta_rad) } };

    for (int spline_idx = 0; spline_idx < p_window->p_spline_arr->size;
         spline_idx++)
    {
        spline_t * p_spline = p_window->p_spline_arr->p_spline_arr[spline_idx];
        spline_t * p_spline_rotated = spline_create();
        for (int vertex_idx = 0; vertex_idx < p_spline->size; vertex_idx++)
        {
            double_vertex_t translated_vertex = d_apply_rotation(
                *(p_spline->pp_raw_vertices[vertex_idx]), rotation_matrix);

            translated_vertex = (double_vertex_t) {
                (translated_vertex.x * p_window->view_mod.scale_factor)
                    + p_window->view_mod.x_offset,
                (translated_vertex.y * p_window->view_mod.scale_factor)
                    + p_window->view_mod.y_offset,
                translated_vertex.z
            };
            // draw the point
            // SDL_Rect rect = { translated_vertex.x - (POINT_SIZE / 2),
            //                   translated_vertex.y - (POINT_SIZE / 2),
            //                   POINT_SIZE,
            //                   POINT_SIZE };
            // SDL_RenderFillRect(p_window->p_renderer, &rect);
            spline_add_vertex(p_spline_rotated, &translated_vertex);

            // double_vertex_t translated_vertex2 = d_apply_rotation(
            //     *(p_spline->pp_raw_vertices[vertex_idx + 1]),
            //     rotation_matrix);
            // SDL_Point point1
            //     = { (int)(translated_vertex.x *
            //     p_window->view_mod.scale_factor)
            //             + p_window->view_mod.x_offset,
            //         (int)(translated_vertex.y *
            //         p_window->view_mod.scale_factor)
            //             + p_window->view_mod.y_offset };
            // SDL_Point point2 = {
            //     (int)(translated_vertex2.x * p_window->view_mod.scale_factor)
            //         + p_window->view_mod.x_offset,
            //     (int)(translated_vertex2.y * p_window->view_mod.scale_factor)
            //         + p_window->view_mod.y_offset
            // };

            // if (1 == p_window->present_points)
            // {
            //     SDL_Rect rect = { point1.x - 4, point1.y - 4, 8, 8 };
            //     SDL_RenderFillRect(p_window->p_renderer, &rect);
            //     rect = (SDL_Rect) { point2.x - 4, point2.y - 4, 8, 8 };
            //     SDL_RenderFillRect(p_window->p_renderer, &rect);
            // }

            // SDL_RenderDrawLine(
            //     p_window->p_renderer, point1.x, point1.y, point2.x,
            //     point2.y);
            //     if (1 == p_window->show_mirror)
            //     {
            //         double_vertex_t mirror_vertex = d_apply_rotation(
            //             *(p_window->p_mirror_spline_arr->p_spline_arr[spline_idx]
            //                   ->pp_raw_vertices[vertex_idx]),
            //             rotation_matrix);
            //         // double_vertex_t mirror_vertex = d_apply_rotation(
            //         //
            //         *(p_window->p_mirror_spline_arr->p_spline_arr[spline_idx]
            //         //           ->pp_raw_vertices[vertex_idx]),
            //         //     rotation_x);
            //         // mirror_vertex = d_apply_rotation(mirror_vertex,
            //         rotation_y);
            //         // mirror_vertex = d_apply_rotation(mirror_vertex,
            //         rotation_z);
            //         SDL_Point mirror_point1
            //             = { mirror_vertex.x * p_window->view_mod.scale_factor
            //                     + p_window->view_mod.x_offset,
            //                 mirror_vertex.y * p_window->view_mod.scale_factor
            //                     + p_window->view_mod.y_offset };
            //         double_vertex_t mirror_vertex2 = d_apply_rotation(
            //             *(p_window->p_mirror_spline_arr->p_spline_arr[spline_idx]
            //                   ->pp_raw_vertices[vertex_idx + 1]),
            //             rotation_matrix);
            //         // double_vertex_t mirror_vertex2 = d_apply_rotation(
            //         //
            //         *(p_window->p_mirror_spline_arr->p_spline_arr[spline_idx]
            //         //           ->pp_raw_vertices[vertex_idx + 1]),
            //         //     rotation_x);
            //         // mirror_vertex2 = d_apply_rotation(mirror_vertex2,
            //         // rotation_y); mirror_vertex2 =
            //         // d_apply_rotation(mirror_vertex2, rotation_z);
            //         SDL_Point mirror_point2
            //             = { mirror_vertex2.x *
            //             p_window->view_mod.scale_factor
            //                     + p_window->view_mod.x_offset,
            //                 mirror_vertex2.y *
            //                 p_window->view_mod.scale_factor
            //                     + p_window->view_mod.y_offset };
            //         SDL_RenderDrawLine(p_window->p_renderer,
            //                            mirror_point1.x,
            //                            mirror_point1.y,
            //                            mirror_point2.x,
            //                            mirror_point2.y);
            //     }
        }

        // sort p_spline_rotated->pp_raw_vertices by x

        // iterate through the spline until x direction changes
        spline_t *        p_subspline   = spline_create();
        double               direction     = 0;
        double_vertex_t * p_prev_vertex = p_spline_rotated->pp_raw_vertices[0];
        double_vertex_t * p_curr_vertex = p_spline_rotated->pp_raw_vertices[1];
        direction = p_curr_vertex->x >= p_prev_vertex->x ? 1 : -1;
        // direction = p_curr_vertex->x - p_prev_vertex->x;
        spline_add_vertex(p_subspline, p_prev_vertex);
        spline_add_vertex(p_subspline, p_curr_vertex);
        p_prev_vertex     = p_curr_vertex;
        int spline_idx    = 2;
        double new_direction = 0;
        while (spline_idx < p_spline_rotated->size)
        {
            p_curr_vertex = p_spline_rotated->pp_raw_vertices[spline_idx];
            // new_direction = p_curr_vertex->x - p_prev_vertex->x;
            new_direction = p_curr_vertex->x >= p_prev_vertex->x ? 1 : -1;
            if (new_direction != direction || fabs(p_curr_vertex->x - p_prev_vertex->x) < 10) // added too close check
            {
                if (p_subspline->size >= 3) // added equality check
                {
                    // sort subspline on y
                    // qsort(p_subspline->pp_raw_vertices,
                    //       p_subspline->size,
                    //       sizeof(double_vertex_t *),
                    //       compare_double_vertex_y);

                    printf("Rendering spline for idx %d\n", spline_idx);
                    cubic_coefficients_t * coefficients
                        = calculate_spline_coefficients(
                            p_subspline->pp_raw_vertices, p_subspline->size);

                    double_vertex_t * interpolated_points
                        = generate_interpolated_points(
                            coefficients,
                            p_subspline->pp_raw_vertices,
                            p_subspline->size,
                            INTERPOLATE_SIZE);

                    for (int i = 0; i < INTERPOLATE_SIZE; i++)
                    {
                        SDL_Rect rect
                            = { interpolated_points[i].x - (CSERP_SIZE / 2),
                                spline_idx * 10,
                                CSERP_SIZE,
                                CSERP_SIZE };
                        SDL_RenderDrawRect(p_window->p_renderer, &rect);

                        rect = (SDL_Rect) {
                            interpolated_points[i].x - (CSERP_SIZE / 2),
                            interpolated_points[i].y - (CSERP_SIZE / 2),
                            CSERP_SIZE,
                            CSERP_SIZE
                        };
                        SDL_RenderDrawRect(p_window->p_renderer, &rect);
                    }
                    free(interpolated_points);
                    free(coefficients);
                }
                spline_destroy(p_subspline);
                p_subspline = spline_create();
                direction   = new_direction;
                spline_add_vertex(p_subspline, p_curr_vertex);
            }
            else
            {
                spline_add_vertex(p_subspline, p_curr_vertex);
            }
            spline_idx++;
            p_prev_vertex = p_curr_vertex;
        }
        // sort on x
        qsort(p_subspline->pp_raw_vertices,
              p_subspline->size,
              sizeof(double_vertex_t *),
              compare_double_vertex_x);
        // if (p_subspline->size > 3)
        {
            printf("Rendering spline for idx %d\n", spline_idx);
            cubic_coefficients_t * coefficients = calculate_spline_coefficients(
                p_subspline->pp_raw_vertices, p_subspline->size);

            double_vertex_t * interpolated_points
                = generate_interpolated_points(coefficients,
                                               p_subspline->pp_raw_vertices,
                                               p_subspline->size,
                                               INTERPOLATE_SIZE);

            for (int i = 0; i < INTERPOLATE_SIZE; i++)
            {
                SDL_Rect rect = { interpolated_points[i].x - (CSERP_SIZE / 2),
                                  spline_idx * 10,
                                  CSERP_SIZE,
                                  CSERP_SIZE };
                SDL_RenderDrawRect(p_window->p_renderer, &rect);

                rect = (SDL_Rect) { interpolated_points[i].x - (CSERP_SIZE / 2),
                                    interpolated_points[i].y - (CSERP_SIZE / 2),
                                    CSERP_SIZE,
                                    CSERP_SIZE };
                SDL_RenderDrawRect(p_window->p_renderer, &rect);
            }
            free(interpolated_points);
            free(coefficients);
        }

        // cubic_coefficients_t * coefficients = calculate_spline_coefficients(
        //     p_spline_rotated->pp_raw_vertices, p_spline_rotated->size);

        // // Generate the interpolated points
        // double_vertex_t * interpolated_points
        //     = generate_interpolated_points(coefficients,
        //                                    p_spline_rotated->pp_raw_vertices,
        //                                    p_spline_rotated->size,
        //                                    INTERPOLATE_SIZE);

        // // draw interpolated subsplines until direction changes
        // spline_t *        p_subspline   = spline_create();
        // int               direction     = 0;
        // double_vertex_t * p_prev_vertex = &interpolated_points[0];
        // double_vertex_t * p_curr_vertex = &interpolated_points[1];
        // direction = p_curr_vertex->x > p_prev_vertex->x ? 1 : -1;
        // spline_add_vertex(p_subspline, p_prev_vertex);
        // spline_add_vertex(p_subspline, p_curr_vertex);
        // p_prev_vertex     = p_curr_vertex;
        // int spline_idx    = 2;
        // int new_direction = 0;
        // for (;spline_idx < INTERPOLATE_SIZE; spline_idx++)
        // {
        //     p_curr_vertex = &interpolated_points[spline_idx];
        //     new_direction = p_curr_vertex->x > p_prev_vertex->x ? 1 : -1;

        //     if (direction != new_direction)
        //     {
        //         spline_destroy(p_subspline);
        //         p_subspline = spline_create();
        //         direction   = new_direction;
        //     }
        //     else
        //     {
        //         if (p_subspline->size > 3)
        //         {
        //             printf("Rendering spline for idx %d\n", spline_idx);
        //             cubic_coefficients_t * coefficients
        //                 = calculate_spline_coefficients(
        //                     p_subspline->pp_raw_vertices, p_subspline->size);

        //             double_vertex_t * interpolated_points
        //                 = generate_interpolated_points(
        //                     coefficients,
        //                     p_subspline->pp_raw_vertices,
        //                     p_subspline->size,
        //                     INTERPOLATE_SIZE);

        //             for (int i = 0; i < INTERPOLATE_SIZE; i++)
        //             {
        //                 SDL_Rect rect
        //                     = { interpolated_points[i].x - (CSERP_SIZE / 2),
        //                         interpolated_points[i].y - (CSERP_SIZE / 2),
        //                         CSERP_SIZE,
        //                         CSERP_SIZE };
        //                 SDL_RenderDrawRect(p_window->p_renderer, &rect);
        //             }
        //             free(interpolated_points);
        //             free(coefficients);
        //         }
        //     }

        //     spline_add_vertex(p_subspline, p_curr_vertex);
        //     spline_idx++;
        //     p_prev_vertex = p_curr_vertex;
        // }

        // Draw the interpolated points
        // for (int i = 0; i < INTERPOLATE_SIZE; i++)
        // {
        //     SDL_Rect rect = { interpolated_points[i].x - (CSERP_SIZE / 2),
        //                       interpolated_points[i].y - (CSERP_SIZE / 2),
        //                       CSERP_SIZE,
        //                       CSERP_SIZE };
        //     SDL_RenderDrawRect(p_window->p_renderer, &rect);
        // }
        // free(interpolated_points);
        // free(coefficients);
        // spline_destroy(p_spline_rotated);
    }
}

/**
 * @brief Presents the window to the screen.
 *
 * @param p_window Pointer to the window to present.
 */
void window_present (window_t * p_window)
{
    SDL_RenderPresent(p_window->p_renderer);
}

bool g_activate_mouse = false;
int  g_prev_mousex = 0, g_prev_mousey = 0;

void window_input (window_t * p_window)
{
    SDL_Event event  = { 0 };
    int       mouseX = 0;
    int       mouseY = 0;

    while (SDL_PollEvent(&event))
    {
        switch (event.type)
        {
            case SDL_QUIT:
                exit(0);
                break;
            case SDL_KEYDOWN:
                switch (event.key.keysym.sym)
                {
                    case SDLK_i:
                        p_window->angles.phi += 5;
                        break;
                    case SDLK_k:
                        p_window->angles.phi -= 5;
                        break;
                    case SDLK_j:
                        p_window->angles.theta += 5;
                        break;
                    case SDLK_l:
                        p_window->angles.theta -= 5;
                        break;
                    case SDLK_u:
                        p_window->angles.psi += 5;
                        break;
                    case SDLK_o:
                        p_window->angles.psi -= 5;
                        break;
                    // use wasd to set offsets inverted
                    case SDLK_w:
                        p_window->view_mod.y_offset += 20;
                        break;
                    case SDLK_s:
                        p_window->view_mod.y_offset -= 20;
                        break;
                    case SDLK_a:
                        p_window->view_mod.x_offset += 20;
                        break;
                    case SDLK_d:
                        p_window->view_mod.x_offset -= 20;
                        break;
                    case SDLK_q:
                        p_window->view_mod.scale_factor
                            = p_window->view_mod.scale_factor <= 0.1
                                  ? p_window->view_mod.scale_factor
                                  : p_window->view_mod.scale_factor - 0.1;
                        break;
                    case SDLK_e:
                        p_window->view_mod.scale_factor += 0.1;
                        break;
                    case SDLK_x:
                        p_window->angles.theta = 0;
                        p_window->angles.phi   = -90;
                        p_window->angles.psi   = 0;
                        break;
                    case SDLK_y:
                        p_window->angles.theta = 0;
                        p_window->angles.phi   = 0;
                        p_window->angles.psi   = -90;
                        break;
                    case SDLK_z:
                        p_window->angles.theta = 0;
                        p_window->angles.phi   = -90;
                        p_window->angles.psi   = -90;
                        break;
                    case SDLK_r:
                        p_window->angles.theta = 0;
                        p_window->angles.phi   = 0;
                        p_window->angles.psi   = 0;
                        break;
                    case SDLK_p:
                        p_window->present_points = !p_window->present_points;
                        break;
                    case SDLK_m:
                        p_window->show_mirror = !p_window->show_mirror;
                        break;
                    case SDLK_LSHIFT:
                        g_activate_mouse = true; // Set the flag when mouse
                        SDL_GetMouseState(&g_prev_mousex, &g_prev_mousey);
                        break;
                    default:
                        break;
                }
                break;
            case SDL_KEYUP:
                switch (event.key.keysym.sym)
                {
                    case SDLK_LSHIFT:
                        g_activate_mouse = false; // Set the flag when mouse
                        break;
                    default:
                        break;
                }
                break;
            case SDL_MOUSEWHEEL:
                if (g_activate_mouse)
                {
                    printf("Event type: %d\n", event.type);
                    printf("Event wheel: %d\n", event.wheel.y);
                    printf("Event wheel: %d\n", event.wheel.x);
                    if (event.wheel.y > 0 || event.wheel.x > 0)
                    {
                        p_window->view_mod.scale_factor += 0.1;
                    }
                    else if (event.wheel.y < 0 || event.wheel.x < 0)
                    {
                        p_window->view_mod.scale_factor
                            = p_window->view_mod.scale_factor <= 0.1
                                  ? p_window->view_mod.scale_factor
                                  : p_window->view_mod.scale_factor - 0.1;
                    }
                }
                else
                {
                    if (event.wheel.y > 0)
                    {
                        p_window->view_mod.y_offset += 20;
                    }
                    else if (event.wheel.y < 0)
                    {
                        p_window->view_mod.y_offset -= 20;
                    }
                    else if (event.wheel.x > 0)
                    {
                        p_window->view_mod.x_offset -= 20;
                    }
                    else if (event.wheel.x < 0)
                    {
                        p_window->view_mod.x_offset += 20;
                    }
                }

                break;
            // case SDL_MOUSEWHEEL:
            //     if (event.wheel.y > 0)
            //     {
            //         p_window->view_mod.y_offset += 20;
            //         // p_window->view_mod.scale_factor += 0.1;
            //     }
            //     else if (event.wheel.y < 0)
            //     {
            //         // p_window->view_mod.scale_factor
            //         //     = p_window->view_mod.scale_factor <= 0.1
            //         //           ? p_window->view_mod.scale_factor
            //         //           : p_window->view_mod.scale_factor - 0.1;
            //         p_window->view_mod.y_offset -= 20;
            //     }
            //     else if (event.wheel.x > 0)
            //     {
            //         p_window->view_mod.x_offset -= 20;
            //     }
            //     else if (event.wheel.x < 0)
            //     {
            //         p_window->view_mod.x_offset += 20;
            //     }
            //     break;
            case SDL_MOUSEBUTTONDOWN:
                // in the future, this will be used for point selection
                // SDL_GetMouseState(&mouseX, &mouseY);
                // // find the nearest point
                // int    nearest_point_idx = 0;
                // double min_distance      = 100;

                // for (int i = 0; i < p_window->p_spline_arr->size; i++)
                // {
                //     spline_t * p_spline
                //         = p_window->p_spline_arr->p_spline_arr[i];
                //     for (int j = 0; j < p_spline->size; j++)
                //     {
                //         double_vertex_t * p_vertex
                //             = p_spline->pp_raw_vertices[j];
                //         double distance = sqrt(pow(p_vertex->x - mouseX, 2)
                //                                + pow(p_vertex->y - mouseY,
                //                                2));
                //         if (distance < min_distance)
                //         {
                //             min_distance      = distance;
                //             nearest_point_idx = j;
                //         }
                //     }
                // }

                // printf("nearest point: %d\n", nearest_point_idx);

                break;
            // case SDL_MOUSEBUTTONUP:
            //     g_activate_mouse = false;
            //     printf("mouse up\n");
            //     break;
            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_LEAVE)
                {
                    g_activate_mouse = false;
                    printf("mouse left the window\n");
                }
                break;
            case SDL_MOUSEMOTION:
                if (g_activate_mouse)
                { // Only track motion when mouse button is down
                    int current_mousex, current_mousey;
                    SDL_GetMouseState(&current_mousex, &current_mousey);

                    // Calculate the distance
                    int    dx       = current_mousex - g_prev_mousex;
                    int    dy       = current_mousey - g_prev_mousey;
                    double distance = sqrt(dx * dx + dy * dy);

                    p_window->angles.phi
                        += (double)dy / p_window->height * 180.0;
                    p_window->angles.psi
                        += (double)dx / p_window->width * 180.0;

                    // Update the previous mouse position
                    g_prev_mousex = current_mousex;
                    g_prev_mousey = current_mousey;
                }
                break;
            default:
                // printf("event type: %d\n", event.type);
                break;
        }
    }
}
