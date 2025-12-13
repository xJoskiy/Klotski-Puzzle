

#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <magic_enum/magic_enum.hpp>
#include <queue>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace klotski {

#define KLOTSKI_INLINE __attribute__((always_inline))

constexpr std::uint8_t height = 5;
constexpr std::uint8_t width = 4;
constexpr std::uint8_t piece_count = 10;

static std::array<std::array<std::array<std::uint32_t, width>, height>, 10> mask_table{};

using Matrix = std::array<std::int8_t, height * width>;

KLOTSKI_INLINE static constexpr std::size_t index(std::size_t row, std::size_t col) {
    return row * width + col;
}

constexpr static std::array<std::pair<int, int>, 10> pieces{
        {std::pair<int, int>{2, 2}, std::pair<int, int>{2, 1}, std::pair<int, int>{1, 2},
         std::pair<int, int>{2, 1}, std::pair<int, int>{2, 1}, std::pair<int, int>{1, 1},
         std::pair<int, int>{1, 1}, std::pair<int, int>{1, 1}, std::pair<int, int>{1, 1},
         std::pair<int, int>{2, 1}}};


static inline constexpr std::array<std::array<uint8_t, 4>, 4> piece_groups = {{
            {{0}},           // Группа 0: уникальный блок 2×2
            {{1, 3, 4, 9}},  // Группа 1: все 2×1 блоки
            {{2}},           // Группа 2: единственный 1×2 блок
            {{5, 6, 7, 8}},  // Группа 3: четыре 1×1 блока
    }};


class Board;

/*
 *   1   0   0    3
 *   1   0   0    3
 *   4   5   6    9
 *   4   7   8    9
 *  -1   2   2   -1
 */

// 0 0   -> 4 положения
// 0 0 0 -> 8 положений

// 5 байт на деталь
// всего 10 деталей

// Upper left corners of each piece
struct State {
    // Младшие биты -> строка
    // Старшие биты -> колонка
    std::uint64_t state_bits = 0;

    static constexpr int row_bits = 3;
    static constexpr int col_bits = 2;
    static constexpr int bits_per_piece = row_bits + col_bits;
    static constexpr std::uint8_t piece_mask = (1 << bits_per_piece) - 1;
    static constexpr std::uint8_t row_mask = (1 << row_bits) - 1;
    static constexpr std::uint8_t col_mask = (1 << col_bits) - 1;

    static_assert(bits_per_piece <= 8, "fits into byte for each piece");
    static_assert(bits_per_piece * piece_count <= 64, "fits into 64 bits");

    KLOTSKI_INLINE constexpr std::uint8_t piece_state(std::uint8_t id) const noexcept {
        std::uint64_t offset = static_cast<std::uint64_t>(bits_per_piece) * id;
        return (state_bits >> offset) & piece_mask;
    }

    KLOTSKI_INLINE constexpr std::uint8_t row(std::uint8_t id) const noexcept {
        std::uint8_t state = piece_state(id);
        return state & row_mask;
    }

    KLOTSKI_INLINE constexpr std::uint8_t col(std::uint8_t id) const noexcept {
        std::uint8_t state = piece_state(id);
        return (state >> row_bits) & col_mask;
    }

    KLOTSKI_INLINE void constexpr set_row(std::uint8_t id, std::uint8_t val) noexcept {
        val &= row_mask;
        std::uint8_t state = piece_state(id);
        state = (state & (col_mask << row_bits)) | val;

        set_piece_state(id, state);
    }

    KLOTSKI_INLINE void constexpr set_col(std::uint8_t id, std::uint8_t val) noexcept {
        val &= col_mask;
        std::uint8_t state = piece_state(id);
        state = (state & row_mask) | (val << row_bits);

        set_piece_state(id, state);
    }

    KLOTSKI_INLINE void constexpr set_piece_state(std::uint8_t id, uint8_t state) {
        // filling state bits
        std::uint64_t offset = bits_per_piece * id;
        state_bits &= ~(static_cast<std::uint64_t>(piece_mask) << offset);
        state_bits |= static_cast<std::uint64_t>(state & piece_mask) << offset;
    }

    KLOTSKI_INLINE constexpr std::uint64_t state() const noexcept {
        return state_bits;
    }

    KLOTSKI_INLINE void constexpr set_piece(std::uint8_t id, std::uint8_t row, std::uint8_t col) {
        std::uint32_t new_state = row | (col << row_bits);
        set_piece_state(id, new_state);
    }

    State() = default;

    constexpr State(std::array<std::uint8_t, piece_count> rows,
                    std::array<std::uint8_t, piece_count> cols) {
        for (std::size_t i = 0; i < piece_count; ++i) {
            set_piece(i, rows[i], cols[i]);
        }
    }

