#include "pseudo.h"

void matrix_multiply(double* A, int a_rows, int a_cols, double* B, int b_cols, double* C) {
    for (int i = 0; i < a_rows; i++) {
        for (int j = 0; j < b_cols; j++) {
            C[i * b_cols + j] = 0;
            for (int k = 0; k < a_cols; k++) {
                C[i * b_cols + j] += A[i * a_cols + k] * B[k * b_cols + j];
            }
        }
    }
}

void transpose_matrix(double* A, int rows, int cols, double* AT) {
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            AT[j * rows + i] = A[i * cols + j];
        }
    }
}

// Function to calculate SVD (dummy function for structure; should be replaced with actual SVD computation)
void svd(double* A, int m, int n, double* U, double* S, double* VT) {
    // Initialize U, S, VT with identity matrices for simplicity
    for (int i = 0; i < m * m; i++) {
        U[i] = (i % (m + 1)) ? 0 : 1;
    }
    for (int i = 0; i < n * n; i++) {
        VT[i] = (i % (n + 1)) ? 0 : 1;
    }
    for (int i = 0; i < n; i++) {
        S[i] = 1; // Dummy singular values
    }
}

void pseudo_inverse(double* A, int rows, int cols, double* pseudo_inv) {
    double U[MATRIX_ROWS * MATRIX_ROWS];
    double S[MATRIX_COLS];
    double VT[MATRIX_COLS * MATRIX_COLS];

    svd(A, rows, cols, U, S, VT);

    // Compute S inverse
    double S_inv[MATRIX_COLS * MATRIX_ROWS];
    for (int i = 0; i < cols; i++) {
        for (int j = 0; j < rows; j++) {
            if (i == j) {
                S_inv[i * rows + j] = (S[i] != 0) ? 1.0 / S[i] : 0;
            } else {
                S_inv[i * rows + j] = 0;
            }
        }
    }

    // Compute V * S_inv
    double V_S_inv[MATRIX_COLS * MATRIX_ROWS];
    matrix_multiply(VT, cols, cols, S_inv, rows, V_S_inv);

    // Compute pseudo-inverse A^+
    matrix_multiply(V_S_inv, cols, rows, U, rows, pseudo_inv);
}

float findSQRT(int number) {
    int start = 0, end = number;
    int mid;
    float ans;
    while (start <= end) {
        mid = (start + end) / 2;
        if (mid * mid == number) {
            ans = mid;
            break;
        }
        if (mid * mid < number) {
            ans = start;
            start = mid + 1;
        } else {
            end = mid - 1;
        }
    }
    float increment = 0.1;
    for (int i = 0; i < 5; i++) {
        while (ans * ans <= number) {
            ans += increment;
        }
        ans = ans - increment;
        increment = increment / 10;
    }
    return ans;
}
