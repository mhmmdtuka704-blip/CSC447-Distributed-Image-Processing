#ifndef BATCH_H
#define BATCH_H

#include <limits.h>

typedef struct {
    int width, height, brightness;
} Recipe;

typedef struct {
    char *input;
    int status;
} Job;

typedef struct {
    Job *jobs;
    int count;
    char output[PATH_MAX];
    Recipe recipe;
} Batch;

typedef struct {
    int completed, failed;
    double busy_seconds;
} RankStats;

enum { PENDING = -1, SAVED, LOAD_FAILED, ALLOC_FAILED, SAVE_FAILED };

int prepare_batch(Batch *batch, int argc, char **argv);
void free_batch(Batch *batch);
int process_image(const char *input, const char *output, int id, Recipe recipe);
int report_batch(const Batch *batch, double seconds, const RankStats *stats, int ranks);

#endif
