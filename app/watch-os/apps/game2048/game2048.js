/**
 * Watch OS · 2048
 */

var G2048_N = 4;
var g2048Grid = [];
var g2048Score = 0;
var g2048Best = 0;
var g2048Over = false;
var g2048Won = false;

var G2048_TILES = {
    0: { bg: "#2a2a2e", fg: "#3a3a3e", fs: 18 },
    2: { bg: "#3d4f6a", fg: "#e8f0ff", fs: 20 },
    4: { bg: "#4a6085", fg: "#e8f0ff", fs: 20 },
    8: { bg: "#5c7aa8", fg: "#ffffff", fs: 20 },
    16: { bg: "#6e8fc4", fg: "#ffffff", fs: 18 },
    32: { bg: "#8a6eb8", fg: "#ffffff", fs: 18 },
    64: { bg: "#a85898", fg: "#ffffff", fs: 18 },
    128: { bg: "#c9a227", fg: "#1a1a1a", fs: 16 },
    256: { bg: "#d4ad2a", fg: "#1a1a1a", fs: 16 },
    512: { bg: "#e0b82e", fg: "#1a1a1a", fs: 15 },
    1024: { bg: "#00b4d8", fg: "#001018", fs: 13 },
    2048: { bg: "#00d4ff", fg: "#001018", fs: 13 }
};

function onGame2048Load() {
    g2048NewGame();
}

function onGame2048Show() {
    applyWatchTheme();
    g2048Render();
}

function g2048NewGame() {
    g2048Score = 0;
    g2048Over = false;
    g2048Won = false;
    g2048InitGrid();
    g2048AddRandom();
    g2048AddRandom();
    g2048SetStatus("合并相同数字，目标 2048");
    g2048Render();
}

function g2048InitGrid() {
    var r;
    g2048Grid = [];
    for (r = 0; r < G2048_N; r++) {
        g2048Grid[r] = [0, 0, 0, 0];
    }
}

function g2048CellId(r, c) {
    return "g2048_c" + (r * G2048_N + c);
}

function g2048TileStyle(v) {
    if (G2048_TILES[v]) {
        return G2048_TILES[v];
    }
    return { bg: "#ff375f", fg: "#ffffff", fs: 12 };
}

function g2048SetStatus(msg) {
    YUI.setText("g2048_status", msg);
}

function g2048Render() {
    var r;
    var c;
    var v;
    var id;
    var style;

    for (r = 0; r < G2048_N; r++) {
        for (c = 0; c < G2048_N; c++) {
            v = g2048Grid[r][c];
            id = g2048CellId(r, c);
            style = g2048TileStyle(v);
            YUI.update({
                target: id,
                change: {
                    text: v > 0 ? String(v) : " ",
                    bgColor: style.bg,
                    color: style.fg,
                    fontSize: style.fs
                }
            });
        }
    }
    YUI.setText("g2048_score", String(g2048Score));
    YUI.setText("g2048_best", String(g2048Best));
}

function g2048AddRandom() {
    var empty = [];
    var r;
    var c;
    var pick;

    for (r = 0; r < G2048_N; r++) {
        for (c = 0; c < G2048_N; c++) {
            if (!g2048Grid[r][c]) {
                empty.push({ r: r, c: c });
            }
        }
    }
    if (!empty.length) {
        return false;
    }
    pick = empty[Math.floor(Math.random() * empty.length)];
    g2048Grid[pick.r][pick.c] = Math.random() < 0.9 ? 2 : 4;
    return true;
}

function g2048SlideRow(row, countScore) {
    var arr = [];
    var out = [];
    var i;
    var merged;

    if (countScore === undefined) {
        countScore = true;
    }

    for (i = 0; i < row.length; i++) {
        if (row[i] > 0) {
            arr.push(row[i]);
        }
    }
    i = 0;
    while (i < arr.length) {
        if (i + 1 < arr.length && arr[i] === arr[i + 1]) {
            merged = arr[i] * 2;
            out.push(merged);
            if (countScore) {
                g2048Score += merged;
            }
            i += 2;
        } else {
            out.push(arr[i]);
            i++;
        }
    }
    while (out.length < G2048_N) {
        out.push(0);
    }
    return out;
}

function g2048RowsEqual(a, b) {
    var r;
    for (r = 0; r < G2048_N; r++) {
        if (a[r].join(",") !== b[r].join(",")) {
            return false;
        }
    }
    return true;
}

function g2048CloneGrid(g) {
    var r;
    var out = [];
    for (r = 0; r < G2048_N; r++) {
        out[r] = g[r].slice();
    }
    return out;
}

