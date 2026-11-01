// main.cpp
// -----------------------------------------------------------------------------
// The command-line menu. This file only talks to the keyboard (std::cin) and the
// screen (std::cout). Every file operation is a call into record_manager.cpp.
// -----------------------------------------------------------------------------
#include <algorithm>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "student.hpp"

// ============================ input helpers ==================================

static std::string trim(const std::string& text) {
    const std::size_t first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    const std::size_t last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

// Print a prompt and read one line. Returns false when the input has ended
// (Ctrl+D, or the end of piped input) so the program can exit cleanly.
static bool readLine(const std::string& prompt, std::string& out) {
    std::cout << prompt;
    return static_cast<bool>(std::getline(std::cin, out));
}

// Keep asking until the user types a whole number between lo and hi.
// If 'keep' is given, pressing Enter on an empty line keeps the current value.
static bool readInt(const std::string& prompt, int lo, int hi, int& out, const int* keep = nullptr) {
    while (true) {
        std::string line;
        if (!readLine(prompt, line)) return false;
        line = trim(line);

        if (line.empty() && keep != nullptr) {
            out = *keep;
            return true;
        }
        try {
            std::size_t pos = 0;
            const int value = std::stoi(line, &pos);
            if (pos == line.size() && value >= lo && value <= hi) {
                out = value;
                return true;
            }
        } catch (const std::exception&) {
            // not a number - fall through to the message below
        }
        std::cout << "  Please enter a whole number from " << lo << " to " << hi << ".\n";
    }
}

// Same idea for names. Commas are rejected because the file is comma-separated.
static bool readName(const std::string& prompt, std::string& out, const std::string* keep = nullptr) {
    while (true) {
        std::string line;
        if (!readLine(prompt, line)) return false;
        line = trim(line);

        if (line.empty() && keep != nullptr) {
            out = *keep;
            return true;
        }
        if (line.empty()) {
            std::cout << "  The name cannot be empty.\n";
            continue;
        }
        if (line.find(',') != std::string::npos) {
            std::cout << "  Commas are not allowed (the data file is comma-separated).\n";
            continue;
        }
        out = line;
        return true;
    }
}

// ============================ output helpers =================================

static void printHeader() {
    std::cout << std::left << std::setw(8) << "ID" << std::setw(22) << "Name"
              << std::right << std::setw(7) << "Maths" << std::setw(9) << "Science"
              << std::setw(9) << "English" << std::setw(8) << "Avg" << std::setw(7) << "Grade" << '\n'
              << std::string(70, '-') << '\n';
}

static void printRow(const Student& s) {
    const std::string shownName = s.name.size() > 20 ? s.name.substr(0, 19) + "~" : s.name;
    std::cout << std::left << std::setw(8) << s.id << std::setw(22) << shownName
              << std::right << std::setw(7) << s.maths << std::setw(9) << s.science
              << std::setw(9) << s.english << std::setw(8) << std::fixed << std::setprecision(1)
              << s.average() << std::setw(7) << s.grade() << '\n';
}

static void printFailure(Status st) {
    switch (st) {
        case Status::NotFound:
            std::cout << "  No student found with that ID.\n";
            break;
        case Status::Duplicate:
            std::cout << "  A student with that ID already exists.\n";
            break;
        case Status::InvalidData:
            std::cout << "  Invalid data (names cannot be empty or contain commas).\n";
            break;
        case Status::IoError:
            std::cout << "  A file error occurred - see the error message above.\n";
            break;
        case Status::Ok:
            break;
    }
}

static void printMenu() {
    std::cout << "\n==== Student Grading and Record Management System ====\n"
              << "  1. Add a student\n"
              << "  2. Display all students\n"
              << "  3. Search by ID\n"
              << "  4. Search by name\n"
              << "  5. Update a student\n"
              << "  6. Delete a student\n"
              << "  7. Class statistics\n"
              << "  0. Exit\n";
}

// ============================ menu actions ===================================

static void handleAdd() {
    Student s;
    if (!readInt("Student ID (1-999999): ", 1, 999999, s.id)) return;

    // Check the ID first so the user does not type a whole record for nothing.
    Student existing;
    const Status lookup = findStudentById(s.id, existing);
    if (lookup == Status::Ok) {
        printFailure(Status::Duplicate);
        return;
    }
    if (lookup == Status::IoError) {
        printFailure(lookup);
        return;
    }

    if (!readName("Name: ", s.name)) return;
    if (!readInt("Maths   (0-100): ", 0, 100, s.maths)) return;
    if (!readInt("Science (0-100): ", 0, 100, s.science)) return;
    if (!readInt("English (0-100): ", 0, 100, s.english)) return;

    const Status st = addStudent(s);
    if (st == Status::Ok) {
        std::cout << "  Saved! Average " << std::fixed << std::setprecision(1) << s.average()
                  << ", grade " << s.grade() << ".\n";
    } else {
        printFailure(st);
    }
}

static void handleDisplayAll() {
    std::vector<Student> all;
    const Status st = loadAllStudents(all);
    if (st != Status::Ok) {
        printFailure(st);
        return;
    }
    if (all.empty()) {
        std::cout << "  No records yet. Use option 1 to add a student.\n";
        return;
    }
    printHeader();
    for (const Student& s : all) printRow(s);
    std::cout << std::string(70, '-') << '\n' << all.size() << " student(s)\n";
}

static void handleSearchById() {
    int id = 0;
    if (!readInt("Student ID to find: ", 1, 999999, id)) return;

    Student s;
    const Status st = findStudentById(id, s);
    if (st == Status::Ok) {
        printHeader();
        printRow(s);
    } else {
        printFailure(st);
    }
}

static void handleSearchByName() {
    std::string text;
    if (!readLine("Name contains: ", text)) return;
    text = trim(text);
    if (text.empty()) {
        std::cout << "  Please type at least one letter.\n";
        return;
    }

    std::vector<Student> matches;
    const Status st = searchByName(text, matches);
    if (st != Status::Ok) {
        printFailure(st);
        return;
    }
    if (matches.empty()) {
        std::cout << "  No names contain \"" << text << "\".\n";
        return;
    }
    printHeader();
    for (const Student& s : matches) printRow(s);
}

static void handleUpdate() {
    int id = 0;
    if (!readInt("Student ID to update: ", 1, 999999, id)) return;

    Student current;
    const Status found = findStudentById(id, current);
    if (found != Status::Ok) {
        printFailure(found);
        return;
    }

    std::cout << "Current record:\n";
    printHeader();
    printRow(current);
    std::cout << "(Press Enter to keep a value unchanged)\n";

    Student updated = current; // start from the old values
    if (!readName("New name [" + current.name + "]: ", updated.name, &current.name)) return;
    if (!readInt("Maths   [" + std::to_string(current.maths) + "]: ", 0, 100, updated.maths, &current.maths)) return;
    if (!readInt("Science [" + std::to_string(current.science) + "]: ", 0, 100, updated.science, &current.science)) return;
    if (!readInt("English [" + std::to_string(current.english) + "]: ", 0, 100, updated.english, &current.english)) return;

    const Status st = updateStudent(updated);
    if (st == Status::Ok) {
        std::cout << "  Record updated.\n";
    } else {
        printFailure(st);
    }
}

static void handleDelete() {
    int id = 0;
    if (!readInt("Student ID to delete: ", 1, 999999, id)) return;

    Student s;
    const Status found = findStudentById(id, s);
    if (found != Status::Ok) {
        printFailure(found);
        return;
    }

    printHeader();
    printRow(s);

    std::string answer;
    if (!readLine("Really delete this student? (y/N): ", answer)) return;
    answer = trim(answer);
    if (answer.empty() || (answer[0] != 'y' && answer[0] != 'Y')) {
        std::cout << "  Cancelled. Nothing was deleted.\n";
        return;
    }

    const Status st = deleteStudent(id);
    if (st == Status::Ok) {
        std::cout << "  Record deleted.\n";
    } else {
        printFailure(st);
    }
}

static void handleStats() {
    std::vector<Student> all;
    const Status st = loadAllStudents(all);
    if (st != Status::Ok) {
        printFailure(st);
        return;
    }
    if (all.empty()) {
        std::cout << "  No records yet.\n";
        return;
    }

    double sumOfAverages = 0.0;
    const Student* best = &all[0];
    const Student* lowest = &all[0];
    for (const Student& s : all) {
        sumOfAverages += s.average();
        if (s.average() > best->average()) best = &s;
        if (s.average() < lowest->average()) lowest = &s;
    }

    std::cout << std::fixed << std::setprecision(1)
              << "Students       : " << all.size() << '\n'
              << "Class average  : " << sumOfAverages / static_cast<double>(all.size()) << '\n'
              << "Top student    : " << best->name << " (" << best->average() << ")\n"
              << "Lowest average : " << lowest->name << " (" << lowest->average() << ")\n"
              << "Grade spread   :";
    for (const char* grade : {"A+", "A", "B", "C", "D", "F"}) {
        const auto count = std::count_if(all.begin(), all.end(),
                                         [&](const Student& s) { return s.grade() == grade; });
        std::cout << "  " << grade << "=" << count;
    }
    std::cout << '\n';
}

// ================================= main ======================================

int main() {
    while (true) {
        printMenu();

        int choice = 0;
        if (!readInt("Choose an option: ", 0, 7, choice)) {
            break; // input ended (Ctrl+D or end of piped input)
        }
        std::cout << '\n';

        switch (choice) {
            case 1: handleAdd(); break;
            case 2: handleDisplayAll(); break;
            case 3: handleSearchById(); break;
            case 4: handleSearchByName(); break;
            case 5: handleUpdate(); break;
            case 6: handleDelete(); break;
            case 7: handleStats(); break;
            case 0:
                std::cout << "Goodbye!\n";
                return 0;
            default: break; // readInt already limits the range to 0-7
        }
    }

    std::cout << "\nInput closed. Goodbye!\n";
    return 0;
}
