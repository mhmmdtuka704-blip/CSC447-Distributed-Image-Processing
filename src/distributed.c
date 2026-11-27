#include "batch.h"

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { WORK = 1, INPUT_PATH, RESULT, STOP, STOPPED };

static void send_next(const Batch *batch, int worker, int *next, int *assigned) {
    if (*next == batch->count) {
        MPI_Send(NULL, 0, MPI_INT, worker, STOP, MPI_COMM_WORLD);
        *assigned = 0;
        return;
    }
    const char *input = batch->jobs[*next].input;
    int id = ++(*next);
    *assigned = id;
    MPI_Send(&id, 1, MPI_INT, worker, WORK, MPI_COMM_WORLD);
    MPI_Send(input, (int)strlen(input) + 1, MPI_CHAR, worker, INPUT_PATH, MPI_COMM_WORLD);
}

static double coordinate(Batch *batch, int size, int *assigned, RankStats *stats) {
    double start = MPI_Wtime();
    int next = 0;
    for (int worker = 1; worker < size; ++worker)
        send_next(batch, worker, &next, &assigned[worker]);

    for (int received = 0; received < batch->count; ++received) {
        int result[2];
        MPI_Status status;
        MPI_Recv(result, 2, MPI_INT, MPI_ANY_SOURCE, RESULT, MPI_COMM_WORLD, &status);
        int worker = status.MPI_SOURCE;
        int id = result[0];
        if (id < 1 || id > batch->count || id != assigned[worker] ||
            batch->jobs[id - 1].status != PENDING || result[1] < SAVED || result[1] > SAVE_FAILED) {
            fprintf(stderr, "Unexpected completion for job %d from rank %d\n", id, worker);
            MPI_Abort(MPI_COMM_WORLD, 3);
        }
        batch->jobs[id - 1].status = result[1];
        if (result[1] == SAVED)
            ++stats[worker].completed;
        else
            ++stats[worker].failed;

        /* After a result, that same rank can take another whole image immediately. */
        send_next(batch, worker, &next, &assigned[worker]);
    }

    for (int worker = 1; worker < size; ++worker)
        MPI_Recv(&stats[worker].busy_seconds, 1, MPI_DOUBLE, worker, STOPPED,
                 MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    return MPI_Wtime() - start;
}

static void work(const char *output, Recipe recipe) {
    double busy = 0;
    for (;;) {
        int id;
        MPI_Status status;
        MPI_Recv(&id, 1, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        if (status.MPI_TAG == STOP)
            break;
        char input[PATH_MAX];
        MPI_Recv(input, (int)sizeof input, MPI_CHAR, 0, INPUT_PATH, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        double start = MPI_Wtime();
        int result[2] = {id, process_image(input, output, id, recipe)};
        busy += MPI_Wtime() - start;
        MPI_Send(result, 2, MPI_INT, 0, RESULT, MPI_COMM_WORLD);
    }
    MPI_Send(&busy, 1, MPI_DOUBLE, 0, STOPPED, MPI_COMM_WORLD);
}

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    Batch batch = {0};
    RankStats *stats = NULL;
    int *assigned = NULL;
    int ready = 0;
    if (rank == 0) {
        if (size < 2) {
            fprintf(stderr, "Use at least two MPI ranks (one coordinator and one worker).\n");
        } else {
            stats = calloc((size_t)size, sizeof *stats);
            assigned = calloc((size_t)size, sizeof *assigned);
            if (!stats || !assigned)
                fprintf(stderr, "Could not allocate coordinator state.\n");
            else
                ready = prepare_batch(&batch, argc, argv);
        }
    }
    MPI_Bcast(&ready, 1, MPI_INT, 0, MPI_COMM_WORLD);
    int exit_code = 2;
    if (ready) {
        int recipe[] = {batch.recipe.width, batch.recipe.height, batch.recipe.brightness};
        MPI_Bcast(recipe, 3, MPI_INT, 0, MPI_COMM_WORLD);
        MPI_Bcast(batch.output, (int)sizeof batch.output, MPI_CHAR, 0, MPI_COMM_WORLD);
        MPI_Barrier(MPI_COMM_WORLD);
        if (rank == 0) {
            double elapsed = coordinate(&batch, size, assigned, stats);
            exit_code = report_batch(&batch, elapsed, stats, size);
        } else {
            work(batch.output, (Recipe){recipe[0], recipe[1], recipe[2]});
            exit_code = 0;
        }
    }
    free(stats);
    free(assigned);
    free_batch(&batch);
    MPI_Finalize();
    return exit_code;
}