function g2048Transpose(g) {
    var r;
    var c;
    var t = [];
    for (r = 0; r < G2048_N; r++) {
        t[r] = [];
        for (c = 0; c < G2048_N; c++) {
            t[r][c] = g[c][r];
        }
    }
    return t;
}

function g2048ReverseRows(g) {
    var r;
    var out = [];
    for (r = 0; r < G2048_N; r++) {
        out[r] = g[r].slice().reverse();
    }
    return out;
}

function g2048MoveLeftOn(g, countScore) {
    var r;
    if (countScore === undefined) {
        countScore = true;
    }
    for (r = 0; r < G2048_N; r++) {
        g[r] = g2048SlideRow(g[r], countScore);
    }
    return g;
}

function g2048ApplyMove(dir) {
    var before;
    var g;

    if (dir === "left") {
        before = g2048CloneGrid(g2048Grid);
        g2048MoveLeftOn(g2048Grid);
        return !g2048RowsEqual(before, g2048Grid);
    }
    if (dir === "right") {
        before = g2048CloneGrid(g2048Grid);
        g2048Grid = g2048ReverseRows(g2048Grid);
        g2048MoveLeftOn(g2048Grid);
        g2048Grid = g2048ReverseRows(g2048Grid);
        return !g2048RowsEqual(before, g2048Grid);
    }
    if (dir === "up") {
        before = g2048CloneGrid(g2048Grid);
        g2048Grid = g2048Transpose(g2048Grid);
        g2048MoveLeftOn(g2048Grid);
        g2048Grid = g2048Transpose(g2048Grid);
        return !g2048RowsEqual(before, g2048Grid);
    }
    if (dir === "down") {
        before = g2048CloneGrid(g2048Grid);
        g2048Grid = g2048Transpose(g2048Grid);
        g2048Grid = g2048ReverseRows(g2048Grid);
        g2048MoveLeftOn(g2048Grid);
        g2048Grid = g2048ReverseRows(g2048Grid);
        g2048Grid = g2048Transpose(g2048Grid);
        return !g2048RowsEqual(before, g2048Grid);
    }
    return false;
}

function g2048HasEmpty() {
    var r;
    var c;
    for (r = 0; r < G2048_N; r++) {
        for (c = 0; c < G2048_N; c++) {
            if (!g2048Grid[r][c]) {
                return true;
            }
        }
    }
    return false;
}

function g2048CanMove() {
    var dirs = ["left", "right", "up", "down"];
    var i;
    var g;
    var before;

    for (i = 0; i < dirs.length; i++) {
        g = g2048CloneGrid(g2048Grid);
        before = g2048CloneGrid(g);
        if (dirs[i] === "left") {
            g2048MoveLeftOn(g, false);
        } else if (dirs[i] === "right") {
            g = g2048ReverseRows(g);
            g2048MoveLeftOn(g, false);
            g = g2048ReverseRows(g);
        } else if (dirs[i] === "up") {
            g = g2048Transpose(g);
            g2048MoveLeftOn(g, false);
            g = g2048Transpose(g);
        } else {
            g = g2048Transpose(g);
            g = g2048ReverseRows(g);
            g2048MoveLeftOn(g, false);
            g = g2048ReverseRows(g);
            g = g2048Transpose(g);
        }
        if (!g2048RowsEqual(before, g)) {
            return true;
        }
    }
    return false;
}

function g2048CheckWin() {
    var r;
    var c;
    if (g2048Won) {
        return;
    }
    for (r = 0; r < G2048_N; r++) {
        for (c = 0; c < G2048_N; c++) {
            if (g2048Grid[r][c] >= 2048) {
                g2048Won = true;
                g2048SetStatus("达成 2048！继续挑战更高分");
                return;
            }
        }
    }
}

function g2048CheckGameOver() {
    if (g2048HasEmpty() || g2048CanMove()) {
        return;
    }
    g2048Over = true;
    g2048SetStatus("游戏结束 · 点「新游戏」重来");
}

function g2048Move(dir) {
    if (g2048Over) {
        return;
    }
    if (!g2048ApplyMove(dir)) {
        return;
    }
    g2048AddRandom();
    if (g2048Score > g2048Best) {
        g2048Best = g2048Score;
    }
    g2048Render();
    g2048CheckWin();
    g2048CheckGameOver();
}

function g2048MoveLeft() {
    g2048Move("left");
}

function g2048MoveRight() {
    g2048Move("right");
}

function g2048MoveUp() {
    g2048Move("up");
}

function g2048MoveDown() {
    g2048Move("down");
}
