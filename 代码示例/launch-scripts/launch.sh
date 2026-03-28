#!/bin/bash
# OpenClaw U盘便携版启动脚本

set -e

# 设置环境变量
export OPENCLAW_HOME=$(dirname "$0")
export OPENCLAW_CONFIG_PATH="$OPENCLAW_HOME/config"
export OPENCLAW_WORKSPACE="$OPENCLAW_HOME/workspace"
export OPENCLAW_LOG_PATH="$OPENCLAW_HOME/logs"

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 日志函数
log() {
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] $1"
}

log_success() {
    echo "${GREEN}[SUCCESS] $1${NC}"
}

log_error() {
    echo "${RED}[ERROR] $1${NC}"
}

log_warning() {
    echo "${YELLOW}[WARNING] $1${NC}"
}

log_info() {
    echo "${BLUE}[INFO] $1${NC}"
}

# 检查目录结构
check_directories() {
    log_info "检查目录结构..."
    
    # 必需目录
    required_dirs=(
        "$OPENCLAW_HOME/runtime"
        "$OPENCLAW_HOME/config"
        "$OPENCLAW_HOME/workspace"
        "$OPENCLAW_HOME/logs"
        "$OPENCLAW_HOME/skills"
    )
    
    for dir in "${required_dirs[@]}"; do
        if [ ! -d "$dir" ]; then
            log_error "目录不存在: $dir"
            return 1
        fi
    done
    
    # 必需文件
    required_files=(
        "$OPENCLAW_HOME/runtime/node/bin/node"
        "$OPENCLAW_HOME/runtime/openclaw-dist/openclaw.mjs"
        "$OPENCLAW_HOME/config/hardware.bin"
        "$OPENCLAW_HOME/config/license.json"
    )
    
    for file in "${required_files[@]}"; do
        if [ ! -f "$file" ]; then
            log_error "文件不存在: $file"
            return 1
        fi
    done
    
    log_success "目录结构检查完成"
}

# 硬件验证
hardware_check() {
    log_info "进行硬件验证..."
    
    # 获取当前USB设备ID
    USB_ID=$(blkid -o value -s UUID $(mount | grep "/dev/sd" | head -1 | awk '{print $1}'))
    
    if [ -z "$USB_ID" ]; then
        log_error "无法获取USB设备ID"
        return 1
    }
    
    log_info "当前USB设备UUID: $USB_ID"
    
    # 读取存储的硬件ID
    STORED_ID=$(cat "$OPENCLAW_HOME/config/hardware.bin")
    
    # 比较硬件ID
    if [ "$USB_ID" != "$STORED_ID" ]; then
        log_error "硬件验证失败: 设备ID不匹配"
        log_warning "存储ID: $STORED_ID"
        log_warning "当前ID: $USB_ID"
        
        # 计算相似度（允许轻微变化）
        similarity=$(awk -v s="$STORED_ID" -v c="$USB_ID" '
            BEGIN {
                len = length(s)
                matches = 0
                for (i = 1; i <= len; i++) {
                    if (substr(s, i, 1) == substr(c, i, 1)) matches++
                }
                print matches / len
            }
        ')
        
        if [ "$similarity" < "0.95" ]; then
            log_error "设备ID差异过大，拒绝启动"
            return 1
        else
            log_warning "设备ID轻微变化，可能是USB重新分区或更新"
            log_warning "相似度: $similarity"
            
            # 更新硬件ID（可选）
            log_info "更新硬件ID记录..."
            echo "$USB_ID" > "$OPENCLAW_HOME/config/hardware.bin"
        fi
    else
        log_success "硬件验证成功"
    fi
    
    return 0
}

# 授权验证
license_check() {
    log_info "进行授权验证..."
    
    # 检查授权文件
    if [ ! -f "$OPENCLAW_HOME/config/license.json" ]; then
        log_error "授权文件不存在"
        return 1
    fi
    
    # 解析授权文件
    LICENSE_DATA=$(cat "$OPENCLAW_HOME/config/license.json")
    
    # 检查授权状态
    STATUS=$(echo "$LICENSE_DATA" | jq -r '.status')
    if [ "$STATUS" != "active" ] && [ "$STATUS" != "trial" ]; then
        log_error "授权状态无效: $STATUS"
        return 1
    fi
    
    # 检查过期时间
    EXPIRATION=$(echo "$LICENSE_DATA" | jq -r '.expiration_date')
    if [ "$EXPIRATION" != "null" ]; then
        EXPIRATION_TS=$(date -d "$EXPIRATION" +%s)
        CURRENT_TS=$(date +%s)
        
        if [ "$CURRENT_TS" -gt "$EXPIRATION_TS" ]; then
            log_error "授权已过期"
            return 1
        fi
        
        # 计算剩余天数
        REMAIN_DAYS=$(( ($EXPIRATION_TS - $CURRENT_TS) / 86400 ))
        if [ "$REMAIN_DAYS" -lt 0 ]; then
            log_error "授权已过期"
            return 1
        elif [ "$REMAIN_DAYS" -lt 7 ]; then
            log_warning "授权即将过期: 剩余 $REMAIN_DAYS 天"
        else
            log_info "授权有效期: $REMAIN_DAYS 天"
        fi
    fi
    
    log_success "授权验证成功"
    return 0
}

