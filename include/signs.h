#pragma once
#include <vector>
#include <string>

struct font {
    std::string name;
    int sizeX;
    int sizeY;
    int offset;
    std::vector<std::vector<int>> data;
};

extern std::vector<int> letterU;
extern std::vector<int> letterR;
extern std::vector<int> degreeSymbol;
extern std::vector<int> minusSymbol;
extern std::vector<std::vector<int>> smallNumbers;
extern std::vector<std::vector<int>> bigNumbers;
extern std::vector<std::vector<int>> weatherIcons;
extern std::vector<font> fonts;
