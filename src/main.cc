#include <iostream>
#include <string>
#include <boost/algorithm/string.hpp>
#include "interpreter.h"

using namespace std;

int main(int argc, const char *argv[]) {
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

        itp.ExecSQL(sql);
        cout << endl;
    }
    return 0;
}
