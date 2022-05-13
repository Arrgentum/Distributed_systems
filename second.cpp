//
// Created by alexey on 23.12.2021.
//

#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <mpi-ext.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define SUICIDE_PROC 4
#define SUICIDE_I 123456
#define COORDINATOR_NUM 0


//   /home/alexey/Desktop/study/Distributed_systems/systems/bin/mpicc second.cpp -o main
//   /home/alexey/Desktop/study/Distributed_systems/systems/bin/mpiexec -np 6 --with-ft ulfm  ./main 



MPI_Comm mpi_comm_world;
bool error_occured = false;
char killed_filename[20];
char new_killed_filename[20] = "./logs/reserve.txt";
char filename[20];
int saved_iteration;
int rank, size;
double a = 0, b = 100;
int n = 10000000;
    

double f(double x)
{
    return x;
    return ( (2*x*x*x + x*x - 15*x -3) / (x*x*x - x + 2) );
}

void save_into_file(char* filename, long double num)
{
    int fd = open(filename, O_CREAT | O_WRONLY | O_TRUNC, 0666);
    lseek(fd, 0, SEEK_SET); 
    write(fd, &num, sizeof(long double));
    close(fd);
}

void read_from_file(char* filename, long double *num)
{
    int fd = open(filename, O_RDONLY);
    read(fd, num, sizeof(long double));
    close(fd);
}



void calc(double x[3])
{
    int fd = open(filename, O_CREAT | O_RDWR | O_TRUNC, 0666);
    double sum = 0;
    int n = (x[1]-x[0])/ (h*rank);
    double a1 = ((double)(size - rank) * x[0] + (double)(rank - 1)*x[1])/(double)(size - 1);
    double b1 = ((double)(size - rank - 1) * x[0] + (double)(rank)*x[0])/(double)(size - 1);
    for(int i = 0; i < n; i++){
        sum += f(a1 + i * x[2]) * x[2];
        lseek(fd, 0, SEEK_SET); 
        write(fd, &s, sizeof(double));
    }
    close(fd);
    printf(" rank = %d, sum = %lf \n", rank, s);
    MPI_Barrier(mpi_comm_world);
}


static void verbose_errhandler (MPI_Comm *comm, int *err, ...) {
    error_occured = true;
    char errstr[MPI_MAX_ERROR_STRING];
    int num_failed, len;
    MPI_Group group_failed;
    int old_rank = rank;
    MPI_Comm_size(mpi_comm_world, &size);
    MPIX_Comm_failure_ack(mpi_comm_world);
    MPIX_Comm_failure_get_acked(mpi_comm_world, &group_failed);
    MPI_Group_size(group_failed, &num_failed);
    MPI_Error_string(*err, errstr, &len);

    MPIX_Comm_shrink(mpi_comm_world, &mpi_comm_world);
    MPI_Comm_rank(mpi_comm_world, &rank);
    MPI_Comm_size(mpi_comm_world, &size);
    printf("New process rank %d: %d\n", old_rank, rank);
    MPI_Barrier(mpi_comm_world);
    int *ranks = malloc(sizeof(int) * size);
    MPI_Gather(&old_rank, 1, MPI_INT, ranks, 1, MPI_INT, 0, mpi_comm_world);
    if (rank == COORDINATOR_NUM) {
        int killed_proc_num;
        for (int i = 0; i < size - 1; ++i) {
            if (ranks[i + 1] - ranks[i] > 1) {
                killed_proc_num = ranks[i] + 1;
            }
        }
        printf("Killed proc: %d\n", killed_proc_num);
        sprintf(killed_filename, "./logs/%d.txt", killed_proc_num);
        long double num;
        read_from_file(killed_filename, &num);
        save_into_file(new_killed_filename, num);
    }
    double a1 = ((double)(size + 1 - killed_proc_num) * a + (double)(killed_proc_num - 1) * b)/(double)(size );
    double b1 = ((double)(size + 1 - killed_proc_num - 1) * a + (double)(killed_proc_num) * b)/(double)(size );
    double h = (b1 - a1) / (n/size);
    x[3] = {a1,b1,h};
    calc(x);
}

int main(int argc, char* argv[])
{
    int rc;
    double s = 0, sum;
    double h = (b - a) / n;
    double x[3] = {a,b,h};
    if (rc = MPI_Init(&argc, &argv)) {
        printf("Error of execution \n");
        MPI_Abort(MPI_COMM_WORLD, rc);
    }

    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    mpi_comm_world = MPI_COMM_WORLD;

    MPI_Errhandler errh;
    MPI_Comm_create_errhandler(verbose_errhandler, &errh);
    MPI_Comm_set_errhandler(MPI_COMM_WORLD, errh);
    MPI_Barrier(mpi_comm_world);

    if (rank == 0) {
        s = (f(a) + f(b)) / 2;
    }

    MPI_Bcast(&n, 1, MPI_INT, 0, mpi_comm_world);

    sprintf(filename, "./logs/%d.txt", rank);
    save_into_file(filename, s);

    s = 0;
    printf(" %lf  %lf  %d  %lf  %d\n" ,a,b,n,h,size);
    MPI_Barrier(mpi_comm_world);

    calc(x);
    int fd = open(filename, O_CREAT | O_RDWR | O_TRUNC, 0666);
    for (int i = rank+1; i < n; i += size) {
        if ((rank == SUICIDE_PROC) && (i >= SUICIDE_I)) {
            close(fd);
            raise(SIGKILL);
        }
        s += f(a + i * h) * h;
        lseek(fd, 0, SEEK_SET); 
        write(fd, &s, sizeof(double));
    }
    close(fd);
    printf(" rank = %d, sum = %lf \n", rank, s);
    MPI_Barrier(mpi_comm_world);
    save_into_file(filename, s);
    long double num;
    read_from_file(filename, &num);
    
    MPI_Reduce(&num, &sum, 1, MPI_LONG_DOUBLE, MPI_SUM, 0, mpi_comm_world);

    if (rank == 0) {
        long double num;
        read_from_file(new_killed_filename, &num);

        int died_it = SUICIDE_I - (SUICIDE_I - SUICIDE_PROC - 1) % (size + 1) + (size + 1);
        long double tmp_s = num;
        for (int i = died_it; i < n; i += size + 1)
            tmp_s += f(a + i * h);
        double I = h * (sum + tmp_s);
        printf("Your integral is: %lf \n", I);
    }
    MPI_Finalize();
    return 0;
}
