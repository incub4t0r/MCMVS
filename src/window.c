#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "../include/window.h"
#include "../include/vertices.h"
#include "../include/spline.h"
#include <stdbool.h>

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

    p_window->width = WIDTH;
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
    p_window->width               = WIDTH;
    p_window->height              = HEIGHT;
    p_window->angles              = (angles_t) { 0, 0, 0 };
    p_window->view_mod            = (view_mod_t) { p_window->width / 2, p_window->height / 2, 1 };
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

    // double theta = p_window->angles.theta;
    // double phi   = p_window->angles.phi;
    // double psi   = p_window->angles.psi;

    // Rotation matrix
    // const double rotation_x[3][3] = { { 1, 0, 0 },
    //                                   { 0, cos(theta), -sin(theta) },
    //                                   { 0, sin(theta), cos(theta) } };
    // const double rotation_y[3][3] = { { cos(phi), 0, sin(phi) },
    //                                   { 0, 1, 0 },
    //                                   { -sin(phi), 0, cos(phi) } };
    // const double rotation_z[3][3] = { { cos(psi), -sin(psi), 0 },
    //                                   { sin(psi), cos(psi), 0 },
    //                                   { 0, 0, 1 } };
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
        for (int vertex_idx = 0; vertex_idx < p_spline->size - 1; vertex_idx++)
        {
            double_vertex_t translated_vertex = d_apply_rotation(
                *(p_spline->pp_raw_vertices[vertex_idx]), rotation_matrix);
            // double_vertex_t translated_vertex = d_apply_rotation(
            //     *(p_spline->pp_raw_vertices[vertex_idx]), rotation_x);
            // translated_vertex = d_apply_rotation(translated_vertex,
            // rotation_y); translated_vertex =
            // d_apply_rotation(translated_vertex, rotation_z);

            double_vertex_t translated_vertex2 = d_apply_rotation(
                *(p_spline->pp_raw_vertices[vertex_idx + 1]), rotation_matrix);
            // double_vertex_t translated_vertex2 = d_apply_rotation(
            //     *(p_spline->pp_raw_vertices[vertex_idx + 1]), rotation_x);
            // translated_vertex2
            //     = d_apply_rotation(translated_vertex2, rotation_y);
            // translated_vertex2
            //     = d_apply_rotation(translated_vertex2, rotation_z);

            SDL_Point point1
                = { (int)(translated_vertex.x * p_window->view_mod.scale_factor)
                        + p_window->view_mod.x_offset,
                    (int)(translated_vertex.y * p_window->view_mod.scale_factor)
                        + p_window->view_mod.y_offset };
            SDL_Point point2 = {
                (int)(translated_vertex2.x * p_window->view_mod.scale_factor)
                    + p_window->view_mod.x_offset,
                (int)(translated_vertex2.y * p_window->view_mod.scale_factor)
                    + p_window->view_mod.y_offset
            };

            if (1 == p_window->present_points)
            {
                SDL_Rect rect = { point1.x - 2, point1.y - 2, 4, 4 };
                SDL_RenderFillRect(p_window->p_renderer, &rect);
                rect = (SDL_Rect) { point2.x - 2, point2.y - 2, 4, 4 };
                SDL_RenderFillRect(p_window->p_renderer, &rect);
            }

            SDL_RenderDrawLine(
                p_window->p_renderer, point1.x, point1.y, point2.x, point2.y);

            if (1 == p_window->show_mirror)
            {
                double_vertex_t mirror_vertex = d_apply_rotation(
                    *(p_window->p_mirror_spline_arr->p_spline_arr[spline_idx]
                          ->pp_raw_vertices[vertex_idx]),
                    rotation_matrix);
                // double_vertex_t mirror_vertex = d_apply_rotation(
                //     *(p_window->p_mirror_spline_arr->p_spline_arr[spline_idx]
                //           ->pp_raw_vertices[vertex_idx]),
                //     rotation_x);
                // mirror_vertex = d_apply_rotation(mirror_vertex, rotation_y);
                // mirror_vertex = d_apply_rotation(mirror_vertex, rotation_z);

                SDL_Point mirror_point1
                    = { mirror_vertex.x * p_window->view_mod.scale_factor
                            + p_window->view_mod.x_offset,
                        mirror_vertex.y * p_window->view_mod.scale_factor
                            + p_window->view_mod.y_offset };

                double_vertex_t mirror_vertex2 = d_apply_rotation(
                    *(p_window->p_mirror_spline_arr->p_spline_arr[spline_idx]
                          ->pp_raw_vertices[vertex_idx + 1]),
                    rotation_matrix);
                // double_vertex_t mirror_vertex2 = d_apply_rotation(
                //     *(p_window->p_mirror_spline_arr->p_spline_arr[spline_idx]
                //           ->pp_raw_vertices[vertex_idx + 1]),
                //     rotation_x);
                // mirror_vertex2 = d_apply_rotation(mirror_vertex2,
                // rotation_y); mirror_vertex2 =
                // d_apply_rotation(mirror_vertex2, rotation_z);

                SDL_Point mirror_point2
                    = { mirror_vertex2.x * p_window->view_mod.scale_factor
                            + p_window->view_mod.x_offset,
                        mirror_vertex2.y * p_window->view_mod.scale_factor
                            + p_window->view_mod.y_offset };

                SDL_RenderDrawLine(p_window->p_renderer,
                                   mirror_point1.x,
                                   mirror_point1.y,
                                   mirror_point2.x,
                                   mirror_point2.y);
            }
        }
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

// /**
//  * @brief Adds a point to the window's points.
//  *
//  * @param p_window Pointer to the window to add the point to.
//  * @param new_point The point to add.
//  */
// void window_add_point (window_t * p_window, SDL_Point new_point)
// {
//     p_window->p_points->size++;
//     p_window->p_points->p_points
//         = realloc(p_window->p_points->p_points,
//                   p_window->p_points->size * sizeof(SDL_Point));
//     p_window->p_points->p_points[p_window->p_points->size - 1] = new_point;
// }
bool g_is_mouse_down = false;
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
                    default:
                        break;
                }
                break;
            case SDL_MOUSEWHEEL:
                if (event.wheel.y > 0)
                {
                    p_window->view_mod.y_offset += 20;
                    // p_window->view_mod.scale_factor += 0.1;
                }
                else if (event.wheel.y < 0)
                {
                    // p_window->view_mod.scale_factor
                    //     = p_window->view_mod.scale_factor <= 0.1
                    //           ? p_window->view_mod.scale_factor
                    //           : p_window->view_mod.scale_factor - 0.1;
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
                break;
            case SDL_MOUSEBUTTONDOWN:
                g_is_mouse_down = !g_is_mouse_down; // Set the flag when mouse
                                                    // button is down
                printf("mouse down: %d\n", g_is_mouse_down);
                SDL_GetMouseState(&g_prev_mousex, &g_prev_mousey);
                break;
            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_LEAVE)
                {
                    g_is_mouse_down = false;
                    printf("mouse left the window\n");
                }
                break;
            case SDL_MOUSEMOTION:
                if (g_is_mouse_down)
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