    constexpr std::pair<std::uint8_t, std::uint8_t> operator[](std::uint8_t id) const {
        std::uint8_t state = piece_state(id);
        std::uint8_t col = (state >> row_bits) & col_mask;
        std::uint8_t row = state & row_mask;
        return {row, col};
    }

    KLOTSKI_INLINE constexpr bool operator==(State const& rhs) const {
        for (auto const& grp : piece_groups) {
            std::array<uint8_t, 16> va{}, vb{};
            int k = 0;

            for (uint8_t id : grp) {
                va[k] = (this->row(id) << 2) | this->col(id);
                vb[k] = (rhs.row(id) << 2) | rhs.col(id);
                k++;
            }

            std::sort(va.begin(), va.begin() + k);
            std::sort(vb.begin(), vb.begin() + k);

            for (int i = 0; i < k; ++i)
                if (va[i] != vb[i]) return false;
        }
        return true;
    }

    KLOTSKI_INLINE constexpr bool operator!=(State const& lhs) const {
        return !(*this == lhs);
    }
};

class Board {
public:
    enum class directions { LEFT, RIGHT, UP, DOWN };
    inline static std::unordered_map<directions, std::pair<int, int>> const dir_to_dif{
            {directions::LEFT, {0, -1}},
            {directions::RIGHT, {0, 1}},
            {directions::UP, {-1, 0}},
            {directions::DOWN, {1, 0}}};

    struct Piece {
        std::uint8_t height;
        std::uint8_t width;
    };

    struct Move {
        std::uint8_t id;
        directions dir;

        bool operator==(Move const& move) const = default;
    };

    State state;
    Matrix matrix;

    inline static std::vector<Piece> pieces{{2, 2}, {2, 1}, {1, 2}, {2, 1}, {2, 1},
                                            {1, 1}, {1, 1}, {1, 1}, {1, 1}, {2, 1}};

    Board(State const& st) : state(st) {
        matrix = buildMatrixFromState(st);
    }

    Board(Board const& board) = default;
    Board(Board&& board) = default;

    // methods
    static void movePiece(std::vector<Board>& res, Board const& board, int piece_index,
                          directions dir) {
        using enum directions;

        State const& state = board.state;
        Matrix const& m = board.matrix;

        auto [h, w] = Board::pieces[piece_index];
        auto [row, col] = state[piece_index];

        int8_t drow = 0, dcol = 0;

        switch (dir) {
            case LEFT:
                dcol = -1;
                if (col == 0) return;
                for (std::uint8_t i = row; i < row + h; ++i)
                    if (m[index(i, col - 1)] != -1) return;
                break;

            case RIGHT:
                dcol = 1;
                if (col + w == width) return;
                for (int i = row; i < row + h; ++i)
                    if (m[index(i, col + w)] != -1) return;
                break;

            case UP:
                drow = -1;
                if (row == 0) return;
                for (int i = col; i < col + w; ++i)
                    if (m[index(row - 1, i)] != -1) return;
                break;

            case DOWN:
                drow = 1;
                if (row + h == height) return;
                for (int i = col; i < col + w; ++i)
                    if (m[index(row + h, i)] != -1) return;
                break;
        }

        Board cur = board;
        Matrix& matrix = cur.matrix;
        State& pos = cur.state;

        switch (dir) {
            case LEFT:
                for (std::uint8_t i = row; i < row + h; ++i) {
                    matrix[index(i, col - 1)] = piece_index;
                    matrix[index(i, col + w - 1)] = -1;
                }
                break;

            case RIGHT:
                for (std::uint8_t i = row; i < row + h; ++i) {
                    matrix[index(i, col + w)] = piece_index;
                    matrix[index(i, col)] = -1;
                }
                break;

            case UP:
                for (std::uint8_t i = col; i < col + w; ++i) {
                    matrix[index(row - 1, i)] = piece_index;
                    matrix[index(row + h - 1, i)] = -1;
                }
                break;

            case DOWN:
                for (std::uint8_t i = col; i < col + w; ++i) {
                    matrix[index(row + h, i)] = piece_index;
                    matrix[index(row, i)] = -1;
                }
                break;
        }

        pos.set_piece(piece_index, row + drow, col + dcol);
        res.push_back(std::move(cur));
    }

    inline static std::vector<Board> GetNeighbours(Board const& board) {
        std::vector<Board> neighbors;
        neighbors.reserve(16);
        for (std::uint8_t i = 0; i < pieces.size(); i++) {
            for (auto dir : magic_enum::enum_values<directions>()) {
                movePiece(neighbors, board, i, dir);
            }
        }

        return neighbors;
    }

