#include <iostream>
#include <fstream>
#include <unordered_map>
#include <unordered_set>
#include <regex>

#include <cadical.hpp>

const std::regex INPUT_VARIABLE(R"(INPUT\((\w+)\))");
const std::regex OUTPUT_VARIABLE(R"(OUTPUT\((\w+)\))");
const std::regex AND(R"((\w+) = AND\((\w+), (\w+)\))");
const std::regex NAND(R"((\w+) = NAND\((\w+), (\w+)\))");
const std::regex OR(R"((\w+) = OR\((\w+), (\w+)\))");
const std::regex NOR(R"((\w+) = NOR\((\w+), (\w+)\))");
const std::regex NOT(R"((\w+) = NOT\((\w+)\))");
const std::regex XOR(R"((\w+) = XOR\((\w+), (\w+)\))");
const std::regex XNOR(R"((\w+) = XNOR\((\w+), (\w+)\))");
const std::regex BIN_OP(R"((\w+) = (AND|NAND|OR|NOR|XOR|XNOR)\((\w+), (\w+)\))");

int getOrInsert(std::unordered_map<std::string, int>& map, std::string&& s) {
    if (!map.contains(s)) {
        map.emplace(std::move(s), map.size() + 1);
        return map.size();
    } else {
        return map[s];
    }
}

void binOp(std::string&& op, int res, int left, int right, CaDiCaL::Solver& solver) {
    if (op == "AND") {
        solver.add(-left); solver.add(-right); solver.add(res); solver.add(0);
        solver.add(left); solver.add(-res); solver.add(0);
        solver.add(right); solver.add(-res); solver.add(0);
    } else if (op == "NAND") {
        solver.add(-left); solver.add(-right); solver.add(-res); solver.add(0);
        solver.add(left); solver.add(res); solver.add(0);
        solver.add(right); solver.add(res); solver.add(0);
    } else if (op == "OR") {
        solver.add(left); solver.add(right); solver.add(-res); solver.add(0);
        solver.add(-left); solver.add(res); solver.add(0);
        solver.add(-right); solver.add(res); solver.add(0);
    } else if (op == "NOR") {
        solver.add(left); solver.add(right); solver.add(res); solver.add(0);
        solver.add(-left); solver.add(-res); solver.add(0);
        solver.add(-right); solver.add(-res); solver.add(0);
    } else if (op == "XOR") {
        solver.add(-left); solver.add(-right); solver.add(-res); solver.add(0);
        solver.add(left); solver.add(right); solver.add(-res); solver.add(0);
        solver.add(left); solver.add(-right); solver.add(res); solver.add(0);
        solver.add(-left); solver.add(right); solver.add(res); solver.add(0);
    } else if (op == "XNOR") {
        solver.add(left); solver.add(right); solver.add(res); solver.add(0);
        solver.add(left); solver.add(-right); solver.add(-res); solver.add(0);
        solver.add(-left); solver.add(-right); solver.add(res); solver.add(0);
        solver.add(-left); solver.add(right); solver.add(-res); solver.add(0);
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: translator BENCH_FILE" << std::endl;
        return 1;
    }

    std::unordered_map<std::string, int> name_to_idx;
    std::unordered_set<std::string> input_nodes;
    std::string output_variable;

    CaDiCaL::Solver solver;

    std::ifstream input(argv[1]);
    if (!input.is_open()) {
        return 2;
    }

    for (std::string line; std::getline(input, line); ) {
        if (line.size() == 0 || line[0] == '#') {
            continue;
        }

        std::smatch match;
        if (std::regex_match(line, match, INPUT_VARIABLE)) {
            std::string variable = match[1].str();
            input_nodes.insert(variable);
            getOrInsert(name_to_idx, std::move(variable));
        } else if (std::regex_match(line, match, OUTPUT_VARIABLE)) {
            std::string variable = match[1].str();
            int res = getOrInsert(name_to_idx, std::move(variable));
            solver.assume(res);
        } else if (std::regex_match(line, match, BIN_OP)) {
            std::string res_var = match[1].str();
            std::string left_var = match[3].str();
            std::string right_var = match[4].str();
            int res = getOrInsert(name_to_idx, std::move(res_var));
            int left = getOrInsert(name_to_idx, std::move(left_var));
            int right = getOrInsert(name_to_idx, std::move(right_var));

            std::string op = match[2].str();
            binOp(std::move(op), res, left, right, solver);
        } else if (std::regex_match(line, match, NOT)) {
            std::string res_var = match[1].str();
            std::string in_var = match[2].str();
            int res = getOrInsert(name_to_idx, std::move(res_var));
            int in = getOrInsert(name_to_idx, std::move(in_var));
            solver.add(-res); solver.add(-in); solver.add(0);
            solver.add(res); solver.add(in); solver.add(0);
        }
    }

    int res = solver.solve();
    if (res == 10) {
        std::cout << "SATISFIABLE" << std::endl;
        std::cout << "inputs:" << std::endl;
        for (const auto& input : input_nodes) {
            std::cout << "  " << input << ": " << (solver.val(name_to_idx[input]) > 0 ? "true" : "false") << std::endl;
        }
    } else if (res == 20) {
        std::cout << "UNSATISFIABLE" << std::endl;
    } else {
        std::cout << "UNKNOWN STATUS" << std::endl;
        return 2;
    }
}
