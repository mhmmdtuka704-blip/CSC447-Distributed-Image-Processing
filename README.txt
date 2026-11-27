CSC447 Project 08 | PixelDispatch
Distributed Image Processing with Open MPI

We built PixelDispatch to see how much faster we could prepare a batch
of poster previews by giving whole images to different MPI workers.
Both C17 programs follow the Studio recipe. They resize each image with
nearest-neighbor sampling, adjust brightness and clip the result, then
apply a 3 x 3 box blur that repeats pixels at the edges. Each job saves
one lossless RGB PNG.

The sequential version works through the list in order. In the MPI
version, rank 0 gives each worker one image at a time. When a worker
returns a result, rank 0 can send it the next image.

Build on Linux or Ubuntu in WSL2

Run these commands from the project folder.

  sudo apt update
  sudo apt install build-essential openmpi-bin libopenmpi-dev libpng-dev python3-pil
  make -j

After the build, you'll find both programs in build/. Use Open MPI for
mpicc and mpirun. Run from your regular Linux account if possible. If
your local WSL setup runs as root, put --allow-run-as-root right after
mpirun in the demo commands. The benchmark script adds it automatically
for root runs.

Quick demo with four generated images

The first command makes a temporary directory. The preparation script
generates the same four test images each time, so you can run the demo
without downloading a dataset.

  demo=$(mktemp -d)
  python3 tools/prepare.py "$demo/input" --sizes 4
  ./build/sequential "$demo/input/batch-4.txt" "$demo/seq" 161 103 18
  mpirun --map-by slot --bind-to none -np 2 ./build/distributed "$demo/input/batch-4.txt" "$demo/mpi2" 161 103 18
  python3 tools/verify.py "$demo/seq" "$demo/mpi2" 4 161 103
  mpirun --map-by slot --bind-to none -np 8 ./build/distributed "$demo/input/batch-4.txt" "$demo/mpi8" 161 103 18
  python3 tools/verify.py "$demo/seq" "$demo/mpi8" 4 161 103

Both checks should report four files, 66,332 RGB pixels and zero
differences. With eight ranks, rank 0 coordinates and seven ranks can
process images, so at least three workers sit idle in this small demo.
Try -np 4 for three workers if your machine has enough slots.

You can also run all three rank counts and the missing-input test with
one command.

  bash tools/smoke.sh

Use your own images

  sequential  MANIFEST NEW_OUTPUT_DIR WIDTH HEIGHT BRIGHTNESS
  distributed MANIFEST NEW_OUTPUT_DIR WIDTH HEIGHT BRIGHTNESS

List one input path per line in the manifest. Relative paths start from
the folder containing the manifest. The program skips blank lines and
assigns job IDs in list order, starting at 1. In MPI, each worker needs
to read its inputs and write to the shared output folder. On a single
WSL2 instance, the ranks already share the same filesystem. If you use
multiple hosts, mount the shared folders at the same absolute paths on
each host and configure those hosts in Open MPI.

Use RGB PNGs as inputs, or use tools/prepare.py to convert PNG, JPEG or
WebP files to RGB PNG. Inputs and outputs can be up to 10,000 pixels
along either axis and 10 million pixels in total. The brightness offset
can range from -255 to +255.

Give each run a new output folder with an existing parent directory.
The program names the files job-00001.png, job-00002.png and so on. If a
job fails, the other jobs still finish. The program reports the failed
job ID and returns a nonzero exit code. Use a fresh folder for a retry.

Repeat the poster benchmark

We ran our benchmark on WSL's ext4 filesystem. To use the same setup,
run from a Linux folder such as your WSL home directory. Point the
first command at a folder with at least 48 source images.

  python3 tools/prepare.py local-data/posters --source /path/to/poster-folder --mixed
  python3 tools/benchmark.py local-data/posters local-data/repeat

The preparation script sorts the filenames without regard to case and
takes the first 48. It converts them to RGB and alternates the long side
among 720, 1120 and 1550 pixels while keeping each image's proportions.
The smaller batches use the first 4 and 16 images from the same list.
Every run uses 960 x 640 output, brightness +18 and a box blur.

The benchmark runs the sequential program and MPI at 2, 4 and 8 total
ranks. Those MPI runs have 1, 3 and 7 workers. For each case, it runs one
warmup and five timed trials, then checks each output against the
sequential baseline outside the timed interval.

You can use generated images for the same experiment by leaving out
--source and its path and choosing a new destination. We prepared the
measured poster inputs with Pillow 11.3.0. Their sizes and PNG SHA-256
hashes are in results/inputs.csv. PNG hashes can vary with the encoder
version.

Use a fresh directory for the benchmark results. The run and per-rank
CSVs include the warmup as run 0. The default summary uses runs 1-5.

We start the sequential timer just before loading the first image and
stop it after the last save. The MPI timer starts before the first
dispatch and ends after the saves, result messages and STOP
acknowledgments. Startup, manifest and directory setup, the initial
barrier, printing, MPI_Finalize and pixel checks all fall outside the
timed interval. The save includes closing and renaming the file. The
operating system can flush pending writes to disk later.

Compare with Creative Space Studio

While Studio is running on port 8765, open a Windows terminal and use
the same manifest and settings you used for MPI. Replace the example
paths with your own.

  python tools/studio_reference.py path/to/batch-4.txt path/to/studio-output 161 103 18
  python tools/verify.py path/to/studio-output path/to/mpi-output 4 161 103

The first command gets the expected PNGs from Studio's Project 8
sequential reference. In Studio, the box blur is called Denoise. The
second command compares the decoded RGB pixels, so differences in PNG
metadata or compression don't affect the check. You can also upload
your PNGs in Studio using the names in its exported kit.

References

Dr Rachad Atat, CSC447 Fall 2026 Project Requirements, pp. 1, 4-5.
CSC447 Module 4 (ranks and execution) and Module 5 (point-to-point protocol).
LAU Libraries Creative Space Parallel Computing Studio, Project 8 and
SampleDataset, used as the recipe and independent validation reference.
Open MPI C API documentation
  https://docs.open-mpi.org/en/v5.0.x/man-openmpi/man3/
libpng documentation
  https://www.libpng.org/pub/png/libpng.html
Pillow documentation
  https://pillow.readthedocs.io/
