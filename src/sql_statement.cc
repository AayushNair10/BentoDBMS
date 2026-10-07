#include "sql_statement.h"

#include <iomanip>
#include <iostream>

#include <boost/algorithm/string.hpp>

#include "commons.h"
#include "exceptions.h"

using namespace boost::algorithm;
using namespace std;

std::ostream &operator<<(std::ostream &out, const TKey &object) {
  switch (object.key_type_) {
  case 0: {
    int a;
    memcpy(&a, object.key_, object.length_);
    cout << setw(12) << left << a;
  } break;
  case 1: {
    float a;
    memcpy(&a, object.key_, object.length_);
    cout << setw(12) << left << a;
  } break;
  case 2: {
    // A value that fills the whole column has no terminating zero
    cout << setw(12) << left
         << std::string(object.key_, strnlen(object.key_, object.length_));
  } break;
  }

  return out;
}

bool TKey::operator<(const TKey t1) {
  switch (t1.key_type_) {
    case 0: return *(int *)key_ < *(int *)t1.key_;
    case 1: return *(float *)key_ < *(float *)t1.key_;
    case 2: return strncmp(key_, t1.key_, length_) < 0;
    default: return false;
  }
}

bool TKey::operator>(const TKey t1) {
  switch (t1.key_type_) {
    case 0: return *(int *)key_ > *(int *)t1.key_;
    case 1: return *(float *)key_ > *(float *)t1.key_;
    case 2: return strncmp(key_, t1.key_, length_) > 0;
    default: return false;
  }
}

bool TKey::operator<=(const TKey t1) {
  return !(*this > t1);
}

bool TKey::operator>=(const TKey t1) {
  return !(*this < t1);
}

bool TKey::operator==(const TKey t1) {
  switch (t1.key_type_) {
    case 0: return *(int *)key_ == *(int *)t1.key_;
    case 1: return *(float *)key_ == *(float *)t1.key_;
    case 2: return strncmp(key_, t1.key_, length_) == 0;
    default: return false;
  }
}

bool TKey::operator!=(const TKey t1) {
  switch (t1.key_type_) {
    case 0: return *(int *)key_ != *(int *)t1.key_;
    case 1: return *(float *)key_ != *(float *)t1.key_;
    case 2: return strncmp(key_, t1.key_, length_) != 0;
    default: return false;
  }
}

// Builds the "expected X but found Y" text for a syntax error at position pos
static std::string Expected(std::string what, std::vector<std::string> &sql_vector,
                            unsigned int pos) {
  if (pos >= sql_vector.size()) {
    return "expected " + what + " but the statement ended";
  }
  return "expected " + what + " but found '" + sql_vector[pos] + "'";
}

int SQL::ParseDataType(std::vector<std::string> sql_vector, Attribute &attr,
  unsigned int pos) {
  boost::algorithm::to_lower(sql_vector.at(pos));

  if (sql_vector.at(pos) == "int") {
    debug_out() << "TYPE: "<< "int" << std::endl;
    attr.set_data_type(T_INT);
    attr.set_length(4);
    pos++;
    if (sql_vector.at(pos) == ",") {
      pos++;
    }
  } else if (sql_vector.at(pos) == "float") {
    debug_out() << "TYPE: "<< "float" << std::endl;
    attr.set_data_type(T_FLOAT);
    attr.set_length(4);
    pos++;
    if (sql_vector.at(pos) == ",") {
      pos++;
    }
  } else if (sql_vector.at(pos) == "char") {
    attr.set_data_type(T_CHAR);
    pos++;
    if (sql_vector.at(pos) == "(") {
      pos++;
    }
    attr.set_length(atoi(sql_vector.at(pos).c_str()));
    if (attr.length() <= 0) {
      throw SyntaxErrorException("char needs a size of at least 1, as in char(8)");
    }
    pos++;
    if (sql_vector.at(pos) == ")") {
      pos++;
    }
    if (sql_vector.at(pos) == ",") {
      pos++;
    }
  } else {
    throw SyntaxErrorException("unknown data type '" + sql_vector.at(pos) + "' (use int, float or char(N))");
  }

  return pos;
}

