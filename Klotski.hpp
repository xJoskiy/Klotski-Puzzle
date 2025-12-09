

#include <array>
#include <cstdint>
#include <iostream>
#include <magic_enum/magic_enum.hpp>
#include <queue>
#include <unordered_map>
#include <vector>

namespace klotski {

#define KLOTSKI_INLINE __attribute__((always_inline))

constexpr std::uint8_t const height = 5;
constexpr std::uint8_t const width = 4;
constexpr std::uint8_t const piece_count = 10;

// mask_table[id][r][c]
std::vector<std::vector<std::vector<std::uint32_t>>> mask_table(
        piece_count,
        std::vector<std::vector<std::uint32_t>>(height, std::vector<std::uint32_t>(width)));

using Matrix = std::array<std::int8_t, height * width>;

KLOTSKI_INLINE constexpr std::size_t index(std::size_t row, std::size_t col) {
    return row * width + col;
}

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
    std::uint32_t occupancy_mask = 0;

    static constexpr int row_bits = 3;
    static constexpr int col_bits = 2;
    static constexpr int bits_per_piece = row_bits + col_bits;
    static constexpr std::uint8_t piece_mask = (1 << bits_per_piece) - 1;
    static constexpr std::uint8_t row_mask = (1 << row_bits) - 1;
    static constexpr std::uint8_t col_mask = (1 << col_bits) - 1;

    static_assert(bits_per_piece <= 8, "fits into byte for each piece");
    static_assert(bits_per_piece * piece_count <= 64, "fits into 64 bits");

    KLOTSKI_INLINE constexpr std::uint8_t piece_state(std::uint8_t idx) const noexcept {
        std::uint64_t offset = static_cast<std::uint64_t>(bits_per_piece) * idx;
        return (state_bits >> offset) & piece_mask;
    }

    KLOTSKI_INLINE constexpr std::uint8_t row(std::uint8_t idx) const noexcept {
        std::uint8_t state = piece_state(idx);
        return state & row_mask;
    }

    KLOTSKI_INLINE constexpr std::uint8_t col(std::uint8_t idx) const noexcept {
        std::uint8_t state = piece_state(idx);
        return (state >> row_bits) & col_mask;
    }

    KLOTSKI_INLINE void constexpr set_row(std::uint8_t idx, std::uint8_t val) noexcept {
        val &= row_mask;
        std::uint8_t state = piece_state(idx);
        state = (state & (col_mask << row_bits)) | val;

        set_piece_state(idx, state);
    }

    KLOTSKI_INLINE void constexpr set_col(std::uint8_t idx, std::uint8_t val) noexcept {
        val &= col_mask;
        std::uint8_t state = piece_state(idx);
        state = (state & row_mask) | (val << row_bits);

        set_piece_state(idx, state);
    }

    KLOTSKI_INLINE void constexpr set_piece_state(std::uint8_t idx, uint8_t state) {
        std::uint64_t offset = bits_per_piece * idx;
        state_bits &= ~(static_cast<std::uint64_t>(piece_mask) << offset);
        state_bits |= static_cast<std::uint64_t>(state & piece_mask) << offset;
    }

    KLOTSKI_INLINE constexpr std::uint64_t state() const noexcept {
        return state_bits;
    }

    KLOTSKI_INLINE void constexpr set_piece(std::uint8_t idx, std::uint8_t row, std::uint8_t col) {
        set_row(idx, row);
        set_col(idx, col);
    }

    State() = default;

    constexpr State(std::array<std::uint8_t, piece_count> rows,
                    std::array<std::uint8_t, piece_count> cols) {
        for (std::size_t i = 0; i < piece_count; ++i) {
            set_piece(i, rows[i], cols[i]);
        }
    }

    constexpr std::pair<std::uint8_t, std::uint8_t> operator[](std::uint8_t i) const {
        return {row(i), col(i)};
    }

    KLOTSKI_INLINE constexpr bool operator==(State const& lhs) const {
        return state_bits == lhs.state_bits;
    }

    KLOTSKI_INLINE constexpr bool operator!=(State const& lhs) const {
        return !(*this == lhs);
    }
};

struct StateHash {
    std::size_t operator()(State const& st) const noexcept {
        return static_cast<std::size_t>(st.state_bits);
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
    };

    Matrix matrix;
    State state;  //= State({0, 0, 4, 0, 2, 2, 2, 3, 3, 2}, {1, 0, 1, 3, 0, 1, 2, 1, 2, 3});

    // TODO: fill from initial matrix
    inline static std::vector<Piece> pieces{{2, 2}, {2, 1}, {1, 2}, {2, 1}, {2, 1},
                                            {1, 1}, {1, 1}, {1, 1}, {1, 1}, {2, 1}};

    Board(Matrix const& m) : matrix(m) {
        state = buildStateFromMatrix(m);
    }

    Matrix const& GetMatrix() const noexcept {
        return matrix;
    };

