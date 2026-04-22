/*
 * gextedit -  editor de text
 * 
 * 
 * 
 * compilare Linux/macOS:
 *   g++ -o editor editor.cpp -lncurses
 *
 * compilare Windows (mingw + pdcurses):
 *   g++ -o editor editor.cpp -lpdcurses
 *
 * scurtaturi:
 *   ctrl+s - salveaza
 *   ctrl+o - deschide fisier (open)
 *   ctrl+n - fisier nou
 *   ctrl+f - find (cauta) in text
 *   ctrl+x - iesire
 *   navigare prin sageti
 */

#ifdef _WIN32
#include <curses.h>    // pdcurses pe Windows (https://github.com/wmcbrine/PDCurses/blob/master/curses.h)   
#else
#include <ncurses.h>   // ncurses pe linux/macOS (via apt/pacman/brew/etc)
#endif

#include <vector>
#include <string>
#include <fstream>

using namespace std;

// --- var ----------------------------------------------------------------------

vector<string> buf;
int curRow = 0, curCol = 0;
int scrollRow = 0, scrollCol = 0;
string filename = "";
bool modified = false;

int editorRows() { return LINES - 2; }
int editorCols() { return COLS; }

void limitCursor() {
    if (curRow < 0) curRow = 0;
    if (curRow >= (int)buf.size()) curRow = (int)buf.size() - 1;
    if (curCol < 0) curCol = 0;
    if (curCol > (int)buf[curRow].size()) curCol = (int)buf[curRow].size();
}

// --- afisare -------------------------------------------------------------------

void drawTitleBar() {
    attron(A_REVERSE);
    string title = "  gextEdit  |  ";
    title += filename.empty() ? "[fisier nou]" : filename;
    if (modified) title += " *";
    title.resize(COLS, ' ');
    mvaddstr(0, 0, title.c_str());
    attroff(A_REVERSE);
}

void drawStatusBar() {
    attron(A_REVERSE);
    string pos = "  L:" + to_string(curRow + 1) + " C:" + to_string(curCol + 1) + "  ";
    string cmds = "  ^S salveaza  ^O deschide  ^N nou  ^F cauta  ^X iesire";
    int space = COLS - (int)pos.size();
    if ((int)cmds.size() > space) cmds = cmds.substr(0, space);
    cmds.resize(space, ' ');
    string bar = cmds + pos;
    mvaddstr(LINES - 1, 0, bar.c_str());
    attroff(A_REVERSE);
}

void drawEditor() {
    for (int y = 0; y < editorRows(); y++) {
        int li = scrollRow + y;
        move(y + 1, 0);
        clrtoeol();
        if (li < (int)buf.size()) {
            const string& line = buf[li];
            string vis = (scrollCol < (int)line.size())
                ? line.substr(scrollCol, editorCols())
                : "";
            addstr(vis.c_str());
        }
    }
}

void render() {
    if (curRow < scrollRow) scrollRow = curRow;
    if (curRow >= scrollRow + editorRows()) scrollRow = curRow - editorRows() + 1;
    if (curCol < scrollCol) scrollCol = curCol;
    if (curCol >= scrollCol + editorCols()) scrollCol = curCol - editorCols() + 1;

    drawTitleBar();
    drawEditor();
    drawStatusBar();
    move(curRow - scrollRow + 1, curCol - scrollCol);
    refresh();
}

// --- confirmari ----------------------------------------------------------------

void showMessage(const string& msg) {
    attron(A_REVERSE);
    string s = "  " + msg;
    s.resize(COLS, ' ');
    mvaddstr(LINES - 1, 0, s.c_str());
    attroff(A_REVERSE);
    refresh();
    napms(1300);
}

string prompt(const string& label) {
    string input;
    while (true) {
        attron(A_REVERSE);
        string bar = "  " + label + input;
        bar.resize(COLS, ' ');
        mvaddstr(LINES - 1, 0, bar.c_str());
        attroff(A_REVERSE);
        move(LINES - 1, 2 + (int)label.size() + (int)input.size());
        refresh();

        int ch = getch();
        if (ch == '\n' || ch== '\r' || ch == KEY_ENTER) break;
        if (ch == 27) { input = ""; break; }
        if (ch == KEY_BACKSPACE || ch == 127 || ch == 8) {
            if (!input.empty()) input.pop_back();
        }
        else if (ch >= 32 && ch < 256) {
            input += (char)ch;
        }
    }
    return input;
}

// --- fisier -------------------------------------------------------------------

bool loadFile(const string& path) {
    ifstream fin(path);
    if (!fin.is_open()) return false;
    buf.clear();
    string line;
    while (getline(fin, line)) buf.push_back(line);
    if (buf.empty()) buf.push_back("");
    fin.close();
    return true;
}

