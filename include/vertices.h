#ifndef VERTICES_H
#define VERTICES_H

typedef struct vertex_t
{
    int x;
    int y;
    int z;
} vertex_t;

typedef struct double_vertex_t
{
    double x;
    double y;
    double z;
    double t;
} double_vertex_t;

typedef struct raw_vertices_t
{
    double_vertex_t * p_vertex;
    int               size;
} raw_vertices_t;

#endif // VERTICES_H

/*** end of file ***/