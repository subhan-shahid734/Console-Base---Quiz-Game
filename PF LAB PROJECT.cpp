/*================================================================================
PROJECT: Console - Based Quiz Game
AUTHORS : Subhan, Asad, Hamza
VERSION : 1.2
DATE : 09 / 12 / 2025

DESCRIPTION:
An interactive quiz game featuring :
-5 Categories(Science, Computer, Sports, History, IQ)
- 3 Difficulty Levels
- Real - time countdown timer without blocking input
- Lifelines(50 / 50, Skip, Replace, Extra Time)
- Persistent High Score and Log tracking
- Input validation to prevent crashes

NOTE :
-Uses <conio.h> for _kbhit() (keyboard hit detection).
- Uses <thread> and <chrono> for accurate timing and sleeping.
================================================================================
*/

#include <iostream>
#include <cstdlib>      // For rand() and srand()
#include <fstream>      // For file handling (txt files)
#include <string>       // For string manipulation
#include <ctime>        // For time()
#include <algorithm>    // For swap()
#include <chrono>       // For high-resolution timer
#include <thread>       // For this_thread::sleep_for()
#include <conio.h>      // For _kbhit() and _getch() (Windows specific)

using namespace std;

// ======================= CONFIGURATION =======================
const int TOTAL_QUESTIONS = 150;     // Max capacity for loading questions
const int SESSION_QUESTIONS = 10;    // How many questions per game

// ======================= GLOBAL VARIABLES =======================

// --- File & Question Data ---
string filename;                     // Current category filename
string questions[TOTAL_QUESTIONS];   // Buffer to hold loaded lines from file
int indices[50];                     // Indices available for current difficulty
int used_indices[50];                // Track used questions to prevent repeats
int used_count = 0;                  // How many questions used so far

// --- Game State ---
int score = 0;                       // Current player score
int category = 0;                    // User selected category (1-5)
int difficulty = 0;                  // User selected difficulty (1-3)
string playername;                   // Player name for logging
int streak = 0;                      // Consecutive correct answers
int timer = 15;                      // Seconds allowed per question
bool replace_requested = false;      // Flag to trigger question replacement

// --- Lifeline Status (True = Available, False = Used) ---
bool lifeline_5050 = true;
bool lifeline_skip = true;
bool lifeline_replace = true;
bool lifeline_extratime = true;

// --- Review System Data ---
// Stores details of wrong answers to show at the end
string incorrect_questions[SESSION_QUESTIONS];
string incorrect_options[SESSION_QUESTIONS][4];
int incorrect_correct_pos[SESSION_QUESTIONS];
int incorrect_count = 0;             // Total wrong answers
int correct_count = 0;               // Total correct answers
int wrong_count = 0;                 // Total wrong answers (same as above, distinct counter)

// ======================= FUNCTION PROTOTYPES =======================
string cut(string& s);
void shuffle_array(int arr[], int n);
int load_questions(string filename);
void display_question(string question, string options[], int correct_pos, int q_num, bool is_review);
void start_quiz();
void save_high_score(string player, int score, string category, string difficulty);
void show_high_scores();
bool get_answer_with_timer(int& answer, int time_limit);
void display_timer_bar(int remaining);
void use_lifeline_5050(string options[], int correct_pos);
void use_lifeline_skip();
bool use_lifeline_replace(int& current_index, int used_indices[], int& used_count);
void use_lifeline_extratime(int& timer_ref);
void reset_lifelines();
string get_current_datetime();
void save_quiz_log(string player, string category_str, string difficulty_str, int correct, int wrong, int total_score);
void review_incorrect_questions();
void post_quiz_menu();

// ======================= MAIN EXECUTION =======================

