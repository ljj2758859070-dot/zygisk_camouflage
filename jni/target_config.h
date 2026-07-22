#ifndef TARGET_CONFIG_H
#define TARGET_CONFIG_H

// ===================== 【修改区域，按需更改】=====================
// 目标游戏包名
#define TARGET_PACKAGE_NAME     "com.tencent.tmgp.dfm"

// 设备基础信息
#define PROD_MODEL              "SGT-AL00"
#define PROD_BRAND              "huawei"
#define PROD_MANUFACTURER       "huawei"
#define SOC_MODEL               "Kirin 9030 Pro"
#define BOARD_PLATFORM          "hi3030"
#define HW_NAME                 "kirin9030"
#define SOC_COMPAT_STR          "hisilicon,hi3030"
#define CPU_HARDWARE_STR        "HiSilicon Kirin 9030 Pro"

// CPU频率配置
#define CPU_PRIME_MAX           2750000
#define CPU_PRIME_MIN           800000
#define CPU_BIG_MAX             2270000
#define CPU_BIG_MIN             600000
#define CPU_LIT_MAX             1720000
#define CPU_LIT_MIN             400000
#define GPU_MAX_FREQ            933000
#define REAL_CPU_MAX            3200000

// GPU信息
#define GPUINFO_TEXT \
"GPU Type        : Maleoon 935\n"\
"GPU Version     : r1p0\n"\
"GPU Frequency   : 933 MHz\n"\
"GPU Core Count  : 6\n"\
"VRAM Size       : 4096 MB\n"
// ================================================================

#endif
