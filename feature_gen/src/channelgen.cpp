#include <algorithm>        // for std::min
#include <iostream>
#include <ostream>
#include <queue>
#include <regex>
#include <stdexcept>
#include <stdio.h>
#include <stdlib.h>        // for exit()
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <fstream>


using namespace std;

template<class T>
using Map2d = vector<vector<T> >;

void ToFile2d(const vector<vector<int> > &vec2, const string &tensorFile);

bool file2lines(const string &fname, vector<string> &lines);

struct Position {
    size_t x;
    size_t y;

    Position(size_t _x, size_t _y) {
        x = _x;
        y = _y;
    }

    Position() {
        x = SIZE_T_MAX;
        y = SIZE_T_MAX;
    }
};

struct Agent {
    Position startpos;
    Position goalpos;
    vector<vector<Map2d<bool> > > kSubPath; // k * timeunit * Map2d < True iff cell is visitable at timeunit t on k-sub Path>
    vector<Map2d<int> > OnePath; // timeunit * Map2d
    int opt_path_length;

    string toString() const;
};

string Agent::toString() const {
    stringstream ss;
    ss << "<" << startpos.x << "," << startpos.y << ">";
    ss << " -> ";
    ss << "<" << goalpos.x << "," << goalpos.y << ">";
    ss << " ";
    ss << "Longest Path:" << kSubPath[0].size();
    return ss.str();
}

template<typename T>
string Map2string(Map2d<T> m) {
    stringstream ss;
    for (const auto &l: m) {
        for (const auto &x: l) {
            ss << int(x) << " ";
        }
        ss << "\n";
    }
    return ss.str();
}

class Datagen {
public:
    string scenfile;
    string mapfile;
    string outputPrefix;

    int mapHeight;
    int mapWidth;
    int kmax;
    int numAgentsMax;
    int numAgentsStep;

    Map2d<bool> nonObstacleMap;
    Map2d<bool> obstacleMap;
    vector<Map2d<int>> collisionMap;
    vector<Map2d<int>> singlePathCollisionMap;
    vector<int> nToRuns;

    vector<Agent> agents;

    string lines2agents(const vector<string> &lines);

    void lines2map2d(const vector<string> &lines);

    Map2d<int> GetDistFromPosMap(Position);

    void PeekAgentPathLength(Agent &agent);

    void SolveAgent(Agent &agent);

    std::vector<Position> GetAvailMove(Position);

    void Load(const string &sceneFile, const string &mapFile);

    void Solve();

    bool isCloserToEdnge(Position A, Position B);

    Map2d<bool> MarkPos(vector<Agent> &agents, bool startpos = 1);
};

template<typename T, typename U>
void ReduceMap(Map2d<T> &accumulated, Map2d<U> &increment, T (*func)(T, U)) {
    for (size_t i = 0; i < accumulated.size(); ++i) {
        for (size_t j = 0; j < accumulated[0].size(); ++j) {
            accumulated[i][j] = func(accumulated[i][j], increment[i][j]);
        }
    }
}

// return false on error
bool file2lines(const string &fname, vector<string> &lines) {
    ifstream myfile(fname.c_str(), ios_base::in);
    string line;
    if (myfile.is_open()) {
        for (; getline(myfile, line);) {
            lines.push_back(line);
        }
        myfile.close();
        return true;
    }
    return false;
}

void Datagen::lines2map2d(const vector<string> &lines) {
    string dimension_patten = "^[a-zA-Z]+ (\\d+)$";

    smatch match;
    auto matcher = regex(dimension_patten);

    regex_search(lines[1], match, matcher);
    mapHeight = stoi(match.str(1));
    regex_search(lines[2], match, matcher);
    mapWidth = stoi(match.str(1));

    nonObstacleMap.resize(mapHeight, vector<bool>(mapWidth, 0));
    // drop first 4 lines;
    for (size_t i = 4; i < lines.size(); ++i) {
        if (lines[i].size() != mapWidth) {
            printf("inconsistancy, expecting mapWidth = %d, got %lu", mapWidth, lines[i].size());
        }
        for (size_t j = 0; j < lines[i].size(); ++j) {
            nonObstacleMap[i - 4][j] = lines[i][j] == '.';
        }
    }
}

