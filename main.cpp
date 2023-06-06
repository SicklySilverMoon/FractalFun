#include <iostream>
#include <cstdio>
#include <cstdint>
#include <ctime>
#include <cstdlib>

#include <unistd.h> //todo: figure out an OS neutral way to get thread count (like asking for it)
#include <threads.h>
#include <sys/stat.h>

#include "lodepng/lodepng.h"
#include "bmpWriter.h"

#include "complex_t.h"
#include "colours.h"

typedef struct thread_args {
    size_t num_threads;
    size_t thread_num; //[0, num_threads - 1]
    size_t max_itrs;
    size_t img_width;
    size_t img_height;
    complex_t left_top;
    complex_t right_bottom;
//    complex_t* grid;
    uint32_t* pixels;
} thread_args;

int compute_fractal(void* args);

int check_argc_range(size_t i, size_t val, int argc, char const* option) {
    if (i + val >= argc) {
        std::cout << "the " << option << " requires " << val << " parameters";
        return -1;
    }
    return 0;
}

int main(int argc, char** argv) {
//    delta_real = (right_bottom.real() - left_top.real()) / IMG_WIDTH;
//    delta_img = (left_top.imag() - right_bottom.imag()) / IMG_HEIGHT;

    size_t max_itrs = 1500;
    size_t img_width = 16384;
    size_t img_height = 16384;

    complex_t left_top{-2, 1.5};
    complex_t right_bottom{1, -1.5};

    if (argc > 1) { //means 2 pixel coordinate values were passed in, and we want to know what the coordinates are for them
        if (strcmp(argv[1], "-p") == 0) {
            if (argc < 12) {
                fprintf(stderr, "Pixel to co-ord mapping requires 10 arguments: width height left_top_x left_top_y bottom_right_x bottom_right_y pixel_left_top_x pixel_left_top_y pixel_bottom_right_x pixel_bottom_right_y\n");
                return 1;
            }
            img_width = strtoull(argv[2], nullptr, 10);
            img_height = strtoull(argv[3], nullptr, 10);

            double top_left_real = strtod(argv[4], nullptr);
            double top_left_img = strtod(argv[5], nullptr);
            double bottom_right_real = strtod(argv[6], nullptr);
            double bottom_right_img = strtod(argv[7], nullptr);
            const complex_t::value_type delta_real = (bottom_right_real - top_left_real) / img_width;
            const complex_t::value_type delta_img = (top_left_img - bottom_right_img) / img_height;

            //todo: these *should* really be error checked
            double x1 = strtod(argv[8], nullptr);
            double y1 = strtod(argv[9], nullptr);
            double x2 = strtod(argv[10], nullptr);
            double y2 = strtod(argv[11], nullptr);

            complex_t first = complex_t{left_top.real() + x1 * delta_real, left_top.imag() - y1 * delta_img};
            complex_t second = complex_t{left_top.real() + x2 * delta_real, left_top.imag() - y2 * delta_img};
            printf("Pixels (%.2f, %.2f), and (%.2f, %.2f) at image width and height (%zu, %zu) are at (%.10f, %+.10f) and (%.10f, %+.10f)\n", x1, y1, x2, y2,
                   img_width, img_height,
                   first.real(), first.imag(),
                   second.real(), second.imag());
            return 0;
        } else {
            if (argc < 5) {
                std::cout << "Please enter 4 co-ords" << std::endl;
                return 0;
            }
            size_t i = 1;
            size_t coords_added = 0;
            complex_t::value_type coords[4];
            while (i < argc) {
                if (strcmp(argv[i], "-i") == 0) {
                    if (check_argc_range(i, 1, argc, "i"))
                        return 1;
                    max_itrs = strtoull(argv[i + 1], nullptr, 0);
                    i += 2;
                    continue;
                } else if (strcmp(argv[i], "-w") == 0) {
                    if (check_argc_range(i, 1, argc, "w"))
                        return 1;
                    img_width = strtoull(argv[i + 1], nullptr, 0);
                    i += 2;
                    continue;
                } else if (strcmp(argv[i], "-h") == 0) {
                    if (check_argc_range(i, 1, argc, "h"))
                        return 1;
                    img_height = strtoull(argv[i + 1], nullptr, 0);
                    i += 2;
                    continue;
                } else {
                    coords[coords_added] = strtod(argv[i], nullptr);
                    coords_added++;
                    i++;
                }
            }

            if (4 > coords_added) {
                std::cout << "Please enter 4 co-ords" << std::endl;
                return 2;
            } else {
                left_top = {coords[0], coords[1]};
                right_bottom = {coords[2], coords[3]};
            }
        }
    } else {
        std::cout << "FractalFun C1x C1y C2x C2y [-p P1x P1y P2x P2y | [-i itrs] [-w width] [-h height]]" << std::endl;
//        return 0;
    }

    struct timespec start{};
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start);

    const size_t num_threads = sysconf(_SC_NPROCESSORS_CONF); //very POSIX specific

