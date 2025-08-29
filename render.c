#include "layer.h"
#include "render.h"
#include <limits.h>

// ====================== 全局渲染器 ======================
SDL_Renderer* renderer = NULL;
float scale=1.0;

int is_cjson_float(const cJSON *item) {
    if (item == NULL || !cJSON_IsNumber(item)) {
        return 0;
    }
    
    // 获取双精度值
    double value = item->valuedouble;
    
    // 检查是否为有限数（非无穷大和非NaN）
    if (!isfinite(value)) {
        return 0;
    }
    
    // 检查值是否超出整数范围
    if (value > INT_MAX || value < INT_MIN) {
        return 1; // 超出整数范围，肯定是浮点数
    }
    
    // 检查值是否包含小数部分
    double int_part;
    double frac_part = modf(value, &int_part);
    
    // 如果小数部分的绝对值大于一个很小的容差值，则是浮点数
    if (fabs(frac_part) > DBL_EPSILON) {
        return 1;
    }
    
    return 0; // 是整数
}

// 替换字符串中的占位符
char* replace_placeholder(const char* template, const char* placeholder, const char* value) {
    // 查找占位符位置
    char* pos = strstr(template, placeholder);
    if (pos == NULL) return NULL;
    
    // 计算新字符串长度
    size_t template_len = strlen(template);
    size_t placeholder_len = strlen(placeholder);
    size_t value_len = strlen(value);
    size_t new_len = template_len - placeholder_len + value_len + 1;
    
    // 分配内存
    char* result = (char*)malloc(new_len);
    if (result == NULL) return NULL;
    
    // 复制第一部分
    size_t prefix_len = pos - template;
    strncpy(result, template, prefix_len);
    result[prefix_len] = '\0';
    
    // 追加替换值
    strcat(result, value);
    
    // 追加剩余部分
    strcat(result, pos + placeholder_len);
    
    return result;
}

// 替换所有出现的占位符
char* replace_all_placeholders(const char* template, const char* placeholder, const char* value) {
    char* result = strdup(template);
    if (result == NULL) return NULL;
    
    char* pos = strstr(result, placeholder);
    while (pos != NULL) {
        // 计算新字符串长度
        size_t result_len = strlen(result);
        size_t placeholder_len = strlen(placeholder);
        size_t value_len = strlen(value);
        size_t new_len = result_len - placeholder_len + value_len + 1;
        
        // 分配临时内存
        char* temp = (char*)malloc(new_len);
        if (temp == NULL) {
            free(result);
            return NULL;
        }
        
        // 复制第一部分
        size_t prefix_len = pos - result;
        strncpy(temp, result, prefix_len);
        temp[prefix_len] = '\0';
        
        // 追加替换值
        strcat(temp, value);
        
        // 追加剩余部分
        strcat(temp, pos + placeholder_len);
        
        // 释放旧结果，使用新结果
        free(result);
        result = temp;
        
        // 查找下一个占位符
        pos = strstr(result, placeholder);
    }
    
    return result;
}

// ====================== 资源加载器 ======================
void load_textures(Layer* root) {
    if (root->type==IMAGE&& strlen(root->source) > 0) {
        // 修改为使用SDL_image支持多种格式
        char path[MAX_PATH];
        snprintf(path, sizeof(path), "%s/%s", root->assets->path, root->source);

        SDL_Surface* surface = IMG_Load(path);
        if (surface) {
            root->texture = SDL_CreateTextureFromSurface(renderer, surface);
            SDL_FreeSurface(surface);
        } else {
            printf("Failed to load image %s: %s\n", root->source, IMG_GetError());
        }
    }
}

