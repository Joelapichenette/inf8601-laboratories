#include <stdio.h>

extern "C" {
#include "filter.h"
#include "pipeline.h"
}
#include "tbb/pipeline.h"

class Load {
        image_dir_t* directory;
    public:
        Load (image_dir_t* dir) : directory(dir) {};
        image_t* operator()(tbb::flow_control& fc) const {
            image_t* image = image_dir_load_next(directory);
            if (image == nullptr) {
                fc.stop();
                return nullptr;
            }
            return image;
        }
};

class Scale {
    public:
        image_t* operator()(image_t* image) const {
            if (image == nullptr) {
                return nullptr;
            }
            image_t* new_img = filter_scale_up(image, 2);
            image_destroy(image);
            return new_img;
        }
};

class Sharpen {
    public:
        image_t* operator()(image_t* image) const {
            if (image == nullptr) {
                return nullptr;
            }
            image_t* new_img = filter_sharpen(image);
            image_destroy(image);
            return new_img;
        }
};

class Sobel {
    public:
        image_t* operator()(image_t* image) const {
            if (image == nullptr) {
                return nullptr;
            }
            image_t* new_img = filter_sobel(image);
            image_destroy(image);
            return new_img;
        }
};

class Save {
        image_dir_t* directory;
    public:
        Save (image_dir_t* dir) : directory(dir) {};
        void operator()(image_t* image) const {
            if (image == nullptr) {
                return;
            }
            printf(".");
            fflush(stdout);
            image_dir_save(directory, image);
            image_destroy(image);
            return;
        }
};

int pipeline_tbb(image_dir_t* image_dir) {
    size_t ntoken = 100;

    tbb::parallel_pipeline(ntoken,
        tbb::make_filter<void,image_t*>(
            tbb::filter::serial_in_order, Load(image_dir) )
    &
        tbb::make_filter<image_t*,image_t*>(
            tbb::filter::parallel, Scale() )
    &
        tbb::make_filter<image_t*,image_t*>(
            tbb::filter::parallel, Sharpen() )
    &
        tbb::make_filter<image_t*,image_t*>(
            tbb::filter::parallel, Sobel() )
    &
        tbb::make_filter<image_t*,void>(
            tbb::filter::parallel, Save(image_dir) ) );

    return 0;
}