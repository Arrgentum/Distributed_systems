#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <mpi-ext.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define SUICIDE_PROC 2
#define SUICIDE_I 1253
#define Master 0


//  Компиляция - /home/alexey/Desktop/study/Distributed_systems/systems/bin/mpicc second.c -o main
//  Запуск с k ядрами /home/alexey/Desktop/study/Distributed_systems/systems/bin/mpiexec -np k --with-ft ulfm  ./main 


MPI_Comm mpi_comm_world = MPI_COMM_WORLD;
bool error_occured = false;
char killed_filename[20];
char new_killed_filename[20] = "./logs/reserve.txt";
char filename[20];
int saved_iteration;
int rank, size;

double f(double x)
{
	return ( (2*x*x*x + x*x - 15*x -3) / (x*x*x - x + 2) );
}

void save_into_file(char* filename, double num)
{
    int fd = open(filename, O_CREAT | O_WRONLY | O_TRUNC, 0666);
    lseek(fd, 0, SEEK_SET); 
	write(fd, &num, sizeof(double));
    close(fd);
}

void read_from_file(char* filename, double *num)
{
    int fd = open(filename, O_RDONLY);
    read(fd, num, sizeof(double));
    close(fd);
}

static void verbose_errhandler (MPI_Comm *comm, int *err, ...) {
    error_occured = true;
	char errstr[MPI_MAX_ERROR_STRING];
    int num_failed, len, old_rank = rank;
    MPI_Group group_failed;
    MPI_Comm_size(mpi_comm_world, &size);
    MPIX_Comm_failure_ack(mpi_comm_world);
    MPIX_Comm_failure_get_acked(mpi_comm_world, &group_failed);
    MPI_Group_size(group_failed, &num_failed);
    MPI_Error_string(*err, errstr, &len);

    MPIX_Comm_shrink(mpi_comm_world, &mpi_comm_world);
    MPI_Comm_rank(mpi_comm_world, &rank);
    MPI_Comm_size(mpi_comm_world, &size);
    //printf("New process rank %d: %d\n", old_rank, rank);
    MPI_Barrier(mpi_comm_world);
    int *ranks = malloc(sizeof(int)*size);
    MPI_Gather(&old_rank, 1, MPI_INT, ranks, 1, MPI_INT, 0, mpi_comm_world);
    if (rank == Master) {
        int killed_proc_num;
        for (int i = 0; i < size - 1; ++i) {
            if (ranks[i + 1] - ranks[i] > 1) {
                killed_proc_num = ranks[i] + 1;
            }
     	}
        printf("Killed proc: %d\n", killed_proc_num);
		sprintf(killed_filename, "./logs/%d.txt", killed_proc_num);
		double num;
		read_from_file(killed_filename, &num);
        save_into_file(new_killed_filename, num);
    }
}

int main(int argc, char* argv[])
{
	int n = 10000000;
	double a = 0, b = 10, h = (b - a) / n;
	double s = 0, sum, num;
	double stime = 0, ftime = 0;
	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);
 	MPI_Errhandler errh;
    MPI_Comm_create_errhandler(verbose_errhandler, &errh);
    MPI_Comm_set_errhandler(MPI_COMM_WORLD, errh);
    stime = MPI_Wtime();
	MPI_Bcast(&n, 1, MPI_INT, 0, mpi_comm_world);
	sprintf(filename, "./logs/%d.txt", rank);
	MPI_Barrier(mpi_comm_world);
	int fd = open(filename, O_CREAT | O_RDWR | O_TRUNC, 0666);
	for (int i = rank + 1; i < n; i += size) {
		if ((rank == SUICIDE_PROC) && (i >= SUICIDE_I)) {
			close(fd);
			raise(SIGKILL);
		}
		s += f(a + i * h) * h;
		lseek(fd, 0, SEEK_SET); 
		write(fd, &s, sizeof(double));
	}
	close(fd);
	MPI_Barrier(mpi_comm_world);
	save_into_file(filename, s);
	read_from_file(filename, &num);
	MPI_Reduce(&num, &sum, 1, MPI_DOUBLE, MPI_SUM, 0, mpi_comm_world);
	MPI_Status status;
	if (rank == Master) {
		read_from_file(new_killed_filename, &num);
		int died_id = SUICIDE_I - (SUICIDE_I - SUICIDE_PROC - 1) % (size + 1) + (size + 1);
		for (int i = died_ids; i < n; i += size)
			num += f(a + i * h) * h;
		sum += num;
		ftime = MPI_Wtime();
		printf("Your integral = %lf \n", sum);
		printf("Yout time = %lf \n", ftime - stime);
	}
	MPI_Finalize();
	return 0;
}
