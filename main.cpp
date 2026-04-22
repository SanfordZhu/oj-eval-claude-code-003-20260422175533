#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <sstream>
#include <memory>

using namespace std;

enum class JudgeStatus {
    Accepted,
    Wrong_Answer,
    Runtime_Error,
    Time_Limit_Exceed
};

struct Submission {
    string problem_name;
    string team_name;
    JudgeStatus status;
    int time;

    Submission(const string& prob, const string& team, JudgeStatus st, int t)
        : problem_name(prob), team_name(team), status(st), time(t) {}
};

struct ProblemStatus {
    int incorrect_attempts = 0;
    int solve_time = -1;  // -1 means not solved
    int frozen_submissions = 0;
    bool is_frozen = false;
};

struct TeamInfo {
    string name;
    map<string, ProblemStatus> problems;  // problem_name -> status
    int total_solved = 0;
    int total_penalty = 0;
    vector<int> solve_times;  // for tie-breaking

    // Calculate penalty for a specific problem
    int getProblemPenalty(const string& prob) const {
        auto it = problems.find(prob);
        if (it == problems.end() || it->second.solve_time == -1) {
            return 0;
        }
        return 20 * it->second.incorrect_attempts + it->second.solve_time;
    }

    // Update total solved and penalty
    void updateTotals() {
        total_solved = 0;
        total_penalty = 0;
        solve_times.clear();

        for (const auto& [prob, status] : problems) {
            if (status.solve_time != -1) {
                total_solved++;
                total_penalty += 20 * status.incorrect_attempts + status.solve_time;
                solve_times.push_back(status.solve_time);
            }
        }

        sort(solve_times.rbegin(), solve_times.rend());  // descending order for tie-breaking
    }
};

class ICPCManager {
private:
    map<string, TeamInfo> teams;
    vector<Submission> submissions;
    bool competition_started = false;
    bool is_frozen = false;
    int duration_time = 0;
    int problem_count = 0;
    vector<string> problem_names;

    // Parse judge status from string
    JudgeStatus parseStatus(const string& status) {
        if (status == "Accepted") return JudgeStatus::Accepted;
        if (status == "Wrong_Answer") return JudgeStatus::Wrong_Answer;
        if (status == "Runtime_Error") return JudgeStatus::Runtime_Error;
        if (status == "Time_Limit_Exceed") return JudgeStatus::Time_Limit_Exceed;
        return JudgeStatus::Wrong_Answer;  // default
    }

    // Get status string
    string getStatusString(JudgeStatus status) {
        switch (status) {
            case JudgeStatus::Accepted: return "Accepted";
            case JudgeStatus::Wrong_Answer: return "Wrong_Answer";
            case JudgeStatus::Runtime_Error: return "Runtime_Error";
            case JudgeStatus::Time_Limit_Exceed: return "Time_Limit_Exceed";
        }
        return "Unknown";
    }

public:
    // Add a team
    bool addTeam(const string& team_name) {
        if (competition_started) {
            cout << "[Error]Add failed: competition has started." << endl;
            return false;
        }

        if (teams.find(team_name) != teams.end()) {
            cout << "[Error]Add failed: duplicated team name." << endl;
            return false;
        }

        teams[team_name].name = team_name;
        cout << "[Info]Add successfully." << endl;
        return true;
    }

    // Start competition
    bool startCompetition(int duration, int problems) {
        if (competition_started) {
            cout << "[Error]Start failed: competition has started." << endl;
            return false;
        }

        competition_started = true;
        duration_time = duration;
        problem_count = problems;

        // Generate problem names (A, B, C, ...)
        problem_names.clear();
        for (int i = 0; i < problems; i++) {
            problem_names.push_back(string(1, 'A' + i));
        }

        // Initialize problems for each team
        for (auto& [team_name, team] : teams) {
            for (const string& prob : problem_names) {
                team.problems[prob] = ProblemStatus();
            }
        }

        cout << "[Info]Competition starts." << endl;
        return true;
    }

    // Submit solution
    void submit(const string& problem_name, const string& team_name,
                const string& submit_status, int time) {
        JudgeStatus status = parseStatus(submit_status);
        submissions.emplace_back(problem_name, team_name, status, time);

        // Update team status
        auto& team = teams[team_name];
        auto& prob_status = team.problems[problem_name];

        // Only update if problem is not already solved
        if (prob_status.solve_time == -1) {
            if (status == JudgeStatus::Accepted) {
                prob_status.solve_time = time;
                // If accepted while frozen, problem is no longer frozen
                prob_status.is_frozen = false;
            } else {
                if (!is_frozen) {
                    prob_status.incorrect_attempts++;
                }
            }

            // If frozen and problem was not solved before freeze, mark as frozen
            if (is_frozen && prob_status.solve_time == -1) {
                prob_status.is_frozen = true;
                prob_status.frozen_submissions++;
                // Still count incorrect attempts for frozen submissions
                prob_status.incorrect_attempts++;
            }
        }
    }