// 添加文字渲染函数
SDL_Texture* render_text(Layer* layer,const char* text, SDL_Color color) {
    if(layer->font==NULL){
        printf("error not found font %s %d\n",layer->id,layer->type);
        return NULL;
    }
    if (!layer->font->default_font) return NULL;
    
    SDL_Surface* surface = TTF_RenderUTF8_Blended(layer->font->default_font, text, color);
    if (!surface) return NULL;
    
    
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_SetTextureScaleMode(texture, SDL_ScaleModeBest);

    SDL_FreeSurface(surface);
    
    return texture;
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
            SDL_Texture* text_texture = render_text(layer,layer->text, text_color);
            
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
            SDL_Texture* text_texture = render_text(layer,layer->label, text_color);
            
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
            SDL_Texture* text_texture = render_text(layer,layer->text, text_color);
            
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
            load_textures(layer);
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

        // 绘制背景
        if(layer->bgColor.a > 0) {
            SDL_SetRenderDrawColor(renderer, 
                                layer->bgColor.r, 
                                layer->bgColor.g, 
                                layer->bgColor.b, 
                                layer->bgColor.a);
            SDL_RenderFillRect(renderer, &layer->rect);
        }
        
        int padding_top = layer->layout_manager ? layer->layout_manager->padding[0] : 0;
        int padding_left = layer->layout_manager ? layer->layout_manager->padding[3] : 0;
        int spacing = layer->layout_manager ? layer->layout_manager->spacing : 5;
        
        int current_y = layer->rect.y + padding_top;

        if (layer->scrollable) {
            current_y -= layer->scroll_offset;
        }

        //printf("layer %d %s %s %d,%d\n",layer->type,layer->id,layer->text,layer->rect.x,layer->rect.y);
        
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
                
                // 简单替换${name}为实际数据
                if (strstr(layer->children[i]->text, "${")) {
                    cJSON* item=cJSON_GetArrayItem(layer->data->json,i);
             
                    cJSON* it=item->child;
                    char name[256];
                    char val[256];

                    while(it!=NULL){
                        sprintf(name,"${%s}",it->string);
                        if(cJSON_IsString(it)){
                            sprintf(val,"%s",it->valuestring);
                        }else if(cJSON_IsNumber(it)){
                            if(is_cjson_float(it)){
                                sprintf(val,"%f",it->valuedouble);
                            }else{
                                sprintf(val,"%d",it->valueint);
                            }
                            
                        }

                        char* result = replace_placeholder(layer->children[i]->text, name, val);
                        if(result){
                            strcpy(layer->children[i]->text, result);
                            free(result);
                        }
                        it=it->next;
                    }
                    
                }
                
                current_y += layer->children[i]->rect.h + spacing;
            }
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

    // 保存当前渲染目标的裁剪区域
    SDL_Rect prev_clip;
    render_clip_start(layer,&prev_clip);

    // 递归渲染子图层
    for (int i = 0; i < layer->child_count; i++) {
        render_layer(layer->children[i]);
    }

    // 递归渲染子图层
    if(layer->sub!=NULL){
        render_layer(layer->sub);
    }

    // 渲染滚动条
    if (layer->scrollable && layer->scrollbar && layer->scrollbar->visible) {
       render_scrollbar(layer);
    }
    
    render_clip_end(layer,&prev_clip);


}

void render_clip_start(Layer* layer,SDL_Rect* prev_clip){
    SDL_RenderGetClipRect(renderer, prev_clip);
    // 设置当前图层的裁剪区域
    SDL_Rect clip_rect = layer->rect;
    
    // 如果存在父级裁剪区域，则取交集作为最终裁剪区域
    if (prev_clip->w > 0 && prev_clip->h > 0) {
        // 计算两个矩形的交集
        int left = fmax(clip_rect.x, prev_clip->x);
        int top = fmax(clip_rect.y, prev_clip->y);
        int right = fmin(clip_rect.x + clip_rect.w, prev_clip->x + prev_clip->w);
        int bottom = fmin(clip_rect.y + clip_rect.h, prev_clip->y + prev_clip->h);
        
        if (left < right && top < bottom) {
            clip_rect.x = left;
            clip_rect.y = top;
            clip_rect.w = right - left;
            clip_rect.h = bottom - top;
        } else {
            // 没有交集，设置一个空的裁剪区域
            clip_rect.w = 0;
            clip_rect.h = 0;
        }
    }
    
    // 应用裁剪区域
    SDL_RenderSetClipRect(renderer, &clip_rect);
}

void render_clip_end(Layer* layer,SDL_Rect* prev_clip){
    // 恢复之前的裁剪区域
    SDL_RenderSetClipRect(renderer, prev_clip);
    
}

void render_scrollbar(Layer* layer){
    int spacing = layer->layout_manager ? layer->layout_manager->spacing : 5;
    // 计算内容总高度
    int content_height = 0;
    for (int i = 0; i < layer->child_count; i++) {
        content_height += layer->children[i]->rect.h;
        if (i > 0) content_height += spacing;
    }
    
    int visible_height = layer->rect.h - layer->layout_manager->padding[0] - layer->layout_manager->padding[2];
    
    // 只有当内容高度超过可见高度时才显示滚动条
    if (content_height > visible_height) {
        // 计算滚动条尺寸和位置
        int scrollbar_width = layer->scrollbar->thickness > 0 ? layer->scrollbar->thickness : 8;
        int scrollbar_height = (int)((float)visible_height / content_height * visible_height);
        if (scrollbar_height < 20) scrollbar_height = 20; // 最小高度
        
        int scrollbar_x = layer->rect.x + layer->rect.w - scrollbar_width;
        int scrollbar_y = layer->rect.y + (int)((float)layer->scroll_offset / (content_height - visible_height) * (visible_height - scrollbar_height));
        
        // 确保滚动条位置在有效范围内
        if (scrollbar_y < layer->rect.y) scrollbar_y = layer->rect.y;
        if (scrollbar_y > layer->rect.y + visible_height - scrollbar_height) 
            scrollbar_y = layer->rect.y + visible_height - scrollbar_height;
        
        SDL_Rect scrollbar_rect = {scrollbar_x, scrollbar_y, scrollbar_width, scrollbar_height};
        
        // 绘制滚动条
        SDL_SetRenderDrawColor(renderer, 
                                layer->scrollbar->color.r,
                                layer->scrollbar->color.g,
                                layer->scrollbar->color.b,
                                layer->scrollbar->color.a);
        SDL_RenderFillRect(renderer, &scrollbar_rect);
    }
}