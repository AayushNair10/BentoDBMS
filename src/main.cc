#include <algorithm>
#include <chrono>
#include <iostream>
#include <string>
#include <vector>
#include <boost/algorithm/string.hpp>
#include "interpreter.h"

using namespace std;

bool quiet_mode = false;
Stats stats = {0, 0, 0, 0, 0, 0, 0};

// Time taken by each statement in microseconds, only filled with --stats
static vector<long> statement_times;

// Prints the counters and statement timings to stderr, so results on stdout stay clean
static void PrintStats() {
    cerr << "--- stats ---" << endl;
    cerr << "statements: " << statement_times.size() << endl;
    cerr << "block_reads: " << stats.block_reads << endl;
    cerr << "block_writes: " << stats.block_writes << endl;
    cerr << "cache_hits: " << stats.cache_hits << endl;
    cerr << "cache_misses: " << stats.cache_misses << endl;
    cerr << "evictions: " << stats.evictions << endl;
    cerr << "catalog_writes: " << stats.catalog_writes << endl;
    cerr << "node_visits: " << stats.node_visits << endl;
    if (!statement_times.empty()) {
        sort(statement_times.begin(), statement_times.end());
        size_t n = statement_times.size();
        cerr << "latency_p50_us: " << statement_times[n / 2] << endl;
        cerr << "latency_p99_us: " << statement_times[n * 99 / 100] << endl;
        cerr << "latency_max_us: " << statement_times[n - 1] << endl;
    }
}

ostream &debug_out() {
    static ostream null_stream(NULL); // No buffer attached, so output is dropped
    return quiet_mode ? null_stream : cout;
}

int main(int argc, const char *argv[]) {
    bool show_stats = false;
    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
        if (arg == "-q" || arg == "--quiet") {
            quiet_mode = true;
        } else if (arg == "--stats") {
            show_stats = true;
        } else {
            cerr << "Usage: " << argv[0] << " [-q | --quiet] [--stats]" << endl;
            return 1;
        }
    }
    if (show_stats) {
        // QUIT ends the program with exit(), so the stats are printed from atexit
        atexit(PrintStats);
    }

    string sql;
    Interpreter itp;

    while (true) {
        cout << "MiniDB> ";
        if (!getline(cin, sql)) {
            break; // Exit on EOF or input error
        }
        boost::algorithm::trim(sql);

        if (sql == "exit" || sql == "quit") {
            itp.ExecSQL("quit");
            break;
        }

        while (sql.find(";") == string::npos) {
            string continuation;
            if (!getline(cin, continuation)) {
                break; // Exit on EOF or input error
            }
            sql += "\n" + continuation;
        }

        if (!sql.empty()) {
            // Handle command history if needed
        }

        if (show_stats) {
            chrono::steady_clock::time_point start = chrono::steady_clock::now();
            itp.ExecSQL(sql);
            statement_times.push_back(chrono::duration_cast<chrono::microseconds>(
                chrono::steady_clock::now() - start).count());
        } else {
            itp.ExecSQL(sql);
        }
        cout << endl;
    }
    return 0;
}
