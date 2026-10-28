// GameQuest - Gaming Tournament and Player Ranking System (C++)
// Theme: Valorant Champions-style tournament
// Concepts: Classes, Inheritance, Polymorphism, Operator Overloading,
//           File Handling, Exception Handling
// Build:  g++ -std=c++17 -Wall -Wextra -o gamequest gamequest.cpp

#include <algorithm>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace std;

// ==== PART A: custom exceptions and the Player base class ===========
class InvalidScoreError : public runtime_error {
public:
    explicit InvalidScoreError(const string& m) : runtime_error(m) {}
};
class DuplicatePlayerError : public runtime_error {
public:
    explicit DuplicatePlayerError(const string& m) : runtime_error(m) {}
};
class PlayerNotFoundError : public runtime_error {
public:
    explicit PlayerNotFoundError(const string& m) : runtime_error(m) {}
};
class FileError : public runtime_error {
public:
    explicit FileError(const string& m) : runtime_error(m) {}
};

class Player {
protected:
    static int nextId;
    int id;
    string name, gamertag, agent;
    double totalScore;
    int matchesPlayed, wins;

public:
    Player(const string& n, const string& g, const string& a)
        : id(nextId++), name(n), gamertag(g), agent(a),
          totalScore(0), matchesPlayed(0), wins(0) {}
    virtual ~Player() {}

    // Polymorphic interface - implemented by each subclass
    virtual double calculatePoints(double k, double d, double a, bool won) const = 0;
    virtual int calculateReward(int rank) const = 0;
    virtual string category() const = 0;
    virtual string currency() const = 0;

    void record(double points, bool won) {
        totalScore += points;
        matchesPlayed++;
        if (won) wins++;
    }
    int getId() const { return id; }
    const string& getGamertag() const { return gamertag; }
    double getScore() const { return totalScore; }
    int getWins() const { return wins; }
    int getMatches() const { return matchesPlayed; }

    // Operator overloading
    double operator+(const Player& o) const { return totalScore + o.totalScore; }
    double operator+(double v) const { return totalScore + v; }
    friend double operator+(double v, const Player& p) { return v + p.totalScore; }
    bool operator<(const Player& o) const { return totalScore < o.totalScore; }
    bool operator>(const Player& o) const { return totalScore > o.totalScore; }
    bool operator==(const Player& o) const { return totalScore == o.totalScore; }

    friend ostream& operator<<(ostream& os, const Player& p) {
        ios::fmtflags saved = os.flags();
        os << "[" << left << setw(6) << p.category() << "] " << setw(12) << p.gamertag
           << " (" << setw(8) << p.agent << ") Score: " << right << fixed
           << setprecision(1) << setw(6) << p.totalScore << "  Wins: " << p.wins
           << "/" << p.matchesPlayed;
        os.flags(saved);
        return os;
    }
};
int Player::nextId = 1;

// ==== PART B: CasualPlayer, ProPlayer and Match =====================
class CasualPlayer : public Player {
public:
    CasualPlayer(const string& n, const string& g, const string& a) : Player(n, g, a) {}
    double calculatePoints(double k, double d, double a, bool won) const override {
        double points = k * 2 + a * 1 - d * 0.5;
        if (won) points += 10;
        return max(points, 0.0);
    }
    int calculateReward(int rank) const override { return rank <= 3 ? 500 : 100; }
    string category() const override { return "Casual"; }
    string currency() const override { return "credits"; }
};

class ProPlayer : public Player {
    string team;

public:
    ProPlayer(const string& n, const string& g, const string& a, const string& t)
        : Player(n, g, a), team(t) {}
    double calculatePoints(double k, double d, double a, bool won) const override {
        double points = k * 3 + a * 1.5 - d * 1;
        if (k >= 25) points += 15;  // ace-performance bonus
        if (won) points += 25;
        return max(points, 0.0);
    }
    int calculateReward(int rank) const override {
        if (rank == 1) return 50000;
        if (rank == 2) return 25000;
        if (rank == 3) return 10000;
        return 1000;
    }
    string category() const override { return "Pro"; }
    string currency() const override { return "USD"; }
    const string& getTeam() const { return team; }
};

