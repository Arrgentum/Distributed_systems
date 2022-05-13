#include <iostream>
#include <vector>
#include <mpi.h>


//Задание - реализовать MPI_Gather для тренспьютерной матрицы 4*4 (сбор даннх со всех процессов матрицы в точку (0*0))


/*
0  - 1  - 2  - 3
|    |    |    |
4  - 5  - 6  - 7
|    |    |    |
8  - 9  - 10 - 11
|    |    |    |
12 - 13 - 14 - 15
*/


/*
Компиляция - mpic++ first.cpp -o main
Запуск с k потоками - mpiexec -n k ./main
Запуск с максимальным количеством потоков - mpirun ./main
*/


//функция для каждого элемента транспьютерной матрицы получает какие-то данные (вкктор из 4 элементов степеней номера элемета матрицы)
void create_data(int rank, std::vector<int> &data)
{
    int elem = rank;
    for (int i = 0;  i < 4; i++, elem *= rank)
        data.push_back(elem);
}

//выводит в терминал ответ (сумму) и конечный вектор элементов
int print(std::vector<int> buffer)
{
    std::cout << "Вывод итогового вектора в элементе матрицы (0,0)" << std::endl;
    int sum = 0;
    for (auto i : buffer) {
        std::cout << i << "   ";
        sum += i;
    }
    std::cout << std::endl << "SUM = "<< sum << std::endl;
    return sum;
}

//функция ищет кому должен что-то передать элемент транспьютерной матрицы
int send_to(int i, std::vector<std::pair<int,int>> process_id, int num){
    if (i > num - 1){
        for (auto it : process_id)
            if (it.first == i - num)
                return it.second;
    }
    if (i != 0){
        for (auto it : process_id)
            if (it.first == i - 1)
                return it.second;
    }
    return -1;
}

//функция ищет отправителя для данного элемента матрицы
int resieve_from(int i, std::vector<std::pair<int,int>> process_id, bool flag, int num){
    if (flag && i < num * (num -1)){
        for (auto it : process_id)
            if (it.first == i + num)
                return it.second;
    }
    if (!flag && i < num - 1){
        for (auto it : process_id)
            if (it.first == i + 1)
                return it.second;
    }
    return -1;
}


//выполняет все действия
std::vector<int> experiment(int rank, std::vector<std::pair<int,int>> process_id, int num)
{
    MPI_Status status;
    std::vector<int> result, buffer;
    int vec_size = 0, tag = 1;
    for (auto i : process_id){
        if (i.second == rank){  //если номер потока этого элемента матрицы соответствует номеру данного потока - делаем
            int id;
            std::cout << " rank  = " << rank << " id matrix =  " << i.first << std::endl;
            create_data(i.first, result);  //создаем значения для отправки
            if (i.first < (num * (num -1)) && (id = resieve_from(i.first, process_id, true, num)) != -1){  //получение сообщения при вертикальной отправке, находим от какого процесса будем получать сообщение
                MPI_Recv(&vec_size, 1, MPI_INT, id, tag,  MPI_COMM_WORLD, &status); //ставим в режим ожидания элементы, которые должны что-то получить при вертикальной отправке, считываем длину буфера, который будем дальше считывать
                buffer.resize(vec_size);
                MPI_Recv(&buffer[0], vec_size, MPI_INT, id, tag, MPI_COMM_WORLD, &status);  // получаем сам буфер
                result.insert(result.end(), buffer.begin(), buffer.end());  
                //insert(result, buffer);
                buffer.clear();
            }
            if (i.first > (num -1) && (id = send_to(i.first, process_id, num)) != -1){ //отправка сообщения по вертикали, находим какому процессу отправляем сообщение
                vec_size = result.size();
                MPI_Send(&vec_size, 1, MPI_INT, id, tag, MPI_COMM_WORLD);  //отправляем длину буфера 
                MPI_Send(result.data(), vec_size, MPI_INT, id, tag ,MPI_COMM_WORLD); //сам буфер 
                result.clear();
            }
            if (i.first < (num -1) && (id = resieve_from(i.first, process_id, false, num)) != -1){ //получение значения по горизонтали в верхней стркоке матрицы
                MPI_Recv(&vec_size, 1, MPI_INT, id, tag,  MPI_COMM_WORLD, &status);
                buffer.resize(vec_size);
                MPI_Recv(&buffer[0], vec_size, MPI_INT, id, tag, MPI_COMM_WORLD, &status);
                result.insert(result.end(), buffer.begin(), buffer.end());
                //insert(result, buffer);
                buffer.clear();
            }
            if (i.first < num && i.first != 0 && (id = send_to(i.first, process_id, num)) != -1){ //отправка значения по горизонтали в верхней стркоке матрицы
                vec_size = result.size();
                MPI_Send(&vec_size, 1, MPI_INT, id, tag, MPI_COMM_WORLD);
                MPI_Send(result.data(), vec_size, MPI_INT, id, tag ,MPI_COMM_WORLD);
                result.clear();
            }
        }
    }
    return result;
}


int main(int argc, char** argv)
{
    int ierr = 0, num = 4; //квадратная матрица num * num
    if ( (ierr = MPI_Init(&argc, &argv) ) != 0)
        return ierr;
    double ftime, stime;
    std::vector<int> result;
    int rank, size;
    MPI_Comm_rank (MPI_COMM_WORLD, &rank);
    MPI_Comm_size (MPI_COMM_WORLD, &size);
    std::vector<std::pair<int, int>> process_id; //соответствие 1 число - номер элемента в матрице, 2 число - номер потока, который будет выполнять действия с этим номером матрицы
    for (int i = num*num -1; i >= 0; i--)
        process_id.push_back( {i, i % size} ); //реализовано сначало в каждом столбце передача элементов вверх, потом по горихонтали вправо к 0, поэтому элемменты тут должны находиться в порядке убывания, тогда при последовательном выполнении все будет корректно, никто не уснет
    stime = MPI_Wtime();
    result = experiment(rank, process_id, num);
    ftime = MPI_Wtime();
    if (rank == 0){
        print(result);
        std::cout <<"Разница времени = "<< ftime - stime << std::endl;
    }
    MPI_Finalize();
    return 0;
}
