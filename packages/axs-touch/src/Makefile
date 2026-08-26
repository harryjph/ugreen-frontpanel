
ccflags-y += -Wall

# 禁用警告
KBUILD_CFLAGS += -Wno-error

# 启用额外警告但禁用错误（可选）
ccflags-y += -Wall
KBUILD_CFLAGS += -Wno-error

# 定义模块名称（最终生成 axs_touch.ko）
obj-m += axs_touch.o

# 列出所有需要编译的源文件（自动生成 .o 文件）
axs_touch-objs := \
    axs_core.o \
    axs_i2c.o \
    axs_spi.o \
    axs_debug.o \
    axs_upgrade.o \
    axs_download.o \
    axs_gesture.o \
    axs_esd.o
