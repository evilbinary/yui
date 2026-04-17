// Camera Application JavaScript Logic
// 相机应用 JavaScript 逻辑

// 相机状态
var cameraState = {
    isRecording: false,
    mode: 'photo',        // 'photo' 或 'video'
    currentLut: 0,
    ev: 0.0,
    iso: 100,
    wb: 5500,
    photoCount: 4321,
    batteryLevel: 75,
    cameraReady: false,
    stream: null
};

// LUT 列表
var lutList = [
    { name: 'PORTRAIT', desc: '人像' },
    { name: 'LANDSCAPE', desc: '风景' },
    { name: 'CINEMATIC', desc: '电影' },
    { name: 'VINTAGE', desc: '复古' },
    { name: 'B&W', desc: '黑白' },
    { name: 'VIVID', desc: '鲜艳' },
    { name: 'SOFT', desc: '柔和' },
    { name: 'COOL', desc: '冷色' }
];

// 辅助函数：更新图层属性
function updateLayerProperty(layerId, property, value) {
    var change = {};
    change[property] = value;
    var update = {
        "target": layerId,
        "change": change
    };
    YUI.update(JSON.stringify([update]));
}

// 辅助函数：更新图层样式
function updateLayerStyle(layerId, styleProperty, value) {
    var style = {};
    style[styleProperty] = value;
    var update = {
        "target": layerId,
        "change": {
            "style": style
        }
    };
    YUI.update(JSON.stringify([update]));
}

// 初始化相机
function initCamera() {
    YUI.log("initCamera: Initializing camera application...");
    
    // 更新电池显示
    updateBatteryDisplay();
    
    // 更新 LUT 显示
    updateLutDisplay();
    
    // 更新相机参数显示
    updateCameraParams();
    
    // 初始化摄像头
    initCameraStream();
    
    YUI.log("initCamera: Camera application initialized!");
}

// 初始化摄像头流
function initCameraStream() {
    // 在 Emscripten 环境中使用 JavaScript 访问摄像头
    // 这个函数会在 JS 层面调用 navigator.mediaDevices.getUserMedia
    var result = startCameraStream();
    if (result) {
        cameraState.cameraReady = true;
        YUI.log("initCameraStream: Camera stream started successfully");
    } else {
        YUI.log("initCameraStream: Failed to start camera stream");
        // 显示占位符
        showPlaceholder();
    }
}

// 启动摄像头流 (调用原生 JavaScript)
function startCameraStream() {
    // 这部分逻辑在 Emscripten 中通过 EM_ASM 实现
    // 在纯 JS 环境中，我们直接使用浏览器的 API
    return true;
}

// 显示占位符
function showPlaceholder() {
    // 设置一个占位图像或文本
    YUI.setText("viewfinder", "[取景器]\n点击开始相机");
}

// 更新电池显示
function updateBatteryDisplay() {
    var level = cameraState.batteryLevel;
    var color = "#00ff00";  // 绿色
    
    if (level < 20) {
        color = "#ff0000";   // 红色
    } else if (level < 50) {
        color = "#ffff00";   // 黄色
    }
    
    // 更新电池颜色 (通过边框颜色)
    updateLayerStyle("batteryIcon", "borderColor", color);
    updateLayerStyle("batteryTip", "bgColor", color);
}

// 更新 LUT 显示
function updateLutDisplay() {
    var lut = lutList[cameraState.currentLut];
    YUI.setText("lutLabel", "LUT: " + lut.name);
}

// 更新相机参数显示
function updateCameraParams() {
    var evSign = cameraState.ev >= 0 ? "+" : "";
    YUI.setText("evLabel", "EV: " + evSign + cameraState.ev.toFixed(1));
    YUI.setText("isoLabel", "ISO: " + cameraState.iso);
    YUI.setText("wbLabel", "WB: " + cameraState.wb + "K");
    YUI.setText("countLabel", "" + cameraState.photoCount);
    
    // 更新模式显示
    YUI.setText("modeLabel", cameraState.mode === 'photo' ? "PHOTO" : "VIDEO");
}

// 快门按钮点击
function onShutterClick() {
    YUI.log("onShutterClick: Shutter button clicked");
    
    if (cameraState.mode === 'photo') {
        // 拍照模式
        takePhoto();
    } else {
        // 录像模式
        toggleRecording();
    }
}

// 拍照
function takePhoto() {
    YUI.log("takePhoto: Taking photo...");
    
    // 模拟拍照效果 - 直接更新计数
    cameraState.photoCount--;
    updateCameraParams();
    
    // 应用当前选择的 LUT
    var currentLut = lutList[cameraState.currentLut];
    YUI.log("takePhoto: Applying LUT: " + currentLut.name);
    applyLut(currentLut.name);
    
    // 添加拍照视觉反馈
    flashEffect();
    
    YUI.log("takePhoto: Photo saved! Remaining: " + cameraState.photoCount + " with LUT: " + currentLut.name);
}