//    auto grid = new complex_t[img_height * img_width];
    auto pixels = new uint32_t[img_height * img_width]; //exit() is our garbage collector for this one

    auto* args = new thread_args[num_threads];
    auto* thread_ids = new thrd_t[num_threads - 1];

    for (size_t i = 0; i < num_threads; i++) {
        args[i] = {num_threads, i, max_itrs, img_width, img_height, left_top, right_bottom, /*grid,*/ pixels};
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

//    delete[] grid;
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
    asprintf(&filename, "%s/(%.10f, %+.10f)-(%.10f, %+.10f) (%zu itr) (%zupx x %zupx).png", type_name, real(left_top),
             imag(left_top), real(right_bottom), imag(right_bottom), max_itrs, img_width, img_height);
    lodepng_encode32_file(filename, (unsigned char*) pixels, img_width, img_height);
//    generateBitmapImage(pixels, img_height, img_width, filename);
    free(filename);

    printf("image write finished\n");

    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &stop);
    result = (stop.tv_sec - start.tv_sec) + (stop.tv_nsec - start.tv_nsec) * 1e-9;
    printf("Time taken on image write: %f\n", result);

    return 0;
}

int compute_fractal(void* args) {
    size_t const thread_num = ((thread_args*) args)->thread_num;
    size_t const num_threads = ((thread_args*) args)->num_threads;
    size_t const max_itrs = ((thread_args*) args)->max_itrs;
    size_t const img_width = ((thread_args*) args)->img_width;
    size_t const img_height = ((thread_args*) args)->img_height;

    complex_t const left_top = ((thread_args*) args)->left_top;
    complex_t const right_bottom = ((thread_args*) args)->right_bottom;
    const complex_t::value_type delta_real = (right_bottom.real() - left_top.real()) / img_width;
    const complex_t::value_type delta_img = (left_top.imag() - right_bottom.imag()) / img_height;

//    complex_t* grid = ((thread_args*) args)->grid;
    auto* pixels = ((thread_args*) args)->pixels;
//    printf("id: %zu, num_threads: %zu, delta_img: %f, delta_real: %f\n", thread_num, num_threads, delta_img, delta_real);

//    for (size_t y = thread_num; y < img_height; y += num_threads) {
//        complex_t y_val = complex_t{0, left_top.imag() - y * delta_img};
//
//        for (size_t x = 0; x < img_width; x++) {
//            complex_t x_val = complex_t{left_top.real() + x * delta_real, 0};
//            grid[y * img_width + x] = x_val + y_val;
//        }
//    }

    for (size_t y = thread_num; y < img_height; y += num_threads) {
        for (size_t x = 0; x < img_width; x++) {
            complex_t z = complex_t{0, 0};
            complex_t c = complex_t{left_top.real() + x * delta_real, left_top.imag() - y * delta_img}; //grid[y * img_width + x];
//            std::cout << c << "\n";
            bool broke = false;
            size_t itr;
            for (itr = 0; itr < max_itrs; itr++) {
//                z = conj(z) * conj(z) + c; //tricorn

                z = z * z + c; //Mandelbrot
                if (norm(z) > (4)) {
                    broke = true;
                    break;
                }
            }
            if (!broke) {
                pixels[y * img_width + x] = inside_colour.packed();
//                grid[y * img_width + x] = 0 + -1 * I; //values inside_colour the set should not affect colourings of those outside
                continue;
            }

            double continuous_index = itr + 1 - (log(2) / abs(z)) / log(2);
            uint8_t red =   ((sin(0.058 * continuous_index + 4) + 1) * (230 / 2.0) + 25);
            uint8_t green = ((sin(0.0565* continuous_index + 2) + 1) * (230 / 2.0) + 25);
            uint8_t blue =  ((sin(0.055 * continuous_index + 1) + 1) * (230 / 2.0) + 25);
            uint8_t alpha = 255;
            Colour colour{red, green, blue, alpha};
            pixels[y * img_width + x] = colour.packed();
        }
    }
//    printf("id: %zu min: %f, max: %f\n", thread_num, min_esc_thr[thread_num], max_esc_thr[thread_num]);
    return 0;
}
