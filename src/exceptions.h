#ifndef MINIDB_EXCEPTIONS_H_
#define MINIDB_EXCEPTIONS_H_

#include <exception>
#include <string>

// Carries a short description of what is wrong with the statement
class SyntaxErrorException : public std::exception {
private:
  std::string msg_;

public:
  SyntaxErrorException(std::string msg) : msg_(msg) {}
  ~SyntaxErrorException() throw() {}
  const char *what() const throw() { return msg_.c_str(); }
};

class NoDatabaseSelectedException : public std::exception {};

class DatabaseNotExistException : public std::exception {};

class DatabaseAlreadyExistsException : public std::exception {};

class TableNotExistException : public std::exception {};

class TableAlreadyExistsException : public std::exception {};

class IndexAlreadyExistsException : public std::exception {};

class IndexNotExistException : public std::exception {};

class OneIndexEachTableException : public std::exception {};

class BPlusTreeException : public std::exception {};

class IndexMustBeCreatedOnPKException : public std::exception {};

class PrimaryKeyConflictException : public std::exception {};

class BufferFullException : public std::exception {};

// Carries the name of the missing column
class ColumnNotExistException : public std::exception {
private:
  std::string msg_;

public:
  ColumnNotExistException(std::string msg) : msg_(msg) {}
  ~ColumnNotExistException() throw() {}
  const char *what() const throw() { return msg_.c_str(); }
};

class ColumnCountNotMatchException : public std::exception {};

class RecordTooLongException : public std::exception {};

#endif
