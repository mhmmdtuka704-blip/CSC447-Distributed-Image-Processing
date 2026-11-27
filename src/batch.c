#include "batch.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static int parse_integer(const char *text, int min, int max, int *result) {
    char *end;
    errno = 0;
    long value = strtol(text, &end, 10);
    if (errno || end == text || *end || value < min || value > max)
        return 0;
    *result = (int)value;
    return 1;
}

static int read_jobs(Batch *batch, const char *manifest) {
    char absolute[PATH_MAX];
    if (!realpath(manifest, absolute)) {
        perror(manifest);
        return 0;
    }
    FILE *list = fopen(absolute, "r");
    if (!list) {
        perror(absolute);
        return 0;
    }
    char *basename = strrchr(absolute, '/');
    basename[1] = '\0';
    char *line = NULL;
    size_t line_capacity = 0;
    int capacity = 0;
    ssize_t length;
    int ok = 1;
    while ((length = getline(&line, &line_capacity, list)) >= 0) {
        while (length > 0 && (line[length - 1] == '\n' || line[length - 1] == '\r'))
            line[--length] = '\0';
        if (!length)
            continue;
        char input[PATH_MAX];
        int written = snprintf(input, sizeof input, "%s%s", line[0] == '/' ? "" : absolute, line);
        if (memchr(line, '\0', (size_t)length) || written < 0 || written >= (int)sizeof input) {
            fprintf(stderr, "Invalid input path after job %d\n", batch->count);
            ok = 0;
            break;
        }
        if (batch->count == capacity) {
            if (capacity > INT_MAX / 2) {
                ok = 0;
                break;
            }
            capacity = capacity ? capacity * 2 : 16;
            Job *larger = realloc(batch->jobs, (size_t)capacity * sizeof *larger);
            if (!larger) {
                ok = 0;
                break;
            }
            batch->jobs = larger;
        }
        char *copy = strdup(input);
        if (!copy) {
            ok = 0;
            break;
        }
        batch->jobs[batch->count++] = (Job){copy, PENDING};
    }
    ok = ok && feof(list) && !ferror(list) && batch->count > 0;
    free(line);
    fclose(list);
    if (!ok)
        fprintf(stderr, "Could not read a nonempty job list from %s\n", manifest);
    return ok;
}

int prepare_batch(Batch *batch, int argc, char **argv) {
    Recipe *recipe = &batch->recipe;
    if (argc != 6 || !parse_integer(argv[3], 1, 10000, &recipe->width) ||
        !parse_integer(argv[4], 1, 10000, &recipe->height) ||
        !parse_integer(argv[5], -255, 255, &recipe->brightness) ||
        recipe->width * recipe->height > 10000000) {
        fprintf(stderr, "Usage: %s MANIFEST NEW_OUTPUT_DIR WIDTH HEIGHT BRIGHTNESS\n", argv[0]);
        return 0;
    }
    if (!read_jobs(batch, argv[1]))
        return 0;

    /* A fresh directory keeps previous outputs from hiding a failed job. */
    if (mkdir(argv[2], 0777) != 0) {
        perror(argv[2]);
        return 0;
    }
    if (!realpath(argv[2], batch->output) || strlen(batch->output) > sizeof batch->output - 32) {
        fprintf(stderr, "Could not resolve output directory or path is too long: %s\n", argv[2]);
        return 0;
    }
    return 1;
}

void free_batch(Batch *batch) {
    for (int i = 0; i < batch->count; ++i)
        free(batch->jobs[i].input);
    free(batch->jobs);
}

int report_batch(const Batch *batch, double seconds, const RankStats *stats, int ranks) {
    const char *messages[] = {"saved", "could not load PNG", "allocation failed", "could not save PNG"};
    int completed = 0;
    for (int i = 0; i < batch->count; ++i) {
        const Job *job = &batch->jobs[i];
        if (job->status == SAVED) {
            ++completed;
        } else {
            const char *message = job->status >= LOAD_FAILED && job->status <= SAVE_FAILED
                ? messages[job->status] : "no valid completion";
            fprintf(stderr, "Job %d (%s): %s\n", i + 1, job->input, message);
        }
    }
    printf("batch seconds=%.9f jobs=%d completed=%d ranks=%d\n", seconds, batch->count, completed, ranks);
    for (int r = 0; r < ranks; ++r)
        printf("rank=%d completed=%d failed=%d busy_seconds=%.9f\n", r, stats[r].completed,
               stats[r].failed, stats[r].busy_seconds);
    return completed != batch->count;
}