void SQLCreateDatabase::Parse(std::vector<std::string> sql_vector) {
  sql_type_ = 30;
  if (sql_vector.size() <= 2) {
    throw SyntaxErrorException("expected a database name");
  } else {
    debug_out() << "DB NAME: " << sql_vector[2] << std::endl;
    db_name_ = sql_vector[2];
  }
}

void SQLDropTable::Parse(std::vector<std::string> sql_vector) {
  sql_type_ = 51;
  if (sql_vector.size() <= 2) {
    throw SyntaxErrorException("expected a table name");
  } else {
    debug_out() << "TB NAME: " << sql_vector[2] << std::endl;
    tb_name_ = sql_vector[2];
  }
}

void SQLDropIndex::Parse(std::vector<std::string> sql_vector) {
  sql_type_ = 52;
  if (sql_vector.size() <= 2) {
    throw SyntaxErrorException("expected an index name");
  } else {
    debug_out() << "IDX NAME: " << sql_vector[2] << std::endl;
    idx_name_ = sql_vector[2];
  }
}

void SQLUse::Parse(std::vector<std::string> sql_vector) {
  sql_type_ = 60;
  if (sql_vector.size() <= 1) {
    throw SyntaxErrorException("expected a database name");
  } else {
    debug_out() << "DB NAME: " << sql_vector[1] << std::endl;
    db_name_ = sql_vector[1];
  }
}

void SQLCreateTable::Parse(std::vector<std::string> sql_vector) {
  sql_type_ = 31;
  unsigned int pos = 2;
  bool is_attr = true;

  if (sql_vector.size() <= pos) {
    throw SyntaxErrorException("expected a table name");
  }

  debug_out() << "TABLE NAME: " << sql_vector.at(pos) << std::endl;
  tb_name_ = sql_vector.at(pos);
  pos++;

  if (sql_vector.at(pos) != "(") {
    throw SyntaxErrorException(Expected("'(' after the table name", sql_vector, pos));
  }
  pos++;

  bool has_pk = false;

  while (is_attr) {
    is_attr = false;

    if (sql_vector.at(pos) == "primary") {
      pos++;
      if (sql_vector.at(pos) != "key") {
        throw SyntaxErrorException(Expected("'key' after 'primary'", sql_vector, pos));
      }
      pos++;

      if (has_pk) {
        throw SyntaxErrorException("only one primary key is allowed");
      }

      if (sql_vector.at(pos) != "(") {
        throw SyntaxErrorException(Expected("'(' after 'primary key'", sql_vector, pos));
      }
      pos++;
      bool pk_found = false;
      for (unsigned int i = 0; i < attrs_.size(); ++i) {
        if (attrs_[i].attr_name() == sql_vector.at(pos)) {
          attrs_[i].set_attr_type(1); 
          pk_found = true;
          debug_out() << "PRIMARY KEY: " << sql_vector.at(pos) << std::endl;
        }
      }
      if (!pk_found) {
        throw SyntaxErrorException("primary key column '" + sql_vector.at(pos) +
                                   "' is not a column of the table");
      }
      pos++;
      if (sql_vector.at(pos) != ")") {
        throw SyntaxErrorException(Expected("')' after the primary key column", sql_vector, pos));
      }
      has_pk = true;
    } else {
      debug_out() << "COLUMN: " << sql_vector.at(pos) << std::endl;
      Attribute attr;
      attr.set_attr_name(sql_vector.at(pos));
      pos++;

      pos = ParseDataType(sql_vector, attr, pos);

      attrs_.push_back(attr);

      if (sql_vector.at(pos) != ")") {
        is_attr = true;
      }
    }
  }
}

