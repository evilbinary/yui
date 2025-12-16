/**
 * Dialog Component 使用示例
 * 
 * 这个示例展示了如何在YUI应用中使用dialog组件
 */

#include "../src/yui.h"
#include "../src/components/dialog_component.h"
#include "../src/components/button_component.h"
#include <stdio.h>

// 全局变量
static DialogComponent* g_info_dialog = NULL;
static DialogComponent* g_warning_dialog = NULL;
static DialogComponent* g_error_dialog = NULL;
static DialogComponent* g_question_dialog = NULL;

// 对话框回调函数
void info_ok_callback(DialogComponent* dialog, void* user_data) {
    printf("信息对话框: 用户点击了确定\n");
}

void warning_continue_callback(DialogComponent* dialog, void* user_data) {
    printf("警告对话框: 用户选择继续操作\n");
}

void warning_cancel_callback(DialogComponent* dialog, void* user_data) {
    printf("警告对话框: 用户取消操作\n");
}

void error_ok_callback(DialogComponent* dialog, void* user_data) {
    printf("错误对话框: 用户点击了确定\n");
}

void question_yes_callback(DialogComponent* dialog, void* user_data) {
    printf("问题对话框: 用户选择是\n");
}

void question_no_callback(DialogComponent* dialog, void* user_data) {
    printf("问题对话框: 用户选择否\n");
}

// 对话框关闭回调
void dialog_close_callback(DialogComponent* dialog, int button_index) {
    printf("对话框关闭，按钮索引: %d\n", button_index);
}

// 按钮点击回调
void show_info_dialog_clicked(void* user_data) {
    if (g_info_dialog) {
        dialog_component_show(g_info_dialog, 200, 150);
    }
}

void show_warning_dialog_clicked(void* user_data) {
    if (g_warning_dialog) {
        dialog_component_show(g_warning_dialog, 200, 150);
    }
}

void show_error_dialog_clicked(void* user_data) {
    if (g_error_dialog) {
        dialog_component_show(g_error_dialog, 200, 150);
    }
}

void show_question_dialog_clicked(void* user_data) {
    if (g_question_dialog) {
        dialog_component_show(g_question_dialog, 200, 150);
    }
}

// 初始化对话框
void init_dialogs(Layer* root_layer) {
    // 创建信息对话框
    Layer* info_layer = layer_create(root_layer, 0, 0, 400, 200);
    g_info_dialog = dialog_component_create(info_layer);
    dialog_component_set_title(g_info_dialog, "信息");
    dialog_component_set_message(g_info_dialog, "这是一个信息对话框示例。\n\n它用于向用户展示一般性的信息。");
    dialog_component_set_type(g_info_dialog, DIALOG_TYPE_INFO);
    dialog_component_add_button(g_info_dialog, "确定", info_ok_callback, NULL, 1, 0);
    g_info_dialog->on_close = dialog_close_callback;
    
    // 创建警告对话框
    Layer* warning_layer = layer_create(root_layer, 0, 0, 400, 200);
    g_warning_dialog = dialog_component_create(warning_layer);
    dialog_component_set_title(g_warning_dialog, "警告");
    dialog_component_set_message(g_warning_dialog, "这是一个警告对话框。\n\n操作可能会导致数据丢失，是否继续？");
    dialog_component_set_type(g_warning_dialog, DIALOG_TYPE_WARNING);
    dialog_component_add_button(g_warning_dialog, "继续", warning_continue_callback, NULL, 1, 0);
    dialog_component_add_button(g_warning_dialog, "取消", warning_cancel_callback, NULL, 0, 1);
    g_warning_dialog->on_close = dialog_close_callback;
    
    // 创建错误对话框
    Layer* error_layer = layer_create(root_layer, 0, 0, 400, 200);
    g_error_dialog = dialog_component_create(error_layer);
    dialog_component_set_title(g_error_dialog, "错误");
    dialog_component_set_message(g_error_dialog, "操作失败！\n\n无法连接到服务器，请检查网络连接后重试。");
    dialog_component_set_type(g_error_dialog, DIALOG_TYPE_ERROR);
    dialog_component_add_button(g_error_dialog, "确定", error_ok_callback, NULL, 1, 0);
    g_error_dialog->on_close = dialog_close_callback;
    
    // 创建问题对话框
    Layer* question_layer = layer_create(root_layer, 0, 0, 400, 200);
    g_question_dialog = dialog_component_create(question_layer);
    dialog_component_set_title(g_question_dialog, "确认");
    dialog_component_set_message(g_question_dialog, "是否删除选中的文件？\n\n此操作不可撤销。");
    dialog_component_set_type(g_question_dialog, DIALOG_TYPE_QUESTION);
    dialog_component_add_button(g_question_dialog, "是", question_yes_callback, NULL, 1, 0);
    dialog_component_add_button(g_question_dialog, "否", question_no_callback, NULL, 0, 1);
    g_question_dialog->on_close = dialog_close_callback;
}

// 主函数
int main(int argc, char* argv[]) {
    // 初始化YUI
    if (!yui_init()) {
        printf("Failed to initialize YUI\n");
        return 1;
    }
    
    // 创建窗口
    Window* window = window_create("Dialog Component Example", 800, 600);
    if (!window) {
        printf("Failed to create window\n");
        yui_cleanup();
        return 1;
    }
    
    // 创建根图层
    Layer* root_layer = layer_create(NULL, 0, 0, 800, 600);
    
    // 创建按钮
    Layer* info_btn_layer = layer_create(root_layer, 50, 50, 150, 40);
    ButtonComponent* info_btn = button_component_create(info_btn_layer);
    button_component_set_text(info_btn, "显示信息对话框");
    
    Layer* warning_btn_layer = layer_create(root_layer, 50, 100, 150, 40);
    ButtonComponent* warning_btn = button_component_create(warning_btn_layer);
    button_component_set_text(warning_btn, "显示警告对话框");
    
    Layer* error_btn_layer = layer_create(root_layer, 50, 150, 150, 40);
    ButtonComponent* error_btn = button_component_create(error_btn_layer);
    button_component_set_text(error_btn, "显示错误对话框");
    
    Layer* question_btn_layer = layer_create(root_layer, 50, 200, 150, 40);
    ButtonComponent* question_btn = button_component_create(question_btn_layer);
    button_component_set_text(question_btn, "显示问题对话框");
    
    // 注意：在实际的YUI框架中，你需要设置按钮的点击回调
    // 这里只是示例代码，实际实现可能需要根据框架的具体API调整
    
    // 初始化对话框
    init_dialogs(root_layer);
    
    // 运行主循环
    printf("Dialog Component Example\n");
    printf("点击按钮显示不同类型的对话框\n");
    printf("按ESC键退出程序\n");
    
    while (window_is_open(window)) {
        // 处理事件
        yui_handle_events();
        
        // 渲染
        backend_render_clear((Color){240, 240, 240, 255});
        layer_render(root_layer);
        
        // 渲染弹窗（包括对话框）
        popup_manager_render();
        
        backend_render_present();
        
        // 延迟
        backend_delay(16); // ~60 FPS
    }
    
    // 清理资源
    if (g_info_dialog) {
        dialog_component_destroy(g_info_dialog);
    }
    if (g_warning_dialog) {
        dialog_component_destroy(g_warning_dialog);
    }
    if (g_error_dialog) {
        dialog_component_destroy(g_error_dialog);
    }
    if (g_question_dialog) {
        dialog_component_destroy(g_question_dialog);
    }
    
    layer_destroy(root_layer);
    window_destroy(window);
    yui_cleanup();
    
    return 0;
}