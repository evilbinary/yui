#include "layer.h"
#include "animate.h"

// 更新图层类型名称数组，添加GRID、Text、Tab、Slider和Listbox
char *layer_type_name[]={"View","Button","Input","Label","Image","List","Grid","Progress","Checkbox","Radiobox","Text","Treeview","Tab","Slider","Listbox","Scrollbar"};

Layer* focused_layer=NULL;

// 移除JSON字符串中的注释
static char* remove_json_comments(char* json_str) {
    if (!json_str) return NULL;
    
    size_t len = strlen(json_str);
    char* result = malloc(len + 1);
    if (!result) return NULL;
    
    int in_string = 0;     // 是否在字符串内
    int in_comment = 0;    // 0: 不在注释内, 1: 单行注释, 2: 多行注释
    size_t j = 0;          // 结果字符串的索引
    
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

cJSON* parse_json(char* json_path){
    FILE* file = fopen(json_path, "r");
    if(!file){
        printf("open file failed %s\n",json_path);
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
    
    // 释放内存
    free(json_str);
    free(cleaned_json);
    
    return root_json;
}

void parse_color(char* valuestring,Color* color){
    // 支持多种颜色格式
    if (strncmp(valuestring, "rgba(", 5) == 0) {
        // 解析 rgba(r, g, b, a) 格式
        int r, g, b;
        float a;
        sscanf(valuestring, "rgba(%d,%d,%d,%f)", &r, &g, &b, &a);
        color->r = (unsigned char)r;
        color->g = (unsigned char)g;
        color->b = (unsigned char)b;
        color->a = (unsigned char)(a * 255); // 将0.0-1.0的范围转换为0-255
    } else if (strncmp(valuestring, "rgb(", 4) == 0) {
        // 解析 rgb(r, g, b) 格式
        int r, g, b;
        sscanf(valuestring, "rgb(%d,%d,%d)", &r, &g, &b);
        color->r = (unsigned char)r;
        color->g = (unsigned char)g;
        color->b = (unsigned char)b;
        color->a = 255; // 默认不透明
    } else if (strlen(valuestring) == 9) {
        // 解析十六进制 #RRGGBBAA 格式
        sscanf(valuestring, "#%02hhx%02hhx%02hhx%02hhx", 
            &color->r, &color->g, &color->b, &color->a);
    } else {
        // 解析十六进制 #RRGGBB 格式
        sscanf(valuestring, "#%02hhx%02hhx%02hhx", 
            &color->r, &color->g, &color->b);
        color->a = 255; // 默认不透明
    }
}

// ====================== JSON解析函数 ======================
Layer* parse_layer(cJSON* json_obj,Layer* parent) {
    if(json_obj==NULL){
        return NULL;
    }
    Layer* layer = malloc(sizeof(Layer));
    memset(layer, 0, sizeof(Layer));
    layer->parent=parent;
    
    // 初始化焦点相关字段
    layer->state = LAYER_STATE_NORMAL;  // 默认处于正常状态
    layer->focusable = 0;  // 默认不可获得焦点
    
    // 用于标记是否已经自定义处理了子图层（如SCROLLBAR类型）
    int has_custom_children = 0;
    
    // 解析基础属性
    if(cJSON_HasObjectItem(json_obj, "id")){
        strcpy(layer->id, cJSON_GetObjectItem(json_obj, "id")->valuestring);
    }
    if(cJSON_HasObjectItem(json_obj, "type")){
        for(int i=0;i<LAYER_TYPE_SIZE;i++ ){
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

        cJSON* fontSize = cJSON_GetObjectItem(json_obj, "fontSize");
        if(fontSize!=NULL){
             layer->font->size=cJSON_GetObjectItem(json_obj, "fontSize")->valueint;
        }
    }else{
        if(parent){
            layer->font=parent->font;
        }
    }
    

    // 解析资源
    cJSON* assets = cJSON_GetObjectItem(json_obj, "assets");
    if(assets!=NULL){
        layer->assets= malloc(sizeof(Assets));
        strcpy(layer->assets->path, 
              cJSON_GetObjectItem(json_obj, "assets")->valuestring);
    }else{
        if(parent){
            layer->assets=parent->assets;
        }
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
        if(cJSON_GetArraySize(size)>1){
            layer->rect.h = cJSON_GetArrayItem(size, 1)->valueint;
        }else{
            layer->rect.h=layer->rect.w;
        }
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
        layer->data->size=cJSON_GetArraySize(data);
    }
    //禁用
    cJSON* disabled = cJSON_GetObjectItem(json_obj, "disabled");
    if (data) {
        if(cJSON_IsTrue(disabled)){
                SET_STATE(layer,LAYER_STATE_DISABLED);
        }
    }

    //数据解析
    cJSON* rotation = cJSON_GetObjectItem(json_obj, "rotation");
    if (rotation) {
        layer->rotation =rotation->valueint;
    }
    
    // 解析列表项模板
    cJSON* item_template = cJSON_GetObjectItem(json_obj, "itemTemplate");
    if (item_template) {
        layer->item_template = parse_layer(item_template,parent);
    }

    
    
    cJSON* scrollbar = cJSON_GetObjectItem(json_obj, "scrollbar");
    if (scrollbar) {
            layer->scrollbar = malloc(sizeof(Scrollbar));
            memset(layer->scrollbar, 0, sizeof(Scrollbar));
        
        cJSON* visible = cJSON_GetObjectItem(scrollbar, "visible");
        if (visible) {
            layer->scrollbar->visible = visible->valueint;
        }
        
        cJSON* thickness = cJSON_GetObjectItem(scrollbar, "thickness");
        if (thickness) {
            layer->scrollbar->thickness = thickness->valueint;
        }
        
        cJSON* color = cJSON_GetObjectItem(scrollbar, "color");
        if (color) {
            parse_color(color->valuestring,&layer->scrollbar->color);
        }
        // 解析滚动属性
        if (cJSON_HasObjectItem(scrollbar, "scrollable")) {
            layer->scrollable = cJSON_GetObjectItem(scrollbar, "scrollable")->valueint;
        }
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
        
        // 解析Grid布局的列数
        cJSON* columns = cJSON_GetObjectItem(layout, "columns");
        if (columns) {
            layer->layout_manager->columns = columns->valueint;
        }
    } else {
        // 如果没有布局配置，创建默认的垂直布局
        layer->layout_manager = malloc(sizeof(LayoutManager));
        memset(layer->layout_manager, 0, sizeof(LayoutManager));
        layer->layout_manager->type = LAYOUT_VERTICAL;
    }
    
    //默认背景颜色
    if(layer->bg_color.a == 0){
        layer->bg_color.r=0xF5;
        layer->bg_color.g=0xF5;
        layer->bg_color.b=0xF5;
        layer->bg_color.a = 0;
    }
    // 解析样式
    cJSON* style = cJSON_GetObjectItem(json_obj, "style");
    if (style) {
        if(cJSON_HasObjectItem(style,"color")){
            parse_color(cJSON_GetObjectItem(style, "color")->valuestring,&layer->color);
        }

        if(cJSON_HasObjectItem(style,"bgColor")){
            parse_color(cJSON_GetObjectItem(style, "bgColor")->valuestring,&layer->bg_color);
        }
        
        // 解析圆角半径属性
        if(cJSON_HasObjectItem(style,"radius")){
            layer->radius = cJSON_GetObjectItem(style, "radius")->valueint;
        }else{
            layer->radius = 0; // 默认没有圆角
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
                layer->blur_radius = 10; // 默认模糊半径
            }
            
            // 解析饱和度
            cJSON* saturation = cJSON_GetObjectItem(style, "saturation");
            if (saturation) {
                layer->saturation = (float)saturation->valuedouble;
            } else {
                layer->saturation = 1.0f; // 默认饱和度
            }
            
            // 解析亮度
            cJSON* brightness = cJSON_GetObjectItem(style, "brightness");
            if (brightness) {
                layer->brightness = (float)brightness->valuedouble;
            } else {
                layer->brightness = 1.0f; // 默认亮度
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
                layer->sub=parse_layer(sub,layer);
            }else{
                printf("cannot load file %s\n",source->valuestring);
            }
            
        }
    }    
    
    // 解析事件绑定
    cJSON* events = cJSON_GetObjectItem(json_obj, "events");
    if(events){
        if (cJSON_HasObjectItem(events, "onClick")) {
            layer->event=malloc( sizeof(Event));
            const char* handler_id = cJSON_GetObjectItem(events, "onClick")->valuestring;
            // 实际项目应建立handler_id到函数的映射表
            strcpy(layer->event->click_name,handler_id);

            EventHandler handler=find_event_by_name(handler_id);
            layer->event->click = handler;
        }
        // 解析滚动事件
        if (cJSON_HasObjectItem(events, "onScroll")) {
            if (!layer->event) {
                layer->event = malloc(sizeof(Event));
                memset(layer->event, 0, sizeof(Event));
            }
            const char* handler_id = cJSON_GetObjectItem(events, "onScroll")->valuestring;
            strcpy(layer->event->scroll_name, handler_id);
            
            EventHandler handler = find_event_by_name(handler_id);
            layer->event->scroll = handler;
        }
        // 解析统一触屏事件
        if (cJSON_HasObjectItem(events, "onTouch")) {
            if (!layer->event) {
                layer->event = malloc(sizeof(Event));
                memset(layer->event, 0, sizeof(Event));
            }
            const char* handler_id = cJSON_GetObjectItem(events, "onTouch")->valuestring;
            strcpy(layer->event->touch_name, handler_id);
            
            // 查找事件处理函数
            EventHandler handler = find_event_by_name(handler_id);
            if (handler) {
                // 由于EventHandler和touch函数签名不同，这里需要进行类型转换
                layer->event->touch = (void (*)(TouchEvent*))handler;
            }
        }
    }
    
    // 解析动画属性配置
    cJSON* animation = cJSON_GetObjectItem(json_obj, "animation");
    if (animation) {
        // 解析持续时间
        float duration = 1.0f; // 默认1秒
        if (cJSON_HasObjectItem(animation, "duration")) {
            duration = (float)cJSON_GetObjectItem(animation, "duration")->valuedouble;
        }
        
        // 解析缓动函数
        float (*easing_func)(float) = ease_in_out_quad; // 默认缓入缓出
        if (cJSON_HasObjectItem(animation, "easing")) {
            const char* easing_str = cJSON_GetObjectItem(animation, "easing")->valuestring;
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
                const char* fill_mode_str = cJSON_GetObjectItem(animation, "fillMode")->valuestring;
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
                const char* repeat_type_str = cJSON_GetObjectItem(animation, "repeatType")->valuestring;
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
                animation_set_repeat_count(anim, cJSON_GetObjectItem(animation, "repeatCount")->valueint);
            }
            
            // 反向重复标志
            if (cJSON_HasObjectItem(animation, "reverseOnRepeat")) {
                animation_set_reverse_on_repeat(anim, cJSON_IsTrue(cJSON_GetObjectItem(animation, "reverseOnRepeat")));
            }
            
            // 解析目标属性
            // 位置
            if (cJSON_HasObjectItem(animation, "x")) {
                animation_set_target(anim, ANIMATION_PROPERTY_X, (float)cJSON_GetObjectItem(animation, "x")->valuedouble);
            }
            if (cJSON_HasObjectItem(animation, "y")) {
                animation_set_target(anim, ANIMATION_PROPERTY_Y, (float)cJSON_GetObjectItem(animation, "y")->valuedouble);
            }
            
            // 大小
            if (cJSON_HasObjectItem(animation, "width")) {
                animation_set_target(anim, ANIMATION_PROPERTY_WIDTH, (float)cJSON_GetObjectItem(animation, "width")->valuedouble);
            }
            if (cJSON_HasObjectItem(animation, "height")) {
                animation_set_target(anim, ANIMATION_PROPERTY_HEIGHT, (float)cJSON_GetObjectItem(animation, "height")->valuedouble);
            }
            
            // 透明度
            if (cJSON_HasObjectItem(animation, "opacity")) {
                animation_set_target(anim, ANIMATION_PROPERTY_OPACITY, (float)cJSON_GetObjectItem(animation, "opacity")->valuedouble);
            }
            
            // 旋转
            if (cJSON_HasObjectItem(animation, "rotation")) {
                animation_set_target(anim, ANIMATION_PROPERTY_ROTATION, (float)cJSON_GetObjectItem(animation, "rotation")->valuedouble);
            }
            
            // 缩放
            if (cJSON_HasObjectItem(animation, "scaleX")) {
                animation_set_target(anim, ANIMATION_PROPERTY_SCALE_X, (float)cJSON_GetObjectItem(animation, "scaleX")->valuedouble);
            }
            if (cJSON_HasObjectItem(animation, "scaleY")) {
                animation_set_target(anim, ANIMATION_PROPERTY_SCALE_Y, (float)cJSON_GetObjectItem(animation, "scaleY")->valuedouble);
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
    
    // 如果是INPUT类型的图层，初始化InputComponent
    if (layer->type == INPUT) {
        layer->component = input_component_create_from_json(layer,json_obj);
    
    }else if(layer->type==BUTTON){
        layer->component = button_component_create(layer);

    }else if(layer->type==LABEL){
        layer->component = label_component_create(layer);
        
    }else if(layer->type==IMAGE){
        layer->component = image_component_create(layer);
        
    }else if(layer->type==PROGRESS){
        layer->component = progress_component_create(layer);
    
    }else if(layer->type==CHECKBOX){
        layer->component = checkbox_component_create_from_json(layer, json_obj);
        
    }else if(layer->type==RADIOBOX){
        layer->component = radiobox_component_create_from_json(layer,json_obj);
        
    } else if(layer->type==TEXT){
        layer->component = text_component_create_from_json(layer,json_obj);
        
        // 设置可获得焦点
        layer->focusable = 1;
    } else if(layer->type==TAB){
        layer->component = tab_component_create(layer);
        
        // 解析选项卡特定属性
        cJSON* tabConfig = cJSON_GetObjectItem(json_obj, "tabConfig");
        if (tabConfig) {
            TabComponent* tabComponent = (TabComponent*)layer->component;
            
            if (cJSON_HasObjectItem(tabConfig, "tabHeight")) {
                tab_component_set_tab_height(tabComponent, cJSON_GetObjectItem(tabConfig, "tabHeight")->valueint);
            }
            
            if (cJSON_HasObjectItem(tabConfig, "closable")) {
                tab_component_set_closable(tabComponent, cJSON_IsTrue(cJSON_GetObjectItem(tabConfig, "closable")));
            }
            
            // 解析选项卡颜色
            cJSON* colors = cJSON_GetObjectItem(tabConfig, "colors");
            if (colors) {
                Color tabColor = {240, 240, 240, 255};
                Color activeTabColor = {255, 255, 255, 255};
                Color textColor = {0, 0, 0, 255};
                Color activeTextColor = {0, 0, 0, 255};
                Color borderColor = {200, 200, 200, 255};
                
                if (cJSON_HasObjectItem(colors, "tabColor")) {
                    parse_color(cJSON_GetObjectItem(colors, "tabColor")->valuestring, &tabColor);
                }
                if (cJSON_HasObjectItem(colors, "activeTabColor")) {
                    parse_color(cJSON_GetObjectItem(colors, "activeTabColor")->valuestring, &activeTabColor);
                }
                if (cJSON_HasObjectItem(colors, "textColor")) {
                    parse_color(cJSON_GetObjectItem(colors, "textColor")->valuestring, &textColor);
                }
                if (cJSON_HasObjectItem(colors, "activeTextColor")) {
                    parse_color(cJSON_GetObjectItem(colors, "activeTextColor")->valuestring, &activeTextColor);
                }
                if (cJSON_HasObjectItem(colors, "borderColor")) {
                    parse_color(cJSON_GetObjectItem(colors, "borderColor")->valuestring, &borderColor);
                }
                
                tab_component_set_colors(tabComponent, tabColor, activeTabColor, textColor, activeTextColor, borderColor);
            }
        }
    } else if(layer->type==SLIDER){
        layer->component = slider_component_create(layer);
        
        // 解析滑块特定属性
        cJSON* sliderConfig = cJSON_GetObjectItem(json_obj, "sliderConfig");
        if (sliderConfig) {
            SliderComponent* sliderComponent = (SliderComponent*)layer->component;
            
            if (cJSON_HasObjectItem(sliderConfig, "min")) {
                float min = (float)cJSON_GetObjectItem(sliderConfig, "min")->valuedouble;
                float max = 100.0f;
                if (cJSON_HasObjectItem(sliderConfig, "max")) {
                    max = (float)cJSON_GetObjectItem(sliderConfig, "max")->valuedouble;
                }
                slider_component_set_range(sliderComponent, min, max);
            }
            
            if (cJSON_HasObjectItem(sliderConfig, "value")) {
                slider_component_set_value(sliderComponent, (float)cJSON_GetObjectItem(sliderConfig, "value")->valuedouble);
            }
            
            if (cJSON_HasObjectItem(sliderConfig, "step")) {
                slider_component_set_step(sliderComponent, (float)cJSON_GetObjectItem(sliderConfig, "step")->valuedouble);
            }
            
            if (cJSON_HasObjectItem(sliderConfig, "orientation")) {
                if (strcmp(cJSON_GetObjectItem(sliderConfig, "orientation")->valuestring, "vertical") == 0) {
                    slider_component_set_orientation(sliderComponent, SLIDER_ORIENTATION_VERTICAL);
                }
            }
            
            // 解析滑块颜色
            cJSON* colors = cJSON_GetObjectItem(sliderConfig, "colors");
            if (colors) {
                Color trackColor = {200, 200, 200, 255};
                Color thumbColor = {100, 100, 100, 255};
                Color activeThumbColor = {50, 50, 50, 255};
                
                if (cJSON_HasObjectItem(colors, "trackColor")) {
                    parse_color(cJSON_GetObjectItem(colors, "trackColor")->valuestring, &trackColor);
                }
                if (cJSON_HasObjectItem(colors, "thumbColor")) {
                    parse_color(cJSON_GetObjectItem(colors, "thumbColor")->valuestring, &thumbColor);
                }
                if (cJSON_HasObjectItem(colors, "activeThumbColor")) {
                    parse_color(cJSON_GetObjectItem(colors, "activeThumbColor")->valuestring, &activeThumbColor);
                }
                
                slider_component_set_colors(sliderComponent, trackColor, thumbColor, activeThumbColor);
            }
        }
    } else if(layer->type==LISTBOX){
        layer->component = listbox_component_create(layer);
        
        // 解析列表框特定属性
        cJSON* listboxConfig = cJSON_GetObjectItem(json_obj, "listboxConfig");
        if (listboxConfig) {
            ListBoxComponent* listboxComponent = (ListBoxComponent*)layer->component;
            
            if (cJSON_HasObjectItem(listboxConfig, "multiSelect")) {
                listbox_component_set_multi_select(listboxComponent, cJSON_IsTrue(cJSON_GetObjectItem(listboxConfig, "multiSelect")));
            }
            
            // 解析列表项数据
            cJSON* items = cJSON_GetObjectItem(listboxConfig, "items");
            if (items && cJSON_IsArray(items)) {
                for (int i = 0; i < cJSON_GetArraySize(items); i++) {
                    cJSON* item = cJSON_GetArrayItem(items, i);
                    if (cJSON_IsString(item)) {
                        listbox_component_add_item(listboxComponent, item->valuestring, NULL);
                    } else if (cJSON_IsObject(item)) {
                        const char* text = "";
                        if (cJSON_HasObjectItem(item, "text")) {
                            text = cJSON_GetObjectItem(item, "text")->valuestring;
                        }
                        listbox_component_add_item(listboxComponent, text, NULL);
                    }
                }
            }
            
            // 解析列表框颜色
            cJSON* colors = cJSON_GetObjectItem(listboxConfig, "colors");
            if (colors) {
                Color bgColor = {255, 255, 255, 255};
                Color textColor = {0, 0, 0, 255};
                Color selectedBgColor = {50, 150, 250, 255};
                Color selectedTextColor = {255, 255, 255, 255};
                
                if (cJSON_HasObjectItem(colors, "bgColor")) {
                    parse_color(cJSON_GetObjectItem(colors, "bgColor")->valuestring, &bgColor);
                }
                if (cJSON_HasObjectItem(colors, "textColor")) {
                    parse_color(cJSON_GetObjectItem(colors, "textColor")->valuestring, &textColor);
                }
                if (cJSON_HasObjectItem(colors, "selectedBgColor")) {
                    parse_color(cJSON_GetObjectItem(colors, "selectedBgColor")->valuestring, &selectedBgColor);
                }
                if (cJSON_HasObjectItem(colors, "selectedTextColor")) {
                    parse_color(cJSON_GetObjectItem(colors, "selectedTextColor")->valuestring, &selectedTextColor);
                }
                
                listbox_component_set_colors(listboxComponent, bgColor, textColor, selectedBgColor, selectedTextColor);
            }
        }
    } else if(layer->type==TREEVIEW){
        layer->component = treeview_component_create_from_json(layer, json_obj);
    } else if(layer->type==SCROLLBAR){
        // 不再创建滑块子图层
        layer->child_count = 0;
        layer->children = NULL;
        
        // 创建滚动条组件
        ScrollbarComponent* scrollbar = NULL;
        if (parent != NULL) {
            scrollbar = scrollbar_component_create_from_json(layer, parent, json_obj);
        } else {
            printf("Warning: SCROLLBAR layer %s has no parent layer\n", layer->id);
            scrollbar = scrollbar_component_create_from_json(layer, layer, json_obj);
        }
        if (scrollbar) {
            layer->component = scrollbar;
        }
        
        // 对于SCROLLBAR类型，不应该覆盖子图层数组，所以跳过后续的子图层解析
        has_custom_children = 1;
    }


    // 递归解析子图层（如果不是SCROLLBAR类型）
    cJSON* children = cJSON_GetObjectItem(json_obj, "children");
    if (children && !has_custom_children) {
        layer->child_count = cJSON_GetArraySize(children);
        layer->children = realloc(layer->children, layer->child_count * sizeof(Layer*));
        
        for (int i = 0; i < layer->child_count; i++) {
            layer->children[i] = parse_layer(cJSON_GetArrayItem(children, i),layer);
        }
    }
    
    return layer;
}