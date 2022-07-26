#include <stdio.h>
#include <complex.h>
#include <stdbool.h>
#include <math.h>
#include <unistd.h> //todo: figure out an OS neutral way to get thread count (like asking for it)
#include <stdint.h>
#include <time.h>
#include <stdlib.h>
#include <threads.h>

#define MAGICKCORE_QUANTUM_DEPTH 16
#define MAGICKCORE_HDRI_ENABLE 1
#include <MagickWand/MagickWand.h>

#include "colours.h"

typedef complex double complex_t; //Makes swapping pixels the particular type easier

#define IMG_WIDTH  16384
#define IMG_HEIGHT 16384
#define MAX_ITRS 1500
const complex_t left_top = -0.1529398937 +1.0397799646 * I;
const complex_t right_bottom = -0.1529394746 +1.0397795414 * I;

complex_t (*grid)[IMG_WIDTH];
uint32_t (*pixels)[IMG_WIDTH];

double delta_real;
double delta_img;

typedef struct thread_args {
    size_t num_threads;
    size_t thread_num; //[0, num_threads - 1]
} thread_args;

int compute_fractal(void* args);

int main(int argc, char** argv) {
    delta_real = (creal(right_bottom) - creal(left_top)) / IMG_WIDTH;
    delta_img = (cimag(left_top) - cimag(right_bottom)) / IMG_HEIGHT;

    if (argc > 1) { //means 2 pixel coordinate values were passed in, and we want to know what the coordinates are for them
        if (argc < 5) {
            fprintf(stderr, "Pixel to co-ord mapping requires 4 arguments!\n");
            return 1;
        }
        //todo: these *should* really be error checked
        double x1 = strtod(argv[1], NULL);
        double y1 = strtod(argv[2], NULL);
        double x2 = strtod(argv[3], NULL);
        double y2 = strtod(argv[4], NULL);

        complex_t first = (creal(left_top) + x1 * delta_real) + ((cimag(left_top) - y1 * delta_img) * I);
        complex_t second = (creal(left_top) + x2 * delta_real) + ((cimag(left_top) - y2 * delta_img) * I);
        printf("Pixels (%.2f, %.2f), and (%.2f, %.2f) are at %.10f %+.10f * I and %.10f %+.10f * I\n", x1, y1, x2, y2, creal(first), cimag(first),
               creal(second), cimag(second));
        return 0;
    }

    struct timespec start;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start);

    const size_t num_threads = sysconf(_SC_NPROCESSORS_CONF); //very POSIX specific

    grid = malloc(sizeof(complex_t[IMG_HEIGHT][IMG_WIDTH]));
    pixels = malloc(sizeof(uint32_t[IMG_HEIGHT][IMG_WIDTH])); //exit() is our garbage collector for this one

    thread_args* args = malloc(sizeof(thread_args) * num_threads);
    thrd_t* thread_ids = malloc(sizeof(thrd_t) * (num_threads - 1));

    for (size_t i = 0; i < num_threads; i++) {
        args[i].num_threads = num_threads;
        args[i].thread_num = i;
        if (i != 0) { //Using the main thread to do the first pool after
            thrd_t id;
            if (thrd_create(&id, &compute_fractal, args + i) == thrd_error) {
                fprintf(stderr, "Failed to create thread num %zu, exiting\n", i);
                exit(1);
            }
            thread_ids[i - 1] = id;
        }
    }
    compute_fractal(args);

    for (size_t i = 0; i < num_threads - 1; i++)
        thrd_join(thread_ids[i], NULL);

    free(grid);
    free(thread_ids);
    free(args);

    struct timespec stop;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &stop);
    double result = (stop.tv_sec - start.tv_sec) + (stop.tv_nsec - start.tv_nsec) * 1e-9;
    printf("Time taken on fractal: %f\n", result);

    printf("starting image write, please wait for finish\n");
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start);

    MagickWand* magick_wand = NewMagickWand();
    MagickWandGenesis();
    MagickConstituteImage(magick_wand, IMG_WIDTH, IMG_HEIGHT, "RGBA", CharPixel, pixels);
    if (MagickWriteImages(magick_wand, "fractal.png", MagickFalse) == MagickFalse) {
        fprintf(stderr, "Failed to save image!\n");
    }
    DestroyMagickWand(magick_wand);
    MagickWandTerminus();
    printf("image write finished\n");

    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &stop);
    result = (stop.tv_sec - start.tv_sec) + (stop.tv_nsec - start.tv_nsec) * 1e-9;
    printf("Time taken on image write: %f\n", result);

    return 0;
}

int compute_fractal(void* args) {
    size_t thread_num = ((thread_args*) args)->thread_num;
    size_t num_threads = ((thread_args*) args)->num_threads;
//    printf("id: %zu, num_threads: %zu, delta_img: %f, delta_real: %f\n", thread_num, num_threads, delta_img, delta_real);

    for (size_t y = thread_num; y < IMG_HEIGHT; y += num_threads) {
        complex_t y_val = (cimag(left_top) - y * delta_img) * I;

        for (size_t x = 0; x < IMG_WIDTH; x++) {
            complex_t x_val = creal(left_top) + x * delta_real;
            grid[y][x] = x_val + y_val;
        }
    }

    for (size_t y = thread_num; y < IMG_HEIGHT; y += num_threads) {
        for (size_t x = 0; x < IMG_WIDTH; x++) {
            complex_t z = 0;
            complex_t c = grid[y][x];
            bool broke = false;
            size_t itr;
            for (itr = 0; itr < MAX_ITRS; itr++) {
                z = z*z + c;
                if (creal(z)*creal(z) + cimag(z)*cimag(z) > (1 << 16)) {
                    broke = true;
                    break;
                }
            }
            if (!broke) {
                pixels[y][x] = inside_colour.packed;
//                grid[y][x] = 0 + -1 * I; //values inside_colour the set should not affect colourings of those outside
                continue;
            }
            //https://en.wikipedia.org/wiki/Plotting_algorithms_for_the_Mandelbrot_set#Continuous_(smooth)_coloring
            double log_zn = log(creal(z) * creal(z) + cimag(z) * cimag(z)) / 2;
            double nu = log2(log_zn / log(2));
            double itr_d = itr + 1 - nu;
            colour first = colours[(size_t) (itr_d / ((double) MAX_ITRS / num_colours))];
            colour second = colours[((size_t) (itr_d / ((double) MAX_ITRS / num_colours))) + 1];
            double frac = fmod(itr_d, 1);
            int rr = second.red   - first.red;
            int rg = second.green - first.green;
            int rb = second.blue  - first.blue;
            int ra = second.alpha - first.alpha;
            colour new = {frac * rr + first.red, frac * rg + first.green, frac * rb + first.blue, frac * ra + first.alpha};
            pixels[y][x] = new.packed;
        }
    }
//    printf("id: %zu min: %f, max: %f\n", thread_num, min_esc_thr[thread_num], max_esc_thr[thread_num]);
    return 0;
}
