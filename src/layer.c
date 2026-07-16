#include "layer.h"
#include "layer_properties.h"
#include "layer_update.h"
#include "layer_lifecycle.h"
#include "util.h"
#include "animate.h"
#include "theme_manager.h"
#include "theme.h"
#include "backend.h"
#include "component_registry.h"
#include "log.h"

Layer* focused_layer = NULL;

// 全局 Inspect 模式开关
int yui_inspect_mode_enabled = 0;
int yui_inspect_show_bounds = 1;
int yui_inspect_show_info = 1;

int layer_padding_apply_from_json(int padding[4], cJSON* value)
{
    if (!padding || !value || !cJSON_IsArray(value)) {
        return 0;
    }

    int count = cJSON_GetArraySize(value);
    if (count >= 4) {
        padding[0] = cJSON_GetArrayItem(value, 0)->valueint;
        padding[1] = cJSON_GetArrayItem(value, 1)->valueint;
        padding[2] = cJSON_GetArrayItem(value, 2)->valueint;
        padding[3] = cJSON_GetArrayItem(value, 3)->valueint;
    } else if (count == 2) {
        int vertical = cJSON_GetArrayItem(value, 0)->valueint;
        int horizontal = cJSON_GetArrayItem(value, 1)->valueint;
        padding[0] = padding[2] = vertical;
        padding[1] = padding[3] = horizontal;
    } else if (count == 1) {
        int all = cJSON_GetArrayItem(value, 0)->valueint;
        padding[0] = padding[1] = padding[2] = padding[3] = all;
    } else {
        return 0;
    }
    return 1;
}

int layer_padding_get(const Layer* layer, int index)
{
    if (!layer || index < 0 || index > 3) {
        return 0;
    }
    if (layer->layout_manager && layer->layout_manager->padding[index] != 0) {
        return layer->layout_manager->padding[index];
    }
    return layer->padding[index];
}

int layer_apply_layout_from_json(Layer* layer, cJSON* layout)
{
    if (!layer || !layout || !cJSON_IsObject(layout)) {
        return 0;
    }

    if (!layer->layout_manager) {
        layer->layout_manager = (LayoutManager*)calloc(1, sizeof(LayoutManager));
        if (!layer->layout_manager) {
            return 0;
        }
        layer->layout_manager->type = LAYOUT_VERTICAL;
    }

    cJSON* layout_type = cJSON_GetObjectItem(layout, "type");
    if (layout_type && cJSON_IsString(layout_type)) {
        if (strcmp(layout_type->valuestring, "horizontal") == 0) {
            layer->layout_manager->type = LAYOUT_HORIZONTAL;
        } else if (strcmp(layout_type->valuestring, "vertical") == 0) {
            layer->layout_manager->type = LAYOUT_VERTICAL;
        } else if (strcmp(layout_type->valuestring, "center") == 0) {
            layer->layout_manager->type = LAYOUT_CENTER;
        } else if (strcmp(layout_type->valuestring, "left") == 0) {
            layer->layout_manager->type = LAYOUT_LEFT;
        } else if (strcmp(layout_type->valuestring, "right") == 0) {
            layer->layout_manager->type = LAYOUT_RIGHT;
        } else if (strcmp(layout_type->valuestring, "grid") == 0) {
            layer->layout_manager->type = LAYOUT_GRID;
        } else {
            layer->layout_manager->type = LAYOUT_ABSOLUTE;
        }
    }

    cJSON* layout_align = cJSON_GetObjectItem(layout, "align");
    if (layout_align && cJSON_IsString(layout_align)) {
        if (strcmp(layout_align->valuestring, "left") == 0) {
            layer->layout_manager->align = LAYOUT_ALIGN_LEFT;
        } else if (strcmp(layout_align->valuestring, "right") == 0) {
            layer->layout_manager->align = LAYOUT_ALIGN_RIGHT;
        } else if (strcmp(layout_align->valuestring, "center") == 0) {
            layer->layout_manager->align = LAYOUT_ALIGN_CENTER;
        }
    }

    cJSON* layout_justify = cJSON_GetObjectItem(layout, "justifyContent");
    if (!layout_justify) {
        layout_justify = cJSON_GetObjectItem(layout, "justify");
    }
    if (layout_justify && cJSON_IsString(layout_justify)) {
        const char* justify_str = layout_justify->valuestring;
        if (strcmp(justify_str, "center") == 0) {
            layer->layout_manager->justify = LAYOUT_ALIGN_CENTER;
        } else if (strcmp(justify_str, "left") == 0 || strcmp(justify_str, "flex-start") == 0) {
            layer->layout_manager->justify = LAYOUT_ALIGN_LEFT;
        } else if (strcmp(justify_str, "right") == 0 || strcmp(justify_str, "flex-end") == 0) {
            layer->layout_manager->justify = LAYOUT_ALIGN_RIGHT;
        } else if (strcmp(justify_str, "space-between") == 0) {
            layer->layout_manager->justify = LAYOUT_ALIGN_SPACE_BETWEEN;
        } else if (strcmp(justify_str, "space-around") == 0) {
            layer->layout_manager->justify = LAYOUT_ALIGN_SPACE_AROUND;
        }
    }

    cJSON* spacing = cJSON_GetObjectItem(layout, "spacing");
    if (spacing && cJSON_IsNumber(spacing)) {
        layer->layout_manager->spacing = spacing->valueint;
    }

    cJSON* gap = cJSON_GetObjectItem(layout, "gap");
    if (gap && cJSON_IsNumber(gap)) {
        layer->layout_manager->spacing = gap->valueint;
    }

    cJSON* padding = cJSON_GetObjectItem(layout, "padding");
    if (padding && layer_padding_apply_from_json(layer->layout_manager->padding, padding)) {
        memcpy(layer->padding, layer->layout_manager->padding, sizeof(layer->padding));
    }

    cJSON* columns = cJSON_GetObjectItem(layout, "columns");
    if (columns && cJSON_IsNumber(columns)) {
        layer->layout_manager->columns = columns->valueint;
    }

    return 1;
}

