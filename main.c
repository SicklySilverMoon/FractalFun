#define _GNU_SOURCE
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h> //todo: figure out an OS neutral way to get thread count (like asking for it)
#include <stdint.h>
#include <time.h>
#include <stdlib.h>
#include <threads.h>
#include <sys/stat.h>
#include "lodepng/lodepng.h"
#include "complex_t.h"
#include "colours.h"

#define IMG_WIDTH  16384
#define IMG_HEIGHT 16384
#define MAX_ITRS 1500
const complex_t left_top = ctor(-2.0000000000, +1.2377929688);
const complex_t right_bottom = ctor(0.4755859375, -1.2377929688);

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
    delta_real = (real(right_bottom) - real(left_top)) / IMG_WIDTH;
    delta_img = (imag(left_top) - imag(right_bottom)) / IMG_HEIGHT;

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

        complex_t first = ctor(real(left_top) + x1 * delta_real, imag(left_top) - y1 * delta_img);
        complex_t second = ctor(real(left_top) + x2 * delta_real, imag(left_top) - y2 * delta_img);
        printf("Pixels (%.2f, %.2f), and (%.2f, %.2f) are at (%.10f, %+.10f) and (%.10f, %+.10f)\n", x1, y1, x2, y2, real(first), imag(first),
               real(second), imag(second));
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
    double result = ((stop.tv_sec - start.tv_sec) + (stop.tv_nsec - start.tv_nsec) * 1e-9) / num_threads;
    printf("Time taken on fractal: %f\n", result);

    printf("starting image write, please wait for finish\n");
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start);

    struct stat statbuf;
    if (stat(str, &statbuf) != -1) {
        if (!S_ISDIR(statbuf.st_mode)) {
            remove(str);
            mkdir(str, S_IRWXU | S_IRWXG | S_IRWXO);
        }
    } else {
        mkdir(str, S_IRWXU | S_IRWXG | S_IRWXO);
    }

    char* filename;
    asprintf(&filename, "%s/(%.10f, %+.10f)-(%.10f, %+.10f) (%u itr) (%upx x %upx).png", str, real(left_top),
             imag(left_top), real(right_bottom), imag(right_bottom), MAX_ITRS, IMG_WIDTH, IMG_HEIGHT);
    lodepng_encode32_file(filename, (unsigned char*) pixels, IMG_WIDTH, IMG_HEIGHT);
    free(filename);

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
        complex_t y_val = ctor(0, imag(left_top) - y * delta_img);

        for (size_t x = 0; x < IMG_WIDTH; x++) {
            complex_t x_val = ctor(real(left_top) + x * delta_real, 0);
            grid[y][x] = add(x_val, y_val);
        }
    }

    for (size_t y = thread_num; y < IMG_HEIGHT; y += num_threads) {
        for (size_t x = 0; x < IMG_WIDTH; x++) {
            complex_t z = ctor(0, 0);
            complex_t c = grid[y][x];
            bool broke = false;
            size_t itr;
            for (itr = 0; itr < MAX_ITRS; itr++) {
                z = sub(z, ctor(1, 0));
                z = add(mul(z, mul(z, z)), c);
//                z = ctor(fabs(real(z)), fabs(imag(z)));
                if (sabs(z) > (4)) {
                    broke = true;
                    break;
                }
            }
            if (!broke) {
                pixels[y][x] = inside_colour.packed;
//                grid[y][x] = 0 + -1 * I; //values inside_colour the set should not affect colourings of those outside
                continue;
            }

            double continuous_index = itr + 1 - (log(2) / abs(z)) / log(2);
            size_t idx = continuous_index / ((double) MAX_ITRS / NUM_COLOURS);
            colour first = colours[idx];
            colour second = colours[(idx + 1) % NUM_COLOURS];
            double frac = fmod(continuous_index, 1);
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
