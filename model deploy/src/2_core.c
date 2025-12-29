// #include <stdio.h>
// #include <string.h>
// #include <stdint.h>
// #include <stdbool.h>

// #include "freertos/FreeRTOS.h"
// #include "freertos/task.h"
// #include "freertos/event_groups.h"
// #include "esp_timer.h"
// #include "esp_log.h"

// #include "test_set_hdc_model.h"

// /* ================= CONFIG ================= */
// #define TAG "HDC_2CORE"

// // Các macro quan trọng
// #define NUM_BYTES      ((D + 7) / 8)
// #define HALF_BYTES     (NUM_BYTES / 2)
// #define BIT_HALF       (D / 2)
// #define SCALE          ((NUM_LEVELS - 1) / 6.0f)

// // Event bits
// #define BIT_START       (1 << 0)
// #define BIT_CORE0_DONE  (1 << 1)
// #define BIT_CORE1_DONE  (1 << 2)

// /* ================= SHARED DATA ================= */
// static TaskHandle_t worker_task[2];
// static EventGroupHandle_t sync_event;

// // Partial similarity results from each core
// volatile int partial_sim[2][NUM_CLASSES];

// // Half hypervectors for each core
// static uint8_t hv_half[2][HALF_BYTES];

// // Full HV cho mỗi feature trước khi bundle (shared)
// static uint8_t feature_hvs[NUM_FEATURES][NUM_BYTES];

// // Normalized sample (shared between cores)
// static float sample_normalized[NUM_FEATURES];

// // Timing measurements
// uint64_t t_encode[2];
// uint64_t t_sim[2];
// uint64_t t_total;

// /* ================= TEST DATA ================= */
// const float raw_example[NUM_FEATURES] = {
//     228.139999f,  // Urms
//     0.59f,        // Irms
//     108.072424f,  // P
//     80.290001f,   // pf
//     134.602594f,  // S
//     80.238454f    // Q
// };

// /* ================= NORMALIZATION PARAMS ================= */
// const float mu[NUM_FEATURES] = {
//     225.57654627f,
//     1.29884934f,
//     266.68591358f,
//     86.85936184f,
//     292.1524991f,
//     104.82009801f
// };

// const float sigma[NUM_FEATURES] = {
//     2.14914949f,
//     0.81581784f,
//     184.29717067f,
//     13.86885008f,
//     182.57227239f,
//     51.1042152f
// };

// /* ================= UTILITY FUNCTIONS ================= */
// static inline int popcount8(uint8_t x)
// {
//     return __builtin_popcount(x);
// }

// static inline bool getBit(const uint8_t *array, int index)
// {
//     int byteIdx = index / 8;
//     int bitIdx  = 7 - (index % 8);
//     return (array[byteIdx] >> bitIdx) & 1;
// }

// static inline void setBit(uint8_t *array, int index, bool value)
// {
//     int byteIdx = index / 8;
//     int bitIdx  = 7 - (index % 8);
//     if (value)
//         array[byteIdx] |=  (1 << bitIdx);
//     else
//         array[byteIdx] &= ~(1 << bitIdx);
// }

// void normalizeSample(const float *raw, float *normalized)
// {
//     for (int i = 0; i < NUM_FEATURES; i++) {
//         normalized[i] = (raw[i] - mu[i]) / sigma[i];
//     }
// }

// /* ================= BIND (XOR) ================= */
// void bindHV(uint8_t *out, const uint8_t *a, const uint8_t *b)
// {
//     for (int i = 0; i < NUM_BYTES; i++)
//         out[i] = a[i] ^ b[i];
// }

// /* ================= ROLL (CIRCULAR SHIFT) ================= */
// void rollHV(uint8_t *hv, int shift)
// {
//     if (shift == 0) return;

//     shift %= D;
//     if (shift < 0) shift += D;

//     uint8_t temp[NUM_BYTES];
//     memcpy(temp, hv, NUM_BYTES);

//     int byteShift = shift / 8;
//     int bitShift  = shift % 8;

//     for (int i = 0; i < NUM_BYTES; i++) {
//         int src1 = (i - byteShift + NUM_BYTES) % NUM_BYTES;
//         int src2 = (src1 - 1 + NUM_BYTES) % NUM_BYTES;

