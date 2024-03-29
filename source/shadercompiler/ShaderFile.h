#pragma once

#include <string>
#include <vector>
#include <set>
#include <filesystem>
#include <chrono>
#include "ShaderMetaConstant.h"

class ShaderFile {

	struct ShaderMetaConstantStorage {
		std::string mName;
		std::string mDefaultValue;
		std::string mWholeMatch;
	};

public:

	ShaderFile(const std::string& path);

	// If the file signature changes we reload the content of the original
	// shader file and clear all valid shader files
	void reload();

	// Compiles the shader by first replacing the constants and returns the
	// path to the spirv file. If already compiled and basefile didn't change
	// it will not recompile!
	std::string compile(const std::vector<ShaderMetaConstant>& constants);

private:
	std::vector<std::pair<std::filesystem::path, std::filesystem::file_time_type>> mDependencies; // A vector with all include files and their respective last change time.
	std::filesystem::path mPath;

	std::string mBaseCode = "";

	std::vector<ShaderMetaConstantStorage> mAllConstants;
	std::set<std::string> mValidShaderVariants;

	void extractConstants();

	/// <summary>
	/// Starting with the given file this function searches for all includes and adds them to the dependency list
	/// </summary>
	void loadDependencies(const std::filesystem::path& path);
	std::filesystem::path getTemporaryPathName(const std::string& ufid);
	std::filesystem::path getSpvPathName(const std::string& ufid);


};