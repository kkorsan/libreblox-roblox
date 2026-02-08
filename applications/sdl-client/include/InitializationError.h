#pragma once

#include <stdexcept>

namespace RBX {

class initialization_error : public std::runtime_error {
public:
	initialization_error(const char* const errorMessage) :
	  std::runtime_error(errorMessage) {}
};

class graphics_initialization_error : public initialization_error {
public:
	graphics_initialization_error(const char* const errorMessage) :
	  initialization_error(errorMessage) {}
};

}