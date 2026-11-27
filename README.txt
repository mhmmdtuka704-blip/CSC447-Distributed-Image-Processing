CSC447 Project 08 | PixelDispatch
Distributed Image Processing with Open MPI

Both C17 programs resize each image with the Studio's nearest-neighbor
coordinates, add a signed brightness offset with clipping, and apply a
3 x 3 box blur with clamped borders. Each job writes one lossless RGB PNG.
The sequential executable walks the job list. In MPI, rank 0 assigns jobs,
keeps one in flight per worker, and collects a result for each job.

BUILD (Linux or WSL2 Ubuntu, from this directory)

  sudo apt update
  sudo apt install build-essential openmpi-bin libopenmpi-dev libpng-dev python3-pil
  make -j

This builds build/sequential and build/distributed. Use Open MPI for both
mpicc and mpirun. Run as a regular Linux user. In a root-only local WSL
distribution, add --allow-run-as-root immediately after mpirun in the
commands below. The benchmark script does that automatically in that case.

REPRODUCIBLE DEMO (four deterministic images, no image downloads)

  demo=$(mktemp -d)
  python3 tools/prepare.py "$demo/input" --sizes 4
  ./build/sequential "$demo/input/batch-4.txt" "$demo/seq" 161 103 18
  mpirun --map-by slot --bind-to none -np 2 ./build/distributed "$demo/input/batch-4.txt" "$demo/mpi2" 161 103 18
  python3 tools/verify.py "$demo/seq" "$demo/mpi2" 4 161 103
  mpirun --map-by slot --bind-to none -np 8 ./build/distributed "$demo/input/batch-4.txt" "$demo/mpi8" 161 103 18
  python3 tools/verify.py "$demo/seq" "$demo/mpi8" 4 161 103

Each check should report four files, 66,332 RGB pixels, and zero
differences. The run with eight ranks has seven workers, at least three
of which have no jobs. Rank 0 always completes zero images in MPI.
Use -np 4 for a third configuration (three workers). Match the number
of ranks to your available slots.

  bash tools/smoke.sh

This runs the fixture at 2, 4, and 8 ranks and checks a missing input.

INPUTS AND CLI

  sequential  MANIFEST NEW_OUTPUT_DIR WIDTH HEIGHT BRIGHTNESS
  distributed MANIFEST NEW_OUTPUT_DIR WIDTH HEIGHT BRIGHTNESS

The manifest has one path per line. Relative paths are relative to the
manifest's directory. Blank lines are ignored. Job IDs follow manifest
order, beginning with 1. Files must be readable by every assigned rank;
all ranks must share the output path. On one WSL2 instance this is the
same filesystem. For multiple hosts, mount the shared files under the
same absolute paths on all hosts and use working Open MPI host setup.

Use RGB PNG files or normalize PNG/JPEG/WebP with tools/prepare.py. Each
image may be up to 10,000 pixels per axis and 10 million pixels total.
Output dimensions have the same limit. Brightness is -255 to +255.
The output directory must be new and its parent must exist. PNGs are
named job-00001.png, job-00002.png, etc. A failed image is reported by
ID after the run. The programs finish the remaining jobs and exit
nonzero if any job failed. Rerun in a fresh directory.

MAIN EXPERIMENT

Run from a Linux filesystem such as the WSL home directory so both the
inputs and temporary outputs use Linux I/O. The reported session used
WSL ext4. Supply at least 48 source images for the full matrix.

  python3 tools/prepare.py local-data/posters --source /path/to/poster-folder --mixed
  python3 tools/benchmark.py local-data/posters local-data/repeat

Preparation selects the first 48 image filenames in case-insensitive order,
converts to RGB, and sets long sides to 720, 1120, or 1550 pixels in
rotation while preserving proportions. The 4, 16, and 48 job manifests
use nested prefixes of that order. The run recipe is 960 x 640, +18, and
box blur. The harness measures the sequential executable and 2, 4, and
8 total MPI ranks (1, 3, and 7 workers), with one warmup and five measured
runs per case. It checks every output against the baseline after timing.

To run the whole matrix with generated images, omit --source in the first
command and choose a fresh destination. That recreates the procedure
with different image content. Measured poster inputs were prepared with
Pillow 11.3.0 and are identified by size and PNG SHA-256 in
results/inputs.csv. Different encoder versions can change PNG file hashes.

Use a new results directory. Runs and per-rank CSV files include run 0
(warmup). The summary uses only runs 1-5 by default. Time starts before
the first load in serial or first dispatch in MPI and ends after saves
and (for MPI) result and STOP acknowledgment collection. Process startup,
directory/manifest setup, the initial rank barrier, diagnostic printing,
MPI_Finalize, and external pixel checks are outside the measurement.
The save returns after the file is closed and renamed. It does not force
the operating system to flush pending writes to physical storage.

OPTIONAL CREATIVE SPACE STUDIO CHECK

Use the same manifest and settings with the local Studio, from Windows
while Studio is reachable on port 8765. Example (replace both paths):

  python tools/studio_reference.py path/to/batch-4.txt path/to/studio-output 161 103 18
  python tools/verify.py path/to/studio-output path/to/mpi-output 4 161 103

This sends the prepared images to the local Project 8 sequential reference
and saves only the returned PNGs. For Studio, the filter is called Denoise.
The Studio UI can validate uploaded outputs when named for its exported
kit. The scripts compare decoded bytes directly, so differences in PNG
metadata and compression do not affect the check.

REFERENCES

Dr Rachad Atat, CSC447 Fall 2026 Project Requirements, pp. 1, 4-5.
CSC447 Module 4 (ranks and execution) and Module 5 (point-to-point protocol).
LAU Libraries Creative Space Parallel Computing Studio, Project 8 and
SampleDataset, used as the recipe and independent validation reference.
Open MPI API: https://docs.open-mpi.org/en/v5.0.x/man-openmpi/man3/
libpng: https://www.libpng.org/pub/png/libpng.html
Pillow: https://pillow.readthedocs.io/
