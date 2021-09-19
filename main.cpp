#include <iostream>
#include <fstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <regex>

#include <cadical.hpp>

const std::regex INPUT_VARIABLE(R"(\s*INPUT\s*\(\s*(\w+)\s*\)\s*)");
const std::regex OUTPUT_VARIABLE(R"(\s*OUTPUT\s*\(\s*(\w+)\s*\)\s*)");
const std::regex NOT(R"(\s*(\w+)\s*=\s*NOT\s*\((\w+)\))");
const std::regex BIN_OP(R"(\s*(\w+)\s*=\s*(AND|NAND|OR|NOR|XOR|NXOR)\s*\(\s*((\w+)\s*,\s*)+(\w+)\s*\)\s*)");
const std::regex NAME(R"(\w+)");

void comb(const std::vector<int>& input, std::vector<int>& combination, int offset, int k, CaDiCaL::Solver& solver) {
  if (k == 0) {
    for (int i = 0; i < input.size(); ++i) {
        if (combination[i] == 0) {
            solver.add(input[i]);
        } else {
            solver.add(-input[i]);
        }
    }
    solver.add(0);
    return;
  }
  for (int i = offset; i <= input.size() - k; ++i) {
    combination[i] = 1;
    comb(input, combination, i + 1, k - 1, solver);
    combination[i] = 0;
  }
}


int getOrInsert(std::unordered_map<std::string, int>& map, std::string&& s) {
    if (!map.contains(s)) {
        map.emplace(std::move(s), map.size() + 1);
        return map.size();
    } else {
        return map[s];
    }
}

void and_op(int res, std::vector<int>&& variables, CaDiCaL::Solver& solver) {
    for (int var : variables) {
        solver.add(var); solver.add(-res); solver.add(0);
    }
    for (int var : variables) {
        solver.add(-var);
    }
    solver.add(res); solver.add(0);
}

void nand_op(int res, std::vector<int>&& variables, CaDiCaL::Solver& solver) {
    for (int var : variables) {
        solver.add(var); solver.add(res); solver.add(0);
    }
    for (int var : variables) {
        solver.add(-var);
    }
    solver.add(-res); solver.add(0);
}

void or_op(int res, std::vector<int>&& variables, CaDiCaL::Solver& solver) {
    for (int var : variables) {
        solver.add(-var); solver.add(res); solver.add(0);
    }
    for (int var : variables) {
        solver.add(var);
    }
    solver.add(-res); solver.add(0);
}

void nor_op(int res, std::vector<int>&& variables, CaDiCaL::Solver& solver) {
    for (int var : variables) {
        solver.add(-var); solver.add(-res); solver.add(0);
    }
    for (int var : variables) {
        solver.add(var);
    }
    solver.add(res); solver.add(0);
}

void xor_op(int res, std::vector<int>&& variables, CaDiCaL::Solver& solver) {
    variables.push_back(res);
    std::vector<int> combi(variables.size(), 0);
    for (int i = 1; i <= variables.size(); i += 2) {
        comb(variables, combi, 0, i, solver);
    }
}

void nxor_op(int res, std::vector<int>&& variables, CaDiCaL::Solver& solver) {
    variables.push_back(res);
    std::vector<int> combi(variables.size(), 0);
    for (int i = 0; i <= variables.size(); i += 2) {
        comb(variables, combi, 0, i, solver);
    }
}

void binOp(const std::string& op, int res, std::vector<int>&& variables, CaDiCaL::Solver& solver) {
    if (op == "AND") {
        and_op(res, std::move(variables), solver);
    } else if (op == "NAND") {
        nand_op(res, std::move(variables), solver);
    } else if (op == "OR") {
        or_op(res, std::move(variables), solver);
    } else if (op == "NOR") {
        nor_op(res, std::move(variables), solver);
    } else if (op == "XOR") {
        xor_op(res, std::move(variables), solver);
    } else if (op == "NXOR") {
        nxor_op(res, std::move(variables), solver);
    } else {
        std::cerr << "Unknown Op " << op << std::endl;
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
            int res = getOrInsert(name_to_idx, match[1].str());
            std::string op = match[2].str();
            std::vector<int> variables;
            std::smatch name_match;
            int toSkip = 2;
            while (std::regex_search(line, name_match, NAME)) {
                if (toSkip > 0) {
                    line = name_match.suffix();
                    toSkip--;
                    continue;
                }
                variables.push_back(getOrInsert(name_to_idx, name_match[0].str()));
                line = name_match.suffix();
            }
            binOp(op, res, std::move(variables), solver);
        } else if (std::regex_match(line, match, NOT)) {
            std::string res_var = match[1].str();
            std::string in_var = match[2].str();
            int res = getOrInsert(name_to_idx, std::move(res_var));
            int in = getOrInsert(name_to_idx, std::move(in_var));
            solver.add(-res); solver.add(-in); solver.add(0);
            solver.add(res); solver.add(in); solver.add(0);
        } else {
            std::cerr << "Can not parse line: " << line << std::endl;
            return 3;
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
