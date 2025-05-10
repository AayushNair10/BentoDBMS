//Checks if MINIDB_CATALOG_MANAGER_H_ is not defined
#ifndef MINIDB_CATALOG_MANAGER_H_
//If not, define it
#define MINIDB_CATALOG_MANAGER_H_

#include <string>
#include <vector>
// Boost serialization libraries allow saving/loading object state to/from disk
//Serialization: a process that converts objects into a format that can be stored in a file or sent over a network, and later reconstructed
//Every data type in every class is serialised, so that storing is possible
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/serialization/utility.hpp>
#include <boost/serialization/vector.hpp>

#include "sql_statement.h"

class CatalogManager;
class Database;
class Table;
class Attribute;
class Index;
class SQLCreateTable;
class SQLDropTable;
class SQLDropIndex;


// CatalogManager: Manages the collection of databases (schemas) and their storage
class CatalogManager {
private:
  // Allow boost serialization to access private members.
  friend class boost::serialization::access;
  // This function defines how to serialize the databases list.
  template <class Archive>

  void serialize(Archive &ar, const unsigned int version) {
    ar &dbs_;  // Serialize the vector of Database objects.
  }
  std::string path_;             // Filesystem path where the catalog (metadata) is stored.
  std::vector<Database> dbs_;      // Collection of databases.

public:
  // Constructor that initializes the catalog with a given path.
  CatalogManager(std::string p);
  ~CatalogManager();

// The function name is dbs().
//The return type is std::vector<Database> &, which means the function returns a reference to a std::vector<Database>.
  std::vector<Database>& dbs() { return dbs_; }
  // Accessor for the catalog storage path.
  std::string path() { return path_; }
  // Retrieve a database by its name.
  Database *GetDB(std::string db_name);
  // Read the catalog file from disk.
  void ReadArchiveFile();
  // Write the current catalog to disk.
  void WriteArchiveFile();
  // Create a new database.
  void CreateDatabase(std::string dbname);
  // Delete an existing database.
  void DeleteDatabase(std::string dbname);
};


class Database {
private:
  friend class boost::serialization::access;
  // Used by boost::serialization. Defined in all classes that need serialization.
  template <class Archive>

  void serialize(Archive &ar, const unsigned int version) {
    ar &db_name_;  // Serialize the database name.
    ar &tbs_;      // Serialize the vector of Table objects.
  }
  std::string db_name_;     
  std::vector<Table> tbs_; // Tables contained in this database.

public:
  Database() {}
  Database(std::string dbname);
  ~Database() {}

  // Retrieve a table from the database by name.
  Table *GetTable(std::string tb_name);
  // Get the database name.
  std::string db_name() { return db_name_; }
  // Create a new table using a SQL CREATE TABLE statement.
  void CreateTable(SQLCreateTable &st);
  // Drop an existing table.
  void DropTable(SQLDropTable &st);
  // Drop an index from a table.
  void DropIndex(SQLDropIndex &st);
  // Accessor for the list of tables.
  std::vector<Table> &tbs() { return tbs_; }
  // Check if an index with a given name exists in the database.
  bool CheckIfIndexExists(std::string index_name);
};

// Table: Represents a table within a database, including its schema and ata pointers.
class Table {
private:
  friend class boost::serialization::access;
  template <class Archive>

  void serialize(Archive &ar, const unsigned int version) {
    ar &tb_name_;            // Table name.
    ar &record_length_;      // Length of a single record.
    ar &first_block_num_;    // Number of the first data block.
    ar &first_rubbish_num_;  // Number of the first free (rubbish) block.
    ar &block_count_;        // Total number of blocks.
    ar &ats_;                // List of attributes (columns).
    ar &ids_;                // List of indexes on the table.
  }

  std::string tb_name_;       // Name of the table.
  int record_length_;         // Size in bytes of a record.
  int first_block_num_;       // First block containing actual records.
  int first_rubbish_num_;     // First block that is available for reuse.
  int block_count_;           // Total count of blocks allocated.
  std::vector<Attribute> ats_; // Attributes (columns) of the table.
  std::vector<Index> ids_;     // Indexes defined on the table.

public:
  // Constructor initializing members to default values.
  Table() : tb_name_(""), record_length_(-1), first_block_num_(-1), first_rubbish_num_(-1), block_count_(0) {}
  ~Table() {}

  // Accessor and mutator for table name.
  std::string tb_name() { return tb_name_; }
  void set_tb_name(std::string tbname) { tb_name_ = tbname; }

  // Accessor and mutator for record length.
  int record_length() { return record_length_; }
  void set_record_length(int len) { record_length_ = len; }

  // Accessor for the list of attributes.
  std::vector<Attribute> &ats() { return ats_; }
  // Retrieve a specific attribute by name.
  Attribute *GetAttribute(std::string name);
  // Retrieve the index of an attribute by name.
  int GetAttributeIndex(std::string name);

  // Accessors and mutators for block pointers and count.
  int first_block_num() { return first_block_num_; }
  void set_first_block_num(int num) { first_block_num_ = num; }
  int first_rubbish_num() { return first_rubbish_num_; }
  void set_first_rubbish_num(int num) { first_rubbish_num_ = num; }
  int block_count() { return block_count_; }

