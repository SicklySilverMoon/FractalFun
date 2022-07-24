#include <stdio.h>
#include <complex.h>
#include <stdbool.h>
#include <math.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include <time.h>
#include <stdlib.h>
#include <threads.h>

typedef complex double complex_t; //Makes swapping out the particular type easier

typedef union colour {
    struct {
        uint8_t red;
        uint8_t green;
        uint8_t blue;
        uint8_t alpha;
    };
    uint32_t packed;
} colour;

//Do be warned about these, it will create a raw pixel output file of IMG_WIDTH * IMG_HEIGHT * 4 bytes, for h&w of 1638
//that's a 1.0 GiB file, the memory use is even worse as it's that plus another IMG_WIDTH * IMG_HEIGHT * 16 bytes
#define IMG_WIDTH  16384
#define IMG_HEIGHT 16384
const complex_t left_top = -2 +1.5 * I;
const complex_t right_bottom = 1 - 1.5 * I;
const colour near = {0xFF, 0xFF, 0xFF, 0xFF}; //todo: add more colours, ex if you have 4 then split the min-max range into like 3 sections,
const colour far = {0x00, 0x00, 0xFF, 0xFF};  //first section goes from colour A to B, 2nd B to C, 3rd C to D, (maybe 4th D to A or something)
const colour inside = {0x00, 0x00, 0x00, 0xFF}; //Black

_Atomic complex_t (*grid)[IMG_WIDTH];
//_Atomic bool (*gridb)[IMG_WIDTH];
//complex_t grid[IMG_HEIGHT][IMG_WIDTH];
uint32_t (*out)[IMG_WIDTH];

double* min_esc_thr;
double* max_esc_thr;

typedef struct thread_args {
    double delta_real;
    double delta_img;
    size_t thread_num; //[0, num_threads - 1]
    size_t num_threads;
} thread_args;

int compute_values(void* args) {
    double delta_img = ((thread_args*) args)->delta_img;
    double delta_real = ((thread_args*) args)->delta_real;
    size_t num = ((thread_args*) args)->thread_num;
    size_t step = ((thread_args*) args)->num_threads;
//    printf("id: %zu, step: %zu, delta_img: %f, delta_real: %f\n", num, step, delta_img, delta_real);

    for (size_t y = num; y < IMG_HEIGHT; y += step) {
        complex_t y_val = (cimag(left_top) - y * delta_img) * I;

        for (size_t x = 0; x < IMG_WIDTH; x++) {
            complex_t x_val = creal(left_top) + x * delta_real;
            grid[y][x] = x_val + y_val;
        }
    }

    for (size_t y = num; y < IMG_HEIGHT; y += step) {
        for (size_t x = 0; x < IMG_WIDTH; x++) {
            complex_t z = 0;
            complex_t c = grid[y][x];
            bool broke = false;
            for (size_t i = 0; i < 1500; i++) {
                z = z*z + c;
                if (creal(z)*creal(z) + cimag(z)*cimag(z) > 4) {
                    broke = true;
                    break;
                }
            }
            if (!broke) {
                grid[y][x] = 0;
                continue;
            }
            double z_abs = cabs(z);
            if (z_abs > max_esc_thr[num])
                max_esc_thr[num] = z_abs;
            if (z_abs < min_esc_thr[num])
                min_esc_thr[num] = z_abs;
            grid[y][x] = z_abs;
        }
    }
//    printf("id: %zu min: %f, max: %f\n", num, min_esc_thr[num], max_esc_thr[num]);
    return 0;
}

