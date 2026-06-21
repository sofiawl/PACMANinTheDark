#ifndef LOG_H
#define LOG_H

#include <iostream>
#include <fstream>
#include <string>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <format>

void log(const std::string& origem, const std::string& nivel, const std::string& mensagem);

#endif