CC = gcc
CFLAGS = -Iinclude -g -Wall
TARGET = tiny_nvs_demo
BUILD_DIR = build

# 自动扫描 src 下所有 .c 文件 + 根目录下的 main.c
SRCS = $(shell find src -name '*.c') main.c
OBJS = $(SRCS:%=$(BUILD_DIR)/%.o)

# 默认目标 (只编译)
all: $(BUILD_DIR)/$(TARGET)

# --- 新增: 运行目标 ---
# 依赖于 all，确保运行前先编译
run: all
	@echo "========================================"
	@echo "Running $(TARGET)..."
	@echo "========================================"
	@./$(BUILD_DIR)/$(TARGET)

# 链接
$(BUILD_DIR)/$(TARGET): $(OBJS)
	@echo "Linking $@"
	@$(CC) $(OBJS) -o $@ $(LDFLAGS)
	@echo "Build Success!"

# 编译
$(BUILD_DIR)/%.c.o: %.c
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -c $< -o $@

# 清理
clean:
	@rm -rf $(BUILD_DIR) flash_mock.bin
	@echo "Cleaned."

# 伪目标 (增加 run)
.PHONY: all clean run