string Datagen::lines2agents(const vector<string> &lines) {
    smatch match;
    string mapline_patten = "^\\d+\\s(\\S+)(\\s\\d+){2}\\s(\\d+)\\s(\\d+)\\s(\\d+)\\s(\\d+)(\\s\\S+)$";
    auto matcher = regex(mapline_patten);

    regex_search(lines[1], match, matcher);
    string map = match.str(1);

    for (size_t i = 1; i < lines.size(); ++i) {
        regex_search(lines[i], match, matcher);
        Position startpos = Position(stoi(match.str(4)), stoi(match.str(3)));
        Position goalpos = Position(stoi(match.str(6)), stoi(match.str(5)));
        Agent a{startpos, goalpos};
        agents.push_back(a);
    }

    return map;
}

Map2d<bool> Datagen::MarkPos(vector<Agent> &agents, bool startpos) {
    Map2d<bool> map(mapHeight, vector<bool>(mapWidth, 0));
    for (Agent &a: agents) {
        Position pos = startpos ? a.startpos : a.goalpos;
        map[pos.x][pos.y] = true;
    }
    return map;
}

void Datagen::Load(const string &sceneFile, const string &mapFile) {
    vector<string> scene_lines;
    if (file2lines(sceneFile, scene_lines) == false) {
        printf("error reading file1 %s", sceneFile.data());
        exit(-1);
    }
    // convert scene_lines to agents and save in Datagen
    lines2agents(scene_lines);

    vector<string> map_lines;
    if (file2lines(mapFile, map_lines) == false) {
        printf("error reading file2 %s", mapFile.data());
        exit(-1);
    }
    //convert map_lines to map2d and save in Datagen
    lines2map2d(map_lines);
}

Map2d<int> Datagen::GetDistFromPosMap(Position initPos) {
    Map2d<int> distanceFromStartMap(mapHeight, vector<int>(mapWidth, INT_MAX));
    queue<pair<Position, int>> toVisit;
    toVisit.push(make_pair(initPos, 0));
    // bfs
    while (!toVisit.empty()) {
        Position pos = toVisit.front().first;
        int distance = toVisit.front().second;
        toVisit.pop();

        if (distance >= distanceFromStartMap[pos.x][pos.y]) {
            continue;
        }
        distanceFromStartMap[pos.x][pos.y] = distance;
        distance += 1;
        if (pos.x > 0 && nonObstacleMap[pos.x - 1][pos.y]) {
            toVisit.push(make_pair(Position(pos.x - 1, pos.y), distance));
        }
        if (pos.y > 0 && nonObstacleMap[pos.x][pos.y - 1]) {
            toVisit.push(make_pair(Position(pos.x, pos.y - 1), distance));
        }
        if (pos.x + 1 < mapHeight && nonObstacleMap[pos.x + 1][pos.y]) {
            toVisit.push(make_pair(Position(pos.x + 1, pos.y), distance));
        }
        if (pos.y + 1 < mapWidth && nonObstacleMap[pos.x][pos.y + 1]) {
            toVisit.push(make_pair(Position(pos.x, pos.y + 1), distance));
        }
    }
    return distanceFromStartMap;
}

bool once = true;

void Datagen::PeekAgentPathLength(Agent &agent) {
    Map2d<int> distanceFromStartMap = GetDistFromPosMap(agent.startpos);
    agent.opt_path_length = distanceFromStartMap[agent.goalpos.x][agent.goalpos.y];
    if (once && agent.opt_path_length == INT_MAX) {
        printf("problem unsolvable for some agents %s", agent.toString().c_str());
        exit(-1);
    }
}

