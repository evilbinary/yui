#include "layer.h"
#include "animate.h"

// 更新图层类型名称数组，添加GRID
char *layer_type_name[]={"View","Button","Input","Label","Image","List","Grid","Progress","Checkbox","Radiobox"};

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
    if(strlen(valuestring)==9){
        sscanf(valuestring, "#%02hhx%02hhx%02hhx%02hhx", 
            &color->r, &color->g, &color->b,&color->a);
    }else{
        sscanf(valuestring, "#%02hhx%02hhx%02hhx", 
            &color->r, &color->g, &color->b);
        color->a = 255;
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
        layer->component = input_component_create(layer);
        
        // 解析placeholder属性
        if (cJSON_HasObjectItem(json_obj, "placeholder")) {
            input_component_set_placeholder(layer->component, cJSON_GetObjectItem(json_obj, "placeholder")->valuestring);
        }
        
        // 解析maxLength属性
        if (cJSON_HasObjectItem(json_obj, "maxLength")) {
            input_component_set_max_length(layer->component, cJSON_GetObjectItem(json_obj, "maxLength")->valueint);
        }
    }else if(layer->type==BUTTON){
        layer->component = button_component_create(layer);

    }else if(layer->type==LABEL){
        layer->component = label_component_create(layer);
        
    }else if(layer->type==IMAGE){
        layer->component = image_component_create(layer);
        
    }else if(layer->type==PROGRESS){
        layer->component = progress_component_create(layer);
        
        // 设置进度值
        if (layer->data && layer->data->json) {
            ProgressComponent* progress_component = (ProgressComponent*)layer->component;
            int value = layer->data->json->valueint;
            // 将0-100的值转换为0.0-1.0
            float progress = value / 100.0f;
            progress_component_set_progress(progress_component, progress);
        }
    }else if(layer->type==CHECKBOX){
        layer->component = checkbox_component_create_from_json(layer, json_obj);
        
    }else if(layer->type==RADIOBOX){

         char* group_id="default";
         if (cJSON_HasObjectItem(layer, "group")) {
            char* group = cJSON_GetObjectItem(layer, "group")->valuestring;
            group_id = strdup(group);
        }
        int checked=0;
        if(cJSON_HasObjectItem(json_obj, "data")){
            checked=cJSON_IsTrue(cJSON_GetObjectItem(json_obj, "data"));
        }
        layer->component = radiobox_component_create(layer,group_id,checked);
        
    }
    
    // 递归解析子图层
    cJSON* children = cJSON_GetObjectItem(json_obj, "children");
    if (children) {
        layer->child_count = cJSON_GetArraySize(children);
        layer->children = malloc(layer->child_count * sizeof(Layer*));
        
        for (int i = 0; i < layer->child_count; i++) {
            layer->children[i] = parse_layer(cJSON_GetArrayItem(children, i),layer);
        }
    }
    
    return layer;
}