//         hv[i] = (uint8_t)(
//             (temp[src1] >> bitShift) |
//             (temp[src2] << (8 - bitShift))
//         );
//     }

//     // Clear tail bits
//     int tailBits = NUM_BYTES * 8 - D;
//     if (tailBits > 0) {
//         uint8_t mask = 0xFF << tailBits;
//         hv[NUM_BYTES - 1] &= mask;
//     }
// }

// /* ================= ENCODE ALL FEATURES (Shared Step) ================= */
// void encode_all_features(const float *sample)
// {
//     for (int f = 0; f < NUM_FEATURES; f++) {
//         // Quantize feature value to level
//         int level = (int)((sample[f] + 3.0f) * SCALE);
//         if (level < 0) level = 0;
//         if (level >= NUM_LEVELS) level = NUM_LEVELS - 1;

//         const uint8_t *im  = IM + f * NUM_BYTES;
//         const uint8_t *val = VALUE_HV + level * NUM_BYTES;

//         // Bind: IM XOR VALUE_HV
//         bindHV(feature_hvs[f], im, val);
        
//         // Roll by feature-specific amount
//         int shift = f * (D / NUM_FEATURES);
//         rollHV(feature_hvs[f], shift);
//     }
// }

// /* ================= BUNDLE HALF (Each core bundles its half) ================= */
// void bundle_half(uint8_t *hv_half_out, int bit_start, int bit_count)
// {
//     int counts[BIT_HALF];
//     memset(counts, 0, sizeof(counts));

//     // Count votes from all features
//     for (int f = 0; f < NUM_FEATURES; f++) {
//         for (int i = 0; i < bit_count; i++) {
//             int bit_idx = bit_start + i;
//             if (getBit(feature_hvs[f], bit_idx)) {
//                 counts[i]++;
//             }
//         }
//     }

//     // Majority vote threshold
//     int threshold = NUM_FEATURES / 2;
    
//     // Clear output buffer
//     memset(hv_half_out, 0, HALF_BYTES);

//     // Set bits based on majority vote
//     for (int i = 0; i < bit_count; i++) {
//         if (counts[i] >= threshold) {
//             setBit(hv_half_out, i, true);
//         }
//     }
// }

// /* ================= HAMMING SIMILARITY (HALF) ================= */
// int hamming_sim_half(const uint8_t *a, const uint8_t *b, int numBytes)
// {
//     int sim = 0;
//     for (int i = 0; i < numBytes; i++) {
//         sim += 8 - popcount8(a[i] ^ b[i]);
//     }
//     return sim;
// }

// /* ================= WORKER CORE 0 ================= */
// void hdc_worker_core0(void *arg)
// {
//     ESP_LOGI(TAG, "Core 0 worker started");
    
//     while (1) {
//         // Wait for start signal
//         xEventGroupWaitBits(
//             sync_event,
//             BIT_START,
//             pdTRUE,   // Clear bit after reading
//             pdTRUE,
//             portMAX_DELAY);

//         uint64_t t_start = esp_timer_get_time();

//         // Encode all features (SHARED STEP - cả 2 core đều làm, nhưng chỉ 1 lần)
//         // Trong thực tế nên để main làm, nhưng để demo đơn giản
//         encode_all_features(sample_normalized);

//         // Bundle first half (bits 0 to D/2-1)
//         bundle_half(hv_half[0], 0, BIT_HALF);
        
//         uint64_t t_enc = esp_timer_get_time();
//         t_encode[0] = t_enc - t_start;

//         // Compute partial similarities for all classes
//         for (int c = 0; c < NUM_CLASSES; c++) {
//             partial_sim[0][c] = hamming_sim_half(
//                 hv_half[0],
//                 &PROTOTYPES[c * NUM_BYTES],
//                 HALF_BYTES
//             );
//         }
        
//         uint64_t t_similarity = esp_timer_get_time();
//         t_sim[0] = t_similarity - t_enc;

//         // Signal Core 0 done
//         xEventGroupSetBits(sync_event, BIT_CORE0_DONE);