    // Flush scoreboard
    void flushScoreboard() {
        cout << "[Info]Flush scoreboard." << endl;
        displayScoreboard();
    }

    // Freeze scoreboard
    bool freezeScoreboard() {
        if (is_frozen) {
            cout << "[Error]Freeze failed: scoreboard has been frozen." << endl;
            return false;
        }

        is_frozen = true;
        cout << "[Info]Freeze scoreboard." << endl;
        return true;
    }

    // Scroll scoreboard
    bool scrollScoreboard() {
        if (!is_frozen) {
            cout << "[Error]Scroll failed: scoreboard has not been frozen." << endl;
            return false;
        }

        cout << "[Info]Scroll scoreboard." << endl;

        // First, display scoreboard before scrolling (after flush)
        displayScoreboard();

        // Process unfreezing
        vector<pair<string, string>> ranking_changes;

        while (true) {
            // Find team with frozen problems and lowest ranking
            vector<pair<string, TeamInfo>> sorted_teams = getSortedTeams();
            string target_team;
            string target_problem;
            bool found = false;

            // Search from lowest rank to highest
            for (int i = sorted_teams.size() - 1; i >= 0; i--) {
                const auto& [team_name, team] = sorted_teams[i];

                // Find frozen problem with smallest ID
                for (const string& prob : problem_names) {
                    if (team.problems.at(prob).is_frozen) {
                        target_team = team_name;
                        target_problem = prob;
                        found = true;
                        break;
                    }
                }
                if (found) break;
            }

            if (!found) break;  // No more frozen problems

            // Unfreeze this problem
            auto& team = teams[target_team];
            auto& prob_status = team.problems[target_problem];

            // Process frozen submissions
            int old_incorrect = prob_status.incorrect_attempts;

            // Count frozen submissions that were wrong
            // (We need to track this better during submission)
            // For now, assume all frozen submissions were wrong
            prob_status.incorrect_attempts += prob_status.frozen_submissions;
            prob_status.is_frozen = false;
            prob_status.frozen_submissions = 0;

            // Check if this caused a ranking change
            team.updateTotals();
            vector<pair<string, TeamInfo>> new_sorted = getSortedTeams();

            // Find if ranking changed for this team
            int old_rank = -1, new_rank = -1;
            for (int i = 0; i < sorted_teams.size(); i++) {
                if (sorted_teams[i].first == target_team) {
                    old_rank = i;
                    break;
                }
            }
            for (int i = 0; i < new_sorted.size(); i++) {
                if (new_sorted[i].first == target_team) {
                    new_rank = i;
                    break;
                }
            }

            if (old_rank != new_rank) {
                // Find which team was displaced
                string displaced_team;
                if (new_rank < old_rank) {  // Moved up
                    displaced_team = sorted_teams[new_rank].first;
                } else {  // Moved down (shouldn't happen in this algorithm)
                    displaced_team = sorted_teams[new_rank].first;
                }

                ranking_changes.push_back({target_team, displaced_team});
                cout << target_team << " " << displaced_team << " "
                     << teams[target_team].total_solved << " "
                     << teams[target_team].total_penalty << endl;
            }
        }

        is_frozen = false;

        // Display final scoreboard
        displayScoreboard();

        return true;
    }

    // Query team ranking
    void queryRanking(const string& team_name) {
        if (teams.find(team_name) == teams.end()) {
            cout << "[Error]Query ranking failed: cannot find the team." << endl;
            return;
        }

        cout << "[Info]Complete query ranking." << endl;
        if (is_frozen) {
            cout << "[Warning]Scoreboard is frozen. The ranking may be inaccurate until it were scrolled." << endl;
        }

        // Find current ranking
        vector<pair<string, TeamInfo>> sorted_teams = getSortedTeams();
        int rank = -1;
        for (int i = 0; i < sorted_teams.size(); i++) {
            if (sorted_teams[i].first == team_name) {
                rank = i + 1;
                break;
            }
        }

        cout << team_name << " NOW AT RANKING " << rank << endl;
    }

    // Query submission
    void querySubmission(const string& team_name, const string& problem_name, const string& status) {
        if (teams.find(team_name) == teams.end()) {
            cout << "[Error]Query submission failed: cannot find the team." << endl;
            return;
        }

        cout << "[Info]Complete query submission." << endl;

        // Find last matching submission
        JudgeStatus target_status = JudgeStatus::Accepted;
        bool check_status = (status != "ALL");
        if (check_status) {
            target_status = parseStatus(status);
        }

        bool check_problem = (problem_name != "ALL");

        for (int i = submissions.size() - 1; i >= 0; i--) {
            const auto& sub = submissions[i];

            if (sub.team_name != team_name) continue;
            if (check_problem && sub.problem_name != problem_name) continue;
            if (check_status && sub.status != target_status) continue;

            // Found matching submission
            cout << sub.team_name << " " << sub.problem_name << " "
                 << getStatusString(sub.status) << " " << sub.time << endl;
            return;
        }

        cout << "Cannot find any submission." << endl;
    }

