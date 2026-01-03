// Whack-a-Mole Game JavaScript Logic
// 打地鼠游戏 JavaScript 逻辑

// 游戏状态
var gameState = {
    score: 0,
    timeLeft: 30,
    gameRunning: false,
    currentMoleHole: -1,  // 当前地鼠所在的洞（1-9）
    moleVisible: false,    // 地鼠是否可见
    hitMoleCount: 0,      // 击中地鼠的次数
    totalClicks: 0        // 总点击次数
};

// 游戏配置
var gameConfig = {
    duration: 30,           // 游戏持续时间（秒）
    scorePerHit: 10         // 每次击中得分
};

// 洞口标识（地鼠表情）
var holeEmoji = "🕳️";
var moleEmoji = "🐹";
var hitEmoji = "💥";  // 击中时的表情

// 初始化游戏 - onLoad 事件触发
function initWhackAMoleGame() {
    YUI.log("initWhackAMoleGame: Initializing Whack-a-Mole game...");
    
    // 重置游戏状态
    gameState.score = 0;
    gameState.timeLeft = gameConfig.duration;
    gameState.gameRunning = false;
    gameState.currentMoleHole = -1;
    gameState.moleVisible = false;
    
    // 更新UI显示
    updateGameUI();
    
    YUI.log("initWhackAMoleGame: Game initialized!");
}

// 更新游戏UI显示
function updateGameUI() {
    // 更新得分标签
    YUI.setText("scoreLabel", "得分: " + gameState.score);

    // 更新时间标签
    YUI.setText("timeLabel", "时间: " + gameState.timeLeft);
}

// 更新游戏信息
function updateGameInfo(message) {
    YUI.setText("gameInfo", message);
}

// 开始游戏 - startBtn.onClick 事件触发
function startWhackAMoleGame() {
    YUI.log("startWhackAMoleGame: Starting game...");
    
    // 如果游戏已在运行，先停止
    if (gameState.gameRunning) {
        stopWhackAMoleGame();
    }
    
    // 重置游戏状态
    gameState.score = 0;
    gameState.timeLeft = gameConfig.duration;
    gameState.gameRunning = true;
    gameState.currentMoleHole = -1;
    gameState.moleVisible = false;
    
    // 更新UI
    updateGameUI();
    
    // 开始游戏循环
    startGameLoop();
    
    YUI.log("startWhackAMoleGame: Game started!");
}

// 停止游戏 - stopBtn.onClick 事件触发
function stopWhackAMoleGame() {
    YUI.log("stopWhackAMoleGame: Stopping game...");

    if (!gameState.gameRunning) {
        YUI.log("stopWhackAMoleGame: Game not running");
        return;
    }

    // 停止游戏状态
    gameState.gameRunning = false;

    // 停止所有定时器
    stopGameLoop();

    // 隐藏所有地鼠
    hideMole();

    // 计算命中率
    var accuracy = 0;
    if (gameState.totalClicks > 0) {
        accuracy = Math.floor((gameState.hitMoleCount / gameState.totalClicks) * 100);
    }

    // 显示最终得分和统计信息
    var message = "游戏结束！最终得分: " + gameState.score;
    message += " | 击中: " + gameState.hitMoleCount + " 次";
    message += " | 命中率: " + accuracy + "%";

    YUI.setText("gameInfo", message);

    YUI.log("stopWhackAMoleGame: Game stopped. Final score: " + gameState.score +
             ", Hits: " + gameState.hitMoleCount +
             ", Accuracy: " + accuracy + "%");
}

// 开始游戏循环（模拟定时器）
function startGameLoop() {
    YUI.log("startGameLoop: Starting game loop");

    // 注意：由于 Mario JS 引擎可能没有完整的定时器 API
    // 这里使用手动触发的方式。在实际应用中，可以通过
    // 用户的点击或其他事件来驱动游戏进度。
    // 或者需要在 C 层面实现定时器回调。

    // 显示第一个地鼠
    showMole();

    // 显示游戏提示
    updateGameInfo("游戏进行中！点击出现的地鼠 🐹 来得分！");
}

// 停止游戏循环
function stopGameLoop() {
    YUI.log("stopGameLoop: Stopping game loop");
    // 清理定时器（如果有）
}

