#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "hdc_1024bit.h"

/* ================= CONFIG ================= */
#define TAG "HDC"
#define SCALE ((NUM_LEVELS - 1) / 6.0f)

static const int sizeBytes = (D + 7) / 8;

const float mu[NUM_FEATURES] = {
    225.57654627f,
    1.29884934f,
    266.68591358f,
    86.85936184f,
    292.1524991f,
    104.82009801
};
const float sigma[NUM_FEATURES] = {
    2.14914949f,
    0.81581784f,
    184.29717067f,
    13.86885008f,
    182.57227239f,
    51.1042152
};
/* =====================================================
   BIT OPS – MSB-first (GIỮ NGUYÊN NHƯ CODE GỐC)
   ===================================================== */

static inline bool getBit(const uint8_t *array, int index)
{
    int byteIdx = index / 8;
    int bitIdx  = 7 - (index % 8);
    return (array[byteIdx] >> bitIdx) & 1;
}

static inline void setBit(uint8_t *array, int index, bool value)
{
    int byteIdx = index / 8;
    int bitIdx  = 7 - (index % 8);
    if (value)
        array[byteIdx] |=  (1 << bitIdx);
    else
        array[byteIdx] &= ~(1 << bitIdx);
}

/* =====================================================
   HDC OPS
   ===================================================== */

// XOR
void bindHV(uint8_t *out, const uint8_t *a, const uint8_t *b)
{
    for (int i = 0; i < sizeBytes; i++)
        out[i] = a[i] ^ b[i];
}

// Roll (circular shift)
void rollHV(uint8_t *hv, int shift)
{
    if (shift == 0) return;

    shift %= D;
    if (shift < 0) shift += D;

    uint8_t temp[sizeBytes];
    memcpy(temp, hv, sizeBytes);

    int byteShift = shift / 8;
    int bitShift  = shift % 8;

    for (int i = 0; i < sizeBytes; i++) {
        int src1 = (i - byteShift + sizeBytes) % sizeBytes;
        int src2 = (src1 - 1 + sizeBytes) % sizeBytes;

        hv[i] = (uint8_t)(
            (temp[src1] >> bitShift) |
            (temp[src2] << (8 - bitShift))
        );
    }

    // clear tail bits
    int tailBits = sizeBytes * 8 - D;
    if (tailBits > 0) {
        uint8_t mask = 0xFF << tailBits;
        hv[sizeBytes - 1] &= mask;
    }
}

// Majority vote
void bundleHV(uint8_t *result,
              uint8_t hvList[][sizeBytes],
              int count)
{
    int counts[D] = {0};

    for (int n = 0; n < count; n++)
        for (int i = 0; i < D; i++)
            counts[i] += getBit(hvList[n], i);

    for (int i = 0; i < D; i++)
        setBit(result, i, counts[i] >= (count / 2));
}

/* =====================================================
   ENCODE + PREDICT
   ===================================================== */

void encodeSample(float sample[], uint8_t *hv_out)
{
    uint8_t hvList[NUM_FEATURES][sizeBytes];

    for (int i = 0; i < NUM_FEATURES; i++) {
        float value = sample[i];

        int level = (int)((value + 3.0f) * SCALE);
        if (level < 0) level = 0;
        if (level >= NUM_LEVELS) level = NUM_LEVELS - 1;

        const uint8_t *im  = IM + i * sizeBytes;
        const uint8_t *val = VALUE_HV + level * sizeBytes;

        bindHV(hvList[i], im, val);
        rollHV(hvList[i], i * (D / NUM_FEATURES));
    }

    bundleHV(hv_out, hvList, NUM_FEATURES);
}

int hammingSim(const uint8_t *a, const uint8_t *b)
{
    int sim = 0;
    for (int i = 0; i < sizeBytes; i++) {
        uint8_t x = a[i] ^ b[i];
        sim += 8 - __builtin_popcount(x);
    }
    return sim;
}

int predict(float sample[])
{
    uint8_t hv[sizeBytes];
    encodeSample(sample, hv);

    int bestClass = 0;
    int bestSim = -1;

    for (int c = 0; c < NUM_CLASSES; c++) {
        const uint8_t *proto = PROTOTYPES + c * sizeBytes;
        int sim = hammingSim(hv, proto);
        if (sim > bestSim) {
            bestSim = sim;
            bestClass = c;
        }
    }
    return bestClass;
}

void normalizeSample(float raw[], float normed[])
{
    for (int i = 0; i < NUM_FEATURES; i++)
        normed[i] = (raw[i] - mu[i]) / sigma[i];
}

/* =====================================================
   MAIN TASK
   ===================================================== */
const float raw_example[NUM_FEATURES] = {
    228.139999f,
    0.59f,
    108.072424f,
    80.290001f,
    134.602594f,
    80.238454f
};


void app_main(void)
{
    float raw[NUM_FEATURES];
    float sample[NUM_FEATURES];

    memcpy(raw, raw_example, sizeof(raw));

    while (1) {
        uint64_t t0 = esp_timer_get_time();

        normalizeSample(raw, sample);
        int label_pred = predict(sample) + 1;

        uint64_t t1 = esp_timer_get_time();

        printf("Predicted class: %d | Latency: %.3f ms\n",
               label_pred,
               (t1 - t0) / 1000.0f);

        vTaskDelay(pdMS_TO_TICKS(2000)); // mỗi 1 giây
    }
}

