#include "helpers.cl"

typedef struct modified_sinoscope {
    unsigned int buffer_size;
    unsigned int width;
    unsigned int height;
    unsigned int taylor;
    unsigned int interval;
} modified_sinoscope_t;

// create a kernel with opencl
__kernel void sinoscope_image_kernel(__global unsigned char* buffer, modified_sinoscope_t mod_sinoscope, float interval_inverse, float time, float max, float phase0, float phase1, float dx, float dy) {
    
    int id = get_global_id(0);
    int j = (int) id / mod_sinoscope.height;
    int i = (int) id % mod_sinoscope.height;

    float px = dx * j - 2 * M_PI;
    float py = dy * i - 2 * M_PI;
    float value = 0;

    for (int k = 1; k <= mod_sinoscope.taylor; k += 2) {
        value += sin(px * k * phase1 + time) / k;
        value += cos(py * k * phase0) / k;
    }

    value = (atan(value) - atan(-value)) / M_PI;
    value = (value + 1) * 100;

    pixel_t pixel;
    color_value(&pixel, value, mod_sinoscope.interval, interval_inverse);

    int index = (i * 3) + (j * 3) * mod_sinoscope.width;

    buffer[index + 0] = pixel.bytes[0];
    buffer[index + 1] = pixel.bytes[1];
    buffer[index + 2] = pixel.bytes[2];
}