void SQLInsert::Parse(std::vector<std::string> sql_vector) {
  sql_type_ = 70;
  unsigned int pos = 1;
  bool is_attr = true;

  if (sql_vector.at(pos) != "into") {
    throw SyntaxErrorException(Expected("'into' after 'insert'", sql_vector, pos));
  }
  pos++;
  debug_out() << "TABLE NAME: " << sql_vector.at(pos) << endl;
  tb_name_ = sql_vector.at(pos);
  pos++;
  if (sql_vector.at(pos) != "values") {
    throw SyntaxErrorException(Expected("'values' after the table name", sql_vector, pos));
  }
  pos++;
  if (sql_vector.at(pos) != "(") {
    throw SyntaxErrorException(Expected("'(' after 'values'", sql_vector, pos));
  }
  pos++;
  while (is_attr) {
    is_attr = false;  // Assume it's the last value unless another is found
    SQLValue sql_value;  // Create a new SQLValue object to hold the value
    std::string value = sql_vector.at(pos);  // Get current value token

    // If value is quoted (string), remove quotes and set data_type = 2
    if (value.at(0) == '\'' || value.at(0) == '\"') {
      value.assign(value, 1, value.length() - 2);  // Remove surrounding quotes
      sql_value.data_type = 2;  // String
    } else {
      // If value contains '.', treat as float
      if (value.find(".") != string::npos) {
        sql_value.data_type = 1;  // Float
      } else {
        sql_value.data_type = 0;  // Integer
      }
    }
    sql_value.value = value;
    debug_out() << sql_value.data_type << " : " << value << endl;
    pos++;
    values_.push_back(sql_value);
    if (sql_vector.at(pos) != ")") {
      is_attr = true;
    }
    pos++;
  }
}

void SQLExec::Parse(std::vector<std::string> sql_vector) {
  sql_type_ = 80;
  if (sql_vector.size() <= 1) {
    throw SyntaxErrorException("expected a file name");
  } else {
    debug_out() << "FILE NAME: " << sql_vector[1] << std::endl;
    file_name_ = sql_vector[1];
  }
}

void SQLSelect::Parse(std::vector<std::string> sql_vector) {
  sql_type_ = 90; //SELECT
  unsigned int pos = 1;

  if (sql_vector.size() <= pos) {
    throw SyntaxErrorException("expected '*' after 'select'");
  }

  if (sql_vector.at(pos) != "*") {
    throw SyntaxErrorException(Expected("'*' after 'select' (column lists are not supported)", sql_vector, pos));
  }
  pos++;

  if (sql_vector.at(pos) != "from") {
    throw SyntaxErrorException(Expected("'from'", sql_vector, pos));
  }
  pos++;

  debug_out() << "TABLE NAME: " << sql_vector.at(pos) << std::endl;
  tb_name_ = sql_vector.at(pos);
  pos++;

  if (sql_vector.size() == pos) {
    return;
  }

  if (sql_vector.at(pos) != "where") {
    throw SyntaxErrorException(Expected("'where'", sql_vector, pos));
  }
  pos++;

  while (true) {
    SQLWhere where;

    where.key = sql_vector.at(pos);
    pos++;

    if (sql_vector.at(pos) == "=") {
      where.sign_type = SIGN_EQ;
    } else if (sql_vector.at(pos) == "<") {
      where.sign_type = SIGN_LT;
    } else if (sql_vector.at(pos) == ">") {
      where.sign_type = SIGN_GT;
    } else if (sql_vector.at(pos) == "<=") {
      where.sign_type = SIGN_LE;
    } else if (sql_vector.at(pos) == ">=") {
      where.sign_type = SIGN_GE;
    } else if (sql_vector.at(pos) == "<>") {
      where.sign_type = SIGN_NE;
    } else {
      throw SyntaxErrorException(
          Expected("a comparison operator (=, <>, <, >, <=, >=)", sql_vector, pos));
    }
    pos++;

    where.value = sql_vector.at(pos);
    pos++;

    if (where.value.at(0) == '\'' || where.value.at(0) == '\"') {
      where.value.assign(where.value, 1, where.value.length() - 2);
    }

    wheres_.push_back(where);
    debug_out() << where.key << " " << where.sign_type << " " << where.value << endl;

    if (sql_vector.size() == pos) {
      break;
    }

    if (sql_vector.at(pos) != "and") {
      throw SyntaxErrorException(Expected("'and'", sql_vector, pos));
    }
    pos++;
  }
}

