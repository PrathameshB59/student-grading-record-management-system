// record_manager.cpp
// -----------------------------------------------------------------------------
// ALL the file handling lives here:
//   * WRITE       -> std::ofstream with std::ios::app            (addStudent)
//   * READ        -> std::ifstream + std::getline + stringstream (load/find/search)
//   * MANIPULATE  -> temporary-file technique + std::filesystem  (update/delete)
//
// QUICK REFERENCE - the four state flags every stream carries:
//   stream.good()  -> everything is fine, last operation worked
//   stream.eof()   -> we tried to read PAST the end of the file (normal at the end)
//   stream.fail()  -> the last operation failed (bad format, or could not open)
//   stream.bad()   -> a serious I/O error (disk problem, etc.)
//
//   if (!stream)  is the same as  if (stream.fail())
//   while (std::getline(stream, line))  keeps looping while the read succeeded.
// -----------------------------------------------------------------------------
#include "student.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

// ============================== small helpers ================================

// Does students.csv exist? The error_code version never throws; if something goes
// wrong, ec is filled in and we simply answer "no".
static bool dbExists() {
    std::error_code ec;
    return fs::exists(DB_FILE, ec);
}

// Files created on Windows end lines with "\r\n". getline removes the '\n' but
// leaves the '\r', so we strip it ourselves.
static void stripCarriageReturn(std::string& line) {
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
}

// "88" -> 88. Returns false for things like "abc", "12abc" or "".
static bool toInt(const std::string& text, int& value) {
    try {
        std::size_t pos = 0;
        value = std::stoi(text, &pos);
        return pos == text.size(); // the WHOLE text must be a number
    } catch (const std::exception&) {
        return false;
    }
}

static std::string toLower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

// A name must not contain ',' (our separator) or a line break, otherwise one
// student would be split across several CSV fields / lines.
static bool nameIsSafe(const std::string& name) {
    return !name.empty() && name.find_first_of(",\r\n") == std::string::npos;
}

// Student object -> one CSV line (WITHOUT the trailing newline).
static std::string toCsvLine(const Student& s) {
    std::ostringstream ss; // a stringstream is just a "file" that lives in memory
    ss << s.id << ',' << s.name << ',' << s.maths << ',' << s.science << ',' << s.english;
    return ss.str();
}

// One CSV line -> Student object. Returns false if the line is malformed.
//
// std::getline(stream, text, ',') reads characters until the next comma (instead
// of the next newline) - that is how we cut one line into fields.
static bool parseLine(const std::string& line, Student& s) {
    std::stringstream ss(line);
    std::string idText, name, mathsText, scienceText, englishText;

    // Every getline returns the stream; the stream converts to false when the read
    // failed (e.g. the line had fewer than 5 fields).
    if (!std::getline(ss, idText, ',')) return false;
    if (!std::getline(ss, name, ',')) return false;
    if (!std::getline(ss, mathsText, ',')) return false;
    if (!std::getline(ss, scienceText, ',')) return false;
    if (!std::getline(ss, englishText, ',')) return false;

    if (!toInt(idText, s.id) || !toInt(mathsText, s.maths) ||
        !toInt(scienceText, s.science) || !toInt(englishText, s.english)) {
        return false;
    }
    if (s.maths < 0 || s.maths > 100 || s.science < 0 || s.science > 100 ||
        s.english < 0 || s.english > 100) {
        return false;
    }
    s.name = name;
    return true;
}

// =============================== READING =====================================

Status loadAllStudents(std::vector<Student>& out) {
    out.clear();

    // First run: the file simply does not exist yet. That is not an error, it
    // just means "no students so far".
    if (!dbExists()) {
        return Status::Ok;
    }

    std::ifstream in(DB_FILE);

    // STATE CHECK 1: did the file actually open? (It can fail even if the file
    // exists, e.g. no read permission.)
    if (!in.is_open()) {
        std::cerr << "Error: could not open '" << DB_FILE << "' for reading.\n";
        return Status::IoError;
    }

    std::string line;
    int lineNumber = 0;

    // STATE CHECK 2: the loop condition IS a state check. getline returns the
    // stream; when it hits end-of-file (or an error) the stream turns "false"
    // and the loop stops.
    while (std::getline(in, line)) {
        ++lineNumber;
        stripCarriageReturn(line);
        if (line.empty()) continue; // ignore blank lines

        Student s;
        if (parseLine(line, s)) {
            out.push_back(s);
        } else {
            std::cerr << "Warning: skipping malformed line " << lineNumber << " in '" << DB_FILE << "'.\n";
        }
    }

    // STATE CHECK 3: why did the loop end? eof() is the normal reason. bad() means
    // a real hardware/OS problem while reading.
    if (in.bad()) {
        std::cerr << "Error: I/O problem while reading '" << DB_FILE << "'.\n";
        return Status::IoError;
    }

    in.close(); // done reading - release the file (it would also close automatically at the end of the function)
    return Status::Ok;
}

Status findStudentById(int id, Student& out) {
    if (!dbExists()) {
        return Status::NotFound;
    }

    std::ifstream in(DB_FILE);
    if (!in.is_open()) {
        std::cerr << "Error: could not open '" << DB_FILE << "' for reading.\n";
        return Status::IoError;
    }

    std::string line;
    while (std::getline(in, line)) {
        stripCarriageReturn(line);
        Student s;
        if (line.empty() || !parseLine(line, s)) continue;

        if (s.id == id) {
            out = s;
            in.close(); // we can stop early - no need to read the rest of the file
            return Status::Ok;
        }
    }

    if (in.bad()) {
        std::cerr << "Error: I/O problem while reading '" << DB_FILE << "'.\n";
        return Status::IoError;
    }

    in.close();
    return Status::NotFound; // we reached end-of-file without a match
}

