#include <stdio.h>
#ifdef D_SDL
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#else
#include <SDL.h>
#include <SDL_ttf.h>
#include <SDL_image.h>
#endif

#include "cJSON.h"

// ====================== 图层数据结构 ======================
typedef enum {
    LAYOUT_ABSOLUTE,
    LAYOUT_HORIZONTAL,
    LAYOUT_VERTICAL,
    LAYOUT_CENTER,
    LAYOUT_LEFT,
    LAYOUT_RIGHT,
    LAYOUT_ALIGN_CENTER,
    LAYOUT_ALIGN_LEFT,
    LAYOUT_ALIGN_RIGHT
} LayoutType;

// 添加图片渲染模式枚举
typedef enum {
    IMAGE_MODE_STRETCH,     // 拉伸填充整个区域
    IMAGE_MODE_ASPECT_FIT,  // 自适应完整显示图片（可能有空白）
    IMAGE_MODE_ASPECT_FILL  // 填充整个区域（可能裁剪图片）
} ImageMode;

typedef struct LayoutManager {
    LayoutType type;
    int spacing;
    int padding[4]; // top, right, bottom, left
    LayoutType align;
} LayoutManager;

// 添加数据绑定结构
typedef struct Binding {
    char path[100];
} Binding;

typedef struct Data {
    int size;
    cJSON* json;
} Data;

typedef struct Font {
    char path[512];
    int size;
    TTF_Font* default_font;  // 添加默认字体
} Font;


typedef enum {
    VIEW,
    BUTTON,
    INPUT,
    LABEL,
    IMAGE,
    LIST
} LayerType;

 char *layer_type_name[]={"View","Button","Input","Label","Image","List"};


typedef struct Layer Layer;
typedef struct Layer {
    char id[50];
    LayerType type;
    SDL_Rect rect;
    SDL_Color color;
    SDL_Color bgColor;
    char source[100];
    SDL_Texture* texture;
    Layer** children;
    int child_count;
    void (*onClick)();  // 事件回调函数指针
    
    // 新增布局管理器
    LayoutManager* layout_manager;
    int fixed_width;
    int fixed_height;
    float flex_ratio;
    
    // 新增label和text字段
    char label[100];
    char text[100];
    
    // 添加图片模式字段
    ImageMode image_mode;
    
    // 添加数据绑定和模板字段
    Binding* binding;
    Layer* item_template;
    //数据
    Data* data;

    //资源文件路径
    Font* font;

    Layer* sub;
} Layer;

// ====================== 全局渲染器 ======================
SDL_Renderer* renderer = NULL;
TTF_Font* default_font=NULL;
float scale=1.0;