int main() {

    srand(time(0)); // Seed random number generator with current time

    while (true) {
        // --- Main Menu Display ---
        system("cls");
        cout << "========================================\n";
        cout << "          CONSOLE-BASED QUIZ GAME\n";
        cout << "========================================\n";
        cout << "1. Start New Quiz\n";
        cout << "2. View High Scores\n";
        cout << "3. Exit\n";
        cout << "Enter choice: ";

        int choice;
        // INPUT VALIDATION: Prevents crash if user enters text (e.g., 'a')
        if (!(cin >> choice)) {
            cout << "\nInvalid Input! Please enter a number.\n";
            cin.clear();            // Clear error flag
            cin.ignore();// Discard bad input
            system("pause");
            continue;               // Restart loop
        }
        cin.ignore();    // Clear newline character from buffer

        if (choice == 3) break;     // Exit application

        switch (choice) {
        case 1:
        {
            // --- New Game Setup ---
            system("cls");
            cout << "Enter your name: ";
            getline(cin, playername);

            // Category Selection with Validation Loop
            while (true) {
                cout << "\n=== SELECT CATEGORY ===\n";
                cout << "1. Science\n2. Computer\n3. Sports\n4. History\n5. IQ/Logic\n";
                cout << "Enter choice: ";
                if (cin >> category && category >= 1 && category <= 5) {
                    cin.ignore(10000, '\n');
                    break;
                }
                cout << "Invalid category! Try again.\n";
                cin.clear();
                cin.ignore();
            }

            // Difficulty Selection with Validation Loop
            while (true) {
                cout << "\n=== SELECT DIFFICULTY ===\n";
                cout << "1. Easy\n2. Medium\n3. Hard\n";
                cout << "Enter choice: ";
                if (cin >> difficulty && difficulty >= 1 && difficulty <= 3) {
                    cin.ignore();
                    break;
                }
                cout << "Invalid difficulty! Try again.\n";
                cin.clear();
                cin.ignore();
            }

            // Reset all game state variables before starting
            streak = 0;
            timer = 15;
            incorrect_count = 0;
            correct_count = 0;
            wrong_count = 0;
            reset_lifelines();

            // Begin the quiz
            start_quiz();
            break;
        }
        case 2:
            system("cls");
            show_high_scores();
            break;
        default:
            cout << "Invalid choice.\n";
            system("pause");
        }
    }

    cout << "\nThank you for playing!\n";
    return 0;
}

// ======================= UTILITY FUNCTIONS =======================

/*
 * Function: cut
 * Purpose: Splits a string based on the '|' delimiter.
 * Mechanism: Finds the first '|', returns the substring before it,
 *            and modifies the original string to remove that part.
 */
string cut(string& s) {
    int pos = s.find('|');
    if (pos == string::npos) {
        string result = s;
        s = ""; // String is empty now
        return result;
    }
    string part = s.substr(0, pos);
    s = s.substr(pos + 1); // Update original string
    return part;
}

/*
 * Function: shuffle_array
 * Purpose: Randomizes an integer array using Fisher-Yates algorithm.
 * Used For: Randomizing question order and option order.
 */
void shuffle_array(int arr[], int n) {
    for (int i = n - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        int temp = arr[i];
        arr[i] = arr[j];
        arr[j] = temp;
    }
}

/*
 * Function: load_questions
 * Purpose: Reads the specific text file into the 'questions' array.
 */
int load_questions(string filename) {
    ifstream fin(filename);
    if (!fin.is_open()) return 0; // File error

    int count = 0;
    while (getline(fin, questions[count]) && count < TOTAL_QUESTIONS) {
        count++;
    }
    fin.close();
    return count; // Returns number of questions loaded
}

/*
 * Function: get_current_datetime
 * Purpose: Returns current system time formatted as "YYYY-MM-DD HH:MM:SS"
 *          Used for logging.
 */