void SQLDropDatabase::Parse(std::vector<std::string> sql_vector) {
  sql_type_ = 50;
  if (sql_vector.size() <= 2) {
    throw SyntaxErrorException("expected a database name");
  } else {
    debug_out() << "DB NAME: " << sql_vector[2] << std::endl;
    db_name_ = sql_vector[2];
  }
}

void SQLCreateIndex::Parse(std::vector<std::string> sql_vector) {
  sql_type_ = 32;
  unsigned int pos = 2;
  if (sql_vector.size() <= pos) {
    throw SyntaxErrorException("expected an index name");
  }

  debug_out() << "INDEX NAME: " << sql_vector.at(pos) << std::endl;
  index_name_ = sql_vector.at(pos);
  pos++;

  if (sql_vector.at(pos) != "on") {
    throw SyntaxErrorException(Expected("'on' after the index name", sql_vector, pos));
  }
  pos++;

  debug_out() << "TABLE NAME: " << sql_vector.at(pos) << std::endl;
  tb_name_ = sql_vector.at(pos);
  pos++;

  if (sql_vector.at(pos) != "(") {
    throw SyntaxErrorException(Expected("'(' after the table name", sql_vector, pos));
  }
  pos++;

  debug_out() << "COLUMN NAME: " << sql_vector.at(pos) << std::endl;
  col_name_ = sql_vector.at(pos);
  pos++;

  if (sql_vector.at(pos) != ")") {
    throw SyntaxErrorException(Expected("')' after the column name", sql_vector, pos));
  }
  pos++;
}

void SQLDelete::Parse(std::vector<std::string> sql_vector) {
  sql_type_ = 100;
  unsigned int pos = 1;

  if (sql_vector.size() <= pos) {
    throw SyntaxErrorException("expected 'from' after 'delete'");
  }

  if (sql_vector.at(pos) != "from") {
    throw SyntaxErrorException(Expected("'from' after 'delete'", sql_vector, pos));
  }
  pos++;

  debug_out() << "TABLE NAME: " << sql_vector.at(pos) << std::endl;
  tb_name_ = sql_vector.at(pos);
  pos++;

  if (sql_vector.size() == pos) {
    return;
  }

  if (sql_vector.at(pos) != "where") {
    throw SyntaxErrorException(Expected("'where'", sql_vector, pos));
  }
  pos++;

  while (true) {
    SQLWhere where;

    where.key = sql_vector.at(pos);
    pos++;

    if (sql_vector.at(pos) == "=") {
      where.sign_type = SIGN_EQ;
    } else if (sql_vector.at(pos) == "<") {
      where.sign_type = SIGN_LT;
    } else if (sql_vector.at(pos) == ">") {
      where.sign_type = SIGN_GT;
    } else if (sql_vector.at(pos) == "<=") {
      where.sign_type = SIGN_LE;
    } else if (sql_vector.at(pos) == ">=") {
      where.sign_type = SIGN_GE;
    } else if (sql_vector.at(pos) == "<>") {
      where.sign_type = SIGN_NE;
    } else {
      throw SyntaxErrorException(
          Expected("a comparison operator (=, <>, <, >, <=, >=)", sql_vector, pos));
    }
    pos++;

    where.value = sql_vector.at(pos);
    pos++;

    if (where.value.at(0) == '\'' || where.value.at(0) == '\"') {
      where.value.assign(where.value, 1, where.value.length() - 2);
    }

    wheres_.push_back(where);
    debug_out() << where.key << " " << where.sign_type << " " << where.value << endl;

    if (sql_vector.size() == pos) {
      break;
    }

    if (sql_vector.at(pos) != "and") {
      throw SyntaxErrorException(Expected("'and'", sql_vector, pos));
    }
    pos++;
  }
}

