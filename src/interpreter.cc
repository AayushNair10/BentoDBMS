#include "interpreter.h"

#include <fstream>
#include <iostream>

#include <fcntl.h>
#include <sys/file.h>

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/regex.hpp>

#include "exceptions.h"
#include "minidb_api.h"

using namespace std;

//Constructor
Interpreter::Interpreter() : sql_type_(-1) {
  const char *home = getenv("HOME");
  if (home == NULL) {
    cerr << "HOME is not set" << endl;
    exit(1);
  }
  string p = string(home) + "/MiniDBData/";
  // Only one instance may use the data folder at a time. The lock is held
  // until the process exits.
  string lock_path = p + ".lock";
  int lock_fd = open(lock_path.c_str(), O_RDWR | O_CREAT, 0644);
  if (lock_fd == -1) {
    cerr << "Cannot open " << lock_path << " (does " << p << " exist?)" << endl;
    exit(1);
  }
  if (flock(lock_fd, LOCK_EX | LOCK_NB) == -1) {
    cerr << "Another MiniDB instance is already using " << p << endl;
    exit(1);
  }
  api = new MiniDBAPI(p);
}

//Destructor
Interpreter::~Interpreter() { delete api; }

//Tokeninizing the inputted SQL Query
vector<string> split(string str, string sep) {
  char *cstr = const_cast<char *>(str.c_str());
  char *current;
  vector<string> arr;
  current = strtok(cstr, sep.c_str());
  while (current != NULL) {
    arr.push_back(current);
    current = strtok(NULL, sep.c_str());
  }
  return arr;
}

//Cleaning the SQL Statement inputted from the user and tokenizing it on the basis of plain spaces.
void Interpreter::FormatSQL() {
  std::transform(sql_statement_.begin(), sql_statement_.end(), sql_statement_.begin(),
                     [](unsigned char c){ return std::tolower(c); });
  // remove newlines, tabs
  boost::regex reg("[\r\n\t]");
  // string newstr(" ");
  sql_statement_ = boost::regex_replace(sql_statement_, reg, " ");

  // remove ; and chars after ;
  reg = ";.*$";
  // string newstr(" ");
  sql_statement_ = boost::regex_replace(sql_statement_, reg, "");

  // remove leading spaces and trailing spaces
  reg = "(^ +)|( +$)";
  sql_statement_ = boost::regex_replace(sql_statement_, reg, "");

  // remove duplicate spaces
  reg = " +";
  sql_statement_ = boost::regex_replace(sql_statement_, reg, " ");

  // insert space before or after ( ) , = <> < >
  reg = " ?(\\(|\\)|,|=|(<>)|<|>) ?";
  sql_statement_ = boost::regex_replace(sql_statement_, reg, " $1 ");
  reg = "< *>";
  sql_statement_ = boost::regex_replace(sql_statement_, reg, "<>");
  reg = "< *=";
  sql_statement_ = boost::regex_replace(sql_statement_, reg, "<=");
  reg = "> *=";
  sql_statement_ = boost::regex_replace(sql_statement_, reg, ">=");

  // split sql_statement_
  sql_vector_ = split(sql_statement_, " ");
}

