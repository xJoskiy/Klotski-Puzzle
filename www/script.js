const board = document.getElementById('board');
const movesCounter = document.getElementById('moves');
let moves = 0;
let isSolving = false;

const ROWS = 5;
const COLS = 4;

// список блоков без пустых {id,type,row,col,h,w,el}
let pieces = [];


// начальная доска (top-left position)

//    1   0   0    3
//    1   0   0    3
//    4   5   6    9
//    4   7   8    9
//   -1   2   2   -1

const initialLayout = [
    { id: 0, type: 'large', row: 0, col: 1 },
    { id: 1, type: 'vertical', row: 0, col: 0 },
    { id: 2, type: 'horizontal', row: 4, col: 1 },
    { id: 3, type: 'vertical', row: 0, col: 3 },
    { id: 4, type: 'vertical', row: 2, col: 0 },
    { id: 5, type: 'tiny', row: 2, col: 1 },
    { id: 6, type: 'tiny', row: 2, col: 2 },
    { id: 7, type: 'tiny', row: 3, col: 1 },
    { id: 8, type: 'tiny', row: 3, col: 2 },
    { id: 9, type: 'vertical', row: 2, col: 3 },
    { id: 10, type: 'empty', row: 4, col: 0 },
    { id: 11, type: 'empty', row: 4, col: 3 }
];

function dimsForType(type) {
    switch (type) {
        case 'large': return { h: 2, w: 2 };
        case 'vertical': return { h: 2, w: 1 };
        case 'horizontal': return { h: 1, w: 2 };
        case 'tiny': return { h: 1, w: 1 };
        case 'empty': return { h: 1, w: 1 };
        default: return { h: 1, w: 1 };
    }
}

// матрица занятости, -1 если пусто, иначе id блока
function buildOccupancy() {
    const occ = Array.from({ length: ROWS }, () => Array.from({ length: COLS }, () => -1));
    for (const p of pieces) {
        for (let dr = 0; dr < p.h; ++dr) {
            for (let dc = 0; dc < p.w; ++dc) {
                const r = p.row + dr;
                const c = p.col + dc;
                if (r >= 0 && r < ROWS && c >= 0 && c < COLS) {
                    occ[r][c] = p.id;
                }
            }
        }
    }
    return occ;
}

function buildState() {
    const state = {}
    for (let id = 0; id < pieces.length; id++) {
        let p = findPieceById(id);
        state[id] = { "row": p.row, "col": p.col };
    }

    return state;
}

// Отрисовка сетки и фоновых ячеек, создание элементов блоков
function renderBoard() {
    board.innerHTML = '';
    moves = 0;
    updateMoves();

    // создаём фоновые ячейки (для сетки)
    for (let r = 0; r < ROWS; ++r) {
        for (let c = 0; c < COLS; ++c) {
            const cell = document.createElement('div');
            cell.className = 'piece empty'; // визуально пустая ячейка
            // разместим, но не используем для логики
            cell.style.gridColumnStart = c + 1;
            cell.style.gridRowStart = r + 1;
            board.appendChild(cell);
        }
    }

    pieces = [];
    for (const item of initialLayout) {
        if (item.type === 'empty') continue;
        const { h, w } = dimsForType(item.type);
        const el = document.createElement('div');
        el.className = 'piece';
        if (item.type === 'large') el.classList.add('large');
        if (item.type === 'vertical') el.classList.add('vertical');
        if (item.type === 'horizontal') el.classList.add('horizontal');
        if (item.type === 'tiny') el.classList.add('tiny');
        el.dataset.id = item.id;
        el.dataset.type = item.type;

        el.style.gridColumnStart = item.col + 1;
        el.style.gridColumnEnd = `span ${w}`;
        el.style.gridRowStart = item.row + 1;
        el.style.gridRowEnd = `span ${h}`;

        el.addEventListener('click', onPieceClick);

        // добавляем в DOM
        board.appendChild(el);

        pieces.push({
            id: item.id,
            type: item.type,
            row: item.row,
            col: item.col,
            h, w,
            el
        });
    }
}

function findPieceById(id) {
    return pieces.find(p => p.id === id);
}