void SQLUpdate::Parse(std::vector<std::string> sql_vector) {
  sql_type_ = 110;
  unsigned int pos = 1;

  if (sql_vector.size() <= pos) {
    throw SyntaxErrorException("expected a table name");
  }

  debug_out() << "TABLE NAME: " << sql_vector.at(pos) << std::endl;
  tb_name_ = sql_vector.at(pos);
  pos++;

  if (sql_vector.size() == pos) {
    return;
  }

  if (sql_vector.at(pos) != "set") {
    throw SyntaxErrorException(Expected("'set' after the table name", sql_vector, pos));
  }
  pos++;

  while (true) {
    SQLKeyValue keyvalue;

    keyvalue.key = sql_vector.at(pos);
    pos++;

    if (sql_vector.at(pos) != "=") {
      throw SyntaxErrorException(Expected("'=' after the column name", sql_vector, pos));
    }
    pos++;

    keyvalue.value = sql_vector.at(pos);
    pos++;
    if (keyvalue.value.at(0) == '\'' || keyvalue.value.at(0) == '\"') {
      keyvalue.value.assign(keyvalue.value, 1, keyvalue.value.length() - 2);
    }

    keyvalues_.push_back(keyvalue);
    debug_out() << keyvalue.key << " " << keyvalue.value << endl;

    if (sql_vector.at(pos) == ",") {
      pos++;
    } else if (sql_vector.at(pos) == "where") {
      break;
    } else {
      throw SyntaxErrorException(Expected("',' or 'where'", sql_vector, pos));
    }
  }

  if (sql_vector.at(pos) != "where") {
    throw SyntaxErrorException(Expected("'where'", sql_vector, pos));
  }
  pos++;

  while (true) {
    SQLWhere where;

    where.key = sql_vector.at(pos);
    pos++;

    if (sql_vector.at(pos) == "=") {
      where.sign_type = SIGN_EQ;
    } else if (sql_vector.at(pos) == "<") {
      where.sign_type = SIGN_LT;
    } else if (sql_vector.at(pos) == ">") {
      where.sign_type = SIGN_GT;
    } else if (sql_vector.at(pos) == "<=") {
      where.sign_type = SIGN_LE;
    } else if (sql_vector.at(pos) == ">=") {
      where.sign_type = SIGN_GE;
    } else if (sql_vector.at(pos) == "<>") {
      where.sign_type = SIGN_NE;
    } else {
      throw SyntaxErrorException(
          Expected("a comparison operator (=, <>, <, >, <=, >=)", sql_vector, pos));
    }
    pos++;

    where.value = sql_vector.at(pos);
    pos++;

    if (where.value.at(0) == '\'' || where.value.at(0) == '\"') {
      where.value.assign(where.value, 1, where.value.length() - 2);
    }

    wheres_.push_back(where);
    debug_out() << where.key << " " << where.sign_type << " " << where.value << endl;

    if (sql_vector.size() == pos) {
      break;
    }

    if (sql_vector.at(pos) != "and") {
      throw SyntaxErrorException(Expected("'and'", sql_vector, pos));
    }
    pos++;
  }
}

void SQLJoin::Parse(std::vector<std::string> sql_vector) {
    for(auto it:sql_vector) debug_out()<<it<<" ";
    debug_out()<<endl;
  sql_type_ = 120;
  unsigned int pos = 1;
  // SYNTAX : JOIN t1 AND t2 ON t1-att = t2-att
  //Size = 8
  if (sql_vector.size() != 8) {
    throw SyntaxErrorException("expected: join table1 and table2 on column1 = column2");
  }
  tb_name1_ = sql_vector.at(pos);
  pos++;
  if (sql_vector.at(pos) != "and") {
    throw SyntaxErrorException(Expected("'and' after the first table name", sql_vector, pos));
  }
  pos++;
  tb_name2_ = sql_vector.at(pos);
  pos++;
  if (sql_vector.at(pos) != "on") {
    throw SyntaxErrorException(Expected("'on' after the second table name", sql_vector, pos));
  }
  pos++;
  col_name1_ = sql_vector.at(pos);
  pos++;
  if (sql_vector.at(pos) != "=") {
    throw SyntaxErrorException(Expected("'=' between the join columns", sql_vector, pos));
  }
  pos++;
  col_name2_ = sql_vector.at(pos);
}
