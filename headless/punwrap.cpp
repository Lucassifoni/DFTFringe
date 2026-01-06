/******************************************************************************
**
**  Copyright 2016 Dale Eason
**  This file is part of DFTFringe
**  is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation version 3 of the License

** DFTFringe is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with DFTFringe.  If not, see <http://www.gnu.org/licenses/>.

****************************************************************************/
#include "punwrap.h"
#include <math.h>
#include <algorithm>
#include <queue>
#include <cstring>
#include <stdlib.h>

static int xsize, ysize;
static double* qmap_global = nullptr;
static double* phase_global;
static double* unwrapped_global;

#define WRAP(x) (((x) > 0.5) ? ((x)-1.0) : (((x) <= -0.5) ? ((x)+1.0) : (x)))

static char* flags_global = nullptr;

#define SWAP(a, i, j) { int t = a[i]; a[i] = a[j]; a[j] = t; }

static void todo_push(int ndx, double *qmap, int *todo, int *end) {
    int child;
    todo[*end] = ndx;
    child = (*end)++;
    while (child > 0) {
        int parent = (child-1) / 2;
        if (qmap[todo[parent]] < qmap[todo[child]]) {
            SWAP(todo, parent, child);
            child = parent;
        } else {
            break;
        }
    }
}

static int todo_pop(double *qmap, int *todo, int *end) {
    int result = todo[0], root;
    --(*end);
    SWAP(todo, 0, *end);
    root = 0;
    while (root*2+1 < *end) {
        int child = root*2+1;
        if (child+1 < *end && qmap[todo[child]] < qmap[todo[child+1]])
            ++child;
        if (qmap[todo[root]] < qmap[todo[child]]) {
            SWAP(todo, root, child);
            root = child;
        } else {
            break;
        }
    }
    return result;
}

#define unwrap_and_insert(ndx, val) { \
    unwrapped[ndx] = val; \
    flags[ndx] |= UNWRAPPED; \
    path[ndx] = order++; \
    todo_push(ndx, qmap, todo, &end); \
}

static void qg_path_follower(int nx, int ny, double *phase, double *qmap,
                             double *unwrapped, double *path) {
    int *todo;
    int end;
    int order = 0;
    int size = nx * ny;
    char *flags = flags_global;

    todo = (int*)calloc(size, sizeof(int));
    end = 0;

    while (1) {
        double m = -HUGE_VAL;
        int mndx = 0;
        for (int k = 0; k < size; ++k)
            if (qmap[k] > m && !flags[k])
                m = qmap[mndx = k];
        if (m == -HUGE_VAL) break;

        unwrap_and_insert(mndx, phase[mndx]);

        while (end) {
            int ndx = todo_pop(qmap, todo, &end);
            int x = ndx % nx;
            int y = ndx / nx;
            double val = unwrapped[ndx];
            if (x > 0 && !flags[ndx-1])
                unwrap_and_insert(ndx-1, val + WRAP(phase[ndx-1] - phase[ndx]));
            if (x < nx-1 && !flags[ndx+1])
                unwrap_and_insert(ndx+1, val + WRAP(phase[ndx+1] - phase[ndx]));
            if (y > 0 && !flags[ndx-nx])
                unwrap_and_insert(ndx-nx, val + WRAP(phase[ndx-nx] - phase[ndx]));
            if (y < ny-1 && !flags[ndx+nx])
                unwrap_and_insert(ndx+nx, val + WRAP(phase[ndx+nx] - phase[ndx]));
        }
    }

    free(todo);
}

static void dv_quality_map(double *pphase, int width, double *qmap, int nx, int ny) {
    double *dx = (double*)calloc(nx * ny, sizeof(double));
    double *dy = (double*)calloc(nx * ny, sizeof(double));

    for (int x = 0; x < nx; ++x) {
        for (int y = 0; y < ny; ++y) {
            int ndx = y * nx + x;
            if (x == nx - 1)
                dx[ndx] = 0.0;
            else
                dx[ndx] = WRAP(pphase[ndx + 1] - pphase[ndx]);
            if (y == ny - 1)
                dy[ndx] = 0.0;
            else
                dy[ndx] = WRAP(pphase[ndx + nx] - pphase[ndx]);
        }
    }

    int start = -(width / 2);
    int end = start + width;
    int size = width * width;
    double* ex = new double[size];
    double* ey = new double[size];

    for (int x = 0; x < nx; ++x) {
        for (int y = 0; y < ny; ++y) {
            int n = 0;
            for (int i = std::max(x + start, 0); i < std::min(x + end, nx - 1); ++i) {
                for (int j = std::max(y + start, 0); j < std::min(y + end, ny - 1); ++j) {
                    int ndx = j * nx + i;
                    if (0.0 != dx[ndx] && 0.0 != dy[ndx]) {
                        ex[n] = dx[ndx];
                        ey[n] = dy[ndx];
                        n++;
                    }
                }
            }
            int ndx = y * nx + x;
            if (n < 1) {
                qmap[ndx] = 0;
            } else {
                double mx = 0, my = 0;
                for (int k = 0; k < n; k++) {
                    mx += ex[k];
                    my += ey[k];
                }
                mx /= n;
                my /= n;
                double sx = 0, sy = 0;
                for (int k = 0; k < n; k++) {
                    sx += pow(ex[k] - mx, 2);
                    sy += pow(ey[k] - my, 2);
                }
                qmap[ndx] = (sqrt(sx) + sqrt(sy)) / size;
            }
        }
    }

    free(dx);
    free(dy);
    delete[] ex;
    delete[] ey;
}

static double* g_qqmap = nullptr;
static double* path_global = nullptr;

void unwrap(double *pphase, double *punwrapped, char *bflags, int nx, int ny) {
    xsize = nx;
    ysize = ny;
    unwrapped_global = punwrapped;
    phase_global = pphase;

    const int size = xsize * ysize;
    flags_global = bflags;

    if (g_qqmap)
        delete[] g_qqmap;
    g_qqmap = new double[size];
    memset(g_qqmap, 0, sizeof(double) * size);

    if (path_global)
        delete[] path_global;
    path_global = new double[size];
    memset(path_global, 0, sizeof(double) * size);

    dv_quality_map(pphase, 5, g_qqmap, nx, ny);
    for (int i = 0; i < size; ++i)
        g_qqmap[i] *= -1.;

    qg_path_follower(nx, ny, pphase, g_qqmap, punwrapped, path_global);
}
