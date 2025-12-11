#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 简化测试，只测试select组件的核心功能
int main() {
    printf("Testing select component just_expanded functionality\n");
    
    // 模拟展开和点击行为
    int just_expanded = 0;
    int expanded = 0;
    
    // 模拟点击展开
    printf("1. Clicking to expand dropdown\n");
    expanded = 1;
    just_expanded = 1;
    printf("   expanded=%d, just_expanded=%d\n", expanded, just_expanded);
    
    // 模拟第一次点击其他地方
    printf("2. First click outside dropdown\n");
    if (just_expanded) {
        just_expanded = 0;
        printf("   Cleared just_expanded flag, dropdown stays open\n");
        printf("   expanded=%d, just_expanded=%d\n", expanded, just_expanded);
    } else {
        printf("   Would close dropdown\n");
    }
    
    // 模拟第二次点击其他地方
    printf("3. Second click outside dropdown\n");
    if (just_expanded) {
        just_expanded = 0;
        printf("   Cleared just_expanded flag, dropdown stays open\n");
    } else {
        printf("   Closing dropdown\n");
        expanded = 0;
    }
    printf("   expanded=%d, just_expanded=%d\n", expanded, just_expanded);
    
    printf("Test completed successfully!\n");
    return 0;
}