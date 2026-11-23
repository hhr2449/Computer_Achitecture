for i in 1 2 3; do
    ./build/matrix_mul
done | awk -F ':' '
    /time spent for original method/ { original += $2 }
    /time spent for new method/      { optimized += $2 }

    END {
        printf "原始方法平均耗时：%.6f s\n", original / 3
        printf "优化方法平均耗时：%.6f s\n", optimized / 3
        printf "加速比：%.3f 倍\n", original / optimized
    }
'