#ifndef QUALITY_H_
#define QUALITY_H_

#define _USE_MATH_DEFINES // 使用cmath中的一些常量
#define MAX_HOP 8
// 自然常数 e  2.71828182845904523536 #define M_E   2.7182818284590452354

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

typedef uint8_t u_int8_t;
typedef uint16_t u_int16_t;

//extern double base;

// 双精度浮点返回类型
double bluetooth_per_approximate_m(double E_SNR, int w, int n);
u_int16_t bluetooth_prr_m(int m, int N, long double E_SNR, int w, int n);
u_int8_t blue_quality(u_int16_t product_term, int hop);
u_int16_t bluetooth_prr_m_1(u_int16_t packet_product_term, u_int16_t neighbor_product_term);
//void Kalman(double *SNRhat, double SNR_Z, double *P, double Q = 0.01, double R = 1);
void Kalman(double *SNRhat, double SNR_Z, double *P, double Q , double R);
double calculate_ber(double SNR);
double calculate_per(double SNR);
double calculate_prr_mac(int w, int n);
uint16_t link_quality(u_int8_t hop, u_int16_t product_term);
double calculate_snr(double signal_power, double noise_power);

#endif