struct Stats {
    double kills, deaths, assists;
};
struct MatchResult {
    int id;
    string mapName, p1, p2;
    double pts1, pts2;
    string winner;
};

class Match {
    int id;
    shared_ptr<Player> p1, p2;
    string mapName;

public:
    static constexpr double MAX_STAT = 60;

    Match(int i, shared_ptr<Player> a, shared_ptr<Player> b, const string& m)
        : id(i), p1(a), p2(b), mapName(m) {}

    static string fmt(double v) {
        ostringstream s;
        s << v;
        return s.str();
    }
    // Converts text to a number, rejecting anything that is not numeric
    static double parseStat(const string& text, const string& label) {
        size_t pos = 0;
        double v = 0;
        try {
            v = stod(text, &pos);
        } catch (const exception&) {
            throw InvalidScoreError(label + " must be a number, got '" + text + "'");
        }
        if (pos != text.size())
            throw InvalidScoreError(label + " must be a number, got '" + text + "'");
        return v;
    }
    static void validateStat(double v, const string& label) {
        if (v != v) throw InvalidScoreError(label + " is not a number");
        if (v < 0) throw InvalidScoreError(label + " cannot be negative (" + fmt(v) + ")");
        if (v > MAX_STAT)
            throw InvalidScoreError(label + " = " + fmt(v) + " exceeds the limit of " + fmt(MAX_STAT));
    }
    static void validate(const Stats& s) {
        validateStat(s.kills, "Kills");
        validateStat(s.deaths, "Deaths");
        validateStat(s.assists, "Assists");
    }

    // The winner is the player with the higher kills + assists - deaths
    MatchResult play(const Stats& s1, const Stats& s2) {
        validate(s1);  // validation happens before anything is recorded
        validate(s2);
        bool win1 = (s1.kills + s1.assists - s1.deaths) >= (s2.kills + s2.assists - s2.deaths);
        double pts1 = p1->calculatePoints(s1.kills, s1.deaths, s1.assists, win1);
        double pts2 = p2->calculatePoints(s2.kills, s2.deaths, s2.assists, !win1);
        p1->record(pts1, win1);
        p2->record(pts2, !win1);
        return {id, mapName, p1->getGamertag(), p2->getGamertag(), pts1, pts2,
                win1 ? p1->getGamertag() : p2->getGamertag()};
    }
};

// ==== PART C: Tournament and file handling ==========================
class Tournament {
    string name, game;
    map<int, shared_ptr<Player>> players;
    vector<MatchResult> matches;

public:
    Tournament(const string& n, const string& g) : name(n), game(g) {}

    void registerPlayer(shared_ptr<Player> p) {
        for (const auto& kv : players)
            if (kv.second->getGamertag() == p->getGamertag())
                throw DuplicatePlayerError(p->getGamertag() + " is already registered");
        players[p->getId()] = p;
    }
    shared_ptr<Player> getPlayer(int id) const {
        auto it = players.find(id);
        if (it == players.end()) throw PlayerNotFoundError("No player with id " + to_string(id));
        return it->second;
    }
    MatchResult scheduleAndPlay(int id1, int id2, const string& mapName,
                                const Stats& s1, const Stats& s2) {
        Match m((int)matches.size() + 1, getPlayer(id1), getPlayer(id2), mapName);
        MatchResult r = m.play(s1, s2);
        matches.push_back(r);
        return r;
    }
    vector<shared_ptr<Player>> leaderboard() const {
        vector<shared_ptr<Player>> v;
        for (const auto& kv : players) v.push_back(kv.second);
        stable_sort(v.begin(), v.end(),
                    [](const shared_ptr<Player>& a, const shared_ptr<Player>& b) { return *a > *b; });
        return v;
    }