Status searchByName(const std::string& fragment, std::vector<Student>& out) {
    std::vector<Student> all;
    const Status st = loadAllStudents(all); // reuse the reading code above
    if (st != Status::Ok) return st;

    out.clear();
    const std::string needle = toLower(fragment);
    for (const Student& s : all) {
        if (toLower(s.name).find(needle) != std::string::npos) {
            out.push_back(s);
        }
    }
    return Status::Ok; // an empty 'out' simply means "no matches"
}

// =============================== WRITING =====================================

Status addStudent(const Student& s) {
    if (!nameIsSafe(s.name)) {
        return Status::InvalidData;
    }

    // Refuse duplicate IDs. Careful: for findStudentById, "Ok" means FOUND.
    Student existing;
    const Status lookup = findStudentById(s.id, existing);
    if (lookup == Status::Ok) return Status::Duplicate;
    if (lookup == Status::IoError) return Status::IoError;

    // std::ios::app = "append". Every write goes to the END of the file, the old
    // content is never touched, and the file is created if it does not exist.
    std::ofstream out(DB_FILE, std::ios::app);

    // STATE CHECK 1: did the file open? (A folder without write permission would fail here.)
    if (!out.is_open()) {
        std::cerr << "Error: could not open '" << DB_FILE << "' for appending.\n";
        return Status::IoError;
    }

    out << toCsvLine(s) << '\n'; // one student = one line

    // STATE CHECK 2: did the write work? (A full disk would set failbit/badbit here.)
    if (out.fail()) {
        std::cerr << "Error: failed while writing to '" << DB_FILE << "'.\n";
        return Status::IoError;
    }

    // close() flushes the buffered text to disk. Output is buffered, so some
    // errors only show up at this moment - check the state AGAIN after closing.
    out.close();
    if (out.fail()) {
        std::cerr << "Error: failed to finish writing '" << DB_FILE << "'.\n";
        return Status::IoError;
    }
    return Status::Ok;
}

// ========================= UPDATE / DELETE (temp file) =======================
//
// A text file cannot be edited "in the middle": you cannot just delete line 3.
// The classic, safe solution is the TEMPORARY FILE TECHNIQUE:
//
//   1. open the real file for reading           (students.csv)
//   2. open a new temp file for writing         (students.tmp)
//   3. copy line by line:
//        - normal line          -> copy unchanged
//        - the target line      -> DELETE: write nothing / UPDATE: write the new line
//   4. close both files
//   5. replace the old database with the temp file (std::filesystem::rename)
//
// If anything goes wrong before step 5, the original file is still untouched.
//
// replacement == nullptr  -> delete the record
// replacement != nullptr  -> overwrite the record with *replacement
static Status rewriteDatabase(int targetId, const Student* replacement) {
    if (!dbExists()) {
        return Status::NotFound;
    }

    // Step 1: input stream on the real database
    std::ifstream in(DB_FILE);
    if (!in.is_open()) {
        std::cerr << "Error: could not open '" << DB_FILE << "' for reading.\n";
        return Status::IoError;
    }

    // Step 2: output stream on the temp file. std::ios::trunc = start empty even
    // if an old temp file was left behind by a crash.
    std::ofstream tmp(TEMP_FILE, std::ios::trunc);
    if (!tmp.is_open()) {
        std::cerr << "Error: could not create temporary file '" << TEMP_FILE << "'.\n";
        in.close();
        return Status::IoError;
    }

    // Step 3: copy line by line
    bool found = false;
    bool ioError = false;
    std::string line;

    while (std::getline(in, line)) {
        stripCarriageReturn(line);
        if (line.empty()) continue;

        Student current;
        const bool parsed = parseLine(line, current);

        if (parsed && current.id == targetId) {
            found = true;
            if (replacement != nullptr) {
                tmp << toCsvLine(*replacement) << '\n'; // UPDATE: write the new version
            }
            // DELETE: write nothing, the line is simply left out of the new file
        } else {
            // Not our target (or a malformed line): copy it unchanged so we never
            // lose data by accident.
            tmp << line << '\n';
        }

        // Check the OUTPUT stream after writing. If it failed, stop immediately.
        if (!tmp.good()) {
            ioError = true;
            break;
        }
    }

    // Did the reading loop stop because of a real error (not just end-of-file)?
    if (in.bad()) {
        ioError = true;
    }

    // Step 4: close BOTH files before touching them with std::filesystem.
    // (Renaming a file that is still open for writing could lose buffered data.)
    in.close();
    tmp.close();
    if (tmp.fail()) {
        ioError = true; // flushing to disk failed
    }

    std::error_code ec; // lets std::filesystem report errors without throwing exceptions

    if (ioError) {
        std::cerr << "Error: I/O problem while rewriting the database. Original file kept.\n";
        fs::remove(TEMP_FILE, ec); // clean up the temp file
        return Status::IoError;
    }

    if (!found) {
        fs::remove(TEMP_FILE, ec); // nothing changed, so the temp file is useless
        return Status::NotFound;
    }

    // Step 5: replace the old database with the new one.
    // On Linux, rename() atomically overwrites the old file, so there is never a
    // moment where the database is missing.
    fs::rename(TEMP_FILE, DB_FILE, ec);
    if (ec) {
        std::cerr << "Error: could not replace '" << DB_FILE << "': " << ec.message() << '\n';
        fs::remove(TEMP_FILE, ec);
        return Status::IoError;
    }
    return Status::Ok;
}

Status updateStudent(const Student& updated) {
    if (!nameIsSafe(updated.name)) {
        return Status::InvalidData;
    }
    return rewriteDatabase(updated.id, &updated);
}

Status deleteStudent(int id) {
    return rewriteDatabase(id, nullptr);
}