std::vector<Position> Datagen::GetAvailMove(Position pos) {
    std::vector<Position> res;
    int x = static_cast<int>(pos.x);
    int y = static_cast<int>(pos.y);
    if (x + 1 < mapHeight) {
        res.push_back(Position(x + 1, y));
    }
    if (x >= 1) {
        res.push_back(Position(x - 1, y));
    }
    if (y + 1 < mapWidth) {
        res.push_back(Position(x, y + 1));
    }
    if (y >= 1) {
        res.push_back(Position(x, y - 1));
    }
    res.push_back(Position(x, y));
    if (res.size() == 1) {
        printf("No Avail Move, that is IMPOSSIBLE!\n");
    }

    struct doComparePos {
        doComparePos(const Datagen &datagen) :
                width(datagen.mapWidth),
                height(datagen.mapHeight) {
        }

        const int width;
        const int height;

        bool operator()(Position A, Position B) {
            int ax = static_cast<int>(A.x);
            int ay = static_cast<int>(A.y);
            int bx = static_cast<int>(B.x);
            int by = static_cast<int>(B.y);
            int a_dist_to_edge = min(min(width - ax, ax), min(height - ay, ay));
            int b_dist_to_edge = min(min(width - bx, bx), min(height - by, by));
            return a_dist_to_edge < b_dist_to_edge;
        }
    };

    // DDM: Make the agent path as far from center as possible (as close to one of the edges as possible)
    sort(res.begin(), res.end(), doComparePos(*this));
    return res;
}

void Datagen::SolveAgent(Agent &agent) {
    Map2d<int> distanceFromStartMap = GetDistFromPosMap(agent.startpos);

    Map2d<int> distanceFromGoalMap = GetDistFromPosMap(agent.goalpos);
    // build up agent.kSubPath


    //TODO: remove
    agent.kSubPath.resize(kmax,
                          vector<vector<vector<bool>>>(agent.opt_path_length + kmax + 1,
                                                       vector<vector<bool>>(mapHeight,
                                                                            vector<bool>(mapWidth, 0))));


    // Iterate over cells
    for (size_t i = 0; i < mapHeight; ++i) {
        for (size_t j = 0; j < mapWidth; ++j) {
            // Iterate over sub-opt tolerance k
            for (int k = 0; k < kmax; ++k) {
                int distanceFromStart = distanceFromStartMap[i][j];
                int distanceFromGoal = distanceFromGoalMap[i][j];
                // the sub-opt path will visit <i,j> at as early as t = distanceFromStart,
                // if there are excess time, the agent can choose to stay at <i,j> for a few moves
                // if distanceFromStart != INT_MAX it means that we are at a non-reachable block, to avoid
                //      integer overflow we need to check it first.
                for (int stay = 0; distanceFromStart != INT_MAX &&
                                   stay <= agent.opt_path_length + k - distanceFromStart - distanceFromGoal; ++stay) {
                    agent.kSubPath[k][distanceFromStart + stay][i][j] = true;
                }
            }
        }
    }


    agent.OnePath.resize(agent.opt_path_length + 1, vector<vector<int>>(mapHeight, vector<int>(mapWidth, 0)));
    Position pos = agent.startpos;
    for (int t = 0; t <= agent.opt_path_length; ++t) {
        agent.OnePath[t][pos.x][pos.y] = 1;
        if (pos.x == agent.goalpos.x && pos.y == agent.goalpos.y) {
            break;
        }
        bool updated = false;
        for (auto &next_pos: GetAvailMove(pos)) {
            if (distanceFromStartMap[next_pos.x][next_pos.y] == t + 1
                && distanceFromGoalMap[next_pos.x][next_pos.y] == agent.opt_path_length - t - 1) {
                pos = next_pos;
                updated = true;
                break;
            }
        }
        if (!updated) {
            printf("Look here! Line 349, something went wrong, not next_pos is selected \n");
            assert(updated);
        }
    }

}

bool myFlip(bool a, bool b) { return !b; }

template<typename T>
int myAdd(int a, T b) {
    return a + b;
}

int myMinusOneOnPositive(int a, int b) { return max(0, a - 1); }