    void saveResults(const string& txtPath, const string& csvPath) const {
        auto board = leaderboard();
        time_t now = time(nullptr);
        char date[16];
        strftime(date, sizeof date, "%Y-%m-%d", localtime(&now));

        ofstream txt(txtPath);
        if (!txt) throw FileError("Cannot write '" + txtPath + "'");
        txt << name << " - " << game << "\nDate: " << date << "\n"
            << string(60, '=') << "\nFINAL RANKINGS\n" << fixed << setprecision(1);
        int rank = 1;
        for (const auto& p : board) {
            txt << rank << ". " << left << setw(12) << p->getGamertag() << setw(7) << p->category()
                << right << setw(6) << p->getScore() << " pts  Reward: "
                << p->calculateReward(rank) << " " << p->currency() << "\n";
            rank++;
        }
        txt << "\nMATCH LOG\n";
        for (const auto& r : matches)
            txt << "M" << r.id << " [" << r.mapName << "] " << r.p1 << " " << r.pts1 << " vs "
                << r.pts2 << " " << r.p2 << " -> " << r.winner << "\n";

        ofstream csv(csvPath);
        if (!csv) throw FileError("Cannot write '" + csvPath + "'");
        csv << "rank,gamertag,category,score,wins,matches,reward\n" << fixed << setprecision(1);
        rank = 1;
        for (const auto& p : board) {
            csv << rank << "," << p->getGamertag() << "," << p->category() << ","
                << p->getScore() << "," << p->getWins() << "," << p->getMatches() << ","
                << p->calculateReward(rank) << "\n";
            rank++;
        }
    }

    // Returns the number of player rows read; throws FileError on a bad file
    static int loadResults(const string& csvPath) {
        ifstream in(csvPath);
        if (!in) throw FileError("File '" + csvPath + "' not found");
        string line;
        getline(in, line);  // skip header
        int rows = 0;
        while (getline(in, line)) {
            if (line.empty()) continue;
            stringstream ss(line);
            string field;
            int cols = 0;
            while (getline(ss, field, ',')) cols++;
            if (cols != 7)
                throw FileError("File '" + csvPath + "' is corrupted at line " + to_string(rows + 2));
            rows++;
        }
        return rows;
    }
};