string get_current_datetime() {
    time_t now = time(0);
    tm t;
    localtime_s(&t, &now);
    char buffer[50];
    sprintf_s(buffer, "%04d-%02d-%02d %02d:%02d:%02d",
        1900 + t.tm_year, 1 + t.tm_mon, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
    return string(buffer);
}

// ======================= TIMER & INPUT LOGIC =======================

/*
 * Function: display_timer_bar
 * Purpose: Updates the timer text on the same line using '\r' (Carriage Return).
 * Note: '\r' moves the cursor to the start of the line, allowing overwrite.
 */
void display_timer_bar(int remaining) {
    cout << "\r[ TIME LEFT: " << remaining << "s ] Your answer (1-4) or Lifeline (5-8): ";
    cout.flush(); // Forces the text to appear immediately
}

/*
 * Function: get_answer_with_timer
 * Purpose: The core non-blocking input loop.
 * Logic:
 * 1. Records start time.
 * 2. Enters a loop that checks time elapsed.
 * 3. Uses _kbhit() to check if keyboard key is pressed WITHOUT pausing execution.
 * 4. If key pressed -> Read input.
 * 5. If time out -> Return false.
 */
bool get_answer_with_timer(int& answer, int time_limit) {
    auto start_time = chrono::steady_clock::now();
    bool input_received = false;

    // Show initial timer
    display_timer_bar(time_limit);

    while (!input_received) {
        // Calculate remaining time
        auto current_time = chrono::steady_clock::now();
        auto elapsed = chrono::duration_cast<chrono::seconds>(current_time - start_time).count();
        int remaining = time_limit - (int)elapsed;

        // Update display if time is left
        if (remaining >= 0) {
            display_timer_bar(remaining);
        }

        // Check if Time is Up
        if (elapsed >= time_limit) {
            cout << "\n\nTime's up!\n";
            while (_kbhit()) _getch(); // Clear any accidental keystrokes buffered during timeout
            return false;
        }

        // Check for Input (Non-blocking)
        if (_kbhit()) {
            // Attempt to read integer
            if (cin >> answer) {
                cin.ignore(10000, '\n');
                return true; // Valid input received
            }
            else {
                // User typed a letter, clear error and let them try again
                cin.clear();
                cin.ignore(10000, '\n');
            }
        }

        // Small sleep to reduce CPU usage
        this_thread::sleep_for(chrono::milliseconds(100));
    }
    return false;
}

// ======================= LIFELINE LOGIC =======================

void reset_lifelines() {
    lifeline_5050 = true;
    lifeline_skip = true;
    lifeline_replace = true;
    lifeline_extratime = true;
}

// Removes 2 incorrect options from the display array
void use_lifeline_5050(string options[], int correct_pos) {
    if (!lifeline_5050) {
        cout << "\n[!] 50/50 already used!\n";
        return;
    }
    lifeline_5050 = false;
    cout << "\n[LIFELINE] 50/50 Used. Removing 2 options...\n\n";

    int removed = 0;
    // Iterate and remove first 2 options that are NOT the correct one
    for (int i = 0; i < 4 && removed < 2; ++i) {
        if (i != correct_pos) {
            options[i] = "[REMOVED]";
            removed++;
        }
    }
}

// Just sets the flag; actual logic happens in display_question (it returns)
void use_lifeline_skip() {
    if (!lifeline_skip) {
        cout << "\n[!] Skip already used!\n";
        return;
    }
    lifeline_skip = false;
    cout << "\n[LIFELINE] Question Skipped!\n";
}

// Finds a new question index that hasn't been used yet
bool use_lifeline_replace(int& current_index, int used_indices[], int& used_count) {
    if (!lifeline_replace) {
        cout << "\n[!] Replace already used!\n";
        return false;
    }
    lifeline_replace = false;
    cout << "\n[LIFELINE] Replace Question Used. Finding new question...\n";

    int start_index = (difficulty - 1) * 50;

    // Linear search for an unused question index
    for (int i = start_index; i < start_index + 50; i++) {
        bool is_used = false;
        for (int j = 0; j < used_count; j++) {
            if (used_indices[j] == i) {
                is_used = true;
                break;
            }
        }
        if (!is_used) {
            current_index = i;              // Update current index
            used_indices[used_count++] = i; // Mark new one as used
            return true;
        }
    }
    return false;
}

// Adds 10 seconds to the reference timer variable
void use_lifeline_extratime(int& timer_ref) {
    if (!lifeline_extratime) {
        cout << "\nExtra Time already used!\n";
        return;
    }
    lifeline_extratime = false;
    timer_ref += 10;
    cout << "\n[LIFELINE] +10 Seconds Added!\n";
}

// ======================= DATA PERSISTENCE =======================

// Appends detailed logs to quiz_logs.txt
void save_quiz_log(string player, string category_str, string difficulty_str, int correct, int wrong, int total_score) {
    ofstream fout("quiz_logs.txt", ios::app);
    if (!fout.is_open()) {
        cout << "Error opening quiz_logs.txt\n";
        return;
    }
    fout << "========================================\n";
    fout << "Player: " << player << "\n";
    fout << "Date and Time: " << __DATE__ << "  " << __TIME__ << "\n";
    fout << "Category: " << category_str << "\n";
    fout << "Difficulty: " << difficulty_str << "\n";
    fout << "Correct: " << correct << " | Wrong: " << wrong << "\n";
    fout << "Score: " << total_score << "/" << SESSION_QUESTIONS << "\n";
    fout << "========================================\n\n";
    fout.close();
}

// Appends high score to high_scores.txt (Pipe delimited)
void save_high_score(string player, int score, string category, string difficulty) {
    ofstream fout("high_scores.txt", ios::app);
    if (!fout.is_open()) return;
    fout << player << "|" << score << "|" << category << "|" << difficulty << "\n";
    fout.close();
}

// Reads, parses, sorts, and displays high scores
void show_high_scores() {
    const int maxentries = 100;
    string player[maxentries];
    int score[maxentries];
    string category[maxentries];
    string difficulty[maxentries];
    int count = 0;

    ifstream fin("high_scores.txt");
    if (!fin.is_open()) {
        cout << "No high scores found!\nPress Enter...";
        cin.get();
        return;
    }

    // Parse file line by line
    string line;
    while (getline(fin, line) && count < maxentries) {
        string temp = line;
        player[count] = cut(temp);
        score[count] = stoi(cut(temp));
        category[count] = cut(temp);
        difficulty[count] = cut(temp);
        count++;
    }
    fin.close();

    // Bubble Sort (Descending Order by Score)
    for (int i = 0; i < count - 1; i++) {
        for (int j = 0; j < count - i - 1; j++) {
            if (score[j] < score[j + 1]) {
                swap(score[j], score[j + 1]);
                swap(player[j], player[j + 1]);
                swap(category[j], category[j + 1]);
                swap(difficulty[j], difficulty[j + 1]);
            }
        }
    }

    cout << "========================================\n";
    cout << "               HIGH SCORES\n";
    cout << "========================================\n\n";
    cout << "Player\tScore\tCategory\tDifficulty\n";
    for (int i = 0; i < count; i++) {
        cout << player[i] << "\t" << score[i] << "\t" << category[i] << "\t\t" << difficulty[i] << "\n";
    }
    cout << "\nPress Enter to return...";
    cin.ignore();
    cin.get();
}

// ======================= GAME LOGIC =======================

// Displays incorrect questions stored during the session
void review_incorrect_questions() {
    if (incorrect_count == 0) {
        cout << "\nGreat job! No incorrect answers.\nPress Enter...";
        cin.get();
        return;
    }

    system("cls");
    cout << "REVIEWING INCORRECT ANSWERS\n";
    cout << "Press Enter to start...";
    cin.get();

    for (int i = 0; i < incorrect_count; i++) {
        system("cls");
        cout << "Review Q" << i + 1 << "\n\n";
        // Calls display_question in 'review mode' (is_review = true)
        display_question(incorrect_questions[i], incorrect_options[i], incorrect_correct_pos[i], 0, true);
    }
    cout << "\nReview complete! Press Enter...";
    cin.get();
}

// Handles end-of-game options
void post_quiz_menu() {
    while (true) {
        system("cls");
        cout << "========================================\n";
        cout << "           QUIZ COMPLETE!\n";
        cout << "========================================\n";
        cout << "Score: " << score << "\n";
        cout << "Correct: " << correct_count << " | Wrong: " << wrong_count << "\n\n";
        cout << "1. Review Incorrect Questions\n";
        cout << "2. Replay Quiz\n";
        cout << "3. Main Menu\n";
        cout << "Enter choice: ";

        int choice;
        // Validation check
        if (!(cin >> choice)) {
            cout << "Invalid Input.\n";
            cin.clear();
            cin.ignore(10000, '\n');
            system("pause");
            continue;
        }
        cin.ignore(10000, '\n');

        if (choice == 1) {
            review_incorrect_questions();
        }
        else if (choice == 2) {
            // Reset variables for replay
            streak = 0;
            timer = 15;
            incorrect_count = 0;
            correct_count = 0;
            wrong_count = 0;
            reset_lifelines();
            start_quiz();
            return;
        }
        else if (choice == 3) {
            return;
        }
    }
}

/*
 * Function: display_question
 * Purpose: Displays the question, handles option shuffling, inputs, and scoring.
 * Logic:
 * 1. Shuffles options so Answer A isn't always the correct one.
 * 2. Displays question text and shuffled options.
 * 3. Calls get_answer_with_timer() to wait for input.
 * 4. Processes correct answer, wrong answer, or lifeline usage.
 */
void display_question(string question, string options[], int correct_pos, int q_num, bool is_review = false) {
    int map[4] = { 0,1,2,3 }; // Maps shuffled positions to original positions
    int current_timer = timer;

    // Copy original options to a temp array for shuffling
    string shuffled_options[4];
    for (int i = 0; i < 4; i++) shuffled_options[i] = options[i];

    // Shuffle Logic (Only if not in review mode)
    if (!is_review) {
        for (int i = 3; i > 0; i--) {
            int j = rand() % (i + 1);
            swap(shuffled_options[i], shuffled_options[j]);
            swap(map[i], map[j]);
        }
    }
    else {
        // In review mode, keep original order or simple order
        for (int i = 0; i < 4; i++) map[i] = i;
    }

    // Determine where the correct answer moved to
    int new_correct = 0;
    for (int i = 0; i < 4; i++) {
        if (map[i] == correct_pos) new_correct = i;
    }

    // Display Text
    cout << question << "\n\n";
    for (int i = 0; i < 4; i++) cout << i + 1 << ") " << shuffled_options[i] << "\n";

    // --- Review Mode Exit ---
    if (is_review) {
        cout << "\n** Correct Answer: " << shuffled_options[new_correct] << " **\n";
        cout << "Press Enter...";
        cin.get();
        return;
    }

    // --- Active Game Mode ---
    cout << "\n--- Lifelines ---\n";
    cout << "5) 50/50" << (lifeline_5050 ? " [OK]" : " [USED]") << "  ";
    cout << "6) Skip" << (lifeline_skip ? " [OK]" : " [USED]") << "\n";
    cout << "7) Replace" << (lifeline_replace ? " [OK]" : " [USED]") << "  ";
    cout << "8) +Time" << (lifeline_extratime ? " [OK]" : " [USED]") << "\n\n";

    int answer;
    // Call the timer function to get input
    bool answered_in_time = get_answer_with_timer(answer, current_timer);

    // Calculate penalty based on difficulty
    int negativemark = (difficulty == 1) ? 2 : (difficulty == 2) ? 3 : 5;

    // --- TIMEOUT HANDLER ---
    if (!answered_in_time) {
        cout << "Time's up! Correct: " << shuffled_options[new_correct] << "\n";
        score -= negativemark;
        streak = 0;
        wrong_count++;
        // Save for review
        incorrect_questions[incorrect_count] = question;
        for (int i = 0; i < 4; i++) incorrect_options[incorrect_count][i] = shuffled_options[i];
        incorrect_correct_pos[incorrect_count] = new_correct;
        incorrect_count++;
        cout << "Score: " << score << "\nPress Enter...";
        cin.ignore(); cin.get();
        return;
    }

    // --- LIFELINE HANDLERS ---
    if (answer == 5) {
        use_lifeline_5050(shuffled_options, new_correct);
        cout << "Options updated:\n";
        for (int i = 0; i < 4; i++) cout << i + 1 << ") " << shuffled_options[i] << "\n";

        // Ask again with remaining time
        answered_in_time = get_answer_with_timer(answer, current_timer);
        if (!answered_in_time) {
            // Handle timeout after using lifeline
            score -= negativemark;
            wrong_count++;
            incorrect_questions[incorrect_count] = question;
            for (int i = 0; i < 4; i++) incorrect_options[incorrect_count][i] = shuffled_options[i];
            incorrect_correct_pos[incorrect_count] = new_correct;
            incorrect_count++;
            cout << "Time's up! Score: " << score << "\nPress Enter...";
            cin.ignore(); cin.get();
            return;
        }
    }
    else if (answer == 6) {
        use_lifeline_skip();
        cout << "Press Enter...";
        cin.ignore(); cin.get();
        return;
    }
    else if (answer == 7) {
        if (lifeline_replace) {
            use_lifeline_replace(indices[q_num], used_indices, used_count);
            replace_requested = true; // Signals start_quiz loop to restart this iteration
        }
        cout << "Press Enter...";
        cin.ignore(); cin.get();
        return;
    }
    else if (answer == 8) {
        use_lifeline_extratime(current_timer);
        // Ask again with new time
        answered_in_time = get_answer_with_timer(answer, current_timer);
        if (!answered_in_time) {
            score -= negativemark;
            wrong_count++;
            incorrect_questions[incorrect_count] = question;
            for (int i = 0; i < 4; i++) incorrect_options[incorrect_count][i] = shuffled_options[i];
            incorrect_correct_pos[incorrect_count] = new_correct;
            incorrect_count++;
            cout << "Time's up! Score: " << score << "\nPress Enter...";
            cin.ignore(); cin.get();
            return;
        }
    }

    // --- SCORE CALCULATION ---
    answer--; // Convert input (1-4) to 0-based index (0-3)

    if (answer == new_correct) {
        cout << "\nCorrect!\n";
        score++;
        streak++;
        correct_count++;
        // Bonus points logic
        if (streak == 3) { score += 5; cout << "Streak Bonus +5!\n"; }
        if (streak == 5) { score += 15; cout << "Streak Bonus +15!\n"; streak = 0; }
    }
    else {
        cout << "\nWrong! Correct: " << shuffled_options[new_correct] << "\n";
        score -= negativemark;
        cout << "Penalty: -" << negativemark << "\n";
        streak = 0;
        wrong_count++;

        // Save for review
        incorrect_questions[incorrect_count] = question;
        for (int i = 0; i < 4; i++) incorrect_options[incorrect_count][i] = shuffled_options[i];
        incorrect_correct_pos[incorrect_count] = new_correct;
        incorrect_count++;
    }

    cout << "Score: " << score << "\nPress Enter...";
    cin.ignore(); cin.get();
}

/*
 * Function: start_quiz
 * Purpose: Main game loop. Loads questions and iterates through them.
 */
void start_quiz() {
    // Select File
    switch (category) {
    case 1: filename = "science.txt"; break;
    case 2: filename = "computer.txt"; break;
    case 3: filename = "sports.txt"; break;
    case 4: filename = "history.txt"; break;
    case 5: filename = "iq.txt"; break;
    }

    if (load_questions(filename) == 0) {
        cout << "Failed to load questions. Check file existence.\nPress Enter...";
        cin.get();
        return;
    }

    // Prepare Indices (Difficulty Logic)
    used_count = 0;
    int startindex = (difficulty - 1) * 50; // Easy=0, Medium=50, Hard=100
    for (int i = 0; i < 50; i++) indices[i] = startindex + i;
    shuffle_array(indices, 50);
    score = 0;

    // Loop through 10 questions
    for (int q = 0; q < SESSION_QUESTIONS; ) {
        system("cls");
        string line = questions[indices[q]];

        // Parse the line (Question|Opt1|Opt2|Opt3|Opt4|CorrectIdx)
        string question = cut(line);
        string a = cut(line), b = cut(line), c = cut(line), d = cut(line);
        string correct_str = cut(line);

        // Validation against empty lines
        if (correct_str.empty() || question.empty()) {
            q++; continue;
        }

        string options[4] = { a, b, c, d };
        cout << "Question " << q + 1 << " of " << SESSION_QUESTIONS << "\n\n";

        // Show Question
        display_question(question, options, stoi(correct_str) - 1, q, false);

        // Handle 'Replace Question' Lifeline
        if (replace_requested) {
            replace_requested = false;
            continue; // Skip the increment of 'q' to retry this slot
        }

        // Mark question as used and move to next
        used_indices[used_count++] = indices[q];
        q++;
    }

    // Prepare strings for logging
    string cat_str = (category == 1 ? "Science" : category == 2 ? "Computer" :
        category == 3 ? "Sports" : category == 4 ? "History" : "IQ");
    string diff_str = (difficulty == 1 ? "Easy" : difficulty == 2 ? "Medium" : "Hard");

    // Save Data
    save_quiz_log(playername, cat_str, diff_str, correct_count, wrong_count, score);
    save_high_score(playername, score, cat_str, diff_str);

    // Show End Menu
    post_quiz_menu();
}