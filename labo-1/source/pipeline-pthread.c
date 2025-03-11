#include <stdio.h>

#include "filter.h"
#include "pipeline.h"
#include "queue.h"

typedef struct io_image {
	image_dir_t* image_dir;
	queue_t* queue;
} io_image_t;

typedef struct queues {
	queue_t* queue1;
	queue_t* queue2;
} queues_t;

void* load_images(void* load_image_void);
void* scale_images(void* queues_void);
void* sharpen_images(void* queues_void);
void* sobel_images(void* queues_void);
void* save_images(void* save_image_void);

int pipeline_pthread(image_dir_t* image_dir) {
	const int num_threads[4] = {12, 12, 12, 12}; // Number of threads to create
	queue_t* queue1 = queue_create(num_threads[0] * sizeof(image_t*));
	queue_t* queue2 = queue_create(num_threads[1] * sizeof(image_t*));
	queue_t* queue3 = queue_create(num_threads[2] * sizeof(image_t*));
	queue_t* queue4 = queue_create(num_threads[3] * sizeof(image_t*));

	pthread_t thread0;
	pthread_t threads1[num_threads[0]];
	pthread_t threads2[num_threads[1]];
	pthread_t threads3[num_threads[2]];
	pthread_t threads4[num_threads[3]];

	io_image_t load_image = {image_dir, queue1};
	io_image_t save_image = {image_dir, queue4};
	queues_t queues1 = {queue1, queue2};
	queues_t queues2 = {queue2, queue3};
	queues_t queues3 = {queue3, queue4};

	pthread_create(&thread0, NULL, load_images, (void*) &load_image);

	for(int i = 0; i < num_threads[0]; i++) {
		pthread_create(&threads1[i], NULL, scale_images, (void*) &queues1);
	}

	for(int i = 0; i < num_threads[1]; i++) {
		pthread_create(&threads2[i], NULL, sharpen_images, (void*) &queues2);
	}

	for(int i = 0; i < num_threads[2]; i++) {
		pthread_create(&threads3[i], NULL, sobel_images, (void*) &queues3);
	}
	
	for(int i = 0; i < num_threads[3]; i++) {
		pthread_create(&threads4[i], NULL, save_images, (void*) &save_image);
	}

	pthread_join(thread0, NULL);

	for(int i = 0; i < num_threads[0]; i++) {
		pthread_join(threads1[i], NULL);
	}

	for(int i = 0; i < num_threads[1]; i++) {
		pthread_join(threads2[i], NULL);
	}

	for(int i = 0; i < num_threads[2]; i++) {
		pthread_join(threads3[i], NULL);
	}

	for(int i = 0; i < num_threads[3]; i++) {
		pthread_join(threads4[i], NULL);
	}

	queue_destroy(queue1);
	queue_destroy(queue2);
	queue_destroy(queue3);
	queue_destroy(queue4);

	return 0;
}


void* load_images(void* load_image_void) {
	io_image_t* load_image = (io_image_t*) load_image_void;
	image_dir_t* image_dir = load_image->image_dir;
	queue_t* queue = load_image->queue;
	image_t* image;
	while (1) {
		image = image_dir_load_next(image_dir);
		queue_push(queue, image);
		if (image == NULL) {
			for (int i = 0; i < 12; i++) {
				queue_push(queue, image);
			}
			break;
		}
	}
	return NULL;
}

void* scale_images(void* queues_void) {
	queues_t* queues = (queues_t*) queues_void;
	queue_t* queue1 = queues->queue1;
	queue_t* queue2 = queues->queue2;

	image_t* image;
	image_t* new_image;
	while (1) {
		image = queue_pop(queue1);
		if (image == NULL) {
			queue_push(queue2, image);
			break;
		}
		new_image = filter_scale_up(image, 2);
		queue_push(queue2, new_image);
		image_destroy(image);
	}
	return NULL;
}

void* sharpen_images(void* queues_void) {
	queues_t* queues = (queues_t*) queues_void;
	queue_t* queue1 = queues->queue1;
	queue_t* queue2 = queues->queue2;

	image_t* image;
	image_t* new_image;
	while (1) {
		image = queue_pop(queue1);
		if (image == NULL) {
			queue_push(queue2, image);
			break;
		}
		new_image = filter_sharpen(image);
		queue_push(queue2, new_image);
		image_destroy(image);
	}
	return NULL;
}

void* sobel_images(void* queues_void) {
	queues_t* queues = (queues_t*) queues_void;
	queue_t* queue1 = queues->queue1;
	queue_t* queue2 = queues->queue2;

	image_t* image;
	image_t* new_image;
	while (1) {
		image = queue_pop(queue1);
		if (image == NULL) {
			queue_push(queue2, image);
			break;
		}
		new_image = filter_sobel(image);
		queue_push(queue2, new_image);
		image_destroy(image);
	}
	return NULL;
}

void* save_images(void* save_image_void) {
	io_image_t* save_image = (io_image_t*) save_image_void;
	image_dir_t* image_dir = save_image->image_dir;
	queue_t* queue = save_image->queue;
	image_t* image;
	
	while (1) {
		image = queue_pop(queue);
		if (image == NULL) {
			break;
		}
		image_dir_save(image_dir, image);
		printf(".");
        fflush(stdout);
        image_destroy(image);
	}
	return NULL;
}