# 解密核心文件（如果使用加密）
decrypt_files() {
    log_info "解密核心文件..."
    
    # 检查是否需要解密
    if [ -f "$OPENCLAW_HOME/runtime/openclaw-dist/core.js" ]; then
        log_info "核心文件已解密，跳过解密步骤"
        return 0
    fi
    
    # 检查加密文件
    if [ ! -f "$OPENCLAW_HOME/runtime/openclaw-dist/core.js.enc" ]; then
        log_info "未使用加密，跳过解密"
        return 0
    fi
    
    # 读取授权密钥
    LICENSE_KEY=$(echo "$LICENSE_DATA" | jq -r '.license_key')
    
    if [ -z "$LICENSE_KEY" ]; then
        log_error "授权密钥不存在"
        return 1
    fi
    
    # 解密核心文件
    log_info "使用授权密钥解密..."
    openssl enc -d -aes-256-cbc \
        -in "$OPENCLAW_HOME/runtime/openclaw-dist/core.js.enc" \
        -out "$OPENCLAW_HOME/runtime/openclaw-dist/core.js" \
        -pass pass:"$LICENSE_KEY"
    
    if [ $? -eq 0 ]; then
        log_success "核心文件解密成功"
    else
        log_error "核心文件解密失败"
        return 1
    fi
    
    # 解密其他文件（如果有）
    for enc_file in "$OPENCLAW_HOME/runtime/openclaw-dist/*.enc"; do
        if [ -f "$enc_file" ]; then
            base_file=$(basename "$enc_file" .enc)
            openssl enc -d -aes-256-cbc \
                -in "$enc_file" \
                -out "$OPENCLAW_HOME/runtime/openclaw-dist/$base_file" \
                -pass pass:"$LICENSE_KEY"
            
            if [ $? -eq 0 ]; then
                log_info "文件解密成功: $base_file"
            else
                log_warning "文件解密失败: $base_file"
            fi
        fi
    done
    
    return 0
}

# 完整性校验
integrity_check() {
    log_info "进行完整性校验..."
    
    # 加载完整性校验文件
    if [ ! -f "$OPENCLAW_HOME/config/integrity.json" ]; then
        log_info "未配置完整性校验，跳过"
        return 0
    fi
    
    # 计算当前文件哈希
    declare -A checksums
    
    # 校验核心文件
    CORE_FILE="$OPENCLAW_HOME/runtime/openclaw-dist/core.js"
    if [ -f "$CORE_FILE" ]; then
        CORE_HASH=$(sha256sum "$CORE_FILE" | awk '{print $1}')
        EXPECTED_CORE_HASH=$(cat "$OPENCLAW_HOME/config/integrity.json" | jq -r '.core')
        
        if [ "$CORE_HASH" != "$EXPECTED_CORE_HASH" ]; then
            log_error "核心文件完整性校验失败"
            return 1
        fi
        log_info "核心文件校验成功"
    fi
    
    # 校验配置文件
    LICENSE_FILE="$OPENCLAW_HOME/config/license.json"
    if [ -f "$LICENSE_FILE" ]; then
        LICENSE_HASH=$(sha256sum "$LICENSE_FILE" | awk '{print $1}')
        EXPECTED_LICENSE_HASH=$(cat "$OPENCLAW_HOME/config/integrity.json" | jq -r '.license')
        
        if [ "$LICENSE_HASH" != "$EXPECTED_LICENSE_HASH" ]; then
            log_warning "授权文件可能被修改"
        else
            log_info "授权文件校验成功"
        fi
    fi
    
    log_success "完整性校验完成"
    return 0
}

# 启动OpenClaw
start_openclaw() {
    log_info "启动OpenClaw..."
    
    # 设置Node.js路径
    export PATH="$OPENCLAW_HOME/runtime/node/bin:$PATH"
    
    # 检查Node.js
    if [ ! -x "$OPENCLAW_HOME/runtime/node/bin/node" ]; then
        log_error "Node.js不存在或不可执行"
        return 1
    fi
    
    # 检查OpenClaw
    if [ ! -f "$OPENCLAW_HOME/runtime/openclaw-dist/openclaw.mjs" ]; then
        log_error "OpenClaw主程序不存在"
        return 1
    fi
    
    # 创建日志文件
    LOG_FILE="$OPENCLAW_LOG_PATH/$(date '+%Y-%m-%d').log"
    touch "$LOG_FILE"
    
    # 启动OpenClaw
    log_info "启动Node.js + OpenClaw..."
    "$OPENCLAW_HOME/runtime/node/bin/node" \
        "$OPENCLAW_HOME/runtime/openclaw-dist/openclaw.mjs" \
        --workspace "$OPENCLAW_WORKSPACE" \
        --config "$OPENCLAW_CONFIG_PATH/default.yaml" \
        --log-path "$LOG_FILE" \
        start
    
    if [ $? -eq 0 ]; then
        log_success "OpenClaw启动成功"
    else
        log_error "OpenClaw启动失败"
        return 1
    fi
    
    return 0
}

# 主函数
main() {
    log "======================================="
    log " OpenClaw U盘便携版启动"
    log "======================================="
    
    # 1. 检查目录结构
    check_directories || exit 1
    
    # 2. 硬件验证
    hardware_check || {
        log_error "硬件验证失败，启动中止"
        exit 1
    }
    
    # 3. 授权验证
    license_check || {
        log_error "授权验证失败，启动中止"
        exit 1
    }
    
    # 4. 解密文件
    decrypt_files || {
        log_error "文件解密失败，启动中止"
        exit 1
    }
    
    # 5. 完整性校验
    integrity_check || {
        log_error "完整性校验失败，启动中止"
        exit 1
    }
    
    # 6. 启动OpenClaw
    start_openclaw || {
        log_error "OpenClaw启动失败"
        exit 1
    }
    
    log_success "OpenClaw U盘便携版启动完成"
    return 0
}

# 运行主函数
main "$@"