//         // Wait for Core 1 to finish
//         xEventGroupWaitBits(
//             sync_event,
//             BIT_CORE1_DONE,
//             pdTRUE,   // Clear after reading
//             pdTRUE,
//             portMAX_DELAY);

//         /* ===== FUSION & CLASSIFICATION ===== */
//         int best_class = 0;
//         int best_sim = -1;
        
//         for (int c = 0; c < NUM_CLASSES; c++) {
//             int total_sim = partial_sim[0][c] + partial_sim[1][c];
//             if (total_sim > best_sim) {
//                 best_sim = total_sim;
//                 best_class = c;
//             }
//         }

//         uint64_t t_end = esp_timer_get_time();
//         t_total = t_end - t_start;

//         // Print results
//         printf("\n=== HDC INFERENCE RESULT ===\n");
//         printf("Predicted class : %d (Label: %d)\n", best_class, best_class + 1);
//         printf("Best similarity : %d / %d\n", best_sim, D);
//         printf("Core 0: encode=%llu us | sim=%llu us\n", t_encode[0], t_sim[0]);
//         printf("Core 1: encode=%llu us | sim=%llu us\n", t_encode[1], t_sim[1]);
//         printf("Total inference: %llu us (%.3f ms)\n", 
//                t_total, t_total / 1000.0f);
//         printf("===========================\n");
//     }
// }

// /* ================= WORKER CORE 1 ================= */
// void hdc_worker_core1(void *arg)
// {
//     ESP_LOGI(TAG, "Core 1 worker started");
    
//     while (1) {
//         // Wait for start signal
//         xEventGroupWaitBits(
//             sync_event,
//             BIT_START,
//             pdFALSE,  // Don't clear (Core 0 will clear)
//             pdTRUE,
//             portMAX_DELAY);

//         uint64_t t_start = esp_timer_get_time();

//         // Bundle second half (bits D/2 to D-1)
//         bundle_half(hv_half[1], BIT_HALF, BIT_HALF);
        
//         uint64_t t_enc = esp_timer_get_time();
//         t_encode[1] = t_enc - t_start;

//         // Compute partial similarities for all classes
//         for (int c = 0; c < NUM_CLASSES; c++) {
//             partial_sim[1][c] = hamming_sim_half(
//                 hv_half[1],
//                 &PROTOTYPES[c * NUM_BYTES + HALF_BYTES],
//                 HALF_BYTES
//             );
//         }
        
//         uint64_t t_similarity = esp_timer_get_time();
//         t_sim[1] = t_similarity - t_enc;

//         // Signal Core 1 done
//         xEventGroupSetBits(sync_event, BIT_CORE1_DONE);
//     }
// }

// /* ================= MAIN ================= */
// void app_main(void)
// {
//     ESP_LOGI(TAG, "HDC 2-Core EventGroup Inference System");
//     ESP_LOGI(TAG, "D=%d, NUM_CLASSES=%d, NUM_FEATURES=%d", 
//              D, NUM_CLASSES, NUM_FEATURES);

//     // Create event group for synchronization
//     sync_event = xEventGroupCreate();
//     if (!sync_event) {
//         ESP_LOGE(TAG, "Failed to create event group");
//         return;
//     }

//     // Create worker task for Core 0
//     xTaskCreatePinnedToCore(
//         hdc_worker_core0,
//         "hdc_core0",
//         8192,  // Tăng stack size
//         NULL,
//         5,
//         &worker_task[0],
//         0
//     );

//     // Create worker task for Core 1
//     xTaskCreatePinnedToCore(
//         hdc_worker_core1,
//         "hdc_core1",
//         8192,  // Tăng stack size
//         NULL,
//         5,
//         &worker_task[1],
//         1
//     );

//     // Wait for tasks to initialize
//     vTaskDelay(pdMS_TO_TICKS(100));

//     ESP_LOGI(TAG, "Starting inference loop...");

//     while (1) {
//         // Normalize sample ONCE before distributing work
//         normalizeSample(raw_example, sample_normalized);

//         // Trigger both cores to start inference
//         xEventGroupSetBits(sync_event, BIT_START);
        
//         // Wait 2 seconds between inferences
//         vTaskDelay(pdMS_TO_TICKS(2000));
//     }
// }