void Interpreter::TellSQLType() {
  if (sql_vector_.size() == 0) {
    sql_type_ = -1;
    debug_out() << "SQL TYPE: #EMPTY#" << endl;
    return;
  }
  // Second word of the statement, empty when there is none
  string second = sql_vector_.size() > 1 ? sql_vector_[1] : "";
  if (sql_vector_[0] == "quit") {
    debug_out() << "SQL TYPE: #QUIT#" << endl;
    sql_type_ = 10;
  } else if (sql_vector_[0] == "help") {
    debug_out() << "SQL TYPE: #HELP#" << endl;
    sql_type_ = 20;
  } else if (sql_vector_[0] == "create") {
    if (second == "database") {
      debug_out() << "SQL TYPE: #CREATE DATABASE#" << endl;
      sql_type_ = 30;
    } else if (second == "table") {
      debug_out() << "SQL TYPE: #CREATE TABLE#" << endl;
      sql_type_ = 31;
    } else if (second == "index") {
      debug_out() << "SQL TYPE: #CREATE INDEX#" << endl;
      sql_type_ = 32;
    } else {
      sql_type_ = -1;
    }
  } else if (sql_vector_[0] == "show") {
    if (second == "databases") {
      debug_out() << "SQL TYPE: #SHOW DATABASES#" << endl;
      sql_type_ = 40;
    } else if (second == "tables") {
      debug_out() << "SQL TYPE: #SHOW TABLES#" << endl;
      sql_type_ = 41;
    } else if (second == "table") {
        debug_out() << "SQL TYPE: #SHOW TABLE#" << endl;
        sql_type_ = 42;
    } else {
      sql_type_ = -1;
    }
  } else if (sql_vector_[0] == "drop") {
    if (second == "database") {
      debug_out() << "SQL TYPE: #DROP DATABASE#" << endl;
      sql_type_ = 50;
    } else if (second == "table") {
      debug_out() << "SQL TYPE: #DROP TABLE#" << endl;
      sql_type_ = 51;
    } else if (second == "index") {
      debug_out() << "SQL TYPE: #DROP INDEX#" << endl;
      sql_type_ = 52;
    } else {
      sql_type_ = -1;
    }
  } else if (sql_vector_[0] == "use") {
    debug_out() << "SQL TYPE: #USE#" << endl;
    sql_type_ = 60;
  } else if (sql_vector_[0] == "insert") {
    debug_out() << "SQL TYPE: #INSERT#" << endl;
    sql_type_ = 70;
  } else if (sql_vector_[0] == "exec") {
    debug_out() << "SQL TYPE: #EXEC#" << endl;
    sql_type_ = 80;
  } else if (sql_vector_[0] == "select") {
    debug_out() << "SQL TYPE: #SELECT#" << endl;
    sql_type_ = 90;
  } else if (sql_vector_[0] == "delete") {
    debug_out() << "SQL TYPE: #DELETE#" << endl;
    sql_type_ = 100;
  } else if (sql_vector_[0] == "update") {
    debug_out() << "SQL TYPE: #UPDATE#" << endl;
    sql_type_ = 110;
  } else if (sql_vector_[0] == "join") {
    debug_out() << "SQL TYPE: #JOIN#" << endl;
    sql_type_ = 120;
  } else {
    sql_type_ = -1;
    debug_out() << "SQL TYPE: #UNKNOWN#" << endl;
  }
}

