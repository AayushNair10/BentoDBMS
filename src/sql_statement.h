#ifndef MINIDB_SQL_STATEMENT_H_
#define MINIDB_SQL_STATEMENT_H_

#include <string>
#include <vector>

#include "catalog_manager.h"
//Declarations for classes simplify dependencies by informing the compiler of their existence without including full definitions
//This can make code more readable and save time while calling them
class CatalogManager;
class Database;
class Table;
class Attribute;
class Index;


//Declarations for SQL statement classes
class TKey;
class SQL;
class SQLCreateDatabase;
class SQLDropDatabase;
class SQLDropTable;
class SQLDropIndex;
class SQLUse;
class SQLCreateTable;
class SQLInsert;
class SQLExec;
class SQLSelect;
class SQLCreateIndex;
class SQLDelete;
class SQLUpdate;

class TKey {
private:
  int key_type_;
  char *key_;
  int length_;

public:
    
// parameterized constructor
  TKey(int keytype, int length) {
    key_type_ = keytype;
    if (keytype == 2)
      length_ = length;
    else
      length_ = 4;
    key_ = new char[length_];
  }

// Copy Constructor
  TKey(const TKey &t1) {
    key_type_ = t1.key_type_;
    length_ = t1.length_;
    key_ = new char[length_];
    memcpy(key_, t1.key_, length_);
  }

/* Reads a value from a char* (C-style string) and stores it in key_
    atoi() is for char* to int . */
  void ReadValue(const char *content) {
    switch (key_type_) {
    case 0: {
      int a = std::atoi(content);
      memcpy(key_, &a, length_);
    } break;
    case 1: {
      float a = std::atof(content);
      memcpy(key_, &a, length_);
    } break;
    case 2: {
      memcpy(key_, content, length_);
    } break;
    }
  }

// same upper readvalue method but just accept strings instead of char*
  void ReadValue(std::string str) {
    switch (key_type_) {
    case 0: {
      int a = std::atoi(str.c_str());
      memcpy(key_, &a, length_);
    } break;
    case 1: {
      float a = std::atof(str.c_str());
      memcpy(key_, &a, length_);
    } break;
    case 2: {
      memcpy(key_, str.c_str(), length_);
    } break;
    }
  }

// getters
  int key_type() { return key_type_; }
  char *key() { return key_; };
  int length() { return length_; }

// destructor
  ~TKey() {
    if (key_ != NULL)
      delete[] key_;
  }

// To overload the << operator for printing the key object.
  friend std::ostream &operator<<(std::ostream &out, const TKey &object);

// Operator Overlaoding from now on till this class ends.
  bool operator<(const TKey t1);
  bool operator>(const TKey t1);
  bool operator<=(const TKey t1);
  bool operator>=(const TKey t1);
  bool operator==(const TKey t1);
  bool operator!=(const TKey t1);

};



class SQL {
protected:
  int sql_type_;

public:
// default constructor
  SQL() : sql_type_(-1) {}
  SQL(int sqltype) { sql_type_ = sqltype; }
  virtual ~SQL() {}
  int sql_type() { return sql_type_; }
  void set_sql_type(int sqltype) { sql_type_ = sqltype; }
    
//Pure virtual [= 0 at the end], so derived class must implement it
//if [ = 0] not present at end → Virtual but not pure, so derived class can override it, but doesn’t have to.
  virtual void Parse(std::vector<std::string> sql_vector) = 0;
//Store value in attribute
  int ParseDataType(std::vector<std::string> sql_vector, Attribute &attr,
                    unsigned int pos);
};


class SQLCreateDatabase : public SQL {
private:
  std::string db_name_;

public:
  SQLCreateDatabase(std::vector<std::string> sql_vector) { Parse(sql_vector); }
  std::string db_name() { return db_name_; }
  void set_db_name(std::string dbname) { db_name_ = dbname; }
  void Parse(std::vector<std::string> sql_vector);
};

class SQLDropDatabase : public SQL {
private:
  std::string db_name_;

public:
  SQLDropDatabase(std::vector<std::string> sql_vector) { Parse(sql_vector); }
  std::string db_name() { return db_name_; }
  void set_db_name(std::string dbname) { db_name_ = dbname; }
  void Parse(std::vector<std::string> sql_vector);
};

class SQLDropTable : public SQL {
private:
  std::string tb_name_;

public:
  SQLDropTable(std::vector<std::string> sql_vector) { Parse(sql_vector); }
  std::string tb_name() { return tb_name_; }
  void set_tb_name(std::string tbname) { tb_name_ = tbname; }
  void Parse(std::vector<std::string> sql_vector);
};

