#ifndef MINIDB_BLOCK_INFO_H_
#define MINIDB_BLOCK_INFO_H_

#include <sys/types.h>

#include "commons.h"
#include "file_info.h"

class FileInfo;

class BlockInfo {
private:

  FileInfo *file_; //Pointer to the associated file.
  int block_num_; //Block number identifier.
  char *data_; // Pointer to a memory block (allocated as 4 * 1024 bytes).
  bool dirty_; //Flag indicating if the block has been modified.
  long age_; //Age counter for the block.
  BlockInfo *next_; //Pointer to the next BlockInfo block in a linked list.

// Implementing replacement policies like Least Recently Used (LRU) or Least Frequently Used (LFU)
// in a database buffer or cache system

public:
  BlockInfo(int num)
      : dirty_(false), next_(NULL), file_(NULL), age_(0), block_num_(num) {
    data_ = new char[4 * 1024];
  }
  virtual ~BlockInfo() { delete[] data_; }
  FileInfo *file() { return file_; }
  void set_file(FileInfo *f) { file_ = f; }

  int block_num() { return block_num_; }
  void set_block_num(int num) { block_num_ = num; }

  char *data() { return data_; }

  long age() { return age_; }

  bool dirty() { return dirty_; }
  void set_dirty(bool dt) { dirty_ = true; }

  BlockInfo *next() { return next_; }
  void set_next(BlockInfo *block) { next_ = block; }

  void IncreaseAge() { ++age_; }
  void ResetAge() { age_ = 0; }

//   Offset	Purpose
// 0	Previous block number (int)
// 4	Next block number (int)
// 8	Record count (int)
// 12+	Actual content starts

  void SetPrevBlockNum(int num) { *(int *)(data_) = num; }

  int GetPrevBlockNum() { return *(int *)(data_); }

  void SetNextBlockNum(int num) { *(int *)(data_ + 4) = num; }

  int GetNextBlockNum() { return *(int *)(data_ + 4); }

  void SetRecordCount(int count) { *(int *)(data_ + 8) = count; }

  void DecreaseRecordCount() { *(int *)(data_ + 8) = *(int *)(data_ + 8) - 1; }

  int GetRecordCount() { return *(int *)(data_ + 8); }

  char *GetContentAddress() { return data_ + 12; }

  void ReadInfo(std::string path);
  void WriteInfo(std::string path);
};

#endif /* MINIDB_BLOCK_INFO_H_ */
