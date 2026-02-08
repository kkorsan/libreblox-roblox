/* Copyright 2003-2007 ROBLOX Corporation, All Rights Reserved */

#include "ErrorUploader.h"
#include "LogManager.h"

#include <boost/iostreams/device/file.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/concepts.hpp>  // source
#include <boost/filesystem.hpp> // Added for cross-platform file system operations

#include "util/Http.h"
#include "util/standardout.h"


#include "errno.h"

std::string ErrorUploader::MoveRelative(const std::string& fileName, std::string path)
{
	boost::filesystem::path source_path(fileName);
	boost::filesystem::path target_dir_path = source_path.parent_path();
	target_dir_path /= path; // Append the relative path to the base directory

	// Create the target directory if it doesn't exist.
	// boost::filesystem::create_directories handles creating parent directories as well.
	if (!boost::filesystem::exists(target_dir_path)) {
		boost::filesystem::create_directories(target_dir_path);
	}

	boost::filesystem::path file_name_only = source_path.filename();
	boost::filesystem::path final_target_path = target_dir_path / file_name_only; // Combine directory and filename

	// Emulate original Windows behavior: if MoveFile fails, DeleteFile the original.
	// boost::filesystem::rename is typically an atomic move for files on the same filesystem.
	// It throws an exception on failure (e.g., cross-device move, permissions).
	try {
		boost::filesystem::rename(source_path, final_target_path);
	} catch (const boost::filesystem::filesystem_error& e) {
		// If rename fails, delete the original file, matching the original Windows logic.
		// This behavior is a direct translation of the original "DeleteFile on MoveFile failure" logic.
		boost::filesystem::remove(source_path);
		// In a real-world scenario, you might want to log 'e.what()' here for debugging.
	}

	return final_target_path.string(); // Return the final target path as a standard string
}
