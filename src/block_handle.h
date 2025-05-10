#ifndef MINIDB_BLOCK_HANDLE_H_
#define MINIDB_BLOCK_HANDLE_H_

#include "block_info.h"

class BlockHandle {
  //Mostly used to calculate usable blocks, deals with deletion and addidition in usable blocks
  private:
  BlockInfo *first_block_;  // Pointer to the first usable block in the list
  int bsize_;  // Total number of blocks (default 300)
  int bcount_; // Number of usable blocks
  std::string path_;  // File path related to block storage

  // Inits BlockHandle
  BlockInfo *Add(BlockInfo *block);

public:
  BlockHandle(std::string p)
      : first_block_(new BlockInfo(0)), bsize_(300), bcount_(0), path_(p) {
    Add(first_block_);
  }

  ~BlockHandle();

  int bcount() { return bcount_; }

  BlockInfo *GetUsableBlock();

  void FreeBlock(BlockInfo *block);
};

#endif /* defined(MINIDB_BLOCK_HANDLE_H_) */
