#include <iostream>
#include <vector>
#include <map>
#include <set>
#include <string>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <climits>

using namespace std;

enum class SubmissionStatus {
    Accepted,
    Wrong_Answer,
    Runtime_Error,
    Time_Limit_Exceed
};

SubmissionStatus parseStatus(const string& status) {
    if (status == "Accepted") return SubmissionStatus::Accepted;
    if (status == "Wrong_Answer") return SubmissionStatus::Wrong_Answer;
    if (status == "Runtime_Error") return SubmissionStatus::Runtime_Error;
    if (status == "Time_Limit_Exceed") return SubmissionStatus::Time_Limit_Exceed;
    return SubmissionStatus::Wrong_Answer; // default
}

string statusToString(SubmissionStatus status) {
    switch (status) {
        case SubmissionStatus::Accepted: return "Accepted";
        case SubmissionStatus::Wrong_Answer: return "Wrong_Answer";
        case SubmissionStatus::Runtime_Error: return "Runtime_Error";
        case SubmissionStatus::Time_Limit_Exceed: return "Time_Limit_Exceed";
    }
    return "Unknown";
}

struct Submission {
    char problem;
    SubmissionStatus status;
    int time;
    
    Submission(char p, SubmissionStatus s, int t) : problem(p), status(s), time(t) {}
};

struct ProblemState {
    vector<Submission> submissions;
    bool solved = false;
    int solve_time = -1;
    int penalty = 0;
    int wrong_before_solve = 0;
    bool is_frozen = false;
    int wrong_before_freeze = 0;
    int submissions_after_freeze = 0;
    
    void addSubmission(const Submission& sub, bool frozen) {
        submissions.push_back(sub);
        
        if (frozen) {
            submissions_after_freeze++;
            if (!solved) {
                is_frozen = true;
            }
        } else {
            if (!solved && sub.status == SubmissionStatus::Accepted) {
                solved = true;
                solve_time = sub.time;
                penalty = 20 * wrong_before_solve + solve_time;
            } else if (!solved && sub.status != SubmissionStatus::Accepted) {
                wrong_before_solve++;
                wrong_before_freeze++;
            }
        }
    }
    
    string getDisplayStatus() const {
        if (is_frozen) {
            string result = "";
            if (wrong_before_freeze == 0) {
                result = "0";
            } else {
                result = to_string(wrong_before_freeze);
            }
            result = "-" + result + "/" + to_string(submissions_after_freeze);
            return result;
        } else if (solved) {
            if (wrong_before_solve == 0) return "+";
            return "+" + to_string(wrong_before_solve);
        } else {
            int wrong_attempts = wrong_before_solve;
            if (wrong_attempts == 0) return ".";
            return "-" + to_string(wrong_attempts);
        }
    }
};

struct Team {
    string name;
    map<char, ProblemState> problems;
    vector<Submission> submissions;
    int total_solved = 0;
    int total_penalty = 0;
    vector<int> solve_times;
    
    Team() = default;
    Team(const string& n) : name(n) {}
    
    void updateStatistics() {
        total_solved = 0;
        total_penalty = 0;
        solve_times.clear();
        
        for (const auto& [prob_char, prob_state] : problems) {
            if (prob_state.solved) {
                total_solved++;
                total_penalty += prob_state.penalty;
                solve_times.push_back(prob_state.solve_time);
            }
        }
        
        sort(solve_times.rbegin(), solve_times.rend());
    }
    
    void addSubmission(char problem, const Submission& sub, bool frozen) {
        submissions.push_back(sub);
        problems[problem].addSubmission(sub, frozen);
        updateStatistics();
    }
    
    void freezeProblems() {
        for (auto& [prob_char, prob_state] : problems) {
            if (!prob_state.solved) {
                prob_state.is_frozen = true;
            }
        }
    }
    
    void unfreezeProblem(char problem) {
        if (problems.count(problem)) {
            auto& prob_state = problems[problem];
            if (prob_state.is_frozen) {
                prob_state.is_frozen = false;
                // Process submissions after freeze
                for (size_t i = prob_state.submissions.size() - prob_state.submissions_after_freeze; 
                     i < prob_state.submissions.size(); i++) {
                    const auto& sub = prob_state.submissions[i];
                    if (!prob_state.solved && sub.status == SubmissionStatus::Accepted) {
                        prob_state.solved = true;
                        prob_state.solve_time = sub.time;
                        prob_state.penalty = 20 * prob_state.wrong_before_solve + prob_state.solve_time;
                        break;
                    } else if (!prob_state.solved && sub.status != SubmissionStatus::Accepted) {
                        prob_state.wrong_before_solve++;
                    }
                }
                prob_state.submissions_after_freeze = 0;
            }
        }
        updateStatistics();
    }
};

struct TeamRanking {
    string name;
    int solved;
    int penalty;
    vector<int> solve_times;
    