    // End competition
    void endCompetition() {
        cout << "[Info]Competition ends." << endl;
    }

private:
    // Get sorted teams for ranking
    vector<pair<string, TeamInfo>> getSortedTeams() {
        vector<pair<string, TeamInfo>> result;

        for (auto& [team_name, team] : teams) {
            team.updateTotals();
            result.push_back({team_name, team});
        }

        // Sort according to ranking rules
        sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
            const TeamInfo& teamA = a.second;
            const TeamInfo& teamB = b.second;

            // More solved problems first
            if (teamA.total_solved != teamB.total_solved) {
                return teamA.total_solved > teamB.total_solved;
            }

            // Less penalty time first
            if (teamA.total_penalty != teamB.total_penalty) {
                return teamA.total_penalty < teamB.total_penalty;
            }

            // Compare solve times in descending order
            int n = min(teamA.solve_times.size(), teamB.solve_times.size());
            for (int i = 0; i < n; i++) {
                if (teamA.solve_times[i] != teamB.solve_times[i]) {
                    return teamA.solve_times[i] < teamB.solve_times[i];
                }
            }

            // Lexicographic order of team names
            return a.first < b.first;
        });

        return result;
    }

    // Display scoreboard
    void displayScoreboard() {
        vector<pair<string, TeamInfo>> sorted_teams = getSortedTeams();

        for (int i = 0; i < sorted_teams.size(); i++) {
            const auto& [team_name, team] = sorted_teams[i];
            int rank = i + 1;

            cout << team_name << " " << rank << " " << team.total_solved
                 << " " << team.total_penalty;

            // Display problem status
            for (const string& prob : problem_names) {
                const auto& prob_status = team.problems.at(prob);

                if (prob_status.is_frozen) {
                    // Frozen problem
                    if (prob_status.incorrect_attempts == 0) {
                        cout << " 0/" << prob_status.frozen_submissions;
                    } else {
                        cout << " -" << prob_status.incorrect_attempts
                             << "/" << prob_status.frozen_submissions;
                    }
                } else if (prob_status.solve_time != -1) {
                    // Solved problem
                    if (prob_status.incorrect_attempts == 0) {
                        cout << " +";
                    } else {
                        cout << " +" << prob_status.incorrect_attempts;
                    }
                } else {
                    // Unsolved problem
                    if (prob_status.incorrect_attempts == 0) {
                        cout << " .";
                    } else {
                        cout << " -" << prob_status.incorrect_attempts;
                    }
                }
            }

            cout << endl;
        }
    }
};

int main() {
    ICPCManager manager;
    string line;

    while (getline(cin, line)) {
        if (line.empty()) continue;

        istringstream iss(line);
        string command;
        iss >> command;

        if (command == "ADDTEAM") {
            string team_name;
            iss >> team_name;
            manager.addTeam(team_name);
        }
        else if (command == "START") {
            string dummy;
            int duration, problems;
            iss >> dummy >> duration >> dummy >> problems;
            manager.startCompetition(duration, problems);
        }
        else if (command == "SUBMIT") {
            string problem, dummy1, team, dummy2, status, dummy3;
            int time;
            iss >> problem >> dummy1 >> team >> dummy2 >> status >> dummy3 >> time;
            manager.submit(problem, team, status, time);
        }
        else if (command == "FLUSH") {
            manager.flushScoreboard();
        }
        else if (command == "FREEZE") {
            manager.freezeScoreboard();
        }
        else if (command == "SCROLL") {
            manager.scrollScoreboard();
        }
        else if (command == "QUERY_RANKING") {
            string team_name;
            iss >> team_name;
            manager.queryRanking(team_name);
        }
        else if (command == "QUERY_SUBMISSION") {
            string team_name, dummy, problem, status;
            iss >> team_name >> dummy >> problem >> status;
            // Parse WHERE PROBLEM=... AND STATUS=...
            size_t prob_start = line.find("PROBLEM=") + 8;
            size_t prob_end = line.find(" AND STATUS=");
            size_t status_start = prob_end + 12;  // Length of " AND STATUS="

            string prob_name = line.substr(prob_start, prob_end - prob_start);
            string stat = line.substr(status_start);

            manager.querySubmission(team_name, prob_name, stat);
        }
        else if (command == "END") {
            manager.endCompetition();
            break;
        }
    }

    return 0;
}