// Проверка, можно ли переместить блок p на (dr, dc)
function canMove(p, dr, dc, occ) {
    const newRow = p.row + dr;
    const newCol = p.col + dc;

    if (newRow < 0 || newCol < 0) return false;
    if (newRow + p.h > ROWS) return false;
    if (newCol + p.w > COLS) return false;

    // каждая клетка новой позиции либо пустая (-1) либо p.id
    for (let r = 0; r < p.h; ++r) {
        for (let c = 0; c < p.w; ++c) {
            const rr = newRow + r;
            const cc = newCol + c;
            const occupant = occ[rr][cc];
            if (occupant !== -1 && occupant !== p.id) return false;
        }
    }
    return true;
}

function doMove(p, dr, dc) {
    p.row += dr;
    p.col += dc;

    // DOM - document object model, для обновления html разметки из js 
    p.el.style.gridColumnStart = p.col + 1;
    p.el.style.gridRowStart = p.row + 1;

    // flash/animation
    p.el.classList.add('moving');
    moves++;
    updateMoves();
    if (isSolved()) triggerVictory();

    setTimeout(() => p.el.classList.remove('moving'), 250);
}

function onPieceClick(event) {
    if (isSolving) {
        isSolving = false;
        return;
    }

    const el = event.currentTarget; // элемент, на котором произошёл клик
    const id = parseInt(el.dataset.id);
    const p = findPieceById(id);
    if (!p) return;

    // получаем прямоугольник элемента в пиксельных координатах
    const rect = el.getBoundingClientRect();
    const clickX = event.clientX - rect.left;  // X от левого края элемента
    const clickY = event.clientY - rect.top;   // Y от верхнего края элемента

    const width = rect.width;
    const height = rect.height;
    const k = height / width;

    const occ = buildOccupancy();

    const centerX = width / 2;
    const centerY = height / 2;
    const dx = clickX - centerX;
    const dy = centerY - clickY;

    let dr = 0, dc = 0;

    if (dy >= k * dx && dy > -k * dx) {
        dr = -1;
    } else if (dy < k * dx && dy >= -k * dx) {
        dc = 1;
    } else if (dy <= k * dx && dy < -k * dx) {
        dr = 1;
    } else if (dy > k * dx && dy <= -k * dx) {
        dc = -1;
    }

    if (canMove(p, dr, dc, occ)) {
        doMove(p, dr, dc);
    }
}

function updateMoves() {
    movesCounter.textContent = `Moves: ${moves}`;
}

function resetPuzzle() {
    isSolving = false;
    renderBoard();
}

async function solve() {
    if (isSolving || isSolved()) return;

    isSolving = true;

    const payload = buildState()

    try {
        const resp = await fetch("http://185.92.183.95:8080/solve", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify(payload)
        });

        if (!resp.ok) {
            console.error("Server error", resp.status);
            const errText = await resp.text();
            console.error(errText);
            return;
        }

        const data = await resp.json();
        console.log("Server response:", data.moves);

        for (const move of data.moves) {
            if (!isSolving) break;
            let piece = findPieceById(move["id"]);
            doMove(piece, move["drow"], move["dcol"]);
            await wait(500);
        }
    } catch (e) {
        console.error("Network error:", e);
    }

    isSolving = false;
}

async function hint() {
    if (isSolving || isSolved()) return;

    isSolving = true;

    const payload = buildState();

    try {
        let time = Date.now()
        const resp = await fetch("http://185.92.183.95:8080/hint", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify(payload)
        });

        console.log("Time to response", Date.now() - time);

        if (!resp.ok) {
            console.error("Server error", resp.status);
            const errText = await resp.text();
            console.error(errText);
            return;
        }

        const data = await resp.json();
        console.log("Server response:", data);

        let piece = findPieceById(data["id"]);
        if (!isSolving) return;
        doMove(piece, data["drow"], data["dcol"]);
        await wait(300);
    } catch (e) {
        console.error("Network error:", e);
    }

    isSolving = false;
}

function wait(ms) {
    return new Promise(resolve => setTimeout(resolve, ms));
}

function isSolved() {
    let p = findPieceById(0);
    if (p.row === 3 && p.col === 1) {
        return true;
    }
    return false;
}

function triggerVictory() {
    const largePiece = document.querySelector('.piece.large');
    largePiece.classList.add('victory');

    document.body.classList.add('victory-screen');

    setTimeout(() => {
        alert('🎉 Поздравляем! Вы решили головоломку!');
    }, 800);
}

window.onload = renderBoard;