    TeamRanking(const string& n, int s, int p, const vector<int>& st) 
        : name(n), solved(s), penalty(p), solve_times(st) {}
    
    bool operator<(const TeamRanking& other) const {
        if (solved != other.solved) return solved > other.solved;
        if (penalty != other.penalty) return penalty < other.penalty;
        
        // Compare solve times from largest to smallest
        size_t min_size = min(solve_times.size(), other.solve_times.size());
        for (size_t i = 0; i < min_size; i++) {
            if (solve_times[i] != other.solve_times[i]) {
                return solve_times[i] < other.solve_times[i];
            }
        }
        
        if (solve_times.size() != other.solve_times.size()) {
            return solve_times.size() < other.solve_times.size();
        }
        
        return name < other.name;
    }
    
    bool operator==(const TeamRanking& other) const {
        return name == other.name && solved == other.solved && 
               penalty == other.penalty && solve_times == other.solve_times;
    }
};

class Competition {
private:
    bool started = false;
    bool ended = false;
    int duration = 0;
    int problem_count = 0;
    map<string, Team> teams;
    vector<TeamRanking> current_ranking;
    bool is_frozen = false;
    bool needs_flush = false;
    bool has_flushed_before = false;
    
public:
    void addTeam(const string& name) {
        if (started) {
            cout << "[Error]Add failed: competition has started.\n";
            return;
        }
        
        if (teams.count(name)) {
            cout << "[Error]Add failed: duplicated team name.\n";
            return;
        }
        
        teams[name] = Team(name);
        cout << "[Info]Add successfully.\n";
    }
    
    void startCompetition(int dur, int prob_count) {
        if (started) {
            cout << "[Error]Start failed: competition has started.\n";
            return;
        }
        
        started = true;
        duration = dur;
        problem_count = prob_count;
        
        // Initialize problem states for all teams
        for (auto& [name, team] : teams) {
            for (char c = 'A'; c < 'A' + prob_count; c++) {
                team.problems[c] = ProblemState();
            }
        }
        
        updateRanking();
        cout << "[Info]Competition starts.\n";
    }
    
    void submit(const string& problem, const string& team_name, const string& status, int time) {
        if (!started || ended) return;
        
        if (!teams.count(team_name)) return;
        
        char prob_char = problem[0];
        SubmissionStatus sub_status = parseStatus(status);
        Submission sub(prob_char, sub_status, time);
        
        teams[team_name].addSubmission(prob_char, sub, is_frozen);
        needs_flush = true;
    }
    
    void flush() {
        if (!started || ended) return;
        
        has_flushed_before = true;
        updateRanking();
        needs_flush = false;
        cout << "[Info]Flush scoreboard.\n";
    }
    
    void freeze() {
        if (!started || ended) return;
        
        if (is_frozen) {
            cout << "[Error]Freeze failed: scoreboard has been frozen.\n";
            return;
        }
        
        is_frozen = true;
        for (auto& [name, team] : teams) {
            team.freezeProblems();
        }
        
        cout << "[Info]Freeze scoreboard.\n";
    }
    
    void scroll() {
        if (!started || ended) return;
        
        if (!is_frozen) {
            cout << "[Error]Scroll failed: scoreboard has not been frozen.\n";
            return;
        }
        
        flush();
        
        cout << "[Info]Scroll scoreboard.\n";
        printScoreboard();
        
        // Find teams with frozen problems and unfreeze them one by one
        vector<pair<string, char>> unfreeze_order;
        
        while (true) {
            updateRanking();
            
            // Find lowest-ranked team with frozen problems
            string team_to_unfreeze;
            char problem_to_unfreeze = 0;
            
            for (int i = current_ranking.size() - 1; i >= 0; i--) {
                const string& team_name = current_ranking[i].name;
                const Team& team = teams[team_name];
                
                char smallest_frozen = 0;
                for (char c = 'A'; c < 'A' + problem_count; c++) {
                    if (team.problems.at(c).is_frozen) {
                        if (smallest_frozen == 0 || c < smallest_frozen) {
                            smallest_frozen = c;
                        }
                    }
                }
                
                if (smallest_frozen != 0) {
                    team_to_unfreeze = team_name;
                    problem_to_unfreeze = smallest_frozen;
                    break;
                }
            }
            
            if (team_to_unfreeze.empty()) break;
            
            // Store old ranking for comparison
            vector<TeamRanking> old_ranking = current_ranking;
            
            // Unfreeze the problem
            teams[team_to_unfreeze].unfreezeProblem(problem_to_unfreeze);
            
            // Update ranking
            updateRanking();
            
            // Check if ranking changed
            if (old_ranking != current_ranking) {
                // Find the team that moved up
                for (size_t i = 0; i < current_ranking.size(); i++) {
                    if (current_ranking[i].name == team_to_unfreeze) {
                        int new_rank = i + 1;
                        int old_rank = -1;
                        for (size_t j = 0; j < old_ranking.size(); j++) {
                            if (old_ranking[j].name == team_to_unfreeze) {
                                old_rank = j + 1;
                                break;
                            }
                        }
                        
                        if (old_rank != new_rank && new_rank < old_rank) {
                            // Find the team that was replaced
                            string replaced_team = old_ranking[new_rank - 1].name;
                            cout << team_to_unfreeze << " " << replaced_team << " " 
                                 << current_ranking[i].solved << " " << current_ranking[i].penalty << "\n";
                        }
                        break;
                    }
                }
            }
        }
        
        is_frozen = false;
        printScoreboard();
    }
    
