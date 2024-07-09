#ifndef PSEUDO_H
#define PSEUDO_H

#define MATRIX_ROWS 30
#define MATRIX_COLS 40

void matrix_multiply(double* A, int a_rows, int a_cols, double* B, int b_cols, double* C);
void transpose_matrix(double* A, int rows, int cols, double* AT);
void svd(double* A, int m, int n, double* U, double* S, double* VT);
void pseudo_inverse(double* A, int rows, int cols, double* pseudo_inv);
float findSQRT(int number);

#endif // PSEUDO_H