int main(int argc, char** argv) {
    double delta_real = (creal(right_bottom) - creal(left_top)) / IMG_WIDTH;
    double delta_img = (cimag(left_top) - cimag(right_bottom)) / IMG_HEIGHT;

    if (argc > 1) { //means 2 pixel coordinate values were passed in, and we want to know what the coordinates are for them
        //todo: these *should* really be error checked
        double x1 = strtof(argv[1], NULL);
        double y1 = strtof(argv[2], NULL);
        double x2 = strtof(argv[3], NULL);
        double y2 = strtof(argv[4], NULL);

        complex_t first = (creal(left_top) + x1 * delta_real) + ((cimag(left_top) - y1 * delta_img) * I);
        complex_t second = (creal(right_bottom) + x2 * delta_real) + ((cimag(right_bottom) - y2 * delta_img) * I);
        printf("Pixels (%.2f, %.2f), and (%.2f, %.2f) are at %.10f %+.10f * I and %.10f %+.10f * I\n", x1, y1, x2, y2, creal(first), cimag(first),
               creal(second), cimag(second));
        return 0;
    }

    struct timespec start;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start);

    const size_t num_threads = sysconf(_SC_NPROCESSORS_CONF); //very POSIX specific

    grid = malloc(sizeof(complex_t[IMG_HEIGHT][IMG_WIDTH]));
    out = malloc(sizeof(uint32_t[IMG_HEIGHT][IMG_WIDTH]));
    min_esc_thr = malloc(sizeof(double) * num_threads);
    max_esc_thr = malloc(sizeof(double) * num_threads);

    thread_args* args = malloc(sizeof(thread_args) * num_threads);
    thrd_t* thread_ids = malloc(sizeof(thrd_t) * (num_threads - 1));

    for (size_t i = 0; i < num_threads; i++) {
        min_esc_thr[i] = INFINITY;
        max_esc_thr[i] = -INFINITY;
        args[i].delta_img = delta_img;
        args[i].delta_real = delta_real;
        args[i].num_threads = num_threads;
        args[i].thread_num = i;
        if (i != 0) { //Using the main thread to do the first pool after
            thrd_t id;
            if (thrd_create(&id, &compute_values, args + i) == thrd_error) {
                fprintf(stderr, "Failed to create thread num %zu, exiting\n", i);
                exit(1);
            }
            thread_ids[i - 1] = id;
        }
    }
    compute_values(args);

    for (size_t i = 0; i < num_threads - 1; i++)
        thrd_join(thread_ids[i], NULL);

    double min_esc = min_esc_thr[0];
    double max_esc = max_esc_thr[0];
    for (size_t i = 1; i < num_threads; i++) {
        if (min_esc_thr[i] < min_esc)
            min_esc = min_esc_thr[i];
        if (max_esc_thr[i] > max_esc)
            max_esc = max_esc_thr[i];
    }

    const double diff = max_esc - min_esc;
    printf("min: %f, max: %f, diff: %f\n", min_esc, max_esc, diff);
//    printf("%f\n", diff);

    for (size_t y = 0; y < IMG_HEIGHT; y++) {
        for (size_t x = 0; x < IMG_WIDTH; x++) {
            double val = creal(grid[y][x]);
            if (val == 0) {
//                printf("inside at %04zu, %04zu", x, y);
                out[y][x] = inside.packed;
                continue;
            }
            colour new = {(val - min_esc) / diff * (near.red - far.red) + far.red,
                          (val - min_esc) / diff * (near.green - far.green) + far.green,
                          (val - min_esc) / diff * (near.blue - far.blue) + far.blue,
                          (val - min_esc) / diff * (near.alpha - far.alpha) + far.alpha};
//            printf("%f\n", (val - min_esc_thr) / diff * (near.red   - far.red)   + far.red);
//            printf("%02hhX, \n", (char)((val - min_esc_thr) / diff * (near.red - far.red)));
            out[y][x] = new.packed;
        }
    }

    int fd = open("out.bin", O_CREAT | O_WRONLY, S_IWUSR | S_IRUSR | S_IWGRP | S_IRGRP | S_IWOTH | S_IROTH);
    ssize_t written = 0;
    while (written != sizeof(uint32_t[IMG_HEIGHT][IMG_WIDTH])) {
        written += write(fd, ((char*)out) + written, sizeof(uint32_t[IMG_HEIGHT][IMG_WIDTH]) - written); //todo: lol stop dumping to a raw file and
    }                                                                                                           //get something to write PNGs
    close(fd); //For now I've just been using python or imagemagik to generate the PNGs from this data

    struct timespec stop;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &stop);
    double result = (stop.tv_sec - start.tv_sec) + (stop.tv_nsec - start.tv_nsec) * 1e-9; // in microseconds
    printf("Time taken: %f", result);
    return 0;
}