void Interpreter::Run() {
  try {
    switch (sql_type_) {
    case 10: {
      api->Quit();
      exit(0);
    } break;
    case 20: {
      api->Help();
    } break;
    case 30: {
      SQLCreateDatabase *st = new SQLCreateDatabase(sql_vector_);
      api->CreateDatabase(*st);
      delete st;
    } break;
    case 31: {
      SQLCreateTable *st = new SQLCreateTable(sql_vector_);
      api->CreateTable(*st);
      delete st;
    } break;
    case 32: {
      SQLCreateIndex *st = new SQLCreateIndex(sql_vector_);
      api->CreateIndex(*st);
      delete st;
    } break;
    case 40: {
      api->ShowDatabases();
    } break;
    case 41: {
      api->ShowTables();
    } break;
    case 42: {
        if (sql_vector_.size() <= 2) {
          throw SyntaxErrorException("expected a table name");
        }
        string tb_name = sql_vector_[2];
        sql_vector_.clear();
        sql_vector_ = {"select", "*", "from", tb_name};
        SQLSelect *st = new SQLSelect(sql_vector_);
        api->Select(*st);
        delete st;
    } break;
    case 50: {
      SQLDropDatabase *st = new SQLDropDatabase(sql_vector_);
      api->DropDatabase(*st);
      delete st;
    } break;
    case 51: {
      SQLDropTable *st = new SQLDropTable(sql_vector_);
      api->DropTable(*st);
      delete st;
    } break;
    case 52: {
      SQLDropIndex *st = new SQLDropIndex(sql_vector_);
      api->DropIndex(*st);
      delete st;
    } break;
    case 60: {
      SQLUse *st = new SQLUse(sql_vector_);
      api->Use(*st);
      delete st;
    } break;
    case 70: {
      SQLInsert *st = new SQLInsert(sql_vector_);
      api->Insert(*st);
      delete st;
    } break;
    case 80: {
      SQLExec *st = new SQLExec(sql_vector_);
      string contents;
      ifstream in(st->file_name(), ios::in | ios::binary);
      if (!in.is_open()) {
        cerr << "Cannot open file: " << st->file_name() << endl;
        delete st;
        break;
      }
      in.seekg(0, std::ios::end);
      contents.resize(in.tellg());
      in.seekg(0, std::ios::beg);
      in.read(&contents[0], contents.size());
      in.close();
      debug_out() << endl;
      vector<string> sqls = split(contents, ";");
      for (int i = 0; i < sqls.size() - 1; ++i) {
        ExecSQL(sqls[i]);
      }
      delete st;
    } break;
    case 90: {
      SQLSelect *st = new SQLSelect(sql_vector_);
      api->Select(*st);
      delete st;
    } break;
    case 100: {
      SQLDelete *st = new SQLDelete(sql_vector_);
      api->Delete(*st);
      delete st;
    } break;
    case 110: {
      SQLUpdate *st = new SQLUpdate(sql_vector_);
      api->Update(*st);
      delete st;
    } break;
    case 120: {
      SQLJoin *st = new SQLJoin(sql_vector_);
        api->Join(*st);
        delete st;
    }
    break;
    default:
      if (sql_vector_.size() != 0) {
        cerr << "Unknown command: " << sql_statement_ << endl;
      }
      break;
    }
  } catch (SyntaxErrorException &e) {
    cerr << "Syntax Error: " << e.what() << endl;
  } catch (std::out_of_range &e) {
    cerr << "Syntax Error: statement is incomplete" << endl;
  } catch (ColumnNotExistException &e) {
    cerr << "Column doesn't exist: " << e.what() << endl;
  } catch (ColumnCountNotMatchException &e) {
    cerr << "Number of values doesn't match the number of columns!" << endl;
  } catch (RecordTooLongException &e) {
    cerr << "Row is too long: a row must fit in one block (4084 bytes)!" << endl;
  } catch (BufferFullException &e) {
    cerr << "Buffer is full: too many blocks in use at once!" << endl;
  } catch (NoDatabaseSelectedException &e) {
    cerr << "No database selected!" << endl;
  } catch (DatabaseNotExistException &e) {
    cerr << "Database doesn't exist!" << endl;
  } catch (DatabaseAlreadyExistsException &e) {
    cerr << "Database already exists!" << endl;
  } catch (TableNotExistException &e) {
    cerr << "Table doesn't exist!" << endl;
  } catch (OneIndexEachTableException &e) {
    cerr << "Each table could only have one index!" << endl;
  } catch (BPlusTreeException &e) {
    cerr << "BPlusTree exception!" << endl;
  } catch (TableAlreadyExistsException &e) {
    cerr << "Table already exists!" << endl;
  } catch (IndexAlreadyExistsException &e) {
    cerr << "Index already exists!" << endl;
  } catch (IndexNotExistException &e) {
    cerr << "Index doesn't exist!" << endl;
  } catch (IndexMustBeCreatedOnPKException &e) {
    cerr << "Index must be created on primary key!" << endl;
  } catch (PrimaryKeyConflictException &e) {
    cerr << "Primary key conflicts!" << endl;
  } catch (std::exception &e) {
    // Anything unexpected is reported instead of ending the session
    cerr << "Error: " << e.what() << endl;
  }
}

void Interpreter::ExecSQL(string statement) {
  sql_statement_ = statement;
  FormatSQL();
  TellSQLType();
  debug_out() << "SQL STATEMENT: " << sql_statement_ << endl;
  Run();
  debug_out() << endl;
}