// 移除JSON字符串中的注释
static char* remove_json_comments(char* json_str) {
  if (!json_str) return NULL;

  size_t len = strlen(json_str);
  char* result = malloc(len + 1);
  if (!result) return NULL;

  int in_string = 0;   // 是否在字符串内
  int in_comment = 0;  // 0: 不在注释内, 1: 单行注释, 2: 多行注释
  size_t j = 0;        // 结果字符串的索引

  for (size_t i = 0; i < len; i++) {
    // 如果在字符串内
    if (in_string) {
      result[j++] = json_str[i];
      // 处理转义字符
      if (json_str[i] == '\\' && i + 1 < len) {
        result[j++] = json_str[++i];
      } else if (json_str[i] == '"') {
        in_string = 0;
      }
      continue;
    }

    // 如果不在注释内
    if (!in_comment) {
      // 检测到字符串开始
      if (json_str[i] == '"') {
        result[j++] = json_str[i];
        in_string = 1;
      }
      // 检测到单行注释
      else if (json_str[i] == '/' && i + 1 < len && json_str[i + 1] == '/') {
        in_comment = 1;
        i++;
      }
      // 检测到多行注释
      else if (json_str[i] == '/' && i + 1 < len && json_str[i + 1] == '*') {
        in_comment = 2;
        i++;
      }
      // 正常字符
      else {
        result[j++] = json_str[i];
      }
    }
    // 如果在单行注释内
    else if (in_comment == 1) {
      if (json_str[i] == '\n' || json_str[i] == '\r') {
        in_comment = 0;
        result[j++] = json_str[i];
      }
    }
    // 如果在多行注释内
    else if (in_comment == 2) {
      if (json_str[i] == '*' && i + 1 < len && json_str[i + 1] == '/') {
        in_comment = 0;
        i++;
      }
    }
  }

  result[j] = '\0';
  return result;
}

cJSON* parse_json(char* json_path) {
  FILE* file = fopen(json_path, "r");
  if (!file) {
    printf("open file failed %s\n", json_path);
    return NULL;
  }
  fseek(file, 0, SEEK_END);
  long fsize = ftell(file);
  fseek(file, 0, SEEK_SET);

  char* json_str = malloc(fsize + 1);
  fread(json_str, fsize, 1, file);
  fclose(file);
  json_str[fsize] = '\0';

  // 预处理JSON，移除注释
  char* cleaned_json = remove_json_comments(json_str);

  // 解析JSON
  cJSON* root_json = cJSON_Parse(cleaned_json);

  // 释放内存 bug?
  free(json_str);
  free(cleaned_json);

  return root_json;
}


// ====================== JSON解析函数 ======================
static void layer_init_strings(Layer* layer) {
  layer->label = NULL;
  layer->text = NULL;
  layer->text_size = 0;
}

static void layer_set_string(char** target, const char* value) {
  if (!target) {
    return;
  }
  if (*target) {
    free(*target);
    *target = NULL;
  }
  if (value) {
    size_t len = strlen(value);
    char* buf = (char*)malloc(len + 1);
    if (!buf) {
      return;
    }
    memcpy(buf, value, len + 1);
    *target = buf;
  }
}

// 专门用于设置Layer的text字段，并更新text_size
static void layer_set_text_with_size(Layer* layer, const char* value) {
  if (!layer) {
    return;
  }
  if (value) {
    size_t len = strlen(value);
    size_t required_size = len + 1; // 包括null终止符
    
    // 如果现有内存足够，直接使用
    if (layer->text && layer->text_size >= required_size) {
      memcpy(layer->text, value, required_size);
    } else {
      // 内存不足，需要重新分配
      char* new_buf = (char*)realloc(layer->text, required_size);
      if (!new_buf) return;
      memcpy(new_buf, value, required_size);
      layer->text = new_buf;
      layer->text_size = required_size;
    }
  } else {
    // 如果value为NULL，释放内存并重置
    if (layer->text) {
      free(layer->text);
      layer->text = NULL;
      layer->text_size = 0;
    }
  }
}

static void layer_copy_strings(Layer* dest, const Layer* src) {
  if (!dest || !src) {
    return;
  }
  if (src->label) {
    layer_set_string(&dest->label, src->label);
  }
  if (src->text) {
    layer_set_text_with_size(dest, src->text);
  }
}

static void layer_free_strings(Layer* layer) {
  if (!layer) {
    return;
  }
  if (layer->label) {
    free(layer->label);
    layer->label = NULL;
  }
  if (layer->text) {
    free(layer->text);
    layer->text = NULL;
  }
}

void layer_set_label(Layer* layer, const char* value) {
  if (!layer) {
    return;
  }
  layer_set_string(&layer->label, value);
}

void layer_set_text(Layer* layer, const char* value) {
  if (!layer) {
    return;
  }
  layer_set_text_with_size(layer, value);
  mark_layer_dirty(layer, DIRTY_TEXT);
}

const char* layer_get_label(const Layer* layer) {
  return layer && layer->label ? layer->label : "";
}

const char* layer_get_text(const Layer* layer) {
  return layer && layer->text ? layer->text : "";
}


Layer* layer_create(Layer* root_layer, int x, int y, int width, int height) {
  Layer* layer = malloc(sizeof(Layer));
  memset(layer, 0, sizeof(Layer));
  layer->parent = root_layer;
  layer->rect.x = x;
  layer->rect.y = y;
  layer->rect.w = width;
  layer->rect.h = height;
  layer_init_strings(layer);
  layer->handle_mouse_event = default_layer_handle_mouse_event;
  return layer;
}

