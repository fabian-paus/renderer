#pragma once

#include <immintrin.h>

#define FP_PI 3.14159265358979323846264338327950288419716939937510

__m256 mm256_sincos_ps(__m256 x, __m256* c) {
    __m256 sign_bit_sin = x;
    /* take the absolute value */
    
    __m256 signMask = _mm256_castsi256_ps(_mm256_set1_epi32(0x80000000));
    __m256 invSignMask = _mm256_castsi256_ps(_mm256_set1_epi32((int)~0x80000000));
    x = _mm256_and_ps(x, invSignMask);
    /* extract the sign bit (upper one) */
    sign_bit_sin = _mm256_and_ps(sign_bit_sin, signMask);

    /* scale by 4/Pi */
    __m256 fourDivPi = _mm256_set1_ps(1.27323954473516f);
    __m256 y = _mm256_mul_ps(x, fourDivPi);
 
    /* store the integer part of y in imm2 */
    __m256i imm2 = _mm256_cvttps_epi32(y);

    /* j=(j+1) & (~1) (see the cephes sources) */
    imm2 = _mm256_add_epi32(imm2, _mm256_set1_epi32(1));
    imm2 = _mm256_and_si256(imm2, _mm256_set1_epi32(~1));

    y = _mm256_cvtepi32_ps(imm2);
    __m256i imm4 = imm2;

    /* get the swap sign flag for the sine */
    __m256i imm0 = _mm256_and_si256(imm2, _mm256_set1_epi32(4));
    imm0 = _mm256_slli_epi32(imm0, 29);
    //__m256 swap_sign_bit_sin = _mm256_castsi256_ps(imm0);

    /* get the polynom selection mask for the sine*/
    imm2 = _mm256_and_si256(imm2, _mm256_set1_epi32(2));
    imm2 = _mm256_cmpeq_epi32(imm2, _mm256_set1_epi32(0));
    //__m256 poly_mask = _mm256_castsi256_ps(imm2);


    __m256 swap_sign_bit_sin = _mm256_castsi256_ps(imm0);
    __m256 poly_mask = _mm256_castsi256_ps(imm2);

    /* The magic pass: "Extended precision modular arithmetic"
       x = ((x - y * DP1) - y * DP2) - y * DP3; */
    __m256 xmm1 = _mm256_set1_ps(-0.78515625f); // DP1
    __m256 xmm2 = _mm256_set1_ps(-2.4187564849853515625e-4); // DP2
    __m256 xmm3 = _mm256_set1_ps(-3.77489497744594108e-8); // DP3
    xmm1 = _mm256_mul_ps(y, xmm1);
    xmm2 = _mm256_mul_ps(y, xmm2);
    xmm3 = _mm256_mul_ps(y, xmm3);
    x = _mm256_add_ps(x, xmm1);
    x = _mm256_add_ps(x, xmm2);
    x = _mm256_add_ps(x, xmm3);

    imm4 = _mm256_sub_epi32(imm4, _mm256_set1_epi32(2));
    imm4 = _mm256_andnot_si256(imm4, _mm256_set1_epi32(4));
    imm4 = _mm256_slli_epi32(imm4, 29);

    __m256 sign_bit_cos = _mm256_castsi256_ps(imm4);

    sign_bit_sin = _mm256_xor_ps(sign_bit_sin, swap_sign_bit_sin);

    /* Evaluate the first polynom  (0 <= x <= Pi/4) */
    __m256 z = _mm256_mul_ps(x, x);
    y = _mm256_set1_ps(2.443315711809948E-005f); // coscof_p0

    y = _mm256_mul_ps(y, z);
    y = _mm256_add_ps(y, _mm256_set1_ps(-1.388731625493765E-003f)); // coscof_p1
    y = _mm256_mul_ps(y, z);
    y = _mm256_add_ps(y, _mm256_set1_ps(4.166664568298827E-002f)); // coscof_p2
    y = _mm256_mul_ps(y, z);
    y = _mm256_mul_ps(y, z);
    __m256 tmp = _mm256_mul_ps(z, _mm256_set1_ps(0.5f));
    y = _mm256_sub_ps(y, tmp);
    y = _mm256_add_ps(y, _mm256_set1_ps(1));

    /* Evaluate the second polynom  (Pi/4 <= x <= 0) */

    __m256 y2 = _mm256_set1_ps(-1.9515295891E-4f); // sincof_p0
    y2 = _mm256_mul_ps(y2, z);
    y2 = _mm256_add_ps(y2, _mm256_set1_ps(8.3321608736E-3f)); // sincof_p1
    y2 = _mm256_mul_ps(y2, z);
    y2 = _mm256_add_ps(y2, _mm256_set1_ps(-1.6666654611E-1f)); // sincof_p2
    y2 = _mm256_mul_ps(y2, z);
    y2 = _mm256_mul_ps(y2, x);
    y2 = _mm256_add_ps(y2, x);

    /* select the correct result from the two polynoms */
    xmm3 = poly_mask;
    __m256 ysin2 = _mm256_and_ps(xmm3, y2);
    __m256 ysin1 = _mm256_andnot_ps(xmm3, y);
    y2 = _mm256_sub_ps(y2, ysin2);
    y = _mm256_sub_ps(y, ysin1);

    xmm1 = _mm256_add_ps(ysin1, ysin2);
    xmm2 = _mm256_add_ps(y, y2);

    /* update the sign */
    *c = _mm256_xor_ps(xmm2, sign_bit_cos);
    return _mm256_xor_ps(xmm1, sign_bit_sin);
}

float sin(float x) {
    __m256 x4 = _mm256_set1_ps(x);
    __m256 ignore;
    __m256 sin4 = mm256_sincos_ps(x4, &ignore);
    return *(float*)&sin4;
}