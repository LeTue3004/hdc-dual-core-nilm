// #ifndef HDC_H
// #define HDC_H

// #include <stdint.h>
// #include <stdbool.h>

// #define D 120
// #define NUM_FEATURES 6
// #define NUM_BYTES (D + 7) / 8
// #define NUM_LEVELS 120
// #define NUM_CLASSES 127
// // Tỷ lệ lượng hóa level theo NUM_LEVELS
// #define SCALE ((NUM_LEVELS - 1) / 6.0f)

// // Khai báo extern để dùng ở nhiều file, không định nghĩa ở đây
// extern const float mu[];
// extern const float sigma[];
// extern const uint8_t IM[];
// extern const uint8_t VALUE_HV[];
// extern const uint8_t PROTOTYPES[];

// int predict(uint8_t hv[]);
// void normalizeSample(const float raw[], float normed[]);
// void encodeSample(const float sample[], uint8_t *hv_out);
// int hammingSim(const uint8_t *a, const uint8_t *b);

// #endif // HDC_H