Layer* layer_create_from_json(cJSON* json_obj, Layer* parent) {
  Layer* layer = malloc(sizeof(Layer));
  memset(layer, 0, sizeof(Layer));  // 初始化为0
  layer->parent = parent;  // 设置父元素指针

  layer=parse_layer_from_json(layer, json_obj, parent);

  return layer;
}

Layer* parse_layer_from_json(Layer* layer,cJSON* json_obj, Layer* parent) {
  if (json_obj == NULL) {
    return NULL;
  }
  if(layer==NULL){
    layer = malloc(sizeof(Layer));
    memset(layer, 0, sizeof(Layer));
    layer->parent = parent;
    layer_init_strings(layer);
  }

  if (!layer->handle_mouse_event) {
    layer->handle_mouse_event = default_layer_handle_mouse_event;
  }

  // 初始化焦点相关字段
  layer->state = LAYER_STATE_NORMAL;  // 默认处于正常状态
  layer->focusable = 0;               // 默认不可获得焦点
  
  // 初始化inspect相关字段
  layer->inspect_enabled = 0;         // 默认不启用inspect
  layer->inspect_show_bounds = 1;     // 默认显示边界
  layer->inspect_show_info = 1;       // 默认显示信息

  // 用于标记是否已经自定义处理了子图层（如SCROLLBAR类型）
  int has_custom_children = 0;
  int layout_padding_set = 0;

  // 解析基础属性
  if (cJSON_HasObjectItem(json_obj, "id")) {
    strcpy(layer->id, cJSON_GetObjectItem(json_obj, "id")->valuestring);
  }
  if (cJSON_HasObjectItem(json_obj, "connectable")) {
    cJSON* connectable_item = cJSON_GetObjectItem(json_obj, "connectable");
    if (cJSON_IsBool(connectable_item)) {
      layer->connectable = cJSON_IsTrue(connectable_item) ? 1 : 0;
    } else if (cJSON_IsNumber(connectable_item)) {
      layer->connectable = connectable_item->valueint != 0;
    }
  }
  layer->variant[0] = '\0';
  {
    cJSON* variants = cJSON_GetObjectItem(json_obj, "variants");
    if (variants && cJSON_IsArray(variants)) {
      int variant_count = cJSON_GetArraySize(variants);
      for (int vi = 0; vi < variant_count; vi++) {
        cJSON* item = cJSON_GetArrayItem(variants, vi);
        if (!item || !cJSON_IsString(item) || !item->valuestring[0]) {
          continue;
        }
        if (layer->variant[0] != '\0') {
          strncat(layer->variant, " ", sizeof(layer->variant) - strlen(layer->variant) - 1);
        }
        strncat(layer->variant, item->valuestring,
                sizeof(layer->variant) - strlen(layer->variant) - 1);
      }
    } else if (cJSON_HasObjectItem(json_obj, "variant")) {
      cJSON* variant_item = cJSON_GetObjectItem(json_obj, "variant");
      if (cJSON_IsString(variant_item)) {
        strncpy(layer->variant, variant_item->valuestring,
                sizeof(layer->variant) - 1);
        layer->variant[sizeof(layer->variant) - 1] = '\0';
      } else if (cJSON_IsArray(variant_item)) {
        int variant_count = cJSON_GetArraySize(variant_item);
        for (int vi = 0; vi < variant_count; vi++) {
          cJSON* item = cJSON_GetArrayItem(variant_item, vi);
          if (!item || !cJSON_IsString(item) || !item->valuestring[0]) {
            continue;
          }
          if (layer->variant[0] != '\0') {
            strncat(layer->variant, " ", sizeof(layer->variant) - strlen(layer->variant) - 1);
          }
          strncat(layer->variant, item->valuestring,
                  sizeof(layer->variant) - strlen(layer->variant) - 1);
        }
      }
    }
  }
  if (cJSON_HasObjectItem(json_obj, "type")) {
    const char* type_str = cJSON_GetObjectItem(json_obj, "type")->valuestring;
    layer->type = yui_type_resolve(type_str);
  } else {
    layer->type = VIEW;
  }

  // 解析字体 - font、fontSize、fontWeight 都是独立属性，各自有默认值
  cJSON* font = cJSON_GetObjectItem(json_obj, "font");
  cJSON* fontSize = cJSON_GetObjectItem(json_obj, "fontSize");
  cJSON* fontWeight = cJSON_GetObjectItem(json_obj, "fontWeight");
  
  // 从 style 中查找属性
  cJSON* style = cJSON_GetObjectItem(json_obj, "style");
  cJSON* styleFont = style ? cJSON_GetObjectItem(style, "font") : NULL;
  cJSON* styleFontSize = style ? cJSON_GetObjectItem(style, "fontSize") : NULL;
  cJSON* styleFontWeight = style ? cJSON_GetObjectItem(style, "fontWeight") : NULL;
  
  // 检查是否有定义任何字体相关属性
  int has_font_attrs = (font && font->valuestring) || 
                       styleFont || 
                       fontSize || styleFontSize || 
                       (fontWeight && fontWeight->valuestring) || styleFontWeight;
  
  // 如果有定义任何字体属性，创建新的 Font 对象
  if (has_font_attrs) {
    layer->font = malloc(sizeof(Font));
    layer->font->default_font = NULL;
    
    // 设置字体路径（优先级：直接属性 > style > 默认）
    if (font && font->valuestring) {
      strncpy(layer->font->path, font->valuestring, MAX_PATH - 1);
      layer->font->path[MAX_PATH - 1] = '\0';
    } else if (styleFont && styleFont->valuestring) {
      strncpy(layer->font->path, styleFont->valuestring, MAX_PATH - 1);
      layer->font->path[MAX_PATH - 1] = '\0';
    } else {
      strcpy(layer->font->path, "Roboto-Regular.ttf");  // 默认字体
    }
    
    // 设置字体大小（优先级：直接属性 > style > 默认）
    if (fontSize) {
      layer->font->size = fontSize->valueint;
    } else if (styleFontSize) {
      layer->font->size = styleFontSize->valueint;
    } else {
      layer->font->size = 16;  // 默认字体大小
    }
    
    // 设置字体粗细（优先级：直接属性 > style > 默认）
    const char* weight_val = NULL;
    if (fontWeight && fontWeight->valuestring) {
      weight_val = fontWeight->valuestring;
    } else if (styleFontWeight && styleFontWeight->valuestring) {
      weight_val = styleFontWeight->valuestring;
    } else {
      weight_val = "normal";  // 默认字体粗细
    }
    strncpy(layer->font->weight, weight_val, sizeof(layer->font->weight) - 1);
    layer->font->weight[sizeof(layer->font->weight) - 1] = '\0';
    
  } else {
    // 没有定义任何字体属性，继承父级字体
    if (parent) {
      layer->font = parent->font;
    } else {
      // 根节点且没有字体属性，创建默认字体
      layer->font = malloc(sizeof(Font));
      layer->font->default_font = NULL;
      strcpy(layer->font->path, "Roboto-Regular.ttf");
      layer->font->size = 16;
      strcpy(layer->font->weight, "normal");
    }
  }

  // 解析资源
  cJSON* assets = cJSON_GetObjectItem(json_obj, "assets");
  if (assets != NULL) {
    layer->assets = malloc(sizeof(Assets));
    strcpy(layer->assets->path,
           cJSON_GetObjectItem(json_obj, "assets")->valuestring);
  } else {
    if (parent) {
      layer->assets = parent->assets;
    }
  }

  {
    cJSON* font_fallback = cJSON_GetObjectItem(json_obj, "fontFallback");
    if (font_fallback && cJSON_IsString(font_fallback) && font_fallback->valuestring[0]) {
      char fallback_path[MAX_PATH];
      if (layer->assets && layer->assets->path[0] != '\0') {
        snprintf(fallback_path, sizeof(fallback_path), "%s/%s",
                 layer->assets->path, font_fallback->valuestring);
      } else {
        snprintf(fallback_path, sizeof(fallback_path), "%s", font_fallback->valuestring);
      }
      backend_set_font_fallback_path(fallback_path);
    }
  }

  // 解析位置尺寸
  cJSON* position = cJSON_GetObjectItem(json_obj, "position");
  if (position != NULL) {
    layer->rect.x = cJSON_GetArrayItem(position, 0)->valueint;
    layer->rect.y = cJSON_GetArrayItem(position, 1)->valueint;
  }

  cJSON* size = cJSON_GetObjectItem(json_obj, "size");
  if (size != NULL) {
    layer->rect.w = cJSON_GetArrayItem(size, 0)->valueint;
    if (cJSON_GetArraySize(size) > 1) {
      layer->rect.h = cJSON_GetArrayItem(size, 1)->valueint;
    } else {
      layer->rect.h = layer->rect.w;
    }
    layer->fixed_height = layer->rect.h;
    if (layer->rect.w > 0) {
      layer->fixed_width = layer->rect.w;
    } else {
      layer->fixed_width = 0;
    }
  }

  // 解析固定尺寸
  if (cJSON_HasObjectItem(json_obj, "width")) {
    layer->fixed_width = cJSON_GetObjectItem(json_obj, "width")->valueint;
    layer->rect.w = layer->fixed_width;  // 同时设置rect.w
  }
  if (cJSON_HasObjectItem(json_obj, "height")) {
    layer->fixed_height = cJSON_GetObjectItem(json_obj, "height")->valueint;
    layer->rect.h = layer->fixed_height;  // 同时设置rect.h
  }

  // 解析弹性比例
  if (cJSON_HasObjectItem(json_obj, "flex")) {
    layer->flex_ratio =
        (float)cJSON_GetObjectItem(json_obj, "flex")->valuedouble;
  }

  // 解析label和text属性
  if (cJSON_HasObjectItem(json_obj, "label")) {
    layer_set_label(layer, cJSON_GetObjectItem(json_obj, "label")->valuestring);
  }
  if (cJSON_HasObjectItem(json_obj, "text")) {
    layer_set_text(layer, cJSON_GetObjectItem(json_obj, "text")->valuestring);
  }

  // 解析可见性
  cJSON* visible_item = cJSON_GetObjectItem(json_obj, "visible");
  if (visible_item) {
    layer->visible = cJSON_IsTrue(visible_item) ? VISIBLE : IN_VISIBLE;
  }

  // 解析数据绑定
  cJSON* binding = cJSON_GetObjectItem(json_obj, "binding");
  if (binding) {
    layer->binding = malloc(sizeof(Binding));
    cJSON* path = cJSON_GetObjectItem(binding, "path");
    if (path) {
      strcpy(layer->binding->path, path->valuestring);
    }
  }
  // 数据解析
  cJSON* data = cJSON_GetObjectItem(json_obj, "data");
  if (data) {
    layer->data = malloc(sizeof(Data));
    cJSON* copy = cJSON_Duplicate(data, 1);
    layer->data->json = copy;
    layer->data->size = cJSON_GetArraySize(data);
  }
  // 禁用
  cJSON* disabled = cJSON_GetObjectItem(json_obj, "disabled");
  if (disabled) {
    if (cJSON_IsTrue(disabled)) {
      SET_STATE(layer, LAYER_STATE_DISABLED);
    }
  }

  // 数据解析
  cJSON* rotation = cJSON_GetObjectItem(json_obj, "rotation");
  if (rotation) {
    layer->rotation = rotation->valueint;
  }

  // 解析列表项模板
  cJSON* item_template = cJSON_GetObjectItem(json_obj, "itemTemplate");
  if (item_template) {
    layer->item_template = parse_layer_from_json(NULL, item_template, layer);
  }

  // 解析滚动属性 - 作为 Layer 的直接属性
  cJSON* scrollable = cJSON_GetObjectItem(json_obj, "scrollable");
  if (scrollable) {
    layer->scrollable = scrollable->valueint;

    // 根据 scrollable 类型自动创建滚动条
    if (layer->scrollable > 0) {
      // 创建垂直滚动条 (支持垂直滚动或双向滚动)
      if (layer->scrollable == 1 || layer->scrollable == 3) {
        layer->scrollbar_v = malloc(sizeof(Scrollbar));
        memset(layer->scrollbar_v, 0, sizeof(Scrollbar));
        layer->scrollbar_v->visible = 1;
        layer->scrollbar_v->thickness = 10;
        layer->scrollbar_v->color = (Color){128, 128, 128, 200};
        layer->scrollbar_v->direction = SCROLLBAR_DIRECTION_VERTICAL;
      }

      // 创建水平滚动条 (支持水平滚动或双向滚动)
      if (layer->scrollable == 2 || layer->scrollable == 3) {
        layer->scrollbar_h = malloc(sizeof(Scrollbar));
        memset(layer->scrollbar_h, 0, sizeof(Scrollbar));
        layer->scrollbar_h->visible = 1;
        layer->scrollbar_h->thickness = 10;
        layer->scrollbar_h->color = (Color){128, 128, 128, 200};
        layer->scrollbar_h->direction = SCROLLBAR_DIRECTION_HORIZONTAL;
        printf(
            "HORIZONTAL SCROLLBAR CREATED for layer '%s' with scrollable=%d, "
            "initial rect.w=%d\n",
            layer->id, layer->scrollable, layer->rect.w);
      }
    }
  }

  // 兼容旧的 scrollbar 配置 (用于自定义样式)
  cJSON* scrollbar = cJSON_GetObjectItem(json_obj, "scrollbar");
  if (scrollbar) {
    // 如果还没有创建滚动条，则创建旧的滚动条结构
    if (!layer->scrollbar) {
      layer->scrollbar = malloc(sizeof(Scrollbar));
      memset(layer->scrollbar, 0, sizeof(Scrollbar));
    }

    cJSON* visible = cJSON_GetObjectItem(scrollbar, "visible");
    if (visible) {
      int vis = cJSON_IsTrue(visible) ? 1 : 0;
      layer->scrollbar->visible = vis;
      if (layer->scrollbar_v) layer->scrollbar_v->visible = vis;
      if (layer->scrollbar_h) layer->scrollbar_h->visible = vis;
    }

    cJSON* thickness = cJSON_GetObjectItem(scrollbar, "thickness");
    if (thickness) {
      layer->scrollbar->thickness = thickness->valueint;
      if (layer->scrollbar_v) layer->scrollbar_v->thickness = thickness->valueint;
      if (layer->scrollbar_h) layer->scrollbar_h->thickness = thickness->valueint;
    }

    cJSON* color = cJSON_GetObjectItem(scrollbar, "color");
    if (color) {
      parse_color(color->valuestring, &layer->scrollbar->color);
      if (layer->scrollbar_v) {
        parse_color(color->valuestring, &layer->scrollbar_v->color);
      }
      if (layer->scrollbar_h) {
        parse_color(color->valuestring, &layer->scrollbar_h->color);
      }
    }
  }

  // 解析布局管理器
  cJSON* layout = cJSON_GetObjectItem(json_obj, "layout");
  if (layout) {
    layer_apply_layout_from_json(layer, layout);
    if (cJSON_GetObjectItem(layout, "padding")) {
      layout_padding_set = 1;
    }
  } else {
    // 如果没有布局配置，创建默认的垂直布局
    layer->layout_manager = malloc(sizeof(LayoutManager));
    memset(layer->layout_manager, 0, sizeof(LayoutManager));
    layer->layout_manager->type = LAYOUT_VERTICAL;
  }

  // 默认背景颜色
  if (layer->bg_color.a == 0) {
    layer->bg_color.r = 0xF5;
    layer->bg_color.g = 0xF5;
    layer->bg_color.b = 0xF5;
  }
  // 解析样式
  if (style) {
    if (cJSON_HasObjectItem(style, "color")) {
      parse_color(cJSON_GetObjectItem(style, "color")->valuestring,
                  &layer->color);
    }

    if (cJSON_HasObjectItem(style, "bgColor")) {
      parse_color(cJSON_GetObjectItem(style, "bgColor")->valuestring,
                  &layer->bg_color);
    }

    // 解析圆角半径属性
    if (cJSON_HasObjectItem(style, "radius")) {
      layer->radius = cJSON_GetObjectItem(style, "radius")->valueint;
    } else if (cJSON_HasObjectItem(style, "borderRadius")) {
      layer->radius = cJSON_GetObjectItem(style, "borderRadius")->valueint;
    }  else {
      layer->radius = 0;  // 默认没有圆角
    }

    {
      cJSON* style_padding = cJSON_GetObjectItem(style, "padding");
      if (style_padding &&
          layer_padding_apply_from_json(layer->padding, style_padding) &&
          !layout_padding_set && layer->layout_manager) {
        memcpy(layer->layout_manager->padding, layer->padding, sizeof(layer->padding));
      }
    }

    // 从style.mode中解析图片渲染模式（优先级高于imageMode）
    cJSON* mode_item = cJSON_GetObjectItem(style, "mode");
    if (mode_item) {
      const char* mode_str = mode_item->valuestring;
      if (strcmp(mode_str, "fit") == 0) {
        layer->image_mode = IMAGE_MODE_ASPECT_FIT;
      } else if (strcmp(mode_str, "fill") == 0) {
        layer->image_mode = IMAGE_MODE_ASPECT_FILL;
      } else if (strcmp(mode_str, "stretch") == 0) {
        layer->image_mode = IMAGE_MODE_STRETCH;
      }
    }

    // 解析毛玻璃效果
    cJSON* backdrop_filter = cJSON_GetObjectItem(style, "backdropFilter");
    if (backdrop_filter) {
      layer->backdrop_filter = cJSON_IsTrue(backdrop_filter) ? 1 : 0;

      // 解析模糊半径
      cJSON* blur_radius = cJSON_GetObjectItem(style, "blurRadius");
      if (blur_radius) {
        layer->blur_radius = blur_radius->valueint;
      } else {
        layer->blur_radius = 10;  // 默认模糊半径
      }

      // 解析饱和度
      cJSON* saturation = cJSON_GetObjectItem(style, "saturation");
      if (saturation) {
        layer->saturation = (float)saturation->valuedouble;
      } else {
        layer->saturation = 1.0f;  // 默认饱和度
      }

      // 解析亮度
      cJSON* brightness = cJSON_GetObjectItem(style, "brightness");
      if (brightness) {
        layer->brightness = (float)brightness->valuedouble;
      } else {
        layer->brightness = 1.0f;  // 默认亮度
      }
    }
  }

  // 解析资源路径
  cJSON* source = cJSON_GetObjectItem(json_obj, "source");
  if (source) {
    strcpy(layer->source, source->valuestring);
    if (layer->type != IMAGE) {
      cJSON* sub = parse_json(source->valuestring);
      if (sub != NULL) {
        layer->sub = parse_layer_from_json(NULL,sub, layer);
      } else {
        printf("cannot load file %s\n", source->valuestring);
      }
    }
  }

  // 解析事件绑定
  cJSON* events = cJSON_GetObjectItem(json_obj, "events");
  if (events) {
    layer_lifecycle_bind_events(layer, events);
    if (cJSON_HasObjectItem(events, "onClick")) {
      layer->event = malloc(sizeof(Event));
      memset(layer->event, 0, sizeof(Event));
      const char* handler_id =
          cJSON_GetObjectItem(events, "onClick")->valuestring;
      const char* lookup_name = handler_id;
      if (handler_id[0] == '@') {
        lookup_name = handler_id + 1;
      }
      strcpy(layer->event->click_name, lookup_name);

      EventHandler handler = find_event_by_name(lookup_name);
      layer->event->click = handler;
    }
    // 解析滚动事件
    if (cJSON_HasObjectItem(events, "onScroll")) {
      if (!layer->event) {
        layer->event = malloc(sizeof(Event));
        memset(layer->event, 0, sizeof(Event));
      }
      const char* handler_id =
          cJSON_GetObjectItem(events, "onScroll")->valuestring;
      const char* lookup_name = handler_id;
      if (handler_id[0] == '@') {
        lookup_name = handler_id + 1;
      }
      strcpy(layer->event->scroll_name, lookup_name);

      EventHandler handler = find_event_by_name(lookup_name);
      layer->event->scroll = handler;
    }
    // 解析统一触屏事件
    if (cJSON_HasObjectItem(events, "onTouch")) {
      if (!layer->event) {
        layer->event = malloc(sizeof(Event));
        memset(layer->event, 0, sizeof(Event));
      }
      const char* handler_id =
          cJSON_GetObjectItem(events, "onTouch")->valuestring;
      const char* lookup_name = handler_id;
      if (handler_id[0] == '@') {
        lookup_name = handler_id + 1;
      }
      strcpy(layer->event->touch_name, lookup_name);

      EventHandler handler = find_event_by_name(lookup_name);
      layer->event->touch = handler;
    }
    if (cJSON_HasObjectItem(events, "onResize")) {
      if (!layer->event) {
        layer->event = malloc(sizeof(Event));
        memset(layer->event, 0, sizeof(Event));
      }
      const char* handler_id =
          cJSON_GetObjectItem(events, "onResize")->valuestring;
      const char* lookup_name = handler_id;
      if (handler_id[0] == '@') {
        lookup_name = handler_id + 1;
      }
      strcpy(layer->event->resize_name, lookup_name);

      EventHandler handler = find_event_by_name(lookup_name);
      if (handler) {
        layer->event->resize = (void (*)(Layer*, const ResizeEvent*))handler;
      }
    }
  }

  // 解析动画属性配置
  cJSON* animation = cJSON_GetObjectItem(json_obj, "animation");
  if (animation) {
    // 解析持续时间
    float duration = 1.0f;  // 默认1秒
    if (cJSON_HasObjectItem(animation, "duration")) {
      duration = (float)cJSON_GetObjectItem(animation, "duration")->valuedouble;
    }

    // 解析缓动函数
    float (*easing_func)(float) = ease_in_out_quad;  // 默认缓入缓出
    if (cJSON_HasObjectItem(animation, "easing")) {
      const char* easing_str =
          cJSON_GetObjectItem(animation, "easing")->valuestring;
      if (strcmp(easing_str, "easeIn") == 0) {
        easing_func = ease_in_quad;
      } else if (strcmp(easing_str, "easeOut") == 0) {
        easing_func = ease_out_quad;
      } else if (strcmp(easing_str, "easeInOut") == 0) {
        easing_func = ease_in_out_quad;
      } else if (strcmp(easing_str, "elasticOut") == 0) {
        easing_func = ease_out_elastic;
      }
    }

    // 创建动画对象
    Animation* anim = animation_create(duration, easing_func);
    if (anim) {
      // 解析填充模式
      if (cJSON_HasObjectItem(animation, "fillMode")) {
        const char* fill_mode_str =
            cJSON_GetObjectItem(animation, "fillMode")->valuestring;
        if (strcmp(fill_mode_str, "none") == 0) {
          animation_set_fill_mode(anim, ANIMATION_FILL_NONE);
        } else if (strcmp(fill_mode_str, "forwards") == 0) {
          animation_set_fill_mode(anim, ANIMATION_FILL_FORWARDS);
        } else if (strcmp(fill_mode_str, "backwards") == 0) {
          animation_set_fill_mode(anim, ANIMATION_FILL_BACKWARDS);
        } else if (strcmp(fill_mode_str, "both") == 0) {
          animation_set_fill_mode(anim, ANIMATION_FILL_BOTH);
        }
      }

      // 解析重复动画属性
      // 重复类型
      if (cJSON_HasObjectItem(animation, "repeatType")) {
        const char* repeat_type_str =
            cJSON_GetObjectItem(animation, "repeatType")->valuestring;
        if (strcmp(repeat_type_str, "none") == 0) {
          animation_set_repeat_type(anim, ANIMATION_REPEAT_NONE);
        } else if (strcmp(repeat_type_str, "infinite") == 0) {
          animation_set_repeat_type(anim, ANIMATION_REPEAT_INFINITE);
        } else if (strcmp(repeat_type_str, "count") == 0) {
          animation_set_repeat_type(anim, ANIMATION_REPEAT_COUNT);
        }
      }

      // 重复次数
      if (cJSON_HasObjectItem(animation, "repeatCount")) {
        animation_set_repeat_count(
            anim, cJSON_GetObjectItem(animation, "repeatCount")->valueint);
      }

      // 反向重复标志
      if (cJSON_HasObjectItem(animation, "reverseOnRepeat")) {
        animation_set_reverse_on_repeat(
            anim,
            cJSON_IsTrue(cJSON_GetObjectItem(animation, "reverseOnRepeat")));
      }

      // 解析目标属性
      // 位置
      if (cJSON_HasObjectItem(animation, "x")) {
        animation_set_target(
            anim, ANIMATION_PROPERTY_X,
            (float)cJSON_GetObjectItem(animation, "x")->valuedouble);
      }
      if (cJSON_HasObjectItem(animation, "y")) {
        animation_set_target(
            anim, ANIMATION_PROPERTY_Y,
            (float)cJSON_GetObjectItem(animation, "y")->valuedouble);
      }

      // 大小
      if (cJSON_HasObjectItem(animation, "width")) {
        animation_set_target(
            anim, ANIMATION_PROPERTY_WIDTH,
            (float)cJSON_GetObjectItem(animation, "width")->valuedouble);
      }
      if (cJSON_HasObjectItem(animation, "height")) {
        animation_set_target(
            anim, ANIMATION_PROPERTY_HEIGHT,
            (float)cJSON_GetObjectItem(animation, "height")->valuedouble);
      }

      // 透明度
      if (cJSON_HasObjectItem(animation, "opacity")) {
        animation_set_target(
            anim, ANIMATION_PROPERTY_OPACITY,
            (float)cJSON_GetObjectItem(animation, "opacity")->valuedouble);
      }

      // 旋转
      if (cJSON_HasObjectItem(animation, "rotation")) {
        animation_set_target(
            anim, ANIMATION_PROPERTY_ROTATION,
            (float)cJSON_GetObjectItem(animation, "rotation")->valuedouble);
      }

      // 缩放
      if (cJSON_HasObjectItem(animation, "scaleX")) {
        animation_set_target(
            anim, ANIMATION_PROPERTY_SCALE_X,
            (float)cJSON_GetObjectItem(animation, "scaleX")->valuedouble);
      }
      if (cJSON_HasObjectItem(animation, "scaleY")) {
        animation_set_target(
            anim, ANIMATION_PROPERTY_SCALE_Y,
            (float)cJSON_GetObjectItem(animation, "scaleY")->valuedouble);
      }

      // 解析自动播放属性
      if (cJSON_HasObjectItem(animation, "autoPlay")) {
        if (cJSON_IsTrue(cJSON_GetObjectItem(animation, "autoPlay"))) {
          animation_start(layer, anim);
        } else {
          // 如果不自动播放，则将动画对象保存到图层，但不启动
          layer->animation = anim;
        }
      } else {
        // 默认不自动播放
        layer->animation = anim;
      }
    }
  }

  int skip_children = 0;

  yui_component_instantiate(layer, json_obj, parent, &has_custom_children);

  const YuiComponentOps* type_ops = yui_type_get_ops(layer->type);
  if (type_ops && (type_ops->flags & YUI_COMP_SKIP_CHILDREN)) {
    skip_children = 1;
  }

  // 递归解析子图层
  cJSON* children = cJSON_GetObjectItem(json_obj, "children");
  if (children && !has_custom_children && !skip_children) {
    layer->child_count = cJSON_GetArraySize(children);
    layer->children =
        realloc(layer->children, layer->child_count * sizeof(Layer*));

    for (int i = 0; i < layer->child_count; i++) {
      cJSON* child =cJSON_GetArrayItem(children, i);
      if(child==NULL){
        printf("Warning: child is null\n");
        continue;
      }
      layer->children[i] = parse_layer_from_json(NULL,child, layer);
    }
  }

  // 应用主题样式（在解析完所有属性后，但在返回前）
  Theme* current_theme = theme_manager_get_current();
  if (current_theme) {
    const char* type_name = yui_type_name(layer->type);
    theme_apply_to_layer(current_theme, layer, layer->id, type_name);
  }

  // 组件 JSON 的 style 在主题之后再次应用，确保本地配置覆盖主题
  if (style && cJSON_IsObject(style) && layer->set_style) {
    layer->set_style(layer, style);
  }

  return layer;
}

