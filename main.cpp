#include <iostream>
#include <nlohmann/json.hpp>
#include <string>

#include "Klotski.hpp"
#include "cpp-httplib/httplib.h"

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
            auto data = nlohmann::json::parse(req.body);
            std::string state = data["state"];

            std::array<std::array<int, 4>, 5> matrix = nlohmann::json::parse(state);
            std::array<std::int8_t, 20> arr;
            for (int i = 0; i < 5; i++) {
                for (int j = 0; j < 4; j++) {
                    arr[i * 4 + j] = matrix[i][j];
                }
            }

            auto moves = klotski::Solver::Solve(arr);

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
            auto data = nlohmann::json::parse(req.body);
            std::string state = data["state"];

            std::array<std::array<int, 4>, 5> matrix = nlohmann::json::parse(state);
            std::array<std::int8_t, 20> arr;
            for (int i = 0; i < 5; i++) {
                for (int j = 0; j < 4; j++) {
                    arr[i * 4 + j] = matrix[i][j];
                }
            }

            klotski::Board::Move move = klotski::Solver::GetNextMove(arr);

            auto [drow, dcol] = klotski::Board::dir_to_dif.at(move.dir);
            nlohmann::json result = {
                {"id", static_cast<int>(move.id)},
                {"drow", drow},
                {"dcol", dcol}
            };

            res.set_content(result.dump(), "application/json");

        } catch (std::exception const& e) {
            nlohmann::json err = {{"error", e.what()}};
            res.status = 400;
            res.set_content(err.dump(), "application/json");
        }
    });

    serv.listen("0.0.0.0", 8080);
}
