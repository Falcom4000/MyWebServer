// 在当前目录创建文件并写入随机数据


#include <iostream>
#include <fstream>
#include <string>
#include <cassert>
#include <fmt/core.h>
#include <sys/stat.h>
using namespace std;

int main()
{   
    string path = "./log";
    string fileName = "2024_12_24.log";
    string fullName = fmt::format("{}/{}", path, fileName);
    auto fp = fopen(fullName.c_str(), "a");
    if (fp == nullptr)
    {
        mkdir(path.c_str(), 0777);
        fp = fopen(fullName.c_str(), "a");
    }
    assert(fp != nullptr);
    return 0;
}