// 随机选择一个洞显示地鼠
function showMole() {
    if (!gameState.gameRunning) {
        return;
    }

    // 先隐藏当前地鼠
    hideMole();

    // 随机选择一个洞（1-9）
    var randomHole = Math.floor(Math.random() * 9) + 1;

    YUI.log("showMole: Mole appears at hole " + randomHole);

    // 显示地鼠
    var holeId = "hole" + randomHole;
    YUI.setText(holeId, moleEmoji);
    YUI.setBgColor(holeId, "#FF9800");  // 橙色背景

    gameState.currentMoleHole = randomHole;
    gameState.moleVisible = true;

    // 注意：地鼠会在一段时间后自动消失
    // 由于 Mario JS 引擎可能没有 setTimeout，这个功能暂时无法实现
    // 地鼠将一直显示直到被击中或游戏停止
}

// 隐藏地鼠
function hideMole() {
    if (gameState.currentMoleHole >= 1 && gameState.currentMoleHole <= 9) {
        var holeId = "hole" + gameState.currentMoleHole;
        YUI.setText(holeId, holeEmoji);
        YUI.setBgColor(holeId, "#3E2723");  // 恢复棕色背景
    }

    gameState.currentMoleHole = -1;
    gameState.moleVisible = false;
}

// 点击地鼠的处理函数
function whackMole(holeIndex) {
    if (!gameState.gameRunning) {
        YUI.log("whackMole: Game not running");
        return;
    }

    YUI.log("whackMole: Clicked hole " + holeIndex);
    gameState.totalClicks++;

    // 检查是否击中地鼠
    if (gameState.moleVisible && gameState.currentMoleHole === holeIndex) {
        YUI.log("whackMole: HIT! Score increased by " + gameConfig.scorePerHit);

        // 增加得分和计数
        gameState.score += gameConfig.scorePerHit;
        gameState.hitMoleCount++;

        // 显示击中效果
        var holeId = "hole" + holeIndex;
        YUI.setText(holeId, hitEmoji);
        YUI.setBgColor(holeId, "#F44336");  // 红色背景表示击中

        // 更新UI
        updateGameUI();
        updateGameInfo("好样的！击中地鼠！当前得分: " + gameState.score);

        // 短暂延迟后显示下一个地鼠
        // 注意：由于没有定时器，这里立即显示
        setTimeout(function() {
            showMole();
        }, 200);
    } else {
        YUI.log("whackMole: Missed!");
        // 显示未击中提示
        if (gameState.totalClicks % 5 === 0) {
            var accuracy = 0;
            if (gameState.totalClicks > 0) {
                accuracy = Math.floor((gameState.hitMoleCount / gameState.totalClicks) * 100);
            }
            updateGameInfo("命中率: " + accuracy + "% | 得分: " + gameState.score);
        }
    }
}

// 模拟 setTimeout（如果引擎不支持）
function setTimeout(callback, delay) {
    // Mario JS 可能不支持 setTimeout
    // 这里直接调用回调
    callback();
}

// 为每个洞创建点击处理函数
function whackMole1() { whackMole(1); }
function whackMole2() { whackMole(2); }
function whackMole3() { whackMole(3); }
function whackMole4() { whackMole(4); }
function whackMole5() { whackMole(5); }
function whackMole6() { whackMole(6); }
function whackMole7() { whackMole(7); }
function whackMole8() { whackMole(8); }
function whackMole9() { whackMole(9); }

// 倒计时更新（需要定时器支持）
function updateTimer() {
    if (!gameState.gameRunning) {
        return;
    }

    gameState.timeLeft--;
    updateGameUI();

    YUI.log("updateTimer: Time left = " + gameState.timeLeft);

    if (gameState.timeLeft <= 0) {
        // 时间到，游戏结束
        stopWhackAMoleGame();
    }
}

// 手动触发地鼠更新（用于没有定时器的情况）
// 可以通过定期调用这个函数来模拟游戏循环
function updateMole() {
    if (gameState.gameRunning && !gameState.moleVisible) {
        showMole();
    }
}

// 获取游戏统计信息
function getGameStats() {
    var accuracy = 0;
    if (gameState.totalClicks > 0) {
        accuracy = Math.floor((gameState.hitMoleCount / gameState.totalClicks) * 100);
    }

    return {
        score: gameState.score,
        timeLeft: gameState.timeLeft,
        hitCount: gameState.hitMoleCount,
        totalClicks: gameState.totalClicks,
        accuracy: accuracy,
        gameRunning: gameState.gameRunning
    };
}

YUI.log("Whack-a-Mole game script loaded successfully!");
YUI.log("Available functions: initWhackAMoleGame, startWhackAMoleGame, stopWhackAMoleGame, whackMole1-9");
YUI.log("Click '开始游戏' to start playing!");
