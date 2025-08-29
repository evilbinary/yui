#include "layer.h"

char *layer_type_name[]={"View","Button","Input","Label","Image","List"};


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
Layer* parse_layer(cJSON* json_obj,Layer* parent) {
    if(json_obj==NULL){
        return NULL;
    }
    Layer* layer = malloc(sizeof(Layer));
    memset(layer, 0, sizeof(Layer));
    layer->parent=parent;
    
    
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
        layer->data->size=cJSON_GetArraySize(data);
    }
    
    // 解析列表项模板
    cJSON* item_template = cJSON_GetObjectItem(json_obj, "itemTemplate");
    if (item_template) {
        layer->item_template = parse_layer(item_template,parent);
    }

    // 解析滚动属性
    if (cJSON_HasObjectItem(json_obj, "scrollable")) {
        layer->scrollable = cJSON_GetObjectItem(json_obj, "scrollable")->valueint;
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
            sscanf(color->valuestring, "#%02hhx%02hhx%02hhx", 
                   &layer->scrollbar->color.r, &layer->scrollbar->color.g, &layer->scrollbar->color.b);
            layer->scrollbar->color.a = 255;
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