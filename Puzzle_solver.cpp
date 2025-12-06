#include <array>
#include <iostream>
#include <queue>
#include <unordered_map>
#include <vector>

constexpr std::size_t const height = 5;
constexpr std::size_t const width = 4;
constexpr std::size_t const piece_count = 10;

using Matrix = std::array<int, height * width>;

inline constexpr std::size_t index(std::size_t row, std::size_t col) {
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

//  Положения верхних левых углов каждой фигуры
struct State {
    std::array<int, piece_count> row;
    std::array<int, piece_count> col;

    bool operator==(const State& lhs) const {
        return row == lhs.row and col == lhs.col;
    }

    bool operator!=(const State& lhs) const {
        return !(*this == lhs);
    }

    std::pair<int, int> operator[](int i) const {
        return {row[i], col[i]};
    }
};

struct ArrayHash {
    constexpr std::size_t operator()(Matrix const& matrix) const noexcept {
        // simple FNV-1a-ish combine (fast)
        std::size_t h = 1469598103934665603ULL;
        for (auto v : matrix) {
            h ^= static_cast<std::size_t>(v) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
        }
        return h;
    }
};

struct StateHash {
    size_t operator()(State const& state) const noexcept {
        size_t h = 1469598103934665603ULL;
        for (std::size_t i = 0; i < piece_count; ++i) {
            h ^= state.row[i];
            h *= 1099511628211ULL;

            h ^= state.col[i];
            h *= 1099511628211ULL;
        }
        return h;
    }
};

class Board {
public:
    struct Piece {
        size_t height;
        size_t width;
    };

    enum directions { LEFT, RIGHT, UP, DOWN };

    Matrix matrix;

    // const static data
    // Upper left corner of a Piece
    State positions{{0, 0, 4, 0, 2, 2, 2, 3, 3, 2}, {1, 0, 1, 3, 0, 1, 2, 1, 2, 3}};

    // TODO: fill from initial matrix
    static inline std::vector<Piece> pieces{{2, 2}, {2, 1}, {1, 2}, {2, 1}, {2, 1},
                                    {1, 1}, {1, 1}, {1, 1}, {1, 1}, {2, 1}};

    // cst
    Board(Matrix const& m) : matrix(m) {}

    Matrix const& GetMatrix() const noexcept {
        return matrix;
    };

    // methods
    static void movePiece(std::vector<Board>& res, Board const& base, int piece_index,
                          directions dir) {
        std::vector<Piece> const& pieces = base.pieces;
        State const& positions = base.positions;
        Matrix const& m = base.matrix;

        auto [h, w] = pieces[piece_index];
        auto [row, col] = positions[piece_index];

        int drow = 0, dcol = 0;

        switch (dir) {
            case LEFT:
                dcol = -1;
                if (col == 0) return;
                for (int i = row; i < row + h; ++i)
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
        auto& pos = cur.positions;

        switch (dir) {
            case LEFT:
                for (int i = row; i < row + h; ++i) {
                    matrix[index(i, col - 1)] = piece_index;
                    matrix[index(i, col + w - 1)] = -1;
                }
                break;

            case RIGHT:
                for (int i = row; i < row + h; ++i) {
                    matrix[index(i, col + w)] = piece_index;
                    matrix[index(i, col)] = -1;
                }
                break;

            case UP:
                for (int i = col; i < col + w; ++i) {
                    matrix[index(row - 1, i)] = piece_index;
                    matrix[index(row + h - 1, i)] = -1;
                }
                break;

            case DOWN:
                for (int i = col; i < col + w; ++i) {
                    matrix[index(row + h, i)] = piece_index;
                    matrix[index(row, i)] = -1;
                }
                break;
        }

        pos.row[piece_index] = row + drow;
        pos.col[piece_index] = col + dcol;
        res.push_back(std::move(cur));
    }

    std::vector<Board> GetNeighbours() {
        std::vector<Board> neighbors;
        neighbors.reserve(16);
        for (int i = 0; i < pieces.size(); i++) {
            movePiece(neighbors, *this, i, UP);
            // std::cout << neighbors.size() << std::endl;
            movePiece(neighbors, *this, i, DOWN);
            // std::cout << neighbors.size() << std::endl;
            movePiece(neighbors, *this, i, LEFT);
            // std::cout << neighbors.size() << std::endl;
            movePiece(neighbors, *this, i, RIGHT);
            // std::cout << neighbors.size() << std::endl;
        }
        return neighbors;
    }

    static void Print(Matrix const& matrix) {
        for (std::size_t i = 0; i < height; i++) {
            for (std::size_t j = 0; j < width; j++) {
                std::cout << matrix[index(i, j)] << ' ';
            }
            std::cout << '\n';
        }
        std::cout << '\n';
    }

    static void buildMatrixFromState(State const& state, Matrix& out) {
        for (int i = 0; i < height * width; ++i) out[i] = -1;

        for (int id = 0; id < piece_count; ++id) {
            auto [row, col] = state[id];
            int height = pieces[id].height;
            int width = pieces[id].width;

            for (int dr = 0; dr < height; ++dr)
                for (int dc = 0; dc < width; ++dc) out[index(row + dr, col + dc)] = id;
        }
    }
};

class Solver {
public:
    template <typename Predicate>
    static std::vector<Matrix> Solve(Matrix const& matrix, Predicate pred) {
        Board init(matrix);
        std::queue<Board> queue;
        queue.push(init);

        std::unordered_map<State, State, StateHash> prev{{init.positions, init.positions}};
        prev.reserve(8'000'000);

        while (!queue.empty()) {
            Board cur_board = std::move(queue.front());
            queue.pop();
            Matrix const& cur_matrix = cur_board.GetMatrix();

            if (pred(cur_matrix)) {
                std::cout << prev.size() << std::endl;
                return GetResult(prev, cur_board.positions, init.positions);
            }
            // if (prev.size() % 10000 == 0)
            //     std::cout << prev.size() << '\n';
            std::vector<Board> neighbors = cur_board.GetNeighbours();
            for (auto& n : neighbors) {
                if (prev.find(n.positions) == prev.end()) {
                    prev.emplace(n.positions, cur_board.positions);
                    queue.push(std::move(n));
                }
            }
        }
        std::cout << "No solution was found" << std::endl;
        return {};
    }

    static std::vector<Matrix> GetResult(
            std::unordered_map<State, State, StateHash> const& result, State cur, State const& init) {
        std::vector<State> states{cur};
        states.reserve(100);
        while (cur != init) {
            State prev = result.at(cur);
            states.push_back(prev);
            cur = std::move(prev);
        }

        std::vector<Matrix> res(states.size());
        for (std::size_t i = 0; i < res.size(); ++i) {
            Board::buildMatrixFromState(states[i], res[i]);
        }

        return res;
    }
};

int main() {
    Matrix initial = {1, 0, 0, 3, 1, 0, 0, 3, 4, 5, 6, 9, 4, 7, 8, 9, -1, 2, 2, -1};

    // Matrix initial = {{1, 0, 0, 3},
    // 		          {1, 0, 0, 3},
    // 		   		  {4, 5, 6, 9},
    // 		   		  {4, 7, 8, 9},
    // 		   		  {-1, 2, 2, -1}};

    // auto isSolved = [](Matrix const& conf) {
    // 	bool p = conf[4][1] == 0 and conf[4][2] == 0 and conf[3][1] == 0 and conf[3][2] == 0;
    // 	return p;
    // };

    auto isSolved = [](Matrix const& m) {
        bool p = m[index(4, 1)] == 0 and m[index(4, 2)] == 0;
        return p;
    };

    // auto isSolved = [](Matrix const& conf) {
    // 	return false;
    // };

    auto ans = Solver::Solve(initial, isSolved);
    for (auto const& m : ans) Board::Print(m);

    return 0;
}