    static std::string asString(Matrix const& matrix) {
        std::stringstream ss;
        for (std::uint8_t i = 0; i < height; i++) {
            for (std::uint8_t j = 0; j < width; j++) {
                ss << static_cast<int>(matrix[index(i, j)]) << ' ';
            }
            ss << '\n';
        }
        return ss.str();
    }

    static Matrix buildMatrixFromState(State const& state) {
        Matrix out;
        for (std::size_t i = 0; i < height * width; ++i) out[i] = -1;

        for (std::size_t id = 0; id < piece_count; ++id) {
            auto [row, col] = state[id];
            size_t ph = pieces[id].height;
            size_t pw = pieces[id].width;

            for (std::size_t dr = 0; dr < ph; ++dr)
                for (std::size_t dc = 0; dc < pw; ++dc) out[index(row + dr, col + dc)] = id;
        }
        return out;
    }

    static constexpr State buildStateFromMatrix(Matrix const& matrix) {
        std::array<std::uint8_t, piece_count> rows{};
        std::array<std::uint8_t, piece_count> cols{};
        std::array<bool, piece_count> found{};

        for (std::size_t i = 0; i < height * width; ++i) {
            std::int8_t id = matrix[i];
            if (id == -1 || found[id]) continue;

            std::size_t row = i / width;
            std::size_t col = i % width;
            rows[id] = row;
            cols[id] = col;
            found[id] = true;
        }

        return {rows, cols};
    }
};

struct StateHash {
    std::size_t operator()(State const& st) const noexcept {
        uint64_t acc = 0xcbf29ce484222325ULL;
        for (auto const& grp : piece_groups) {
            std::array<uint8_t, 16> vals{};
            int k = 0;
            for (uint8_t id : grp) {
                uint8_t r = st.row(id);
                uint8_t c = st.col(id);
                vals[k++] = (r << 2) | c;
            }

            std::sort(vals.begin(), vals.begin() + k);

            for (int i = 0; i < k; ++i) {
                acc ^= vals[i];
                acc *= 0x100000001b3ULL;
            }

            acc ^= 0xff;
            acc *= 0x100000001b3ULL;
        }

        return static_cast<size_t>(acc);
    }
};

class Solver {
public:
    static std::vector<Board::Move> Solve(State const& init_state) {
        static auto pred = [](State const& state) {
            auto [row, col] = state[0];
            return row == 3 and col == 1;
        };

        Board init_board{init_state};

        std::queue<Board> queue;
        queue.push(init_state);

        std::unordered_map<State, State, StateHash> prev{{init_state, init_state}};
        prev.reserve(10'000);

        while (!queue.empty()) {
            Board cur_board = queue.front();
            queue.pop();

            if (pred(cur_board.state)) {
                return GetResult(prev, cur_board.state, init_state);
            }

            std::vector<Board> neighbors = Board::GetNeighbours(cur_board);
            for (auto& n : neighbors) {
                auto n_state = n.state;
                if (prev.find(n_state) == prev.end()) {
                    prev.emplace(n_state, cur_board.state);
                    queue.push(n);
                }
            }
        }
        return {};
    }

    static std::vector<Board::Move> GetResult(
            std::unordered_map<State, State, StateHash> const& result, State cur,
            State const& init) {
        std::vector<State> states{cur};
        states.reserve(100);
        while (cur != init) {
            State prev = result.at(cur);
            states.push_back(prev);
            cur = prev;
        }

        // Which move to make so you get from cur to next state
        auto getMove = [](State const& cur, State const& next) -> Board::Move {
            using enum Board::directions;
            for (std::uint8_t i = 0; i < piece_count; ++i) {
                std::uint8_t cur_row = cur.row(i);
                std::uint8_t cur_col = cur.col(i);
                std::uint8_t next_row = next.row(i);
                std::uint8_t next_col = next.col(i);

                if (next_row == cur_row + 1) return {i, DOWN};
                if (next_row + 1 == cur_row) return {i, UP};
                if (next_col == cur_col + 1) return {i, RIGHT};
                if (next_col + 1 == cur_col) return {i, LEFT};
            }
            assert(false);
            __builtin_unreachable();
        };

        std::vector<Board::Move> moves;
        for (std::size_t i = states.size() - 1; i > 0; --i) {
            moves.push_back(getMove(states[i], states[i - 1]));
        }

        return moves;
    }

    static std::optional<Board::Move> GetNextMove(State const& state) {
        std::vector<Board::Move> solution = Solve(state);
        return solution.size() ? std::optional<Board::Move>(solution[0]) : std::nullopt;
    }

    static void ApplyMoves(std::vector<Board::Move> const& moves, State& st) {
        for (Board::Move move : moves) {
            auto id = move.id;
            auto [row, col] = st[id];
            auto [dr, dc] = Board::dir_to_dif.at(move.dir);
            st.set_piece(id, row + dr, col + dc);
        }
    }
};

}  // namespace klotski