bool saveFile(const string& path) {
    ofstream fout(path);
    if (!fout.is_open()) return false;
    for (const string& line : buf) fout << line << "\n";
    fout.close();
    return true;
}

void cmdOpen() {
    string path = prompt("deschide fisier: ");
    if (path.empty()) return;
    if (loadFile(path)) {
        filename = path; curRow = curCol = scrollRow = scrollCol = 0; modified = false;
        showMessage("deschis: " + path);
    }
    else {
        showMessage("eroare: nu pot deschide " + path);
    }
}

void cmdSave() {
    if (filename.empty()) {
        filename = prompt("salveaza ca: ");
        if (filename.empty()) return;
    }
    if (saveFile(filename)) { modified = false; showMessage("salvat: " + filename); }
    else showMessage("eroare la salvare!");
}

void cmdNew() {
    if (modified) {
        string r = prompt("modificari nesalvate; continui? (y/n): ");
        if (r != "y" && r != "Y") return;
    }
    buf.clear(); buf.push_back("");
    filename = ""; curRow = curCol = scrollRow = scrollCol = 0; modified = false;
}

void cmdFind() {
    string needle = prompt("cauta: ");
    if (needle.empty()) return;
    int startCol = curCol + 1;
    for (int pass = 0; pass < 2; pass++) {
        for (int r = (pass == 0 ? curRow : 0); r < (int)buf.size(); r++) {
            size_t pos = buf[r].find(needle, (r == curRow && pass == 0) ? startCol : 0);
            if (pos != string::npos) {
                curRow = r; curCol = (int)pos;
                showMessage("gasit la linia " + to_string(r + 1));
                return;
            }
        }
        startCol = 0;
    }
    showMessage("nu a fost gasit: " + needle);
}

// --- editare ------------------------------------------------------------------

void handleKey(int ch) {
    switch (ch) {
    case KEY_UP:    curRow--; limitCursor(); break;
    case KEY_DOWN:  curRow++; limitCursor(); break;
    case KEY_LEFT:
        if (curCol > 0) curCol--;
        else if (curRow > 0) { curRow--; curCol = (int)buf[curRow].size(); }
        break;
    case KEY_RIGHT:
        if (curCol < (int)buf[curRow].size()) curCol++;
        else if (curRow < (int)buf.size() - 1) { curRow++; curCol = 0; }
        break;
    case KEY_HOME:  curCol = 0; break;
    case KEY_END:   curCol = (int)buf[curRow].size(); break;
    case KEY_PPAGE: curRow -= editorRows(); limitCursor(); break;
    case KEY_NPAGE: curRow += editorRows(); limitCursor(); break;

    case KEY_BACKSPACE: case 127: case 8:
        if (curCol > 0) {
            buf[curRow].erase(curCol - 1, 1); curCol--; modified = true;
        }
        else if (curRow > 0) {
            int prev = (int)buf[curRow - 1].size();
            buf[curRow - 1] += buf[curRow];
            buf.erase(buf.begin() + curRow);
            curRow--; curCol = prev; modified = true;
        }
        break;

    case KEY_DC:
        if (curCol < (int)buf[curRow].size()) {
            buf[curRow].erase(curCol, 1); modified = true;
        }
        else if (curRow < (int)buf.size() - 1) {
            buf[curRow] += buf[curRow + 1];
            buf.erase(buf.begin() + curRow + 1); modified = true;
        }
        break;

    case '\r': case'\n': case KEY_ENTER: {
        string rest = buf[curRow].substr(curCol);
        buf[curRow] = buf[curRow].substr(0, curCol);
        buf.insert(buf.begin() + curRow + 1, rest);
        curRow++; curCol = 0; modified = true;
        break;
    }

    default:
        if (ch >= 32 && ch < 256) {
            buf[curRow].insert(curCol, 1, (char)ch);
            curCol++; modified = true;
        }
        break;
    }
}

// --- functia main --------------------------------------------------------------

int main(int argc, char* argv[]) {
    initscr();
    raw();
    noecho();
    nonl();
    keypad(stdscr, TRUE);

    buf.push_back("");

    if (argc > 1) {
        string path = argv[1];
        if (loadFile(path)) filename = path;
        else { filename = path; buf.clear(); buf.push_back(""); }
    }

    while (true) {
        render();
        int ch = getch();

        if (ch == 24) { // ctrl+x
            if (modified) {
                string r = prompt("modificari nesalvate; iesi? (y/n): ");
                if (r != "y" && r != "Y") continue;
            }
            break;
        }
        else if (ch == 19) cmdSave();   // ctrl+s
        else if (ch == 15) cmdOpen();   // ctrl+o
        else if (ch == 14) cmdNew();    // ctrl+n
        else if (ch == 6) cmdFind();    // ctrl+f
        else handleKey(ch);
    }

    endwin();
    return 0;
}