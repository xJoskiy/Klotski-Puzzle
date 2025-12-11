#include <chrono>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>

#include "Klotski.hpp"
#include "cpp-httplib/httplib.h"

namespace {

klotski::State getState(auto req) {
    nlohmann::json data = nlohmann::json::parse(req.body);

    klotski::State state;
    for (std::size_t i = 0; i < 10; i++) {
        auto coords = data[std::to_string(i)];
        std::uint8_t row = coords["row"];
        std::uint8_t col = coords["col"];
        state.set_piece(i, row, col);
    }

    return state;
}
}  // namespace

int main() {
    httplib::Server serv;

    auto cors = [&](httplib::Request const& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
    };

    serv.Options(R"(.*)", cors);

    serv.Post("/solve", [&](auto const& req, auto& res) {
        cors(req, res);
        try {
            auto state = getState(req);
            auto time = std::chrono::high_resolution_clock::now();
            auto moves = klotski::Solver::Solve(state);
            std::cout << "Solution was found in " << (std::chrono::duration_cast<std::chrono::milliseconds>(
                                  std::chrono::high_resolution_clock::now() - time))
                                 .count() / 1000 << " seconds"
                      << std::endl;

            nlohmann::json result;
            result["moves"] = nlohmann::json::array();
            std::stringstream ss;
            for (auto const& m : moves) {
                auto [drow, dcol] = klotski::Board::dir_to_dif.at(m.dir);
                result["moves"].push_back(
                        {{"id", static_cast<int>(m.id)}, {"drow", drow}, {"dcol", dcol}});
            }

            res.set_content(result.dump(), "application/json");

        } catch (std::exception const& e) {
            nlohmann::json err = {{"error", e.what()}};
            res.status = 400;
            res.set_content(err.dump(), "application/json");
        }
    });

    serv.Post("/hint", [&](auto const& req, auto& res) {
        cors(req, res);
        try {
            klotski::State state = getState(req);
            klotski::Board::Move move = klotski::Solver::GetNextMove(state);

            auto [drow, dcol] = klotski::Board::dir_to_dif.at(move.dir);
            nlohmann::json result = {
                    {"id", static_cast<int>(move.id)}, {"drow", drow}, {"dcol", dcol}};

            res.set_content(result.dump(), "application/json");

        } catch (std::exception const& e) {
            nlohmann::json err = {{"error", e.what()}};
            res.status = 400;
            res.set_content(err.dump(), "application/json");
        }
    });

    serv.listen("0.0.0.0", 8080);
}
