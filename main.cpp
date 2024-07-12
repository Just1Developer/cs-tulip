#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <chrono>
#include "bitvector.h"

using std::string;

struct command {
    char cmd;
    uint8_t bitValue;
    uint64 position, reply;
};

command getCommand(string&);
void processCommand(command&, bitvector&);

/**
 * This is the main entry point of the bitvector. Please provide the relative filepath for the input file as the first
 * argument. If no such argument is given, or the argument is not an existing file, the program will terminate immediately.
 * <p/>
 * If the file is not a proper input file, the program will run into errors and undefined behavior, so, don't do that.
 * <p/>
 * First, the file is opened and read. A bitvector skeleton is created
 * (no helper structures), and the commands are read and parsed into command structs. This provides faster access later.
 * <p/>
 * Then, the timer starts and helpers are created. In evaluation builds, a second timer is started to measure only query
 * time. All commands are processed and answered, writing their result in the command's reply property. After the timer
 * is stopped, all replies are printed and the RESULT and EVAL, if set, are printed after.
 * @param argc The number of arguments.
 * @param argv The command line arguments.
 * @return
 */
int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Please input a file to open in the first command line argument." << std::endl;
        return 1;
    }
#ifndef CONSOLE
    if (argc < 3) {
        std::cerr << "Please define an output file in the second command line argument or compile with the -DCONSOLE flag." << std::endl;
        return 2;
    }
#endif

    std::ifstream inFile(argv[1]);
    if (!inFile.is_open()) {
        std::cerr << "Could not open file " << argv[1] << std::endl;
        return 3;
    }

    uint64 cmdCount;
    string line, vectorStr;
    std::vector<command> commands;

    // We assume that there is definitely a command count and a bitvector.
    std::getline(inFile, line);
    std::getline(inFile, vectorStr);
    // line should contain command count
    cmdCount = std::stoll(line);
    // Read all commands:
    commands.reserve(cmdCount);
    for (int i = 0; i < cmdCount; ++i) {
        std::getline(inFile, line);
        commands.emplace_back(getCommand(line));
    }
    inFile.close();

    // Create Basic Bitvector without helper structures
    bitvector bitvector(vectorStr);

    // Start the timer
    auto start = std::chrono::high_resolution_clock::now();
    bitvector.buildHelpers();
    auto querystart = std::chrono::high_resolution_clock::now();

    for (auto& cmd : commands) {
        processCommand(cmd, bitvector);
    }

    auto stop = std::chrono::high_resolution_clock::now();
    auto time = duration_cast<std::chrono::milliseconds>(stop - start);
#ifdef EVAL
    auto querytime = duration_cast<std::chrono::nanoseconds>(stop - querystart);
#endif
    auto space = bitvector.size();

#ifdef CONSOLE
    for (auto cmd : commands) {
        std::cout << cmd.reply << std::endl;
    }
#else
    // We definitely have an argument here, create the file, write all outputs, flush, save and close it.
    // This is asserted above.
    std::string filename = argv[2];

    // Create the directory if it does not exist
    std::filesystem::path filePath(filename);
    std::filesystem::path dir = filePath.parent_path();
    if (!dir.empty() && !std::filesystem::exists(dir)) {
        if (!std::filesystem::create_directories(dir)) {
            std::cerr << "Could not create the directory " << dir << std::endl;
            return 5;
        }
    }

    // (Create and) Open the file
    std::ofstream outFile(filename, std::ios::out | std::ios::trunc);

    // See if the file was opened
    if (outFile.is_open()) {
        for (const auto& cmd : commands) {
            outFile << cmd.reply << std::endl;
        }
        outFile.flush();
        outFile.close();
    } else {
        std::cerr << "Could not open the output file " << filename << "." << std::endl;
        return 4;
    }
#endif

    std::cout << "RESULT name=just1developer time=" << time.count() << " space=" << space << std::endl;
#ifdef EVAL
    std::cout << "EVAL query-only-time=" << querytime.count() << std::endl;
#endif
    return 0;
}

/**
 * Creates a new command struct from a string. Valid Commands are \<access | rank | select> \<first number> [second number],
 * In the format provided by Florian Kurpicz:<br/>
 * - access <index><br/>
 * - rank <0/1> <position><br/>
 * - select <0/1> <number><br/>
 * If an invalid command string is provided returns the default command of access 0 [0].
 * @param cmd The command as read from the file. If on windows, may contain trailing \\r.
 * @return A command struct representing the given command.
 */
command getCommand(string& cmd) {
    // \r? is to allow the \r which windows has in addition to \n in the line breaks
    // This regex parses all valid / allowed commands. If access theoretically has two arguments, it would be allowed but ignored.
    std::regex pattern(R"(^(\baccess\b|\brank\b|\bselect\b) (\d+)(?: (\d+))?\r?$)");
    std::smatch matches;

    // Even though it should be a valid query, place an if to be safe
    if (std::regex_search(cmd, matches, pattern)) {
        char commandChar = matches[1].str()[0];
        uint8_t bitValue;
        uint64 position;

        // Single argument command
        if (commandChar == 'a') {
            position = (uint64) std::stoll(matches[2].str());
            return command { commandChar, 0, position };
        } else {
            // Two arguments, where first one is only 0 or 1.
            bitValue = (char) std::stoi(matches[2].str());
            position = (int) (matches.size() > 3 && matches[3].matched ? std::stoll(matches[3].str()) : 0 /* :) */);

            return command { commandChar, bitValue, position };
        }
    }
    // Return a default command. It will work, but it will be really obvious if something
    // went wrong at this stage.
    return command {'a', 0, 0};
}

/**
 * Processes a given command on the provided bitvector. To save time, the result is stored in
 * the reply property of the given command struct and not sent to IO immediately.
 * This allows to have the console IO outside of measured time.
 * @param cmd The command.
 * @param vect The bitvector.
 */
void processCommand(command& cmd, bitvector& vect) {
    switch (cmd.cmd) {
        case 'a':
            cmd.reply = vect.access(cmd.position);
            return;
        case 'r':
            cmd.reply = vect.rank(cmd.position, cmd.bitValue);
            return;
        case 's':
            cmd.reply = vect.select(cmd.position, cmd.bitValue);
            return;
    }
}