// 拍照闪光效果
function flashEffect() {
    // 模拟闪光效果 - 短暂改变取景器背景色
    var viewfinder = YUI.getView("viewfinder");
    if (viewfinder) {
        // 闪光
        updateLayerStyle("viewfinder", "bgColor", "#ffffff");
        
        // 延迟恢复
        setTimeout(function() {
            updateLayerStyle("viewfinder", "bgColor", "#000000");
        }, 100);
    }
}

// 切换录制状态
function toggleRecording() {
    cameraState.isRecording = !cameraState.isRecording;
    
    if (cameraState.isRecording) {
        YUI.log("toggleRecording: Started recording");
        // 显示录制指示器
        updateLayerProperty("recIndicator", "visible", true);
        updateLayerProperty("recIndicator", "size", [480, 4]);
        updateLayerStyle("shutterBtn", "bgColor", "#ff0000");
    } else {
        YUI.log("toggleRecording: Stopped recording");
        // 隐藏录制指示器
        updateLayerProperty("recIndicator", "visible", false);
        updateLayerStyle("shutterBtn", "bgColor", "#ffffff");
    }
}

// 切换到上一个 LUT
function onLutPrev() {
    YUI.log("onLutPrev: Previous LUT");
    cameraState.currentLut--;
    if (cameraState.currentLut < 0) {
        cameraState.currentLut = lutList.length - 1;
    }
    updateLutDisplay();
    
    // 添加 LUT 切换视觉反馈
    lutChangeEffect();
}

// 切换到下一个 LUT
function onLutNext() {
    YUI.log("onLutNext: Next LUT");
    cameraState.currentLut++;
    if (cameraState.currentLut >= lutList.length) {
        cameraState.currentLut = 0;
    }
    updateLutDisplay();
    
    // 添加 LUT 切换视觉反馈
    lutChangeEffect();
}

// LUT 切换视觉效果
function lutChangeEffect() {
    // 短暂改变 LUT 标签颜色，提供视觉反馈
    updateLayerStyle("lutLabel", "color", "#ffcc00");
    
    setTimeout(function() {
        updateLayerStyle("lutLabel", "color", "#ffffff");
    }, 300);
}

// 点击取景器
function onViewfinderClick() {
    YUI.log("onViewfinderClick: Viewfinder clicked");
    // 可以用于对焦或其他操作
}

// 应用 LUT 滤镜效果
function applyLut(lutName) {
    YUI.log("applyLut: Applying LUT: " + lutName);
    
    // 模拟 LUT 应用逻辑
    // 在实际应用中，这会通过 WebGL shader 或图像处理实现
    switch(lutName) {
        case 'PORTRAIT':
            // 人像模式：增强肤色，柔和色调
            YUI.log("applyLut: Portrait mode - Enhancing skin tones and softening colors");
            break;
        case 'LANDSCAPE':
            // 风景模式：增强蓝天和绿地
            YUI.log("applyLut: Landscape mode - Enhancing blues and greens");
            break;
        case 'CINEMATIC':
            // 电影模式：增加对比度和饱和度
            YUI.log("applyLut: Cinematic mode - Increasing contrast and saturation");
            break;
        case 'VINTAGE':
            // 复古模式：降低饱和度，增加暖色调
            YUI.log("applyLut: Vintage mode - Desaturating and adding warm tones");
            break;
        case 'B&W':
            // 黑白模式：转换为灰度
            YUI.log("applyLut: Black & White mode - Converting to grayscale");
            break;
        case 'VIVID':
            // 鲜艳模式：增加饱和度和对比度
            YUI.log("applyLut: Vivid mode - Increasing saturation and contrast");
            break;
        case 'SOFT':
            // 柔和模式：降低对比度，增加曝光
            YUI.log("applyLut: Soft mode - Decreasing contrast and increasing exposure");
            break;
        case 'COOL':
            // 冷色模式：增加蓝色调
            YUI.log("applyLut: Cool mode - Adding blue tones");
            break;
        default:
            YUI.log("applyLut: Unknown LUT: " + lutName);
    }
}

// 设置曝光补偿
function setEV(value) {
    cameraState.ev = Math.max(-3.0, Math.min(3.0, value));
    updateCameraParams();
}

// 设置 ISO
function setISO(value) {
    var isoValues = [100, 200, 400, 800, 1600, 3200, 6400];
    var closest = isoValues[0];
    var minDiff = Math.abs(value - closest);
    
    for (var i = 1; i < isoValues.length; i++) {
        var diff = Math.abs(value - isoValues[i]);
        if (diff < minDiff) {
            minDiff = diff;
            closest = isoValues[i];
        }
    }
    
    cameraState.iso = closest;
    updateCameraParams();
}

// 设置白平衡
function setWB(value) {
    cameraState.wb = Math.max(2000, Math.min(10000, value));
    updateCameraParams();
}

// 切换模式 (拍照/录像)
function switchMode() {
    if (cameraState.isRecording) {
        toggleRecording();  // 停止录制
    }
    
    cameraState.mode = cameraState.mode === 'photo' ? 'video' : 'photo';
    updateCameraParams();
    YUI.log("switchMode: Switched to " + cameraState.mode + " mode");
}
