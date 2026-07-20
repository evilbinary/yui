#ifndef YUI_TREEVIEW_COMPONENT_H
#define YUI_TREEVIEW_COMPONENT_H

#include "../layer.h"
#include "../ytype.h"

#ifdef __cplusplus
extern "C" {
#endif

// 树视图节点
typedef struct TreeNode {
    char* text;
    struct TreeNode** children;
    int child_count;
    int expanded;
    int selected;
    int level;
    int expandable;  // 1 = may have children (show expand icon even if child_count == 0)
    void* user_data;
    struct TreeNode* parent;
    void* extra;             // cJSON object storing any extra JSON fields not handled by C
    char* expand_icon;   // custom icon text when collapsed (e.g. "▶")
    char* collapse_icon; // custom icon text when expanded (e.g. "▼")
    char* expand_icon_path;   // path to SVG/image for collapsed icon
    char* collapse_icon_path; // path to SVG/image for expanded icon
    struct Texture* expand_icon_tex;   // cached texture for expand
    struct Texture* collapse_icon_tex; // cached texture for collapse
    char* icon;       // file path to SVG/image, or programmatic ID (e.g. "database", "table")
    char* icon_text;  // icon text rendered before node label (e.g. "📊", "⚡")
    struct Texture* icon_tex;  // cached texture for icon
} TreeNode;

// 树视图组件
typedef struct TreeViewComponent {
    Layer* layer;
    TreeNode** root_nodes;
    int root_count;
    TreeNode* selected_node;
    int item_height;
    int indent_width;
    int font_size; // 字体大小
    Color text_color;
    Color selected_text_color;
    Color selected_bg_color;
    Color hover_bg_color;
    Color expand_icon_color;
    void* user_data;
    void (*on_node_selected)(TreeNode* node, void* user_data);
    void (*on_node_expanded)(TreeNode* node, int expanded, void* user_data);
    int icon_size;               // max icon size (0 = auto from item_height)
    char* expand_icon;         // default expand icon text when collapsed
    char* collapse_icon;       // default collapse icon text when expanded
    char* expand_icon_path;    // default SVG/image path for collapsed
    char* collapse_icon_path;  // default SVG/image path for expanded
    char* on_select_name;           // event handler name for node selection
    EventHandler on_select_handler;  // cached event handler for node selection
    char* on_expand_name;            // event handler name for node expand/collapse
    EventHandler on_expand_handler;  // cached event handler for node expand/collapse
} TreeViewComponent;

// 创建树视图组件
TreeViewComponent* treeview_component_create(Layer* layer);

// 销毁树视图组件
void treeview_component_destroy(TreeViewComponent* component);

// 创建节点
TreeNode* treeview_create_node(const char* text);

// 销毁节点及其子节点
void treeview_destroy_node(TreeNode* node);

// 清空所有根节点
void treeview_clear_all_root_nodes(TreeViewComponent* component);

// 添加根节点
int treeview_add_root_node(TreeViewComponent* component, TreeNode* node);

// 添加子节点
int treeview_add_child_node(TreeNode* parent, TreeNode* child);

// 移除节点
void treeview_remove_node(TreeViewComponent* component, TreeNode* node);

// 设置节点文本
void treeview_set_node_text(TreeNode* node, const char* text);

// 获取节点文本
const char* treeview_get_node_text(TreeNode* node);

// 展开节点
void treeview_expand_node(TreeNode* node);

// 折叠节点
void treeview_collapse_node(TreeNode* node);

// 切换节点展开状态
void treeview_toggle_node(TreeNode* node);

// 检查节点是否展开
int treeview_is_node_expanded(TreeNode* node);

// 设置选中节点
void treeview_set_selected_node(TreeViewComponent* component, TreeNode* node);

// 获取选中节点
TreeNode* treeview_get_selected_node(TreeViewComponent* component);

// 获取节点的父节点
TreeNode* treeview_get_node_parent(TreeNode* node);

// 获取节点的子节点数量
int treeview_get_child_count(TreeNode* node);

// 获取节点的子节点
TreeNode* treeview_get_child(TreeNode* node, int index);

// 设置项目高度
void treeview_set_item_height(TreeViewComponent* component, int height);

// 设置缩进宽度
void treeview_set_indent_width(TreeViewComponent* component, int width);

// 设置字体大小
void treeview_set_font_size(TreeViewComponent* component, int size);

// 从JSON创建树视图组件
TreeViewComponent* treeview_component_create_from_json(Layer* layer, cJSON* json_obj);

// 递归解析树节点
TreeNode* parse_tree_node(cJSON* node_json, int level, TreeNode* parent);

// 设置颜色
void treeview_set_colors(TreeViewComponent* component, Color text, Color selected_text, Color selected_bg, Color hover_bg, Color expand_icon);

// 设置用户数据
void treeview_set_user_data(TreeViewComponent* component, void* data);

// 设置节点选中回调
void treeview_set_node_selected_callback(TreeViewComponent* component, void (*callback)(TreeNode*, void*));

// 设置节点展开回调
void treeview_set_node_expanded_callback(TreeViewComponent* component, void (*callback)(TreeNode*, int, void*));

// 处理鼠标事件
int treeview_component_handle_pointer_event(Layer* layer, PointerEvent* event);

// 处理键盘事件
int treeview_component_handle_key_event(Layer* layer, KeyEvent* event);

// 渲染树视图
void treeview_component_render(Layer* layer);

// 计算内容总高度
int treeview_calculate_content_height(TreeViewComponent* component);

// 估算内容宽度
int treeview_estimate_content_width(TreeViewComponent* component);

// 更新滚动条状态
void treeview_update_scrollbar(TreeViewComponent* component);

// 滚动到指定节点
void treeview_scroll_to_node(TreeViewComponent* component, TreeNode* target_node);

cJSON* treeview_component_get_property(Layer* layer, const char* property_name);

#ifdef __cplusplus
}
#endif

#endif // TREEVIEW_COMPONENT_H