  // Get the number of attributes.
  unsigned long GetAttributeNum() { return ats_.size(); }
  // Add a new attribute (column) to the table.
  void AddAttribute(Attribute &attr) { ats_.push_back(attr); }
  // Increase the block count 
  void IncreaseBlockCount() { block_count_++; }

  // Accessor for the list of indexes.
  std::vector<Index> &ids() { return ids_; }
  // Retrieve a specific index by its position.
  Index *GetIndex(int num) { return &(ids_[num]); }
  // Get the number of indexes defined.
  unsigned long GetIndexNum() { return ids_.size(); }
  // Add a new index to the table.
  void AddIndex(Index &idx) { ids_.push_back(idx); }
};

// Attribute: Represents a single column in a table with its name, type, and length.
class Attribute {
private:
  friend class boost::serialization::access;
  template <class Archive>

  void serialize(Archive &ar, const unsigned int version) {
    ar &attr_name_;  // Serialize attribute name.
    ar &data_type_;  // Serialize data type.
    ar &length_;     // Serialize length.
    ar &attr_type_;  // Serialize attribute type (e.g., primary key, normal).
  }

  std::string attr_name_; // Name of the attribute.
  int data_type_;         // Data type (for example, integer, string).
  int length_;            // Maximum length in bytes.
  int attr_type_;         // Attribute type indicator (0: normal, 1: primary key, etc.).

public:
  // Default constructor with initial values.
  Attribute() : attr_name_(""), data_type_(-1), length_(-1), attr_type_(0) {}
  ~Attribute() {}

  // Accessor and mutator for attribute name.
  std::string attr_name() { return attr_name_; }
  void set_attr_name(std::string name) { attr_name_ = name; }

  // Accessor and mutator for attribute type.
  int attr_type() { return attr_type_; }
  void set_attr_type(int type) { attr_type_ = type; }

  // Accessor and mutator for data type.
  int data_type() { return data_type_; }
  void set_data_type(int type) { data_type_ = type; }

  // Accessor and mutator for length.
  int length() { return length_; }
  void set_length(int length) { length_ = length; }
};

// Index: Represents an index built on a table's attribute (typically using a B+ tree).
class Index {
private:
  friend class boost::serialization::access;

  // Serialization for Index.
  template <class Archive>
  void serialize(Archive &ar, const unsigned int version) {
    ar &max_count_;
    ar &attr_name_;
    ar &name_;
    ar &key_len_;
    ar &key_type_;
    ar &rank_;
    ar &rubbish_;
    ar &root_;
    ar &leaf_head_;
    ar &key_count_;
    ar &level_;
    ar &node_count_;
  }
  int max_count_;  // Maximum number of keys a node can hold.
  int key_len_;    // Length in bytes of a key.
  int key_type_;   // Data type of the key.
  int rank_;       // Rank or order of the index.
  int rubbish_;    // Free list pointer for reusing nodes.
  int root_;       // Root node index of the index's B+ tree.
  int leaf_head_;  // Pointer to the head of the leaf node chain.
  int key_count_;  // Current number of keys in the index.
  int level_;      // Height (number of levels) in the B+ tree.
  int node_count_; // Total number of nodes in the tree.
  std::string attr_name_; // The attribute name this index is built on.
  std::string name_;      // The name of the index itself.

public:
  // Default constructor.
  Index() {}
  // Constructor to initialize an index with specific properties.
  Index(std::string name, std::string attr_name, int keytype, int keylen,
        int rank) {
    attr_name_ = attr_name;
    name_ = name;
    key_count_ = 0;
    level_ = -1;
    node_count_ = 0;
    root_ = -1;
    leaf_head_ = -1;
    key_type_ = keytype;
    key_len_ = keylen;
    rank_ = rank;
    rubbish_ = -1;
    max_count_ = 0;
  }

  // Accessor for the attribute name of the index.
  std::string attr_name() { return attr_name_; }
  // Accessor for the key length.
  int key_len() { return key_len_; }
  // Accessor for the key type.
  int key_type() { return key_type_; }
  // Accessor for the rank.
  int rank() { return rank_; }
  // Accessor and mutator for the root node.
  int root() { return root_; }
  void set_root(int root) { root_ = root; }
  // Accessor and mutator for the leaf head.
  int leaf_head() { return leaf_head_; }
  void set_leaf_head(int leaf_head) { leaf_head_ = leaf_head; }
  // Accessor and mutator for the key count.
  int key_count() { return key_count_; }
  void set_key_count(int key_count) { key_count_ = key_count; }
  // Accessor and mutator for the tree level.
  int level() { return level_; }
  void set_level(int level) { level_ = level; }
  // Accessor and mutator for the node count.
  int node_count() { return node_count_; }
  void set_node_count(int node_count) { node_count_ = node_count; }
  // Accessor for the index name.
  std::string name() { return name_; }

  // Methods to increase/decrease various counters.
  int IncreaseMaxCount() { return max_count_++; }
  int IncreaseKeyCount() { return key_count_++; }
  int IncreaseNodeCount() { return node_count_++; }
  int IncreaseLevel() { return level_++; }
  
  int DecreaseKeyCount() { return key_count_--; }
  int DecreaseNodeCount() { return node_count_--; }
  int DecreaseLevel() { return level_--; }
};

#endif