cJSON* parse_json(char* json_path){
    FILE* file = fopen(json_path, "r");
    fseek(file, 0, SEEK_END);
    long fsize = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    char* json_str = malloc(fsize + 1);
    fread(json_str, fsize, 1, file);
    fclose(file);
    json_str[fsize] = '\0';
    
    // 解析JSON
    cJSON* root_json = cJSON_Parse(json_str);
    free(json_str);
    return root_json;
}
// ====================== JSON解析函数 ======================
Layer* parse_layer(cJSON* json_obj) {
    if(json_obj==NULL){
        return NULL;
    }
    Layer* layer = malloc(sizeof(Layer));
    memset(layer, 0, sizeof(Layer));
    
    // 解析基础属性
    if(cJSON_HasObjectItem(json_obj, "id")){
        strcpy(layer->id, cJSON_GetObjectItem(json_obj, "id")->valuestring);
    }
    if(cJSON_HasObjectItem(json_obj, "type")){
        for(int i=0;i<sizeof(layer_type_name)/sizeof(layer_type_name[0]);i++ ){
            //printf("cmp %s == %s\n", cJSON_GetObjectItem(json_obj, "type")->valuestring,layer_type_name[i] );
            if(strcmp(cJSON_GetObjectItem(json_obj, "type")->valuestring,layer_type_name[i])==0){
                layer->type=i;
                break;
            }
        }
    }
    
    // 解析资字体
    cJSON* font = cJSON_GetObjectItem(json_obj, "font");
    if(font!=NULL){
        layer->font= malloc(sizeof(Font));
        strcpy(layer->font->path, 
              cJSON_GetObjectItem(json_obj, "font")->valuestring);
    }

    // 解析位置尺寸
    cJSON* position = cJSON_GetObjectItem(json_obj, "position");
    if(position!=NULL){
        layer->rect.x = cJSON_GetArrayItem(position, 0)->valueint;
        layer->rect.y = cJSON_GetArrayItem(position, 1)->valueint;
    }
    
    
    cJSON* size = cJSON_GetObjectItem(json_obj, "size");
    if(size!=NULL){
        layer->rect.w = cJSON_GetArrayItem(size, 0)->valueint;
        layer->rect.h = cJSON_GetArrayItem(size, 1)->valueint;
        layer->fixed_width= layer->rect.w;
        layer->fixed_height= layer->rect.h;

    }
    
    // 解析固定尺寸
    if (cJSON_HasObjectItem(json_obj, "width")) {
        layer->fixed_width = cJSON_GetObjectItem(json_obj, "width")->valueint;
    }
    if (cJSON_HasObjectItem(json_obj, "height")) {
        layer->fixed_height = cJSON_GetObjectItem(json_obj, "height")->valueint;
    }
    
    // 解析弹性比例
    if (cJSON_HasObjectItem(json_obj, "flex")) {
        layer->flex_ratio = (float)cJSON_GetObjectItem(json_obj, "flex")->valuedouble;
    }
    
    // 解析label和text属性
    if (cJSON_HasObjectItem(json_obj, "label")) {
        strcpy(layer->label, cJSON_GetObjectItem(json_obj, "label")->valuestring);
    }
    if (cJSON_HasObjectItem(json_obj, "text")) {
        strcpy(layer->text, cJSON_GetObjectItem(json_obj, "text")->valuestring);
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
    //数据解析
    cJSON* data = cJSON_GetObjectItem(json_obj, "data");
    if (data) {
        layer->data = malloc(sizeof(Data));
        cJSON *copy = cJSON_Duplicate(data, 1);
        layer->data->json= copy;
        layer->data->size=cJSON_GetArraySize(json_obj);
    }
    
    // 解析列表项模板
    cJSON* item_template = cJSON_GetObjectItem(json_obj, "itemTemplate");
    if (item_template) {
        layer->item_template = parse_layer(item_template);
    }

   
    
    // 解析布局管理器
    cJSON* layout = cJSON_GetObjectItem(json_obj, "layout");
    if (layout) {
        layer->layout_manager = malloc(sizeof(LayoutManager));
        memset(layer->layout_manager, 0, sizeof(LayoutManager));
        
        cJSON* layout_type = cJSON_GetObjectItem(layout, "type");
        if (layout_type) {
            if (strcmp(layout_type->valuestring, "horizontal") == 0) {
                layer->layout_manager->type = LAYOUT_HORIZONTAL;
            } else if (strcmp(layout_type->valuestring, "vertical") == 0) {
                layer->layout_manager->type = LAYOUT_VERTICAL;
            }else if (strcmp(layout_type->valuestring, "center") == 0) {
                layer->layout_manager->type = LAYOUT_CENTER;
            }else if (strcmp(layout_type->valuestring, "left") == 0) {
                layer->layout_manager->type = LAYOUT_LEFT;
            }else if (strcmp(layout_type->valuestring, "right") == 0) {
                layer->layout_manager->type = LAYOUT_RIGHT;
            }  else {
                layer->layout_manager->type = LAYOUT_ABSOLUTE;
            }
        } else {
            // 默认使用垂直布局
            layer->layout_manager->type = LAYOUT_VERTICAL;
        }
        cJSON* layout_align = cJSON_GetObjectItem(layout, "align");
        if (layout_align) {
            //child 行为
            if (strcmp(layout_align->valuestring, "left") == 0) {
                layer->layout_manager->align = LAYOUT_ALIGN_LEFT;
            }else if (strcmp(layout_align->valuestring, "right") == 0) {
                layer->layout_manager->align = LAYOUT_ALIGN_RIGHT;
            }else if (strcmp(layout_align->valuestring, "center") == 0) {
                layer->layout_manager->align = LAYOUT_ALIGN_CENTER;
            }else{
                layer->layout_manager->align = LAYOUT_ALIGN_LEFT;
            }
            
        }
        
        if (cJSON_HasObjectItem(layout, "spacing")) {
            layer->layout_manager->spacing = cJSON_GetObjectItem(layout, "spacing")->valueint;
        }
        
        cJSON* padding = cJSON_GetObjectItem(layout, "padding");
        if (padding && cJSON_GetArraySize(padding) == 4) {
            layer->layout_manager->padding[0] = cJSON_GetArrayItem(padding, 0)->valueint; // top
            layer->layout_manager->padding[1] = cJSON_GetArrayItem(padding, 1)->valueint; // right
            layer->layout_manager->padding[2] = cJSON_GetArrayItem(padding, 2)->valueint; // bottom
            layer->layout_manager->padding[3] = cJSON_GetArrayItem(padding, 3)->valueint; // left
        }
    } else {
        // 如果没有布局配置，创建默认的垂直布局
        layer->layout_manager = malloc(sizeof(LayoutManager));
        memset(layer->layout_manager, 0, sizeof(LayoutManager));
        layer->layout_manager->type = LAYOUT_VERTICAL;
    }
    
    // 解析样式
    cJSON* style = cJSON_GetObjectItem(json_obj, "style");
    if (style) {
        if(cJSON_HasObjectItem(style,"color")){
            sscanf(cJSON_GetObjectItem(style, "color")->valuestring, 
                "#%02hhx%02hhx%02hhx", 
                &layer->color.r, &layer->color.g, &layer->color.b);
            layer->color.a = 255;
        }

        if(cJSON_HasObjectItem(style,"bgColor")){
             sscanf(cJSON_GetObjectItem(style, "bgColor")->valuestring, 
               "#%02hhx%02hhx%02hhx", 
               &layer->bgColor.r, &layer->bgColor.g, &layer->bgColor.b);
            layer->bgColor.a = 255;
        }else{
            layer->bgColor.r=0;
            layer->bgColor.g=0;
            layer->bgColor.b=0;
            layer->bgColor.a = 0;
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
       
    }
    
    // 解析资源路径
    cJSON* source=cJSON_GetObjectItem(json_obj, "source");
    if (source) {
        strcpy(layer->source, 
              source->valuestring);
        if(layer->type!=IMAGE){
            cJSON* sub=parse_json(source->valuestring);
            if(sub!=NULL){
                layer->sub=parse_layer(sub);
            }else{
                printf("cannot load file %s\n",source->valuestring);
            }
            
        }
    }    
    
    // 解析事件绑定
    cJSON* events = cJSON_GetObjectItem(json_obj, "events");
    if (events && cJSON_HasObjectItem(events, "onClick")) {
        const char* handler_id = cJSON_GetObjectItem(events, "onClick")->valuestring;
        // 实际项目应建立handler_id到函数的映射表


        //if (strcmp(handler_id, "@handleSubmit") == 0) {
            // layer->onClick = submit_handler; // 绑定实际函数
        //}
    }
    
    // 递归解析子图层
    cJSON* children = cJSON_GetObjectItem(json_obj, "children");
    if (children) {
        layer->child_count = cJSON_GetArraySize(children);
        layer->children = malloc(layer->child_count * sizeof(Layer*));
        
        for (int i = 0; i < layer->child_count; i++) {
            layer->children[i] = parse_layer(cJSON_GetArrayItem(children, i));
        }
    }
    
    return layer;
}

// ====================== 资源加载器 ======================
void load_textures(Layer* root) {
    if (root->type==IMAGE&& strlen(root->source) > 0) {
        // 修改为使用SDL_image支持多种格式
        SDL_Surface* surface = IMG_Load(root->source);
        if (surface) {
            root->texture = SDL_CreateTextureFromSurface(renderer, surface);
            SDL_FreeSurface(surface);
        } else {
            printf("Failed to load image %s: %s\n", root->source, IMG_GetError());
        }
    }
}

// 添加文字渲染函数
SDL_Texture* render_text(const char* text, SDL_Color color) {
    if (!default_font) return NULL;
    
    SDL_Surface* surface = TTF_RenderUTF8_Blended(default_font, text, color);
    if (!surface) return NULL;
    
    
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_SetTextureScaleMode(texture, SDL_ScaleModeBest);

    SDL_FreeSurface(surface);
    
    return texture;
}


void layout_layer(Layer* layer){
// 应用布局管理器
    if (layer->layout_manager && layer->child_count > 0) {
        int padding_top = layer->layout_manager->padding[0];
        int padding_right = layer->layout_manager->padding[1];
        int padding_bottom = layer->layout_manager->padding[2];
        int padding_left = layer->layout_manager->padding[3];
        int spacing = layer->layout_manager->spacing;
        
        int content_width = layer->rect.w - padding_left - padding_right;
        int content_height = layer->rect.h - padding_top - padding_bottom;
        
        if (layer->layout_manager->type == LAYOUT_HORIZONTAL) {
            // 计算总权重
            float total_flex = 0;
            int fixed_width_sum = 0;
            
            for (int i = 0; i < layer->child_count; i++) {
                if (layer->children[i]->flex_ratio > 0) {
                    total_flex += layer->children[i]->flex_ratio;
                } else {
                    fixed_width_sum += layer->children[i]->fixed_width > 0 ? 
                                      layer->children[i]->fixed_width : 50; // 默认宽度
                }
            }
            
            // 分配空间
            int available_width = content_width - fixed_width_sum - 
                                 (layer->child_count - 1) * spacing;
            int current_x = layer->rect.x + padding_left;
            
            for (int i = 0; i < layer->child_count; i++) {
                Layer* child = layer->children[i];
                if (child->flex_ratio > 0 && total_flex > 0) {
                    child->rect.w = (int)(available_width * 
                                        (child->flex_ratio / total_flex));
                } else if (child->fixed_width > 0) {
                    child->rect.w = child->fixed_width;
                } else {
                    child->rect.w = 50; // 默认宽度
                }
                
                child->rect.x = current_x;
                child->rect.y = layer->rect.y + padding_top;
                child->rect.h = content_height;
                
                current_x += child->rect.w + spacing;
            }
        } else if (layer->layout_manager->type == LAYOUT_VERTICAL) {
            // 计算总权重
            float total_flex = 0;
            int fixed_height_sum = 0;
            
            for (int i = 0; i < layer->child_count; i++) {
                if (layer->children[i]->flex_ratio > 0) {
                    total_flex += layer->children[i]->flex_ratio;
                } else {
                    fixed_height_sum += layer->children[i]->fixed_height > 0 ? 
                                       layer->children[i]->fixed_height : 30; // 默认高度
                }
            }
            
            
            // 分配空间
            int available_height = content_height - fixed_height_sum - 
                                  (layer->child_count - 1) * spacing;
            int current_y = layer->rect.y + padding_top;
            
            for (int i = 0; i < layer->child_count; i++) {
                Layer* child = layer->children[i];
                if (child->flex_ratio > 0 && total_flex > 0) {
                    child->rect.h = (int)(available_height * 
                                        (child->flex_ratio / total_flex));
                } else if (child->fixed_height > 0) {
                    child->rect.h = child->fixed_height;
                } else {
                    // 自动计算高度
                    child->rect.h = 30; // 默认高度
                }
                
                child->rect.x = layer->rect.x + padding_left;
                child->rect.y = current_y;
                // 自动计算宽度以填充可用空间
                if(child->rect.w<=0){
                    child->rect.w = content_width;
                }
                //printf("%s %s %s %d\n",child->type,child->id,child->text,child->rect.w);
                
                if(child->layout_manager->type == LAYOUT_CENTER){
                    child->rect.x=layer->rect.x+ child->rect.w/2 + padding_left/2;
                }else if(child->layout_manager->type == LAYOUT_LEFT){
                    child->rect.x =layer->rect.x+ padding_left;
                }else if(child->layout_manager->type == LAYOUT_RIGHT){
                    child->rect.x =layer->rect.x + child->rect.w -padding_left/2;
                }else{
                    //对齐
                    if(layer->layout_manager->align==LAYOUT_ALIGN_CENTER){
                        child->rect.x=layer->rect.x+ child->rect.w/2 + padding_left/2;
                    }else if(layer->layout_manager->align==LAYOUT_ALIGN_LEFT){
                        child->rect.x =layer->rect.x+ padding_left;
                    }else if(layer->layout_manager->align==LAYOUT_ALIGN_RIGHT){
                        child->rect.x =layer->rect.x + child->rect.w -padding_left/2;
                    }
                }
                
                current_y += child->rect.h + spacing;
            }
        }
    }
    if(layer->sub!=NULL){
        layout_layer(layer->sub);
    }
}

// ====================== 渲染管线 ======================
void render_layer(Layer* layer) {
    
    
    // 根据图层类型进行不同的渲染处理
    if (layer->type == BUTTON) {
        // 按钮类型渲染：绘制背景和边框
        if(layer->bgColor.a>0){
            SDL_SetRenderDrawColor(renderer, 
                                layer->bgColor.r, 
                                layer->bgColor.g, 
                                layer->bgColor.b, 
                                layer->bgColor.a);
            SDL_RenderFillRect(renderer, &layer->rect);
        }
        
        // 绘制按钮边框
        SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
        SDL_RenderDrawRect(renderer, &layer->rect);
        
        // 渲染按钮文本
        if (strlen(layer->text) > 0) {
            // 使用SDL_ttf渲染文本
           
            SDL_Color text_color = layer->color; // 白色文字
            SDL_Texture* text_texture = render_text(layer->text, text_color);
            
            if (text_texture) {
                int text_width, text_height;
                SDL_QueryTexture(text_texture, NULL, NULL, &text_width, &text_height);
                
                SDL_Rect text_rect = {
                    layer->rect.x + (layer->rect.w - text_width/ scale) / 2,  // 居中
                    layer->rect.y + (layer->rect.h - text_height/ scale) / 2,
                    text_width / scale,
                    text_height / scale
                };
                
                // 确保文本不会超出按钮边界
                if (text_rect.w > layer->rect.w - 20) {
                    text_rect.w = layer->rect.w - 20;
                    text_rect.x = layer->rect.x + 10;
                }
                
                if (text_rect.h > layer->rect.h) {
                    text_rect.h = layer->rect.h;
                    text_rect.y = layer->rect.y;
                }
                
                SDL_RenderCopy(renderer, text_texture, NULL, &text_rect);
                SDL_DestroyTexture(text_texture);
            }
        }
    } 
    else if (layer->type==INPUT) {
        // 输入框类型渲染：绘制背景和边框
        if(layer->bgColor.a>0){
            SDL_SetRenderDrawColor(renderer, 
                                layer->bgColor.r, 
                                layer->bgColor.g, 
                                layer->bgColor.b, 
                                layer->bgColor.a);
            
            SDL_RenderFillRect(renderer, &layer->rect);
        }
        
        // 绘制输入框边框
        SDL_SetRenderDrawColor(renderer, 150, 150, 150, 255);
        SDL_RenderDrawRect(renderer, &layer->rect);
        
        // 渲染输入框标签
        if (strlen(layer->label) > 0) {
            // 使用SDL_ttf渲染文本
           
            SDL_Color text_color = layer->color;
            SDL_Texture* text_texture = render_text(layer->label, text_color);
            
            if (text_texture) {
                int text_width, text_height;
                SDL_QueryTexture(text_texture, NULL, NULL, &text_width, &text_height);
                
                SDL_Rect text_rect = {
                    layer->rect.x + 5,  // 左侧留5像素边距
                    layer->rect.y + (layer->rect.h - text_height/ scale) / 2,
                    text_width / scale,
                    text_height / scale
                };
                
                // 确保文本不会超出输入框边界
                if (text_rect.x + text_rect.w > layer->rect.x + layer->rect.w - 5) {
                    text_rect.w = layer->rect.x + layer->rect.w - 5 - text_rect.x;
                }
                
                if (text_rect.y + text_rect.h > layer->rect.y + layer->rect.h) {
                    text_rect.h = layer->rect.y + layer->rect.h - text_rect.y;
                }
                
                SDL_RenderCopy(renderer, text_texture, NULL, &text_rect);
                SDL_DestroyTexture(text_texture);
            }
        }
    }
    else if (layer->type==LABEL) {
            if(layer->bgColor.a>0){
                SDL_SetRenderDrawColor(renderer, 
                                    layer->bgColor.r, 
                                    layer->bgColor.g, 
                                    layer->bgColor.b, 
                                    layer->bgColor.a);
                
                SDL_RenderFillRect(renderer, &layer->rect);
            }
        
            SDL_Color text_color = layer->color;
            SDL_Texture* text_texture = render_text(layer->text, text_color);
            
            if (text_texture) {
                int text_width, text_height;
                SDL_QueryTexture(text_texture, NULL, NULL, &text_width, &text_height);
                
                SDL_Rect text_rect = {
                    layer->rect.x + 5 + (layer->rect.w -text_width/ scale)/2,  // 左侧留5像素边距
                    layer->rect.y + (layer->rect.h - text_height/ scale) / 2,
                    text_width / scale,
                    text_height / scale
                };
                
                // 确保文本不会超出输入框边界
                if (text_rect.x + text_rect.w > layer->rect.x + layer->rect.w - 5) {
                    text_rect.w = layer->rect.x + layer->rect.w - 5 - text_rect.x;
                }
                
                if (text_rect.y + text_rect.h > layer->rect.y + layer->rect.h) {
                    text_rect.h = layer->rect.y + layer->rect.h - text_rect.y;
                }
                
                SDL_RenderCopy(renderer, text_texture, NULL, &text_rect);
                SDL_DestroyTexture(text_texture);
            }
    }    
    else if (layer->type == IMAGE) {
        // 图片类型渲染：从文件路径加载并渲染图片（支持多种格式）
        if (strlen(layer->source) > 0 && !layer->texture) {
            // 修改为使用SDL_image支持多种格式
            SDL_Surface* surface = IMG_Load(layer->source);
            if (surface) {
                layer->texture = SDL_CreateTextureFromSurface(renderer, surface);
                SDL_FreeSurface(surface);
            } else {
                printf("Failed to load image %s: %s\n", layer->source, IMG_GetError());
            }
        }
        
        // 渲染图片纹理
        if (layer->texture) {
            // 根据图片模式进行不同的渲染
            if (layer->image_mode == IMAGE_MODE_STRETCH) {
                // 拉伸模式：直接填充整个区域
                SDL_RenderCopy(renderer, layer->texture, NULL, &layer->rect);
            } else {
                // 获取图片原始尺寸
                int img_width, img_height;
                SDL_QueryTexture(layer->texture, NULL, NULL, &img_width, &img_height);
                
                // 计算缩放比例
                float scale_x = (float)layer->rect.w / img_width;
                float scale_y = (float)layer->rect.h / img_height;
                
                SDL_Rect render_rect;
                render_rect.x = layer->rect.x;
                render_rect.y = layer->rect.y;
                render_rect.w = img_width;
                render_rect.h = img_height;
                
                if (layer->image_mode == IMAGE_MODE_ASPECT_FIT) {
                    // 自适应模式：完整显示图片，可能有空白区域
                    float scale = fmin(scale_x, scale_y);
                    render_rect.w = (int)(img_width * scale);
                    render_rect.h = (int)(img_height * scale);
                    render_rect.x = layer->rect.x + (layer->rect.w - render_rect.w) / 2;
                    render_rect.y = layer->rect.y + (layer->rect.h - render_rect.h) / 2;
                } else if (layer->image_mode == IMAGE_MODE_ASPECT_FILL) {
                    // 填充模式：填满整个区域，可能裁剪图片
                    float scale = fmax(scale_x, scale_y);
                    render_rect.w = (int)(img_width * scale);
                    render_rect.h = (int)(img_height * scale);
                    render_rect.x = layer->rect.x + (layer->rect.w - render_rect.w) / 2;
                    render_rect.y = layer->rect.y + (layer->rect.h - render_rect.h) / 2;
                }
                
                SDL_RenderCopy(renderer, layer->texture, NULL, &render_rect);
            }
        } else {
            // 如果图片加载失败，绘制一个占位符
            SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
            SDL_RenderFillRect(renderer, &layer->rect);
            SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
            SDL_RenderDrawRect(renderer, &layer->rect);
        }
    }
    else if ( layer->type==LIST) {
        // List组件渲染：根据数据源动态生成列表项
        // 这里模拟一些数据，实际项目中应该从数据源获取

        int padding_top = layer->layout_manager ? layer->layout_manager->padding[0] : 0;
        int padding_left = layer->layout_manager ? layer->layout_manager->padding[3] : 0;
        int spacing = layer->layout_manager ? layer->layout_manager->spacing : 5;
        
        int current_y = layer->rect.y + padding_top;
        
        // 清理旧的子元素（如果有的话）
        if (layer->children) {
            for (int i = 0; i < layer->child_count; i++) {
                free(layer->children[i]);
            }
            free(layer->children);
            layer->children = NULL;
            layer->child_count = 0;
        }
        
        // 根据数据源和模板动态生成列表项
        if (layer->item_template&& layer->data) {
            int item_count = layer->data->size;
            //json 数据 todo 从接口获取
            layer->child_count = item_count;
            layer->children = malloc(item_count * sizeof(Layer*));
            
            for (int i = 0; i < item_count; i++) {
                // 创建基于模板的新项
                layer->children[i] = malloc(sizeof(Layer));
                memcpy(layer->children[i], layer->item_template, sizeof(Layer));
                
                // 设置位置和尺寸
                layer->children[i]->rect.x = layer->rect.x + padding_left;
                layer->children[i]->rect.y = current_y;
                layer->children[i]->rect.w = layer->rect.w - (padding_left * 2);
                layer->children[i]->rect.h = 30; // 固定高度
                
                // 替换文本中的占位符
                char temp_text[100];
                strcpy(temp_text, layer->children[i]->text);
                // 简单替换${name}为实际数据
                if (strstr(temp_text, "${name}")) {
                    cJSON* item=cJSON_GetArrayItem(layer->data->json,i);
                    if(item&& item->valuestring){
                        
                        strcpy(layer->children[i]->text, item->valuestring);
                    }
                }
                
                current_y += layer->children[i]->rect.h + spacing;
            }
        }
        
        // 绘制背景
        if(layer->bgColor.a > 0) {
            SDL_SetRenderDrawColor(renderer, 
                                layer->bgColor.r, 
                                layer->bgColor.g, 
                                layer->bgColor.b, 
                                layer->bgColor.a);
            SDL_RenderFillRect(renderer, &layer->rect);
        }
        
        // 递归渲染子图层（列表项）
        for (int i = 0; i < layer->child_count; i++) {
            render_layer(layer->children[i]);
        }
    }
    else {
        //printf("layer->%s %d\n",layer->id,layer->type);

        // 默认渲染方式
        SDL_SetRenderDrawColor(renderer, 
                              layer->color.r, 
                              layer->color.g, 
                              layer->color.b, 
                              layer->color.a);
        SDL_RenderFillRect(renderer, &layer->rect);
    }

    // 递归渲染子图层
    for (int i = 0; i < layer->child_count; i++) {
        render_layer(layer->children[i]);
    }

       // 递归渲染子图层
    if(layer->sub!=NULL){
        render_layer(layer->sub);
    }
    
}

// ====================== 事件处理器 ======================
void handle_event(Layer* root, SDL_Event* event) {
    if (event->type == SDL_MOUSEBUTTONDOWN) {
        SDL_Point mouse_pos = { event->button.x, event->button.y };
        
        // 检测点击图层 (实际应使用空间分割优化)
        if (SDL_PointInRect(&mouse_pos, &root->rect)) {
            if (root->onClick) root->onClick();
            
            for (int i = 0; i < root->child_count; i++) {
                handle_event(root->children[i], event);
            }
        }
    }
}

// 检查是否Retina显示屏并获取缩放因子
float getDisplayScale(SDL_Window* window) {
    int renderW, renderH;
    int windowW, windowH;
    
    SDL_GetWindowSize(window, &windowW, &windowH);
    SDL_GL_GetDrawableSize(window, &renderW, &renderH);
    
    return (renderW) / windowW;
}

void load_font(Layer* root){
    // 加载默认字体 (需要在项目目录下提供字体文件)
    TTF_Font* default_font = TTF_OpenFont(root->font->path, 16*scale);
    if (!default_font) {
        printf("Warning: Could not load font 'Roboto-Regular.ttf', trying other fonts\n");
        // 尝试加载其他西文字体
        default_font = TTF_OpenFont("arial.ttf", 16*scale);
        if (!default_font) {
            default_font = TTF_OpenFont("Arial.ttf", 16*scale);
        }
        if (!default_font) {
            default_font = TTF_OpenFont("assets/Roboto-Regular.ttf", 16*scale);
        }
        if (!default_font) {
            default_font = TTF_OpenFont("app/assets/Roboto-Regular.ttf", 16*scale);
        }
    }
    TTF_SetFontHinting(default_font, TTF_HINTING_LIGHT); 
    TTF_SetFontOutline(default_font, 0); // 无轮廓

    root->font->default_font=default_font;
}

// ====================== 主入口 ======================
int main(int argc, char* argv[]) {
    // 初始化SDL
    SDL_Init(SDL_INIT_VIDEO);
    
    // 初始化SDL_image库，支持多种图片格式
    int imgFlags = IMG_INIT_PNG | IMG_INIT_JPG | IMG_INIT_TIF;
    if (!(IMG_Init(imgFlags) & imgFlags)) {
        printf("SDL_image initialization failed: %s\n", IMG_GetError());
        return -1;
    }
    
    // 初始化TTF
    if (TTF_Init() == -1) {
        printf("TTF initialization failed: %s\n", TTF_GetError());
        return -1;
    }
    
    SDL_Window* window = SDL_CreateWindow("YUI Renderer", 
                                        SDL_WINDOWPOS_CENTERED,
                                        SDL_WINDOWPOS_CENTERED,
                                        800, 600, SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_SHOWN);
    renderer = SDL_CreateRenderer(window, -1, 
                                 SDL_RENDERER_ACCELERATED);

    scale = getDisplayScale(window);

    SDL_RenderSetScale(renderer, scale, scale);
    
    
    char* json_path="app.json";
    // 加载UI描述文件
    if(argc>1){
        json_path=argv[1];
    }
    cJSON* root_json=parse_json(json_path);
    Layer* ui_root = parse_layer(root_json);

    
    
    // 如果根图层没有设置宽度和高度，则根据窗口大小设置
    printf("ui_root %d,%d\n",ui_root->rect.w,ui_root->rect.h);
    
    if (ui_root->rect.w <= 0 || ui_root->rect.h <= 0) {
        int window_width, window_height;
        SDL_GetWindowSize(window, &window_width, &window_height);
        if (ui_root->rect.w <= 0) {
            ui_root->rect.w = window_width;
        }
        if (ui_root->rect.h <= 0) {
            ui_root->rect.h = window_height;
        }
    }
    
    cJSON_Delete(root_json);
  
    
    // 加载纹理资源
    load_font(ui_root);
    load_textures(ui_root);
    layout_layer(ui_root);
    
    // 主循环
    SDL_Event event;
    int running = 1;
    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = 0;
            handle_event(ui_root, &event);
        }
        
        SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
        SDL_RenderClear(renderer);
        
        render_layer(ui_root);  // 执行渲染管线
        
        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }
    
    // 清理资源
    IMG_Quit();
    if (default_font) {
        TTF_CloseFont(default_font);
    }
    TTF_Quit();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}