    // methods
    static void movePiece(std::vector<Board>& res, Board const& base, int piece_index,
                          directions dir) {
        using enum directions;

        State const& state = base.state;
        Matrix const& m = base.matrix;

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

        Board cur = base;
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

    std::vector<Board> GetNeighbours() {
        using enum directions;
        std::vector<Board> neighbors;
        neighbors.reserve(16);
        for (std::uint8_t i = 0; i < pieces.size(); i++) {
            for (auto dir : magic_enum::enum_values<directions>()) {
                movePiece(neighbors, *this, i, dir);
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
        ss << '\n';
        return ss.str();
    }

    static void buildMatrixFromState(State const& state, Matrix& out) {
        for (std::size_t i = 0; i < height * width; ++i) out[i] = -1;

        for (std::size_t id = 0; id < piece_count; ++id) {
            auto [row, col] = state[id];
            size_t ph = pieces[id].height;
            size_t pw = pieces[id].width;

            for (std::size_t dr = 0; dr < ph; ++dr)
                for (std::size_t dc = 0; dc < pw; ++dc) out[index(row + dr, col + dc)] = id;
        }
    }

    static State buildStateFromMatrix(Matrix const& matrix) {
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

static void PrintMask(uint32_t mask) {
    for (std::uint8_t r = 0; r < height; ++r) {
        for (std::uint8_t c = 0; c < width; ++c) {
            std::uint32_t bit = 1u << index(r, c);
            std::cout << ((mask & bit) ? "1 " : ". ");
        }
        std::cout << '\n';
    }
    std::cout << '\n';
}

class Solver {
public:
    // template <typename Predicate>
    static std::vector<Board::Move> Solve(Matrix const& matrix) {
        static auto pred = [](Matrix const& m) {
            bool p = m[index(4, 1)] == 0 and m[index(4, 2)] == 0;
            return p;
        };

        Board init(matrix);
        //
        // 0  1  2  3
        // 4  5  6  7
        // 8  9  10 11
        // 12 13 14 15
        // 16 17 18 19
        //
        // mask_table[idx][r][c] -> uint32_t
        //
        // auto make_mask = [](Board::Piece const& p, std::uint8_t r,
        //                     std::uint8_t c) -> std::uint32_t {
        //     auto [ph, pw] = p;
        //     std::uint32_t mask = 0;
        //     for (std::size_t i = r; i < r + ph; ++i) {
        //         for (std::size_t j = c; j < c + pw; ++j) {
        //             mask |= (1U << index(i, j));
        //         }
        //     }

        //     return mask;
        // };

        // for (std::size_t i = 0; i < piece_count; ++i) {
        //     auto [ph, pw] = Board::pieces[i];
        //     for (std::size_t r = 0; r + ph < height; ++r) {
        //         for (std::size_t c = 0; c + pw < width; ++c) {
        //             mask_table[i][r][c] = make_mask(Board::pieces[i], r, c);
        //         }
        //     }
        // }

        // for (std::size_t i = 0; i < piece_count; ++i) {
        //     auto [ph, pw] = Board::pieces[i];
        //     for (std::size_t r = 0; r + ph < height; ++r) {
        //         for (std::size_t c = 0; c + pw < width; ++c) {
        //             PrintMask(mask_table[i][r][c]);
        //         }
        //     }
        // }

        // for (int i = 0; i < piece_count; i++) {
        //     auto [row, col] = init.state[i];
        //     std::cout << static_cast<int>(row) << ' ' << static_cast<int>(col) << '\n';
        // }

        std::queue<Board> queue;
        queue.push(init);

        std::unordered_map<State, State, StateHash> prev{{init.state, init.state}};
        prev.reserve(8'000'000);

        while (!queue.empty()) {
            Board cur_board = std::move(queue.front());
            queue.pop();
            Matrix const& cur_matrix = cur_board.GetMatrix();

            if (pred(cur_matrix)) {
                std::cout << prev.size() << std::endl;
                return GetResult(prev, cur_board.state, init.state);
            }
            // if (prev.size() % 10000 == 0)
            // std::cout << prev.size() << '\n';
            std::vector<Board> neighbors = cur_board.GetNeighbours();
            for (auto& n : neighbors) {
                if (prev.find(n.state) == prev.end()) {
                    prev.emplace(n.state, cur_board.state);
                    queue.push(std::move(n));
                }
            }
        }
        std::cout << "No solution was found" << std::endl;
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

    static Board::Move GetNextMove(Matrix const& matrix) {
        std::vector<Board::Move> solution = Solve(matrix);
        return solution[0];
    }
};

// int main() {
//     Matrix initial = {1, 0, 0, 3, 1, 0, 0, 3, 4, 5, 6, 9, 4, 7, 8, 9, -1, 2, 2, -1};

//     // Matrix initial = {{1, 0, 0, 3},
//     // 		          {1, 0, 0, 3},
//     // 		   		  {4, 5, 6, 9},
//     // 		   		  {4, 7, 8, 9},
//     // 		   		  {-1, 2, 2, -1}};

//     // auto isSolved = [](Matrix const& conf) {
//     // 	bool p = conf[4][1] == 0 and conf[4][2] == 0 and conf[3][1] == 0 and conf[3][2] == 0;
//     // 	return p;
//     // };

//     auto isSolved = [](Matrix const& m) {
//         bool p = m[index(4, 1)] == 0 and m[index(4, 2)] == 0;
//         return p;
//     };

//     // auto isSolved = [](Matrix const& conf) {
//     // 	return false;
//     // };

//     auto ans = Solver::Solve(initial, isSolved);
//     for (auto const& m : ans) Board::Print(m);

//     return 0;
// }

}  // namespace klotski