/* ************************************************************************ */
/* Include standard header file.                                            */
/* ************************************************************************ */
#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <malloc.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <omp.h>

#define N 360

/* time measurement variables */
struct timeval start_time; /* time when program started */
struct timeval comp_time;  /* time when calculation completed */

size_t io_bytes;
uint64_t io_ops;
double io_time;

/* ************************************************************************ */
/*  allocate matrix of size N x N                                           */
/* ************************************************************************ */
static double **
alloc_matrix(void)
{
	double *data;
	double **matrix;
	int i;

	data = malloc(N * N * sizeof(double));
	matrix = malloc(N * sizeof(double *));

	if (data == NULL || matrix == NULL)
	{
		printf("Allocating matrix failed!\n");
		exit(EXIT_FAILURE);
	}

	for (i = 0; i < N; i++)
	{
		matrix[i] = data + (i * N);
	}

	return matrix;
}

/* ************************************************************************* */
/*  init matrix 																                             */
/* ************************************************************************* */
static void
init_matrix(double **matrix)
{
	int i, j;

	for (i = 0; i < N; i++)
	{
		for (j = 0; j < N; j++)
		{
			matrix[i][j] = 10 * j;
		}
	}
}

static size_t
parallel_write(int fd, const void *buf, size_t count, off_t offset)
{
	// size_t nb = 0;

	// printf("%d ", fd);

	// while (nb < count)
	// {
	// 	ssize_t ret;

	// 	ret = pwrite(fd, (char *)buf + nb, count - nb, offset + nb);

	// 	if (ret < 0)
	// 	{
	// 		printf("Error: cannot write to checkpoint\n");
	// 		exit(EXIT_FAILURE);
	// 	}

	// 	nb += ret;
	// }

	size_t nb = 0;
	size_t chunk_size = count / omp_get_num_threads(); // Calculate chunk size

#pragma omp parallel shared(nb)
	{
		size_t tid = omp_get_thread_num();
		size_t start = tid * chunk_size;
		size_t end = (tid == (unsigned int)omp_get_num_threads() - 1) ? count : start + chunk_size;

		size_t local_nb = 0;

		while (start < end)
		{
			ssize_t ret = pwrite(fd, (char *)buf + start, end - start, offset + start);

			if (ret < 0)
			{
				printf("Error: cannot write to checkpoint\n");
				exit(EXIT_FAILURE);
			}

			start += ret;
			local_nb += ret;
		}

#pragma omp atomic
		nb += local_nb;
	}

	return nb;
}

/* ************************************************************************ */
/*  caluclate                                                               */
/* ************************************************************************ */
static void calculate(double **matrix, int iterations, int threads)
{
	int i, j, k, l;
	int fd;
	size_t lnb = 0;
	uint64_t io_counter = 0;
	double iotime_counter = 0.0;

	// Explicitly disable dynamic teams
	omp_set_dynamic(0);
	omp_set_num_threads(threads);

	fd = open("matrix.out", O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

	if (fd == -1)
	{
		fprintf(stderr, "Could not open checkpoint ...\n");
		exit(EXIT_FAILURE);
	}

#pragma omp parallel private(k, l, i, j) \
	reduction(+ : io_counter, iotime_counter, lnb)
	{
		struct timeval io_start_time;
		struct timeval io_end_time;

		iotime_counter = 0.0;
		lnb = 0;
		io_counter = 0;

		for (k = 1; k <= iterations; k++)
		{
#pragma omp for
			// printf("Loop ");
			for (i = 0; i < N; i++)
			{
				for (j = 0; j < N; j++)
				{
					for (l = 1; l <= 4; l++)
					{
						matrix[i][j] = cos(matrix[i][j]) * sin(matrix[i][j]) * sqrt(matrix[i][j]) / tan(matrix[i][j]) / log(matrix[i][j]) * k * l;
					}
				}
			}

			gettimeofday(&io_start_time, NULL);

// matrix[0] works because the underlying buffer is contiguous (see alloc_matrix)
#pragma omp single
			lnb += parallel_write(fd, matrix[0], N * N * sizeof(double), 0);

			gettimeofday(&io_end_time, NULL);
			iotime_counter += (io_end_time.tv_sec - io_start_time.tv_sec) + (io_end_time.tv_usec - io_start_time.tv_usec) * 1e-6;

#pragma omp barrier
		}
	}

	io_ops = io_counter;
	io_time = iotime_counter / threads;
	io_bytes = lnb;

	printf("io_bytes: %lu\n", io_bytes);

	close(fd);
}

/* ************************************************************************ */
/*  displayStatistics: displays some statistics                             */
/* ************************************************************************ */
static void
displayStatistics(void)
{
	double time = (comp_time.tv_sec - start_time.tv_sec) + (comp_time.tv_usec - start_time.tv_usec) * 1e-6;

	printf("Runtime:    %fs\n", time - io_time);
	printf("I/O time:   %fs\n", io_time);
	printf("Throughput: %f MB/s\n", io_bytes / 1024 / 1024 / io_time);
	printf("IOPS:       %f Op/s\n", io_ops / io_time);
	printf("\n");
}

/* ************************************************************************ */
/*  main                                                                    */
/* ************************************************************************ */
int main(int argc, char **argv)
{
	int threads, iterations;
	double **matrix;

	if (argc < 3)
	{
		printf("Usage: %s threads iterations\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	else
	{
		sscanf(argv[1], "%d", &threads);
		sscanf(argv[2], "%d", &iterations);
	}

	matrix = alloc_matrix();
	init_matrix(matrix);

	gettimeofday(&start_time, NULL);
	calculate(matrix, iterations, threads);
	gettimeofday(&comp_time, NULL);

	displayStatistics();

	return 0;
}
