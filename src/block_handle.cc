#include "block_handle.h"

//class BlockInfo variables:
//FileInfo *file_; → Pointer to the associated file.
// int block_num_; → Block number identifier.
// char *data_; → Pointer to a memory block (allocated as 4 * 1024 bytes).
// bool dirty_; → Flag indicating if the block has been modified.
// long age_; → Age counter for the block.
// BlockInfo *next_; → Pointer to the next BlockInfo block in a linked list.
// _ is used for definifn variable while withoug _ is used for obtaining that variable value

BlockInfo *BlockHandle::Add(BlockInfo *block) {
  BlockInfo *current = block;  // Start from the given block
  while (bcount_ < bsize_) {
    BlockInfo *adder = new BlockInfo(0);   
    adder->set_next(current->next());      // Link new block to next
    current->set_next(adder);              // Update current block's next pointer
    current = adder;                       
    bcount_++;                              
  }
  return current;  // Return the last added block
}

//first_block_ points to useless block, first_block_->next() points to first usable block
BlockInfo *BlockHandle::GetUsableBlock() {
  if (bcount_ == 0) {
    return NULL;
  }

  BlockInfo *p = first_block_->next();
  first_block_->set_next(first_block_->next()->next());
  bcount_--;
  p->ResetAge();
  p->set_next(NULL);
  return p;
}

void BlockHandle::FreeBlock(BlockInfo *block) {
  if (bcount_ == 0) {
    first_block_ = block;
    block->set_next(block);
  } else {
    block->set_next(first_block_->next());
    first_block_->set_next(block);
  }
  bcount_++;
}

BlockHandle::~BlockHandle() {
  BlockInfo *p = first_block_;
  while (bcount_ > 0) {
    BlockInfo *pn = p->next();
    delete p;
    p = pn;
    bcount_--;
  }
}