// ====================== 销毁函数 ======================
void destroy_layer(Layer* layer) {
    if (!layer) return;

    layer_lifecycle_before_destroy(layer);
    
    // 递归销毁子图层
    if (layer->children) {
        for (int i = 0; i < layer->child_count; i++) {
            destroy_layer(layer->children[i]);
        }
        free(layer->children);
        layer->children = NULL;
    }
    
    // 销毁子模板
    if (layer->item_template) {
        destroy_layer(layer->item_template);
        layer->item_template = NULL;
    }
    
    // 销毁子图层
    if (layer->sub) {
        destroy_layer(layer->sub);
        layer->sub = NULL;
    }
    
    // 销毁动画
    if (layer->animation) {
        free(layer->animation);
        layer->animation = NULL;
    }
    
    // 销毁事件
    if (layer->event) {
        free(layer->event);
        layer->event = NULL;
    }
    
    // 销毁数据绑定
    if (layer->binding) {
        free(layer->binding);
        layer->binding = NULL;
    }
    
    // 销毁数据
    if (layer->data) {
        if (layer->data->json) {
            cJSON_Delete(layer->data->json);
        }
        free(layer->data);
        layer->data = NULL;
    }
    
    // 销毁布局管理器
    if (layer->layout_manager) {
        free(layer->layout_manager);
        layer->layout_manager = NULL;
    }
    
    // 销毁滚动条
    if (layer->scrollbar) {
        free(layer->scrollbar);
        layer->scrollbar = NULL;
    }
    if (layer->scrollbar_v) {
        free(layer->scrollbar_v);
        layer->scrollbar_v = NULL;
    }
    if (layer->scrollbar_h) {
        free(layer->scrollbar_h);
        layer->scrollbar_h = NULL;
    }
    
    // 注意：不销毁 font 和 assets，因为它们可能是共享的
    // 这些应该由全局资源管理器负责

    if (layer->on_destroy) {
        layer->on_destroy(layer);
        layer->on_destroy = NULL;
    } else {
        const YuiComponentOps* ops = yui_type_get_ops(layer->type);
        if (ops && ops->destroy) {
            ops->destroy(layer);
        }
    }

    layer_free_strings(layer);
    free(layer);
}

