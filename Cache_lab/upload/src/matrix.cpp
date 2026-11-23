#include <iostream>   
#include <time.h>
#include <string.h>
#include <math.h>
using namespace std;   

#define MATRIX_SIZE 1024

int main() {
    clock_t start, finish;
    clock_t start_new, finish_new;

    register int i, j, k;

    int (*a)[MATRIX_SIZE], (*b)[MATRIX_SIZE];
    a = new int[MATRIX_SIZE][MATRIX_SIZE];
    b = new int[MATRIX_SIZE][MATRIX_SIZE];


    for(i = 0; i < MATRIX_SIZE; i ++) {
        for(j = 0; j < MATRIX_SIZE; j ++) {
            a[i][j] = i % (j + 1);
            b[i][j] = i / (j + 1);
        }
    }

    int (*c)[MATRIX_SIZE], (*d)[MATRIX_SIZE];
    c = new int[MATRIX_SIZE][MATRIX_SIZE];
    d = new int[MATRIX_SIZE][MATRIX_SIZE];

    // initialize
    memset(d, 0, MATRIX_SIZE * MATRIX_SIZE * sizeof(int));

    start_new = clock();

    // TODO: Your optimized code:
    //======================================================
    const int block_size = 32;

    // ii、jj 确定当前计算的 D 子块。
    // kk 沿乘法的求和方向移动，将各对子块的乘积累加到 D 子块中。
    for (int ii = 0; ii < MATRIX_SIZE; ii += block_size) {
        for (int jj = 0; jj < MATRIX_SIZE; jj += block_size) {
            for (int kk = 0; kk < MATRIX_SIZE; kk += block_size) {

                // 计算当前子块，块内使用 i → k → j 的循环顺序。
                for (i = ii; i < ii + block_size; ++i) {
                    int* d_row = d[i];

                    for (k = kk; k < kk + block_size; ++k) {
                        // 固定 A 的一个元素，连续读取 B 并更新 D。
                        int a_ik = a[i][k];
                        int* b_row = b[k];

                        for (j = jj; j < jj + block_size; ++j) {
                            d_row[j] += a_ik * b_row[j];
                        }
                    }
                }
            }
        }
    }
    // Stop here.
    //======================================================

    finish_new = clock();

    int (*a_)[MATRIX_SIZE], (*b_)[MATRIX_SIZE];
    a_ = new int[MATRIX_SIZE][MATRIX_SIZE];
    b_ = new int[MATRIX_SIZE][MATRIX_SIZE];


    for (i = 0; i < MATRIX_SIZE; i ++) {
        for (j = 0; j < MATRIX_SIZE; j ++) {
            a_[i][j] = i % (j + 1);
            b_[i][j] = i / (j + 1);
        }
    }

    // initialize
    memset(c, 0, MATRIX_SIZE * MATRIX_SIZE * sizeof(int));

    start = clock();
    for (i = 0; i < MATRIX_SIZE; i ++)
        for (j = 0; j < MATRIX_SIZE; j ++)
            for (k = 0; k < MATRIX_SIZE; k ++)
                c[i][j] += a_[i][k] * b_[k][j];
    finish = clock();


    // compare
    for(i = 0; i < MATRIX_SIZE; i++) {
        for(j = 0; j < MATRIX_SIZE; j++) {
            if (c[i][j] != d[i][j]) {
                cout << "you have got an error in algorithm modification!" << endl;
                exit(1);
            }
        }
    }

    cout << "time spent for original method : " << (double)(finish - start) / CLOCKS_PER_SEC << " s" << endl;
    cout << "time spent for new method : " << (double)(finish_new - start_new) / CLOCKS_PER_SEC << " s" << endl;
    cout << "time ratio of performance optimization : " << (double)(finish - start) / (finish_new - start_new) << endl;
    return 0;
}