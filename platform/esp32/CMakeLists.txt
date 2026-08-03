cmake_minimum_required(VERSION 3.16)

# 使用自定义分区表（含 font 数据分区）
set(PARTITION_TABLE_CSV "partitions.csv")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(yui_esp32)