// 查找图层
Layer* find_layer_by_id(Layer* root, const char* id) {
    if (!root || !id) return NULL;

    if (strcmp(root->id, id) == 0) {
        return root;
    }

    // 递归查找子图层
    for (int i = 0; i < root->child_count; i++) {
        Layer* result = find_layer_by_id(root->children[i], id);
        if (result) return result;
    }

    // 检查sub图层
    if (root->sub) {
        Layer* result = find_layer_by_id(root->sub, id);
        if (result) return result;
    }

    return NULL;
}

Layer* layer_resolve_path(Layer* root, const char* path)
{
    char path_copy[256];
    char* token;

    if (!root || !path || path[0] == '\0') {
        return NULL;
    }

    strncpy(path_copy, path, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';

    token = strtok(path_copy, ".");
    while (token != NULL && root != NULL) {
        if (strcmp(token, "children") == 0) {
            token = strtok(NULL, ".");
            if (!token) {
                break;
            }

            {
                char* endptr;
                long index = strtol(token, &endptr, 10);

                if (*endptr == '\0' && index >= 0 && index < root->child_count) {
                    root = root->children[index];
                } else {
                    Layer* found = NULL;
                    int i;

                    for (i = 0; i < root->child_count; i++) {
                        if (root->children[i] &&
                            strcmp(root->children[i]->id, token) == 0) {
                            found = root->children[i];
                            break;
                        }
                    }
                    root = found;
                }
            }
        } else {
            root = find_layer_by_id(root, token);
        }

        token = strtok(NULL, ".");
    }

    return root;
}

int layer_show(Layer* layer) {
    if (!layer) {
        return 0;
    }
    if (layer->visible == VISIBLE) {
        return 0;
    }

    layer_set_visible(layer, VISIBLE);
    return 1;
}

int layer_hide(Layer* layer) {
    if (!layer) {
        return 0;
    }
    if (layer->visible == IN_VISIBLE) {
        return 0;
    }

    layer_set_visible(layer, IN_VISIBLE);
    return 1;
}

cJSON* parse_json_string(const char* json_str) {
    if (!json_str) {
        return NULL;
    }

    char* cleaned_json = remove_json_comments((char*)json_str);
    if (!cleaned_json) {
        return NULL;
    }

    cJSON* json_obj = cJSON_Parse(cleaned_json);
    free(cleaned_json);
    return json_obj;
}

// 从 JSON 字符串解析并创建图层
Layer* parse_layer_from_string(const char* json_str, Layer* parent) {
    if (!json_str) {
        LOGE("layer", "json_str is NULL");
        return NULL;
    }

    LOGD("layer", "parsing layer from JSON string (length: %zu)", strlen(json_str));

    cJSON* json_obj = parse_json_string(json_str);
    if (!json_obj) {
        LOGE("layer", "failed to parse JSON string");
        return NULL;
    }

    LOGD("layer", "JSON parsed successfully");

    // 创建图层
    Layer* layer = layer_create_from_json(json_obj, parent);

    // 删除 JSON 对象
    cJSON_Delete(json_obj);

    return layer;
}