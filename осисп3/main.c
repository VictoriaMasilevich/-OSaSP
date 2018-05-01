/*Написать программу нахождения массива значений функции y[i]=sin(2*PI*i/N) (i=0,1,2…N-1) с использованием ряда Тейлора. Пользователь задаёт значения N и количество n членов ряда Тейлора. Для расчета одного члена ряда Тейлора запускается отдельный процесс. Каждый процесс выводит на экран и в файл промежуточных результатов
(создать в каталоге /tmp) свой pid, i и рассчитанное значение члена ряда. Головной процесс считывает из файла промежуточных результатов значения всех рассчитанных членов ряда Тейлора для каждого i, суммирует их и полученное значение y[i] записывает в файл результата в виде: y[i] =значение. Проверить работу программы для N=1024 n=10*/

#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include <memory.h>
#include <errno.h>
#include <math.h>
#include <unistd.h>
#include <sys/wait.h>
#include <limits.h>

#define ARGS_COUNT 4

char *module_name;

void print_error(const char *module_name, const char *error_msg, const char *file_name) {
    fprintf(stderr, "%s: %s %s\n", module_name, error_msg, file_name ? file_name : "");
}

double get_sin_taylor_member(double x, int member_number) {
    double result = 1;
    for (int i = 1; i <= member_number * 2 + 1; i++) {
        result *= x / i;
    }
    return (member_number % 2) ? -result : result;
}

int write_result(const int array_size, FILE *tmp_file, FILE *result_file, const char* result_path) {
    double *result = alloca(sizeof(double) * array_size);
    memset(result, 0, sizeof(double) * array_size); //заполняем нулями
    char* full_result_path = alloca(PATH_MAX);
    realpath(result_path, full_result_path);

    int pid, i;
    double member_value;
    rewind(tmp_file); //указатель на начало
    errno = 0;
    while (!feof(tmp_file)) {
        if (fscanf(tmp_file, "%d %d %lf", &pid, &i, &member_value) == EOF && (errno != 0)){
            print_error(module_name, "Error while reading results from temp file:", strerror(errno));
            return 1;
        }
        result[i] += member_value;
    }
	result[array_size-1] -= member_value;

    for (i = 0; i < array_size; i++) {
        if (fprintf(result_file, "y[%d]=%.8lf\n", i, result[i]) == -1){
            print_error(module_name, strerror(errno), full_result_path);
            return 1;
        };
    }

    if (fclose(result_file) == -1){
        print_error(module_name, strerror(errno), full_result_path);
        return 1;
    }
    if (fclose(tmp_file) == -1){
        print_error(module_name, "Error closing temp file: ", strerror(errno));
        return 1;
    }
    return 0;
}

int count_function_values(const int array_size, const int taylor_members_count, const char *result_path) {
    FILE *result_file, *tmp_file;
    if (!(result_file = fopen(result_path, "w+"))) {
        print_error(module_name, strerror(errno), result_path);
        return 1;
    }

    if (!(tmp_file = tmpfile())) {
        print_error(module_name, strerror(errno), NULL);
        fclose(result_file);
        return 1;
    }

    pid_t pid;
    int running_processes = 0;
    for (int i = 0; i < array_size; i++) {
        double x = (2 * M_PI * i) / array_size;
        if (x != 0){
            x = M_PI - x;
        }
        for (int j = 0; j < taylor_members_count; j++) {
            if (running_processes == taylor_members_count){
                wait(NULL);
                running_processes--;
            }
            pid = fork();

            if (pid == 0) {
                double member = get_sin_taylor_member(x, j);
                if (fprintf(tmp_file, "%d %d %.8lf\n", getpid(), i, member) == -1){
                    print_error(module_name, "Error writing result to temp file", NULL);
                    return -1;
                };
                printf("%d %d %lf\n", getpid(), i, member);
                return 0;
            } else if (pid == -1) {
                print_error(module_name, strerror(errno), NULL);
                fclose(result_file);
                fclose(tmp_file);
                return 1;
            }
            running_processes++;
        }
    }
 /* дождаться окончания всех потомков */
    while (wait(NULL) > 0){};

    return write_result(array_size, tmp_file, result_file, result_path);
}

int main(int argc, char *argv[]) {
    module_name = basename(argv[0]);
    int array_size, taylor_members_count; //N and n

    if (argc != ARGS_COUNT) {
        print_error(module_name, "Wrong number of parameters.", NULL);
        return 1;
    }

    if ((array_size = atoi(argv[1])) < 1) {
        print_error(module_name, "Array size must be positive.", NULL);
        return 1;
    };
    if ((taylor_members_count = atoi(argv[2])) < 1) {
        print_error(module_name, "Number of taylor members must be positive.", NULL);
        return 1;
    };

    return count_function_values(array_size, taylor_members_count, argv[3]);
}