// ==== PART D: demonstration driver ==================================
int main() {
    cout << fixed << setprecision(1);
    Tournament t("Valorant Champions Cup 2026", "Valorant");

    cout << "--- Registering players ---\n";
    vector<shared_ptr<Player>> roster = {
        make_shared<ProPlayer>("Tyson Ngo", "TenZ", "Jett", "Sentinels"),
        make_shared<ProPlayer>("Aleksander Zygmunt", "zekken", "Raze", "Sentinels"),
        make_shared<ProPlayer>("Nikita Sirmitev", "Derke", "Raze", "Fnatic"),
        make_shared<CasualPlayer>("Aarav Sharma", "AaravX", "Sage"),
        make_shared<CasualPlayer>("Simran Kaur", "SimiOP", "Phoenix"),
        make_shared<CasualPlayer>("Rohan Mehta", "RohanFPS", "Sova"),
    };
    for (const auto& p : roster) {
        t.registerPlayer(p);
        cout << "  Registered: " << *p << "\n";
    }
    try {
        t.registerPlayer(make_shared<CasualPlayer>("Fake Dup", "TenZ", "Brimstone"));
    } catch (const DuplicatePlayerError& e) {
        cout << "  ! DuplicatePlayerError: " << e.what() << "\n";
    }

    cout << "\n--- Playing matches ---\n";
    struct Fixture { int a, b; string map; Stats s1, s2; };
    vector<Fixture> fixtures = {
        {1, 3, "Ascent", {24, 12, 6}, {21, 15, 8}},
        {2, 4, "Bind",   {27, 10, 4}, {14, 18, 9}},
        {3, 5, "Haven",  {19, 14, 7}, {16, 16, 5}},
        {1, 2, "Split",  {30, 9, 5},  {22, 13, 10}},
        {4, 6, "Icebox", {18, 11, 9}, {20, 12, 6}},
        {5, 6, "Lotus",  {17, 13, 8}, {12, 17, 4}},
    };
    for (const auto& f : fixtures) {
        MatchResult r = t.scheduleAndPlay(f.a, f.b, f.map, f.s1, f.s2);
        cout << "  M" << r.id << " [" << left << setw(6) << r.mapName << "] " << setw(8) << r.p1
             << right << setw(6) << r.pts1 << " vs " << setw(6) << r.pts2 << " " << left
             << setw(8) << r.p2 << " -> Winner: " << r.winner << "\n" << right;
    }

    cout << "\n--- Invalid score tests (Exception Handling) ---\n";
    auto attempt = [&](int a, int b, Stats s1, Stats s2) {
        try {
            t.scheduleAndPlay(a, b, "Ascent", s1, s2);
        } catch (const InvalidScoreError& e) {
            cout << "  ! InvalidScoreError: " << e.what() << "\n";
        } catch (const PlayerNotFoundError& e) {
            cout << "  ! PlayerNotFoundError: " << e.what() << "\n";
        }
    };
    attempt(1, 2, {-5, 10, 3}, {10, 10, 3});
    try {  // text input such as "many" is rejected while converting to a number
        Stats bad{Match::parseStat("many", "Kills"), 10, 3};
        attempt(1, 2, {20, 10, 3}, bad);
    } catch (const InvalidScoreError& e) {
        cout << "  ! InvalidScoreError: " << e.what() << "\n";
    }
    attempt(1, 2, {99, 10, 3}, {10, 10, 3});
    attempt(1, 99, {10, 10, 3}, {10, 10, 3});

    cout << "\n--- Leaderboard ---\n";
    int rank = 1;
    for (const auto& p : t.leaderboard()) {
        cout << "  #" << rank << " " << *p << "  Reward: " << p->calculateReward(rank)
             << " " << p->currency() << "\n";
        rank++;
    }

    cout << "\n--- Operator overloading ---\n";
    const Player &a = *roster[0], &b = *roster[1];
    cout << "  " << a.getGamertag() << " + " << b.getGamertag()
         << " (Sentinels team score) = " << a + b << "\n";
    cout << "  " << a.getGamertag() << " > " << b.getGamertag() << " ? "
         << (a > b ? "true" : "false") << "\n";
    cout << "  " << roster[3]->getGamertag() << " < " << roster[4]->getGamertag() << " ? "
         << (*roster[3] < *roster[4] ? "true" : "false") << "\n";
    double all = accumulate(roster.begin(), roster.end(), 0.0,
                            [](double s, const shared_ptr<Player>& p) { return s + *p; });
    cout << "  sum of all players = " << all << "\n";

    cout << "\n--- Polymorphism (same call, different result) ---\n";
    for (int i : {0, 3}) {
        const Player& p = *roster[i];
        cout << "  " << left << setw(6) << p.category() << " " << setw(8) << p.getGamertag()
             << right << " -> points for 20/10/5 win: " << p.calculatePoints(20, 10, 5, true)
             << ", reward for rank 1: " << p.calculateReward(1) << " " << p.currency() << "\n";
    }

    cout << "\n--- File handling ---\n";
    t.saveResults("results.txt", "results.csv");
    cout << "  Saved results.txt and results.csv\n";
    try {
        cout << "  Loaded back " << Tournament::loadResults("results.csv") << " player rows\n";
        Tournament::loadResults("missing.csv");
    } catch (const FileError& e) {
        cout << "  ! FileError: " << e.what() << "\n";
    }
    return 0;
}
