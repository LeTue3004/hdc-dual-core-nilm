# Hyperdimensional Computing on Dual-Core Embedded Systems

This repository contains the implementation, datasets, and report
for a research project on **Hyperdimensional Computing (HDC)**
optimized for **dual-core embedded systems**, with a focus on
time-series data and resource-constrained devices.

## Project Overview
- Encode time-series sensor data using Hyperdimensional Computing
- Train HDC models offline using Python
- Export trained models as C header files for embedded deployment
- Optimize similarity computation for parallel execution on dual-core MCUs

## Tools & Environment
- **Python** (NumPy, Pandas, Jupyter Notebook) for offline training and evaluation
- **C/C++** for embedded implementation
- **PlatformIO** for firmware development and deployment
- Target platform: **ESP32-class dual-core microcontrollers**

## Repository Structure

```text
hdc-dual-core-project/
│
├── data/                  # Datasets
│   ├── data1288.csv
│   ├── train_dataset.csv
│   ├── valid_dataset.csv
│   └── test_dataset.csv
│
├── src/                   # Training & preprocessing
│   └── train_hdc.ipynb
│
├── results/               # Exported HDC models (C headers)
│   ├── hdc_model_64bit.h
│   ├── hdc_model_128bit.h
│   ├── hdc_model_256bit.h
│   ├── hdc_model_512bit.h
│   ├── hdc_model_1024bit.h
│   └── hdc_model_2048bit.h
│
├── model_deploy/          # Embedded deployment code
│
├── report/                # Final report / thesis
│   └── 22022127_Le_Van_Tue_Do_an_nganh.pdf
│
└── README.md
