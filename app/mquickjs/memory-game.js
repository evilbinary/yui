// Memory Game JavaScript Logic
// 记忆游戏 - 翻牌配对游戏

// 游戏状态
let gameState = {
    moves: 0,
    pairsFound: 0,
    totalPairs: 8,
    cards: [],  // 存储每张卡片的表情
    flipped: [], // 记录哪些卡片被翻开
    matched: [], // 记录哪些卡片已配对
    firstFlip: -1,  // 第一张翻开的卡片索引
    isLocked: false  // 游戏是否被锁定（等待配对检查）
};

// 卡片表情
const cardEmojis = ["🎈", "🎨", "🎯", "🎪", "🎭", "🎸", "🎺", "🎮"];

// Fisher-Yates 洗牌算法
function shuffle(array) {
    let currentIndex = array.length, randomIndex;
    while (currentIndex != 0) {
        randomIndex = Math.floor(Math.random() * currentIndex);
        currentIndex--;
        [array[currentIndex], array[randomIndex]] = [array[randomIndex], array[currentIndex]];
    }
    return array;
}

// 初始化游戏
function initMemoryGame() {
    YUI.log("Initializing Memory Game...");

    // 创建配对卡片数组
    let cardPairs = [];
    for (let i = 0; i < 8; i++) {
        cardPairs.push(cardEmojis[i]);
        cardPairs.push(cardEmojis[i]);
    }

    // 洗牌
    gameState.cards = shuffle(cardPairs);

    // 初始化状态数组
    gameState.flipped = new Array(16).fill(0);
    gameState.matched = new Array(16).fill(0);
    gameState.moves = 0;
    gameState.pairsFound = 0;
    gameState.firstFlip = -1;
    gameState.isLocked = false;

    // 重置所有卡片显示为 "?"
    for (let i = 1; i <= 16; i++) {
        let cardId = "card" + i;
        YUI.setText(cardId, "?");
        YUI.setBgColor(cardId, "#16A085");
    }

    // 更新UI显示
    updateGameUI();
}

// 更新游戏UI显示
function updateGameUI() {
    YUI.setText("movesLabel", "步数: " + gameState.moves);
    YUI.setText("pairsLabel", "配对: " + gameState.pairsFound + "/" + gameState.totalPairs);
}

// 新游戏
function newMemoryGame() {
    YUI.log("Starting new game...");
    initMemoryGame();
}

// 显示提示
function showMemoryHint() {
    YUI.log("Showing hint...");

    if (gameState.isLocked) {
        return;
    }

    // 找到所有未配对的卡片
    let unmatched = [];
    for (let i = 0; i < 16; i++) {
        if (!gameState.matched[i]) {
            unmatched.push(i);
        }
    }

    // 显示所有未配对卡片的内容
    for (let i of unmatched) {
        let cardId = "card" + (i + 1);
        YUI.setText(cardId, gameState.cards[i]);
        YUI.setBgColor(cardId, "#3498DB");
    }

    // 1秒后翻回去
    setTimeout(() => {
        for (let i of unmatched) {
            if (!gameState.matched[i] && !gameState.flipped[i]) {
                let cardId = "card" + (i + 1);
                YUI.setText(cardId, "?");
                YUI.setBgColor(cardId, "#16A085");
            }
        }
    }, 1000);
}

// 翻开卡片
function flipCard(cardIndex) {
    // 检查游戏是否被锁定
    if (gameState.isLocked) {
        return;
    }

    // 检查卡片是否已经翻开或配对
    if (gameState.flipped[cardIndex] || gameState.matched[cardIndex]) {
        return;
    }

    // 翻开卡片
    gameState.flipped[cardIndex] = 1;
    let cardId = "card" + (cardIndex + 1);
    YUI.setText(cardId, gameState.cards[cardIndex]);
    YUI.setBgColor(cardId, "#3498DB");

    // 如果是第一次翻开
    if (gameState.firstFlip === -1) {
        gameState.firstFlip = cardIndex;
    } else {
        // 第二次翻开，检查配对
        checkPair(gameState.firstFlip, cardIndex);
        gameState.firstFlip = -1;
    }
}

// 检查配对
function checkPair(index1, index2) {
    gameState.isLocked = true;
    gameState.moves++;
    updateGameUI();

    if (gameState.cards[index1] === gameState.cards[index2]) {
        // 配对成功
        gameState.matched[index1] = 1;
        gameState.matched[index2] = 1;
        gameState.pairsFound++;
        updateGameUI();

        // 将已配对的卡片设置为绿色
        let cardId1 = "card" + (index1 + 1);
        let cardId2 = "card" + (index2 + 1);
        YUI.setBgColor(cardId1, "#27AE60");
        YUI.setBgColor(cardId2, "#27AE60");

        gameState.isLocked = false;

        // 检查游戏是否完成
        if (gameState.pairsFound === gameState.totalPairs) {
            setTimeout(() => {
                YUI.log("🎉 恭喜！游戏完成！总步数: " + gameState.moves);
            }, 500);
        }
    } else {
        // 配对失败，延迟后翻回
        setTimeout(() => {
            let cardId1 = "card" + (index1 + 1);
            let cardId2 = "card" + (index2 + 1);
            YUI.setText(cardId1, "?");
            YUI.setText(cardId2, "?");
            YUI.setBgColor(cardId1, "#16A085");
            YUI.setBgColor(cardId2, "#16A085");
            gameState.flipped[index1] = 0;
            gameState.flipped[index2] = 0;
            gameState.isLocked = false;
        }, 1000);
    }
}

// 为每个卡片创建翻卡函数
function flipCard1() { flipCard(0); }
function flipCard2() { flipCard(1); }
function flipCard3() { flipCard(2); }
function flipCard4() { flipCard(3); }
function flipCard5() { flipCard(4); }
function flipCard6() { flipCard(5); }
function flipCard7() { flipCard(6); }
function flipCard8() { flipCard(7); }
function flipCard9() { flipCard(8); }
function flipCard10() { flipCard(9); }
function flipCard11() { flipCard(10); }
function flipCard12() { flipCard(11); }
function flipCard13() { flipCard(12); }
function flipCard14() { flipCard(13); }
function flipCard15() { flipCard(14); }
function flipCard16() { flipCard(15); }

// 简单的 setTimeout 实现
let timers = [];
let timerIdCounter = 0;

function setTimeout(callback, delay) {
    let timerId = timerIdCounter++;
    let startTime = Date.now();

    timers.push({
        id: timerId,
        callback: callback,
        triggerTime: startTime + delay
    });

    return timerId;
}

// 检查并触发定时器（需要由主循环调用）
function checkTimers() {
    let now = Date.now();
    for (let i = timers.length - 1; i >= 0; i--) {
        if (now >= timers[i].triggerTime) {
            timers[i].callback();
            timers.splice(i, 1);
        }
    }
}

YUI.log("Memory Game script loaded successfully!");
