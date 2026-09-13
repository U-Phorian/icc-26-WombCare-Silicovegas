/* Minimal host stand-in for CMSIS-DSP, so the REAL wombcare_dsp.c can be
 * compiled and exercised on a PC. Only the routines wombcare_dsp.c calls
 * are provided, with straightforward reference implementations. */
#ifndef TEST_ARM_MATH_H
#define TEST_ARM_MATH_H

#include <stdint.h>
#include <string.h>
#include <math.h>

#define PI 3.14159265358979f
typedef float float32_t;

typedef struct {
    uint16_t numTaps;
    float   *pState;
    const float *pCoeffs;
} arm_fir_instance_f32;

typedef struct {
    uint16_t numTaps;
    float   *pState;
    float   *pCoeffs;
    float    mu;
    float    energy;
    float    x0;
} arm_lms_norm_instance_f32;

static inline void arm_fir_init_f32(arm_fir_instance_f32 *S, uint16_t numTaps,
                                    const float *pCoeffs, float *pState,
                                    uint32_t blockSize)
{
    S->numTaps = numTaps;
    S->pCoeffs = pCoeffs;
    S->pState  = pState;
    memset(pState, 0, sizeof(float) * (numTaps + blockSize - 1));
}

/* Direct-form FIR with zero initial history, matching a freshly
 * initialised CMSIS instance. */
static inline void arm_fir_f32(const arm_fir_instance_f32 *S, const float *pSrc,
                               float *pDst, uint32_t blockSize)
{
    const uint16_t n = S->numTaps;
    for (uint32_t i = 0; i < blockSize; i++) {
        float acc = 0.0f;
        for (uint16_t k = 0; k < n; k++) {
            long idx = (long)i - (long)k;
            if (idx >= 0) acc += S->pCoeffs[k] * pSrc[idx];
        }
        pDst[i] = acc;
    }
}

static inline void arm_lms_norm_init_f32(arm_lms_norm_instance_f32 *S,
                                         uint16_t numTaps, float *pCoeffs,
                                         float *pState, float mu,
                                         uint32_t blockSize)
{
    S->numTaps = numTaps; S->pCoeffs = pCoeffs; S->pState = pState;
    S->mu = mu; S->energy = 0.0f; S->x0 = 0.0f;
    memset(pCoeffs, 0, sizeof(float) * numTaps);
    memset(pState, 0, sizeof(float) * (numTaps + blockSize - 1));
}

/* Normalised LMS. pOut = filter output (maternal estimate),
 * pErr = pRef - pOut (the fetal residual). */
static inline void arm_lms_norm_f32(arm_lms_norm_instance_f32 *S,
                                    const float *pSrc, const float *pRef,
                                    float *pOut, float *pErr, uint32_t blockSize)
{
    const uint16_t n = S->numTaps;
    for (uint32_t i = 0; i < blockSize; i++) {
        float y = 0.0f, energy = 0.0f;
        for (uint16_t k = 0; k < n; k++) {
            long idx = (long)i - (long)k;
            float x = (idx >= 0) ? pSrc[idx] : 0.0f;
            y += S->pCoeffs[k] * x;
            energy += x * x;
        }
        float e = pRef[i] - y;
        pOut[i] = y;
        pErr[i] = e;
        float norm = S->mu / (energy + 1e-6f);
        for (uint16_t k = 0; k < n; k++) {
            long idx = (long)i - (long)k;
            float x = (idx >= 0) ? pSrc[idx] : 0.0f;
            S->pCoeffs[k] += norm * e * x;
        }
    }
}

static inline void arm_mean_f32(const float *p, uint32_t n, float *out)
{
    double s = 0.0;
    for (uint32_t i = 0; i < n; i++) s += p[i];
    *out = (float)(s / (double)n);
}

static inline void arm_std_f32(const float *p, uint32_t n, float *out)
{
    if (n < 2) { *out = 0.0f; return; }
    double m = 0.0;
    for (uint32_t i = 0; i < n; i++) m += p[i];
    m /= (double)n;
    double v = 0.0;
    for (uint32_t i = 0; i < n; i++) { double d = p[i] - m; v += d * d; }
    *out = (float)sqrt(v / (double)(n - 1));
}

static inline void arm_var_f32(const float *p, uint32_t n, float *out)
{
    if (n < 2) { *out = 0.0f; return; }
    double m = 0.0;
    for (uint32_t i = 0; i < n; i++) m += p[i];
    m /= (double)n;
    double v = 0.0;
    for (uint32_t i = 0; i < n; i++) { double d = p[i] - m; v += d * d; }
    *out = (float)(v / (double)(n - 1));
}

static inline void arm_rms_f32(const float *p, uint32_t n, float *out)
{
    double s = 0.0;
    for (uint32_t i = 0; i < n; i++) s += (double)p[i] * p[i];
    *out = (float)sqrt(s / (double)n);
}

static inline void arm_mult_f32(const float *a, const float *b, float *dst,
                                uint32_t n)
{
    for (uint32_t i = 0; i < n; i++) dst[i] = a[i] * b[i];
}

#endif