string GetOutputFileName(string output_prefix, int num_agents, string feature) {
    string file_path = output_prefix + to_string(num_agents) + "_" + feature + ".txt";
    boost::filesystem::path path(file_path);

    if (!boost::filesystem::exists(path)) {
        try {
            boost::filesystem::create_directories(path.parent_path());
            std::ofstream file(file_path.c_str());
            file.close();
        } catch (const boost::filesystem::filesystem_error &e) {
            std::cout << e.what() << std::endl;
        }
    }

    return file_path;
}

void Datagen::Solve() {
    cout << "in Solve" << endl;

    Map2d<bool> obs_map(mapHeight, vector<bool>(mapWidth, 0));
    ReduceMap(obs_map, nonObstacleMap, &myFlip);
    obstacleMap = obs_map;

    size_t tmp = agents.size();
    for (int n = numAgentsStep; n <= min(agents.size(), static_cast<size_t>(numAgentsMax)); n += numAgentsStep) {
        nToRuns.push_back(n);
    }
    agents.resize(nToRuns.back());

    // Build and save goalLoc, obstacle, startLoc Map
    for (int n: nToRuns) {
        vector<Agent> first_n_agents(agents.begin(), agents.begin() + n);
        Map2d<bool> first_n_agents_startLocationMap = MarkPos(first_n_agents, 1);
        Map2d<bool> first_n_agents_goalLocationMap = MarkPos(first_n_agents, 0);

        // convert and store in Map2d<int>
        Map2d<int> startmap(mapHeight, vector<int>(mapWidth, 0));
        Map2d<int> goalmap(mapHeight, vector<int>(mapWidth, 0));
        Map2d<int> obstaclemap(mapHeight, vector<int>(mapWidth, 0));
        ReduceMap(startmap, first_n_agents_startLocationMap, &myAdd);
        ReduceMap(goalmap, first_n_agents_goalLocationMap, &myAdd);
        ReduceMap(obstaclemap, obstacleMap, &myAdd);

        ToFile2d(startmap, GetOutputFileName(outputPrefix, n, "startLoc"));
        ToFile2d(goalmap, GetOutputFileName(outputPrefix, n, "goalLoc"));
        ToFile2d(obstaclemap, GetOutputFileName(outputPrefix, n, "obstacle"));
    }


    // Store the max kSubPath Length among all agents.
    // max_kSubPath_size = max(agent opt path length amoungst agents) + kmax
    size_t max_kSubPath_size = 0;
    for (auto &ag: agents) {
        PeekAgentPathLength(ag);
        max_kSubPath_size = max(max_kSubPath_size, (size_t) ag.opt_path_length + kmax);
    }


    printf("agents.size() = %lu -> %lu ; max_kSubPath_size = %lu\n", tmp, agents.size(), max_kSubPath_size);

    // Start working on kSubPath collision
    vector<vector<Map2d<int>>> kTimeCollision(kmax + 1,
                                              vector<vector<vector<int>>>(max_kSubPath_size,
                                                                          vector<vector<int>>(mapHeight,
                                                                                              vector<int>(mapWidth,
                                                                                                          0))));

    vector<vector<vector<int>>> kTimeStacked(kmax + 1,
                                             vector<vector<int>>(mapHeight,
                                                                 vector<int>(mapWidth, 0)));

    Map2d<int> singlePathCollision(mapHeight, vector<int>(mapWidth, 0));


    // If an agent visit some cell on (k)SubPath at time = i , mark the cell on timeCollision[k][i]
    for (int i = 0; i < nToRuns.back(); i += numAgentsStep) {
        for (int j = 0; j < numAgentsStep; ++j) {
            Agent &ag = agents[i + j];

            SolveAgent(ag);

            //work out kSubPath data and stores in kTimeCollision
            for (int k = 0; k < kmax; ++k) {
                while (ag.kSubPath[k].size() < max_kSubPath_size) {
                    ag.kSubPath[k].push_back(ag.kSubPath[k].back());
                }

                for (int t = 0; t < static_cast<int>(max_kSubPath_size); ++t) {
                    ReduceMap(kTimeCollision[k][t], ag.kSubPath[k][t], &myAdd);
                    ReduceMap(kTimeStacked[k], ag.kSubPath[k][t], &myAdd);
                }

            }


            ag.kSubPath.clear();
            ag.kSubPath.shrink_to_fit();

            // work out SinglePath data and stores in singlePathCollision
            for (int t = 0; t < static_cast<int>(ag.opt_path_length + 1); ++t) {
                ReduceMap(singlePathCollision, ag.OnePath[t], &myAdd);
            }
            // ag.OnePath.clear();
            // ag.OnePath.shrink_to_fit();
        }

        // save kSupPath Collision to k seperate tensor files
        for (int k = 0; k < kmax; ++k) {
            Map2d<int> kCollision(mapHeight, vector<int>(mapWidth, 0));
            for (int t = 0; t < static_cast<int>(max_kSubPath_size); ++t) {
                ReduceMap(kTimeCollision[k][t], kTimeCollision[k][t], &myMinusOneOnPositive);
                ReduceMap(kCollision, kTimeCollision[k][t], &myAdd);
            }
            ToFile2d(kCollision, GetOutputFileName(outputPrefix, i + numAgentsStep, to_string(k) + "subPathColl"));
            ToFile2d(kTimeStacked[k],
                     GetOutputFileName(outputPrefix, i + numAgentsStep, to_string(k) + "subPathStack"));
        }

        // save SinglePathCollision to a file
        ToFile2d(singlePathCollision, GetOutputFileName(outputPrefix, i + numAgentsStep, "singlePathHeat"));

    }

    fflush(stdout);
}

