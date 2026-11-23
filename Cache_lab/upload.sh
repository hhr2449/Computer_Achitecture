#!/usr/bin/env bash
set -e

# 脚本所在目录就是本地 Cache_lab，运行时不需要先切换目录。
local_dir="$(cd -- "$(dirname -- "$0")" && pwd)"
server="ubuntu@36.103.177.136"
server_dir="/home/ubuntu/Computer_Achitecture/Cache_lab"
export SSHPASS='Vo.0HwChRAGwgX4n'

# 上传整个目录，只更新服务器上缺少或较旧的文件。
# 末尾的 / 表示同步目录内容，避免再嵌套一层 Cache_lab。
# 首次连接时记录服务器指纹，之后检查指纹是否一致。
# 服务器端先创建目标目录，再接收文件。
sshpass -e rsync -avzu \
    -e "ssh -o StrictHostKeyChecking=accept-new" \
    --rsync-path="mkdir -p '$server_dir' && rsync" \
    "$local_dir/" "$server:$server_dir/"
