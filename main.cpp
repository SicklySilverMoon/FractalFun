#include <iostream>
#include <cstdio>
#include <cstdint>
#include <ctime>
#include <cstdlib>

#include <unistd.h> //todo: figure out an OS neutral way to get thread count (like asking for it)
#include <threads.h>
#include <sys/stat.h>

#include "lodepng/lodepng.h"

#include "complex_t.h"
#include "colours.h"

#define IMG_WIDTH  1638
#define IMG_HEIGHT 1638
#define MAX_ITRS 1500
const complex_t left_top = complex_t(-1.8005981445, -0.0812377930);
const complex_t right_bottom = complex_t(-1.7136230469, +0.0057373047);

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
    delta_real = (right_bottom.real() - left_top.real()) / IMG_WIDTH;
    delta_img = (left_top.imag() - right_bottom.imag()) / IMG_HEIGHT;

    if (argc > 1) { //means 2 pixel coordinate values were passed in, and we want to know what the coordinates are for them
        if (strcmp(argv[1], "-p") == 0) {
            if (argc < 6) {
                fprintf(stderr, "Pixel to co-ord mapping requires 4 arguments!\n");
                return 1;
            }
            //todo: these *should* really be error checked
            double x1 = strtod(argv[1], nullptr);
            double y1 = strtod(argv[2], nullptr);
            double x2 = strtod(argv[3], nullptr);
            double y2 = strtod(argv[4], nullptr);

            complex_t first = complex_t{left_top.real() + x1 * delta_real, left_top.imag() - y1 * delta_img};
            complex_t second = complex_t{left_top.real() + x2 * delta_real, left_top.imag() - y2 * delta_img};
            printf("Pixels (%.2f, %.2f), and (%.2f, %.2f) are at (%.10f, %+.10f) and (%.10f, %+.10f)\n", x1, y1, x2, y2,
                   first.real(), first.imag(),
                   second.real(), second.imag());
            return 0;
        } else {

        }
    } else {
        std::cout << "FractalFun [-p P1x P1y P2x P2y | [-i itrs] [-w width] [-h height]] C1x C1y C2x C2y" << std::endl;
//        return 0;
    }

    struct timespec start{};
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start);

    const size_t num_threads = sysconf(_SC_NPROCESSORS_CONF); //very POSIX specific

    grid = new complex_t[IMG_HEIGHT][IMG_WIDTH];
    pixels = new uint32_t[IMG_HEIGHT][IMG_WIDTH]; //exit() is our garbage collector for this one

    auto* args = new thread_args[num_threads];
    auto* thread_ids = new thrd_t[num_threads - 1];

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
        thrd_join(thread_ids[i], nullptr);

    delete[] grid;
    delete[] thread_ids;
    delete[] args;

    struct timespec stop{};
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &stop);
    double result = ((stop.tv_sec - start.tv_sec) + (stop.tv_nsec - start.tv_nsec) * 1e-9) / num_threads;
    printf("Time taken on fractal: %f\n", result);

    printf("starting image write, please wait for finish\n");
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start);

    struct stat statbuf{};
    if (stat(type_name, &statbuf) != -1) {
        if (!S_ISDIR(statbuf.st_mode)) {
            remove(type_name);
            mkdir(type_name, S_IRWXU | S_IRWXG | S_IRWXO);
        }
    } else {
        mkdir(type_name, S_IRWXU | S_IRWXG | S_IRWXO);
    }

    char* filename;
    asprintf(&filename, "%s/(%.10f, %+.10f)-(%.10f, %+.10f) (%u itr) (%upx x %upx).png", type_name, real(left_top),
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
        complex_t y_val = complex_t{0, left_top.imag() - y * delta_img};

        for (size_t x = 0; x < IMG_WIDTH; x++) {
            complex_t x_val = complex_t{left_top.real() + x * delta_real, 0};
            grid[y][x] = x_val + y_val;
        }
    }

    for (size_t y = thread_num; y < IMG_HEIGHT; y += num_threads) {
        for (size_t x = 0; x < IMG_WIDTH; x++) {
            complex_t z = complex_t{0, 0};
            complex_t c = grid[y][x];
            bool broke = false;
            size_t itr;
            for (itr = 0; itr < MAX_ITRS; itr++) {
                z = complex_t{fabs(z.real()), fabs(z.imag())}; //burning ship
                z = z * z + c;
                if (norm(z) > (4)) {
                    broke = true;
                    break;
                }
            }
            if (!broke) {
                pixels[y][x] = inside_colour.packed();
//                grid[y][x] = 0 + -1 * I; //values inside_colour the set should not affect colourings of those outside
                continue;
            }

            double continuous_index = itr + 1 - (log(2) / abs(z)) / log(2);
            uint8_t red =   ((sin(0.016 * continuous_index + 4) + 1) * (230 / 2.0) + 25);
            uint8_t green = ((sin(0.013 * continuous_index + 2) + 1) * (230 / 2.0) + 25);
            uint8_t blue =  ((sin(0.01  * continuous_index + 1) + 1) * (230 / 2.0) + 25);
            uint8_t alpha = 255;
            colour colour{red, green, blue, alpha};
            pixels[y][x] = colour.packed();
        }
    }
//    printf("id: %zu min: %f, max: %f\n", thread_num, min_esc_thr[thread_num], max_esc_thr[thread_num]);
    return 0;
}
