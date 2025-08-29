#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>

// Simple test program to check if C++23 features are working correctly with file operations
int main() {
    try {
        // Get current directory
        std::string current_path = std::filesystem::current_path().string();
        
        // Try to create a test log file
        std::ofstream log_file("test_log.txt");
        if (!log_file.is_open()) {
            std::cerr << "Failed to open log file for writing!" << std::endl;
            return 1;
        }
        
        // Write some test data
        log_file << "Current path: " << current_path << std::endl;
        log_file << "C++23 test log file created successfully." << std::endl;
        log_file.close();
        
        std::cout << "Log file created successfully at: " << current_path << "\\test_log.txt" << std::endl;
        std::cout << "Press Enter to exit..." << std::endl;
        std::cin.get();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        std::cout << "Press Enter to exit..." << std::endl;
        std::cin.get();
        return 1;
    }
}
