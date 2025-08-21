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
    LAYOUT_VERTICAL
} LayoutType;

typedef struct LayoutManager {
    LayoutType type;
    int spacing;
    int padding[4]; // top, right, bottom, left
} LayoutManager;

typedef struct Layer {
    char id[50];
    char type[20];
    SDL_Rect rect;
    SDL_Color color;
    SDL_Color bgColor;
    char texture_path[100];
    SDL_Texture* texture;
    struct Layer** children;
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
} Layer;

// ====================== 全局渲染器 ======================
SDL_Renderer* renderer = NULL;
TTF_Font* default_font = NULL;  // 添加默认字体

// ====================== JSON解析函数 ======================
Layer* parse_layer(cJSON* json_obj) {
    Layer* layer = malloc(sizeof(Layer));
    memset(layer, 0, sizeof(Layer));
    
    // 解析基础属性
    strcpy(layer->id, cJSON_GetObjectItem(json_obj, "id")->valuestring);
    strcpy(layer->type, cJSON_GetObjectItem(json_obj, "type")->valuestring);
    
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
            } else {
                layer->layout_manager->type = LAYOUT_ABSOLUTE;
            }
        } else {
            // 默认使用垂直布局
            layer->layout_manager->type = LAYOUT_VERTICAL;
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
       
    }
    
    // 解析资源路径
    if (cJSON_HasObjectItem(json_obj, "source")) {
        strcpy(layer->texture_path, 
              cJSON_GetObjectItem(json_obj, "source")->valuestring);
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
    if (strlen(root->texture_path) > 0) {
        // 修改为使用SDL_image支持多种格式
        SDL_Surface* surface = IMG_Load(root->texture_path);
        if (surface) {
            root->texture = SDL_CreateTextureFromSurface(renderer, surface);
            SDL_FreeSurface(surface);
        } else {
            printf("Failed to load image %s: %s\n", root->texture_path, IMG_GetError());
        }
    }
    
    for (int i = 0; i < root->child_count; i++) {
        load_textures(root->children[i]);
    }
}

// 添加文字渲染函数
SDL_Texture* render_text(const char* text, SDL_Color color) {
    if (!default_font) return NULL;
    
    SDL_Surface* surface = TTF_RenderUTF8_Blended(default_font, text, color);
    if (!surface) return NULL;
    
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    
    return texture;
}

// ====================== 渲染管线 ======================
void render_layer(Layer* layer) {
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
                child->rect.w = content_width;
                
                current_y += child->rect.h + spacing;
            }
        }
    }
    
    // 根据图层类型进行不同的渲染处理
    if (strcmp(layer->type, "Button") == 0) {
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
                    layer->rect.x + (layer->rect.w - text_width) / 2,  // 居中
                    layer->rect.y + (layer->rect.h - text_height) / 2,
                    text_width,
                    text_height
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
    else if (strcmp(layer->type, "Input") == 0) {
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
                    layer->rect.y + (layer->rect.h - text_height) / 2,
                    text_width,
                    text_height
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
    else if (strcmp(layer->type, "Label") == 0) {
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
                    layer->rect.x + 5 + (layer->rect.w -text_width)/2,  // 左侧留5像素边距
                    layer->rect.y + (layer->rect.h - text_height) / 2,
                    text_width,
                    text_height
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
    else if (strcmp(layer->type, "Image") == 0) {
        // 图片类型渲染：从文件路径加载并渲染图片（支持多种格式）
        if (strlen(layer->texture_path) > 0 && !layer->texture) {
            // 修改为使用SDL_image支持多种格式
            SDL_Surface* surface = IMG_Load(layer->texture_path);
            if (surface) {
                layer->texture = SDL_CreateTextureFromSurface(renderer, surface);
                SDL_FreeSurface(surface);
            } else {
                printf("Failed to load image %s: %s\n", layer->texture_path, IMG_GetError());
            }
        }
        
        // 渲染图片纹理
        if (layer->texture) {
            SDL_RenderCopy(renderer, layer->texture, NULL, &layer->rect);
        } else {
            // 如果图片加载失败，绘制一个占位符
            SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
            SDL_RenderFillRect(renderer, &layer->rect);
            SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
            SDL_RenderDrawRect(renderer, &layer->rect);
        }
    }
    else {
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
                                        800, 600, 0);
    renderer = SDL_CreateRenderer(window, -1, 
                                 SDL_RENDERER_ACCELERATED);
    
    // 加载默认字体 (需要在项目目录下提供字体文件)
    default_font = TTF_OpenFont("Roboto-Regular.ttf", 16);
    if (!default_font) {
        printf("Warning: Could not load font 'Roboto-Regular.ttf', trying other fonts\n");
        // 尝试加载其他西文字体
        default_font = TTF_OpenFont("arial.ttf", 16);
        if (!default_font) {
            default_font = TTF_OpenFont("Arial.ttf", 16);
        }
    }

    
    char* json_path="ui_layout.json";
    // 加载UI描述文件
    if(argc>1){
        json_path=argv[1];
    }
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
    Layer* ui_root = parse_layer(root_json);
    
    // 如果根图层没有设置宽度和高度，则根据窗口大小设置
    printf("ui_root%d\n",ui_root->rect.w);
    
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
    free(json_str);
    
    // 加载纹理资源
    load_textures(ui_root);
    
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