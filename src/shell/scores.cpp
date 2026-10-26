#include "scores.h"

#include <cstdio>
#include <ctime>

#include "../core/config.h"

namespace op2 {

namespace {
constexpr const char* kSec = "ColonyGameScores";

// Uma chave indexada ("Name%i" etc.), como o sprintf do original.
std::string key(const char* fmt, int i) {
    char b[32];
    std::snprintf(b, sizeof(b), fmt, i);
    return b;
}
}

std::vector<ScoreEntry> scoresLoad() {
    std::vector<ScoreEntry> out;
    Config cfg("OUTPOST2.INI");
    if (!cfg.load()) return out;
    const int n = cfg.getInt(kSec, "Count", 0);
    for (int i = 1; i <= n; ++i) {                        // 1-based (loop ebp=1..Count)
        ScoreEntry e;
        e.game       = cfg.getString(kSec, key("Game%i", i), "");
        e.difficulty = cfg.getString(kSec, key("Difficulty%i", i), "");
        e.time       = cfg.getInt(kSec, key("Time%i", i), 0);
        e.name       = cfg.getString(kSec, key("Name%i", i), "");
        e.date       = cfg.getString(kSec, key("Date%i", i), "");
        out.push_back(e);
    }
    return out;
}

void scoresAdd(const ScoreEntry& e) {
    Config cfg("OUTPOST2.INI");
    cfg.load();
    const int i = cfg.getInt(kSec, "Count", 0) + 1;       // lea ebp, [eax+1]
    cfg.setInt(kSec, "Count", i);
    cfg.setString(kSec, key("Name%i", i), e.name.substr(0, 20));   // WM_GETTEXT de 21 bytes
    cfg.setInt(kSec, key("Time%i", i), e.time);
    cfg.setString(kSec, key("Difficulty%i", i), e.difficulty);
    cfg.setString(kSec, key("Date%i", i), e.date);
    cfg.setString(kSec, key("Game%i", i), e.game);
    cfg.save();
}

void scoresClear() {
    Config cfg("OUTPOST2.INI");
    cfg.load();
    cfg.setInt(kSec, "Count", 0);
    cfg.save();
}

std::string scoreLine(const ScoreEntry& e) {
    char t[16];
    std::snprintf(t, sizeof(t), "%07i", e.time);          // 0x13012218
    return e.game + ", " + e.difficulty + ", " + t + ", " + e.name + ", " + e.date;
}

const char* scoreDifficultyName(unsigned playerWord) {
    const unsigned d = playerWord & 0x18u;                // and eax, 0x18
    if (d == 0) return "Easy";
    if (d == 8) return "Normal";
    return "Hard";
}

std::string scoreToday() {
    const std::time_t tt = std::time(nullptr);
    char b[32] = "";
    if (const std::tm* lt = std::localtime(&tt))
        std::snprintf(b, sizeof(b), "%i-%i-%i", lt->tm_mon + 1, lt->tm_mday, lt->tm_year + 1900);
    return b;
}

} // namespace op2