    void queryRanking(const string& team_name) {
        if (!teams.count(team_name)) {
            cout << "[Error]Query ranking failed: cannot find the team.\n";
            return;
        }
        
        cout << "[Info]Complete query ranking.\n";
        
        if (is_frozen) {
            cout << "[Warning]Scoreboard is frozen. The ranking may be inaccurate until it were scrolled.\n";
        }
        
        if (needs_flush) {
            updateRanking();
        }
        
        for (size_t i = 0; i < current_ranking.size(); i++) {
            if (current_ranking[i].name == team_name) {
                cout << "[" << team_name << "] NOW AT RANKING " << (i + 1) << "\n";
                return;
            }
        }
    }
    
    void querySubmission(const string& team_name, const string& problem, const string& status) {
        if (!teams.count(team_name)) {
            cout << "[Error]Query submission failed: cannot find the team.\n";
            return;
        }
        
        cout << "[Info]Complete query submission.\n";
        
        const Team& team = teams[team_name];
        const Submission* found = nullptr;
        
        for (auto it = team.submissions.rbegin(); it != team.submissions.rend(); it++) {
            bool problem_match = (problem == "ALL" || string(1, it->problem) == problem);
            bool status_match = (status == "ALL" || statusToString(it->status) == status);
            
            if (problem_match && status_match) {
                found = &(*it);
                break;
            }
        }
        
        if (found) {
            cout << "[" << team_name << "] " << found->problem << " " 
                 << statusToString(found->status) << " " << found->time << "\n";
        } else {
            cout << "Cannot find any submission.\n";
        }
    }
    
    void end() {
        if (!started || ended) return;
        
        ended = true;
        cout << "[Info]Competition ends.\n";
    }
    
private:
    void updateRanking() {
        current_ranking.clear();
        
        for (const auto& [name, team] : teams) {
            current_ranking.emplace_back(name, team.total_solved, team.total_penalty, team.solve_times);
        }
        
        if (has_flushed_before) {
            sort(current_ranking.begin(), current_ranking.end());
        } else {
            // Before first flush, rank by team name lexicographically
            sort(current_ranking.begin(), current_ranking.end(), 
                 [](const TeamRanking& a, const TeamRanking& b) {
                     return a.name < b.name;
                 });
        }
    }
    
    void printScoreboard() {
        for (size_t i = 0; i < current_ranking.size(); i++) {
            const auto& rank = current_ranking[i];
            const Team& team = teams[rank.name];
            
            cout << team.name << " " << (i + 1) << " " << team.total_solved << " " << team.total_penalty;
            
            for (char c = 'A'; c < 'A' + problem_count; c++) {
                cout << " " << team.problems.at(c).getDisplayStatus();
            }
            
            cout << "\n";
        }
    }
};

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    
    Competition competition;
    string line;
    
    while (getline(cin, line)) {
        if (line.empty()) continue;
        
        istringstream iss(line);
        string command;
        iss >> command;
        
        if (command == "ADDTEAM") {
            string team_name;
            iss >> team_name;
            competition.addTeam(team_name);
        } else if (command == "START") {
            string duration_str, problem_str;
            int duration, problem_count;
            iss >> duration_str >> duration >> problem_str >> problem_count;
            competition.startCompetition(duration, problem_count);
        } else if (command == "SUBMIT") {
            string problem, by, team_name, with, status, at, time_str;
            int time;
            iss >> problem >> by >> team_name >> with >> status >> at >> time;
            competition.submit(problem, team_name, status, time);
        } else if (command == "FLUSH") {
            competition.flush();
        } else if (command == "FREEZE") {
            competition.freeze();
        } else if (command == "SCROLL") {
            competition.scroll();
        } else if (command == "QUERY_RANKING") {
            string team_name;
            iss >> team_name;
            competition.queryRanking(team_name);
        } else if (command == "QUERY_SUBMISSION") {
            string team_name, where, problem_str, and_str, status_str;
            iss >> team_name >> where >> problem_str >> and_str >> status_str;
            
            string problem = problem_str.substr(8); // Remove "PROBLEM="
            string status = status_str.substr(7);   // Remove "STATUS="
            
            competition.querySubmission(team_name, problem, status);
        } else if (command == "END") {
            competition.end();
            break;
        }
    }
    
    return 0;
}