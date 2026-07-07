#ifndef ARM_MATH_H
#define ARM_MATH_H

#include <math.h>
#include <stdint.h>

#ifndef __STATIC_INLINE
#define __STATIC_INLINE static inline
#endif

typedef struct {
    uint16_t numTaps;
    float *pState;
    float *pCoeffs;
    float mu;
    uint32_t blockSize;
} arm_lms_norm_instance_f32;

__STATIC_INLINE void arm_lms_norm_init_f32(
    arm_lms_norm_instance_f32 *S,
    uint16_t numTaps,
    float *pCoeffs,
    float *pState,
    float mu,
    uint32_t blockSize) {
    S->numTaps = numTaps;
    S->pCoeffs = pCoeffs;
    S->pState = pState;
    S->mu = mu;
    S->blockSize = blockSize;

    for (uint16_t i = 0; i < numTaps; i++) {
        S->pCoeffs[i] = 0.0f;
    }
    for (uint32_t i = 0; i < numTaps; i++) {
        S->pState[i] = 0.0f;
    }
}

__STATIC_INLINE void arm_lms_norm_f32(
    const arm_lms_norm_instance_f32 *S,
    const float *pSrc,
    const float *pRef,
    float *pDst,
    float *pError,
    uint32_t blockSize) {
    (void)blockSize;
    for (uint32_t n = 0; n < S->blockSize; n++) {
        float x = pSrc[n];
        float d = pRef[n];

        for (uint16_t tap = S->numTaps - 1; tap > 0; tap--) {
            S->pState[tap] = S->pState[tap - 1];
        }
        S->pState[0] = x;

        float y = 0.0f;
        for (uint16_t tap = 0; tap < S->numTaps; tap++) {
            y += S->pCoeffs[tap] * S->pState[tap];
        }

        float err = d - y;
        float norm = 0.0f;
        for (uint16_t tap = 0; tap < S->numTaps; tap++) {
            float v = S->pState[tap];
            norm += v * v;
        }

        if (norm > 1e-6f) {
            float step = S->mu / norm;
            for (uint16_t tap = 0; tap < S->numTaps; tap++) {
                S->pCoeffs[tap] += step * err * S->pState[tap];
            }
        }

        pDst[n] = err;
        pError[n] = err;
    }
}

__STATIC_INLINE void arm_rms_f32(const float *pSrc, uint32_t blockSize, float *pResult) {
    float sumSq = 0.0f;
    for (uint32_t i = 0; i < blockSize; i++) {
        float v = pSrc[i];
        sumSq += v * v;
    }
    *pResult = sqrtf(sumSq / (float)blockSize);
}

__STATIC_INLINE void arm_mean_f32(const float *pSrc, uint32_t blockSize, float *pResult) {
    float sum = 0.0f;
    for (uint32_t i = 0; i < blockSize; i++) {
        sum += pSrc[i];
    }
    *pResult = sum / (float)blockSize;
}

__STATIC_INLINE void arm_var_f32(const float *pSrc, uint32_t blockSize, float *pResult) {
    float mean = 0.0f;
    for (uint32_t i = 0; i < blockSize; i++) {
        mean += pSrc[i];
    }
    mean /= (float)blockSize;

    float variance = 0.0f;
    for (uint32_t i = 0; i < blockSize; i++) {
        float diff = pSrc[i] - mean;
        variance += diff * diff;
    }
    *pResult = variance / (float)blockSize;
}

#endif
