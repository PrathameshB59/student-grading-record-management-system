// student.hpp
// -----------------------------------------------------------------------------
// 1) The Student record (one Student object  <->  one line in students.csv)
// 2) The list of functions that record_manager.cpp provides.
//
// main.cpp includes this file and calls the functions below.
// ALL file-stream code (ifstream / ofstream / std::filesystem) lives in
// record_manager.cpp - main.cpp only talks to the keyboard and the screen.
// -----------------------------------------------------------------------------
#pragma once

#include <string>
#include <vector>

// The flat-file "database". Both files live in the folder you run the program from.
inline constexpr const char* DB_FILE = "students.csv";   // the real database
inline constexpr const char* TEMP_FILE = "students.tmp"; // scratch file for update/delete

// One line of students.csv looks like this:
//     id,name,maths,science,english
//     101,Asha Patil,88,92,79
struct Student {
    int id = 0;
    std::string name;
    int maths = 0;
    int science = 0;
    int english = 0;

    int total() const { return maths + science + english; }
    double average() const { return total() / 3.0; }

    std::string grade() const {
        const double avg = average();
        if (avg >= 90) return "A+";
        if (avg >= 80) return "A";
        if (avg >= 70) return "B";
        if (avg >= 60) return "C";
        if (avg >= 50) return "D";
        return "F";
    }
};

// What happened when we tried to touch the file.
enum class Status {
    Ok,          // success (for the "find" functions: the student WAS found)
    NotFound,    // no student with that ID (or the database file does not exist yet)
    Duplicate,   // tried to add an ID that already exists
    InvalidData, // e.g. a name containing a comma, which would break the CSV format
    IoError      // a file could not be opened / written / renamed (details go to std::cerr)
};

// WRITE  : append one new record to the end of the file (std::ofstream + std::ios::app)
Status addStudent(const Student& s);

// READ   : load every record into a vector (std::ifstream + std::getline + std::stringstream)
Status loadAllStudents(std::vector<Student>& out);

// READ   : look up one student by ID
Status findStudentById(int id, Student& out);

// READ   : find all students whose name contains the given text (case-insensitive)
Status searchByName(const std::string& fragment, std::vector<Student>& out);

// MANIPULATE : replace the record that has updated.id  (temporary-file technique)
Status updateStudent(const Student& updated);

// MANIPULATE : remove the record with this ID           (temporary-file technique)
Status deleteStudent(int id);
