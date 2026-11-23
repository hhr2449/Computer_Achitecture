#!/usr/bin/env bash
set -e

# 脚本所在目录就是本地 Cache_lab。
local_dir="$(cd -- "$(dirname -- "$0")" && pwd)"
server="ubuntu@36.103.177.136"
server_dir="/home/ubuntu/Computer_Achitecture/Cache_lab"
export SSHPASS='Vo.0HwChRAGwgX4n'

# 下载整个目录，只更新本地缺少或较旧的文件。
sshpass -e rsync -avzu \
    -e "ssh -o StrictHostKeyChecking=accept-new" \
    "$server:$server_dir/" "$local_dir/"
