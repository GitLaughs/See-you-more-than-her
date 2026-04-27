#!/bin/bash

# ==============================================================================
# Claude Code 一键启动脚本 (带代理)
# ==============================================================================

# 🔧 代理配置 - 请在这里设置您的代理
readonly HTTP_PROXY="http://127.0.0.1:7890"
readonly HTTPS_PROXY="http://127.0.0.1:7890"
readonly NO_PROXY="127.0.0.1,localhost,::1"

# 🔧 Claude Code 安装路径
readonly CLAUDE_PATH="/c/Users/sun/.trae-cn/binaries/node/versions/24.11.1/claude"

# ==============================================================================
# 以下内容请勿修改
# ==============================================================================

# 脚本常量
readonly CLAUDE_COMMAND="claude"

# 设置代理
set_proxy() {
    # 清除现有的代理设置
    unset http_proxy https_proxy HTTP_PROXY HTTPS_PROXY 2>/dev/null

    # 设置新的代理
    export http_proxy="$HTTP_PROXY"
    export https_proxy="$HTTPS_PROXY"
    export no_proxy="$NO_PROXY"
    export NO_PROXY="$NO_PROXY"
    echo "✅ 代理已设置: $HTTP_PROXY"
}

# 清除代理
unset_proxy() {
    unset http_proxy https_proxy HTTP_PROXY HTTPS_PROXY 2>/dev/null
}

# 主函数
main() {
    echo "🚀 Claude Code 一键启动 (代理模式)"
    echo "=================================="

    # 设置代理
    set_proxy

    # 显示代理设置
    echo "当前代理配置:"
    echo "  HTTP_PROXY:  $HTTP_PROXY"
    echo "  HTTPS_PROXY: $HTTPS_PROXY"
    echo ""

    # 启动 Claude Code
    echo "正在启动 Claude Code..."
    echo "=================================="

    # 执行 Claude Code
    "$CLAUDE_PATH" "$@"
    local exit_code=$?

    # 清除代理
    unset_proxy

    # 返回退出码
    return $exit_code
}

# 运行主函数
main "$@"