class SQLDropIndex : public SQL {
private:
  std::string idx_name_;

public:
  SQLDropIndex(std::vector<std::string> sql_vector) { Parse(sql_vector); }
  std::string idx_name() { return idx_name_; }
  void set_idx_name(std::string idxname) { idx_name_ = idxname; }
  void Parse(std::vector<std::string> sql_vector);
};

class SQLUse : public SQL {
private:
  std::string db_name_;

public:
  SQLUse(std::vector<std::string> sql_vector) { Parse(sql_vector); }
  std::string db_name() { return db_name_; }
  void set_db_name(std::string dbname) { db_name_ = dbname; }
  void Parse(std::vector<std::string> sql_vector);
};

class SQLCreateTable : public SQL {
private:
  std::string tb_name_;
  std::vector<Attribute> attrs_;

public:
  SQLCreateTable(std::vector<std::string> sql_vector) { Parse(sql_vector); }
  std::string tb_name() { return tb_name_; }
  void set_tb_name(std::string tbname) { tb_name_ = tbname; }
  std::vector<Attribute> attrs() { return attrs_; };
  void set_attrs(std::vector<Attribute> att) { attrs_ = att; }
  void Parse(std::vector<std::string> sql_vector);
};

typedef struct {
  int data_type;
  std::string value;
} SQLValue;

class SQLInsert : public SQL {
private:
  std::string tb_name_;
  std::vector<SQLValue> values_;

public:
  SQLInsert(std::vector<std::string> sql_vector) { Parse(sql_vector); }
  void Parse(std::vector<std::string> sql_vector);
  std::string tb_name() { return tb_name_; }
  std::vector<SQLValue> &values() { return values_; }
};

class SQLExec : public SQL {
private:
  std::string file_name_;

public:
  SQLExec(std::vector<std::string> sql_vector) { Parse(sql_vector); }
  void Parse(std::vector<std::string> sql_vector);
  std::string file_name() { return file_name_; }
};

typedef struct {
  std::string key;
  int sign_type;
  std::string value;
} SQLWhere;

class SQLSelect : public SQL {
private:
  std::string tb_name_;
  std::vector<SQLWhere> wheres_;

public:
  SQLSelect(std::vector<std::string> sql_vector) { Parse(sql_vector); }
  void Parse(std::vector<std::string> sql_vector);
  std::string tb_name() { return tb_name_; }
  std::vector<SQLWhere> &wheres() { return wheres_; }
};

class SQLCreateIndex : public SQL {
private:
  std::string index_name_;
  std::string tb_name_;
  std::string col_name_;

public:
  SQLCreateIndex(std::vector<std::string> sql_vector) { Parse(sql_vector); }
  void Parse(std::vector<std::string> sql_vector);
  std::string index_name() { return index_name_; }
  std::string tb_name() { return tb_name_; }
  std::string col_name() { return col_name_; }
};

class SQLDelete : public SQL {
private:
  std::string tb_name_;
  std::vector<SQLWhere> wheres_;

public:
  SQLDelete(std::vector<std::string> sql_vector) { Parse(sql_vector); }
  void Parse(std::vector<std::string> sql_vector);
  std::string tb_name() { return tb_name_; }
  std::vector<SQLWhere> &wheres() { return wheres_; }
};

typedef struct {
  std::string key;
  std::string value;
} SQLKeyValue;

class SQLUpdate : public SQL {
private:
  std::string tb_name_;
  std::vector<SQLWhere> wheres_;
  std::vector<SQLKeyValue> keyvalues_;

public:
  SQLUpdate(std::vector<std::string> sql_vector) { Parse(sql_vector); }
  void Parse(std::vector<std::string> sql_vector);
  std::string tb_name() { return tb_name_; }
  std::vector<SQLWhere> &wheres() { return wheres_; }
  std::vector<SQLKeyValue> &keyvalues() { return keyvalues_; }
};

class SQLJoin : public SQL {
private:
  std::string tb_name1_;
  std::string tb_name2_;
  std::string col_name1_;
  std::string col_name2_;
public:
    SQLJoin(std::vector<std::string> sql_vector) {
        Parse(sql_vector);
        tb_name1();
        tb_name2();
        col_name1();
        col_name2();
    }
  void Parse(std::vector<std::string> sql_vector);
  std::string tb_name1() { return tb_name1_; }
  std::string tb_name2() { return tb_name2_; }
  std::string col_name1() { return col_name1_; }
  std::string col_name2() { return col_name2_; }
};

#endif