template<typename T>
vector<T> linearize2d(const vector<vector<T>> &vec2) {
    vector<T> vec;
    for (const auto &v: vec2) {
        for (auto d: v) {
            vec.push_back(d);
        }
    }
    return vec;
}

template<typename T>
vector<T> linearize3d(const vector<vector<vector<T>>> &v3) {
    vector<T> vec;
    for (const auto &v2: v3) {
        for (const auto &v: v2) {
            for (const auto &item: v) {
                vec.push_back(item);
            }
        }
    }
    return vec;
}

// save a 2d vector to file
void ToFile2d(const vector<vector<int> > &vec2, const string &file) {
    std::ofstream outfile(file);
    outfile << vec2.size() << " " << vec2[0].size() << std::endl;
    for (const auto &i: vec2) {
        for (const auto &j: i) {
            outfile << j << " ";
        }
        outfile << std::endl;
    }
}

int main(int argc, char *argv[]) {

    Datagen datagen;
    try {
        namespace po = boost::program_options;
        // Declare the supported options.
        po::options_description desc("Allowed options");
        desc.add_options()
                ("help", "produce help message")

                // params for the input instance and experiment settings
                ("map,m", po::value<string>()->required(), "input file for map")
                ("agents,a", po::value<string>()->required(), "input file for agents")
                ("output,o", po::value<string>()->required(), "output file for channels")

                ("agentNumMax,n", po::value<int>()->default_value(20), "maximum number of agents")
                ("agentNumStep", po::value<int>()->default_value(10), "number of agents increment per solve")
                ("subOptK,k", po::value<int>()->default_value(3), "number of agents increment per solve")
                // for example, if agentNumMax = 30 and agentNumStep = 10, then the number of agents will be 10, 20, 30,
                // we output corresponding channels for each number of agents

            // ("cutoffTime,t", po::value<double>()->default_value(7200), "cutoff time (seconds)")

            // ("channels,c", po::value<string>()->required(), "output channels")
                ;


        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, desc), vm);

        if (vm.count("help")) {
            cout << desc << endl;
            return 1;
        }

        po::notify(vm);
        datagen.mapfile = vm["map"].as<string>();
        datagen.scenfile = vm["agents"].as<string>();
        datagen.outputPrefix = vm["output"].as<string>();
        datagen.numAgentsMax = vm["agentNumMax"].as<int>();
        datagen.numAgentsStep = vm["agentNumStep"].as<int>();
        datagen.kmax = vm["subOptK"].as<int>();

    }
    catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
    cout << "Loading()" << endl;
    datagen.Load(datagen.scenfile, datagen.mapfile);
    cout << "Solve()" << endl;
    datagen.Solve();
}


