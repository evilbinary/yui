// Memory Game JavaScript Logic
// 记忆游戏 - 翻牌配对游戏

// YUI.log 简化为使用 print
var YUI = {
    log: function() {
        var args = [];
        for (var i = 0; i < arguments.length; i++) {
            if (i != 0) args.push(" ");
            args.push(arguments[i]);
        }
        print(args.join(""));
    }
};

// 游戏状态
var gameState = {
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
var cardEmojis = ["A", "B", "C", "D", "E", "F", "G", "H"];

// Fisher-Yates 洗牌算法
function shuffle(array) {
    var currentIndex = array.length, randomIndex;
    while (currentIndex != 0) {
        randomIndex = Math.floor(Math.random() * currentIndex);
        currentIndex--;
        var temp = array[currentIndex];
        array[currentIndex] = array[randomIndex];
        array[randomIndex] = temp;
    }
    return array;
}

// 初始化游戏 - onLoad 事件触发
function initMemoryGame() {
    YUI.log("initMemoryGame: Initializing Memory Game...");

    // 创建配对卡片数组
    var cardPairs = [];
    for (var i = 0; i < 8; i++) {
        cardPairs.push(cardEmojis[i]);
        cardPairs.push(cardEmojis[i]);
    }

    // 洗牌
    gameState.cards = shuffle(cardPairs);

    // 初始化状态数组
    gameState.flipped = [];
    gameState.matched = [];
    for (var i = 0; i < 16; i++) {
        gameState.flipped.push(0);
        gameState.matched.push(0);
    }
    gameState.moves = 0;
    gameState.pairsFound = 0;
    gameState.firstFlip = -1;
    gameState.isLocked = false;

    // 更新UI显示
    updateGameUI();
    YUI.log("initMemoryGame: Game initialized!");
}

// 更新游戏UI显示
function updateGameUI() {
    YUI.log("updateGameUI: Moves=" + gameState.moves + " Pairs=" + gameState.pairsFound + "/" + gameState.totalPairs);
}

// 新游戏 - newGameBtn.onClick 事件触发
function newMemoryGame() {
    YUI.log("newMemoryGame: Starting new game...");
    initMemoryGame();
}

// 显示提示 - showHintBtn.onClick 事件触发
function showMemoryHint() {
    YUI.log("showMemoryHint: Showing hint...");

    if (gameState.isLocked) {
        return;
    }

    // 找到所有未配对的卡片
    var unmatched = [];
    for (var i = 0; i < 16; i++) {
        if (!gameState.matched[i]) {
            unmatched.push(i);
        }
    }

    YUI.log("showMemoryHint: Found " + unmatched.length + " unmatched cards");

    // 显示所有未配对卡片的内容
    for (var j = 0; j < unmatched.length; j++) {
        var i = unmatched[j];
        YUI.log("showMemoryHint: Card " + (i + 1) + " = " + gameState.cards[i]);
    }
}

// 翻开卡片 - cardX.onClick 事件触发
function flipCard(cardIndex) {
    YUI.log("flipCard: Flipping card " + cardIndex);

    // 检查游戏是否被锁定
    if (gameState.isLocked) {
        YUI.log("flipCard: Game is locked, ignoring click");
        return;
    }

    // 检查卡片是否已经翻开或配对
    if (gameState.flipped[cardIndex] || gameState.matched[cardIndex]) {
        YUI.log("flipCard: Card already flipped or matched");
        return;
    }

    // 翻开卡片
    gameState.flipped[cardIndex] = 1;
    var cardId = "card" + (cardIndex + 1);
    YUI.log("flipCard: " + cardId + " = " + gameState.cards[cardIndex]);

    // 如果是第一次翻开
    if (gameState.firstFlip === -1) {
        gameState.firstFlip = cardIndex;
        YUI.log("flipCard: First flip, waiting for second card");
    } else {
        // 第二次翻开，检查配对
        checkPair(gameState.firstFlip, cardIndex);
        gameState.firstFlip = -1;
    }
}

// 检查配对
function checkPair(index1, index2) {
    YUI.log("checkPair: Checking " + index1 + " vs " + index2);
    gameState.isLocked = true;
    gameState.moves++;
    updateGameUI();

    if (gameState.cards[index1] === gameState.cards[index2]) {
        // 配对成功
        gameState.matched[index1] = 1;
        gameState.matched[index2] = 1;
        gameState.pairsFound++;
        updateGameUI();
        gameState.isLocked = false;

        YUI.log("checkPair: Match found! Total pairs: " + gameState.pairsFound);

        // 检查游戏是否完成
        if (gameState.pairsFound === gameState.totalPairs) {
            YUI.log("checkPair: CONGRATULATIONS! Game completed in " + gameState.moves + " moves!");
        }
    } else {
        // 配对失败，延迟后翻回
        YUI.log("checkPair: No match, flipping back");
        gameState.isLocked = false;
        gameState.flipped[index1] = 0;
        gameState.flipped[index2] = 0;
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

YUI.log("Memory Game script loaded successfully!");
YUI.log("Available functions: initMemoryGame, newMemoryGame, showMemoryHint, flipCard1-16");
YUI.log("Testing Math.random: " + Math.random());
YUI.log("Testing Math.floor: " + Math.floor(3.14));
