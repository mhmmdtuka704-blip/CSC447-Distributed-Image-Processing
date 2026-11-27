#include "batch.h"

#include <time.h>

static double wall_seconds(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (double)now.tv_sec + (double)now.tv_nsec / 1e9;
}

int main(int argc, char **argv) {
    Batch batch = {0};
    if (!prepare_batch(&batch, argc, argv)) {
        free_batch(&batch);
        return 2;
    }

    double start = wall_seconds();
    for (int i = 0; i < batch.count; ++i)
        batch.jobs[i].status = process_image(batch.jobs[i].input, batch.output, i + 1, batch.recipe);
    double elapsed = wall_seconds() - start;

    RankStats stats = {0, 0, elapsed};
    for (int i = 0; i < batch.count; ++i) {
        if (batch.jobs[i].status == SAVED)
            ++stats.completed;
        else
            ++stats.failed;
    }
    int exit_code = report_batch(&batch, elapsed, &stats, 1);
    free_batch(&batch);
    return exit_code;
}
