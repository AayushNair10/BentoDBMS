#include "record_manager.h"

#include <iomanip>
#include <iostream>

#include "index_manager.h"

using namespace std;

BlockInfo *RecordManager::GetBlockInfo(Table *tbl, int block_num) {
  if (block_num == -1) {
    return NULL;
  }
  BlockInfo *block = hdl_->GetFileBlock(db_name_, tbl->tb_name(), 0, block_num);
  return block;
}

void RecordManager::Insert(SQLInsert &st) {
  // Get the table name and the number of values from the SQL insert command.
  string tb_name = st.tb_name();
  unsigned long values_size = st.values().size();

  // If the table doesn't exist, throw an exception.
  Table *tbl = cm_->GetDB(db_name_)->GetTable(tb_name);
  if (tbl == NULL) {
    throw TableNotExistException();
  }

  // Calculate the maximum number of records that can be stored in one block
  int max_count = (4096 - 12) / (tbl->record_length());

  vector<TKey> tkey_values;
  int pk_index = -1; // Index for primary key in the record

  // Convert each input value into the TKey format and check if it is a primary key
  for (int i = 0; i < values_size; i++) {
    int value_type = st.values()[i].data_type;
    string value = st.values()[i].value;
    int length = tbl->ats()[i].length();

    TKey tmp(value_type, length);
    tmp.ReadValue(value.c_str());
    tkey_values.push_back(tmp);

    // If the attribute is the primary key (type = 1), record its index.
    if (tbl->ats()[i].attr_type() == 1) {
      pk_index = i;
    }
  }

  // If there's a primary key, check for conflicts.
  if (pk_index != -1) {
    if (tbl->GetIndexNum() != 0) {
      // If an index exists, use the B+ tree to quickly check for duplicate keys.
      BPlusTree tree(tbl->GetIndex(0), hdl_, cm_, db_name_);
      int value = tree.GetVal(tkey_values[pk_index]);
      if (value != -1) {
        throw PrimaryKeyConflictException();
      }
    } else {
      // If no index exists, iterate through each block and record to check for a duplicate.
      int block_num = tbl->first_block_num();
      for (int i = 0; i < tbl->block_count(); ++i) {
        BlockInfo *bp = GetBlockInfo(tbl, block_num);
        for (int j = 0; j < bp->GetRecordCount(); ++j) {
          vector<TKey> tkey_value = GetRecord(tbl, block_num, j);
          if (tkey_value[pk_index] == tkey_values[pk_index]) { 
            throw PrimaryKeyConflictException();
          }
        }
        block_num = bp->GetNextBlockNum();
      }
    }
  }

  char *content;
  int ub = tbl->first_block_num();    // The first "useful" block.
  int frb = tbl->first_rubbish_num();   // The first "rubbish" (reusable) block.
  int lastub;
  int blocknum, offset;

  // Search for a useful block with free space.
  while (ub != -1) { 
    lastub = ub;
    BlockInfo *bp = GetBlockInfo(tbl, ub);
    if (bp->GetRecordCount() == max_count) {
      // If the current block is full, move to the next block.
      ub = bp->GetNextBlockNum();
      continue;
    }
    // Calculate where the new record should be stored within the block.
    content = bp->GetContentAddress() + bp->GetRecordCount() * tbl->record_length(); 
    // Copy each key's data into the block.
    for (vector<TKey>::iterator iter = tkey_values.begin(); iter != tkey_values.end(); ++iter) {
      memcpy(content, iter->key(), iter->length());
      content += iter->length();
    }
    // Increase the record count for the block.
    bp->SetRecordCount(1 + bp->GetRecordCount());

    blocknum = ub;
    offset = bp->GetRecordCount() - 1;
    hdl_->WriteBlock(bp);

    // Update the index with the new record if an index exists.
    if (tbl->GetIndexNum() != 0) {
      BPlusTree tree(tbl->GetIndex(0), hdl_, cm_, db_name_);
      for (int i = 0; i < tbl->ats().size(); ++i) {
        if (tbl->GetIndex(0)->attr_name() == tbl->GetIndex(i)->attr_name()) {
          tree.Add(tkey_values[i], blocknum, offset);
          break;
        }
      }
    }

    hdl_->WriteToDisk();
    cm_->WriteArchiveFile();
    return;
  }

  // If no useful block has free space, try using a rubbish block.
  if (frb != -1) {
    BlockInfo *bp = GetBlockInfo(tbl, frb);
    content = bp->GetContentAddress();
    for (vector<TKey>::iterator iter = tkey_values.begin(); iter != tkey_values.end(); ++iter) {
      memcpy(content, iter->key(), iter->length());
      content += iter->length();
    }
    bp->SetRecordCount(1);

    // Link the rubbish block into the chain of useful blocks.
    BlockInfo *lastubp = GetBlockInfo(tbl, lastub);
    lastubp->SetNextBlockNum(frb);
    tbl->set_first_rubbish_num(bp->GetNextBlockNum());

    bp->SetPrevBlockNum(lastub);
    bp->SetNextBlockNum(-1);

    blocknum = frb;
    offset = 0;

    hdl_->WriteBlock(bp);
    hdl_->WriteBlock(lastubp);
  } else {
    // If no rubbish block is available, add a new block 
    int next_block = tbl->first_block_num();
    if (tbl->first_block_num() != -1) {
      BlockInfo *upbp = GetBlockInfo(tbl, tbl->first_block_num());
      upbp->SetPrevBlockNum(tbl->block_count());
      hdl_->WriteBlock(upbp);
    }
    tbl->set_first_block_num(tbl->block_count());
    BlockInfo *bp = GetBlockInfo(tbl, tbl->first_block_num());

    bp->SetPrevBlockNum(-1);
    bp->SetNextBlockNum(next_block);
    bp->SetRecordCount(1);

    content = bp->GetContentAddress();
    for (vector<TKey>::iterator iter = tkey_values.begin(); iter != tkey_values.end(); ++iter) {
      memcpy(content, iter->key(), iter->length());
      content += iter->length();
    }

    blocknum = tbl->block_count();
    offset = 0;
    hdl_->WriteBlock(bp);

    tbl->IncreaseBlockCount();
  }

  // After inserting, update the index with the new record if an index exists.
  if (tbl->GetIndexNum() != 0) {
    BPlusTree tree(tbl->GetIndex(0), hdl_, cm_, db_name_);
    for (int i = 0; i < tbl->ats().size(); ++i) {
      if (tbl->GetIndex(0)->attr_name() == tbl->GetIndex(i)->name()) {
        tree.Add(tkey_values[i], blocknum, offset);
        break;
      }
    }
  }
  cm_->WriteArchiveFile();
  hdl_->WriteToDisk();
}


void RecordManager::Select(SQLSelect &st) {
  // Get the table object and print column headers.
  Table *tbl = cm_->GetDB(db_name_)->GetTable(st.tb_name());
  for (int i = 0; i < tbl->GetAttributeNum(); ++i) {
    cout << setw(12) << left << tbl->ats()[i].attr_name();
  }
  cout << endl;

  vector<vector<TKey>> tkey_values;
  bool has_index = false;
  int index_idx, where_idx;

  // Check if any index can be used based on WHERE conditions
  if (tbl->GetIndexNum() != 0) {
    for (int i = 0; i < tbl->GetIndexNum(); ++i) {
      Index *idx = tbl->GetIndex(i);
      for (int j = 0; j < st.wheres().size(); ++j) {
        if (idx->attr_name() == st.wheres()[j].key &&
            st.wheres()[j].sign_type == SIGN_EQ) {
          has_index = true;
          index_idx = i;
          where_idx = j;
        }
      }
    }
  }

  // Full table scan if no index is applicable.
  if (!has_index) {
    int block_num = tbl->first_block_num();
    for (int i = 0; i < tbl->block_count(); ++i) {
      BlockInfo *bp = GetBlockInfo(tbl, block_num);
      for (int j = 0; j < bp->GetRecordCount(); ++j) {
        vector<TKey> tkey_value = GetRecord(tbl, block_num, j);
        //The sats variable is used to check whether a record satisfies all WHERE conditions in the SQL SELECT query.
        bool sats = true;
        for (int k = 0; k < st.wheres().size(); ++k) {
          if (!SatisfyWhere(tbl, tkey_value, st.wheres()[k])) {
            sats = false;
          }
        }
        if (sats) {
          tkey_values.push_back(tkey_value);
        }
      }

      block_num = bp->GetNextBlockNum();
    }
  } 
  else { // Use index search
    // Create a B+ Tree object for the indexed column
    BPlusTree tree(tbl->GetIndex(index_idx), hdl_, cm_, db_name_);

    // Retrieve the type and length of the indexed key
    int type = tbl->GetIndex(index_idx)->key_type();
    int length = tbl->GetIndex(index_idx)->key_len();
    // Extract the search value from the WHERE condition
    std::string value = st.wheres()[where_idx].value;

    TKey dest_key(type, length);
    dest_key.ReadValue(value); // Convert the string value into the appropriate key format
    // Search for the value in the B+ Tree index
    int blocknum = tree.GetVal(dest_key);
    // If the value exists in the index
    if (blocknum != -1) {
        int blockoffset = blocknum; // Store the original blocknum
        blocknum = (blocknum >> 16) & 0xffff;  // Extract the block number
        blockoffset = blockoffset & 0xffff;    // Extract the offset within the block

        // Retrieve the record from the table based on the found block and offset
        vector<TKey> tkey_value = GetRecord(tbl, blocknum, blockoffset);
        
        // Check if the record satisfies all WHERE conditions
        bool sats = true;
        for (int k = 0; k < st.wheres().size(); ++k) {
            if (!SatisfyWhere(tbl, tkey_value, st.wheres()[k])) {
                sats = false;
            }
        }

        // If the record matches all conditions, add it to the result set
        if (sats) {
            tkey_values.push_back(tkey_value);
        }
    }
  }


  // Print the matching records.
  for (int i = 0; i < tkey_values.size(); ++i) {
    for (int j = 0; j < tkey_values[i].size(); ++j) {
      cout << tkey_values[i][j];
    }
    cout << endl;
  }

  // Optionally print the B+ tree structure for the first index for debugging.
  if (tbl->GetIndexNum() != 0) {
    BPlusTree tree(tbl->GetIndex(0), hdl_, cm_, db_name_);
    tree.Print();
  }
}


void RecordManager::Delete(SQLDelete &st) {
  Table *tbl = cm_->GetDB(db_name_)->GetTable(st.tb_name());
  
  bool has_index = false;
  int index_idx;  
  int where_idx;  

  // Determine if an applicable index exists for an equality condition.
  if (tbl->GetIndexNum() != 0) {
    for (int i = 0; i < tbl->GetIndexNum(); ++i) {
      Index *idx = tbl->GetIndex(i);
      for (int j = 0; j < st.wheres().size(); ++j) {
        if (idx->attr_name() == st.wheres()[j].key) {
          index_idx = i;
          // Use the index only when the where condition is an equality.
          if (st.wheres()[j].sign_type == SIGN_EQ) {
            has_index = true;
            where_idx = j;
          }
        }
      }
    }
  }

  // If no suitable index exists, perform a full table scan.
  if (!has_index) {
    // Iterate through all blocks in the table.
    int block_num = tbl->first_block_num();
    for (int i = 0; i < tbl->block_count(); ++i) {
      BlockInfo *bp = GetBlockInfo(tbl, block_num);
      int count = bp->GetRecordCount();
      // For each record in the block, check if it satisfies all conditions.
      for (int j = 0; j < count; ++j) {
        vector<TKey> tkey_value = GetRecord(tbl, block_num, j);
        bool sats = true;
        for (int k = 0; k < st.wheres().size(); ++k) {
          if (!SatisfyWhere(tbl, tkey_value, st.wheres()[k])) {
            sats = false;
          }
        }
        // If record meets all conditions, delete it.
        if (sats) {
          DeleteRecord(tbl, block_num, j);
          // Additionally update the index (if it exists) by removing the corresponding key.
          if (tbl->GetIndexNum() != 0) {
            BPlusTree tree(tbl->GetIndex(index_idx), hdl_, cm_, db_name_);
            int idx = -1;
            for (int i = 0; i < tbl->GetAttributeNum(); ++i) {
              if (tbl->ats()[i].attr_name() == tbl->GetIndex(index_idx)->attr_name()) {
                idx = i;
              }
            }
            tree.Remove(tkey_value[idx]);
          }
        }
      }
      // Move to the next block.
      block_num = bp->GetNextBlockNum();
    }
  } else {  
    // Use the index to quickly locate the record matching the equality condition.
    BPlusTree tree(tbl->GetIndex(index_idx), hdl_, cm_, db_name_);

    // Build a TKey from the where clause value for the indexed attribute.
    int type = tbl->GetIndex(index_idx)->key_type();
    int length = tbl->GetIndex(index_idx)->key_len();
    std::string value = st.wheres()[where_idx].value;
    TKey dest_key(type, length);
    dest_key.ReadValue(value);

    // Use the B+ tree to get the encoded block number where the record is located.
    int blocknum = tree.GetVal(dest_key);
    if (blocknum != -1) {
      // Decode the combined block number and offset.
      int blockoffset = blocknum;
      blocknum = (blocknum >> 16) & 0xffff;
      blockoffset = blockoffset & 0xffff;
      
      // Retrieve the record using the decoded block number and offset.
      vector<TKey> tkey_value = GetRecord(tbl, blocknum, blockoffset);
      bool sats = true;
      for (int k = 0; k < st.wheres().size(); ++k) {
        if (!SatisfyWhere(tbl, tkey_value, st.wheres()[k])) {
          sats = false;
        }
      }
      // If the record satisfies all conditions, delete it and update the index.
      if (sats) {
        DeleteRecord(tbl, blocknum, blockoffset);
        tree.Remove(dest_key);
      }
    }
  }

  // Write changes to disk.
  hdl_->WriteToDisk();
}

void RecordManager::Update(SQLUpdate &st) {
  // Get the table using the database and table names.
  Table *tbl = cm_->GetDB(db_name_)->GetTable(st.tb_name());

  vector<int> indices;    // Attribute indices to be updated.
  vector<TKey> values;    // New values for the attributes.
  int pk_index = -1;      // Primary key attribute index.
  int affect_index = -1;  // Index in the keyvalues vector for the primary key update.

  // Find the primary key attribute.
  for (int i = 0; i < tbl->ats().size(); ++i) {
    if (tbl->ats()[i].attr_type() == 1) {
      pk_index = i;
    }
  }

  // Build indices and new values; also record if the primary key is affected.
  for (int i = 0; i < st.keyvalues().size(); ++i) {
    int index = tbl->GetAttributeIndex(st.keyvalues()[i].key);
    indices.push_back(index);
    TKey value(tbl->ats()[index].data_type(), tbl->ats()[index].length());
    value.ReadValue(st.keyvalues()[i].value);
    values.push_back(value);
    if (index == pk_index) {
      affect_index = i;
    }
  }

  // Check for primary key conflicts if the primary key is being updated.
  if (affect_index != -1) {
    // If an index exists, use it to quickly check for conflicts.
    if (tbl->GetIndexNum() != 0) {
      BPlusTree tree(tbl->GetIndex(0), hdl_, cm_, db_name_);
      int value = tree.GetVal(values[affect_index]);
      if (value != -1) {  // A record with the new primary key already exists.
        throw PrimaryKeyConflictException();
      }
    } else {
      // No index available, so perform a full scan to detect conflicts.
      int block_num = tbl->first_block_num();
      for (int i = 0; i < tbl->block_count(); ++i) {
        BlockInfo *bp = GetBlockInfo(tbl, block_num);
        for (int j = 0; j < bp->GetRecordCount(); ++j) {
          vector<TKey> tkey_value = GetRecord(tbl, block_num, j);
          if (tkey_value[pk_index] == values[affect_index]) {
            throw PrimaryKeyConflictException();
          }
        }
        block_num = bp->GetNextBlockNum();
      }
    }
  }

  // Iterate through the table blocks to update matching records.
  int block_num = tbl->first_block_num();
  for (int i = 0; i < tbl->block_count(); ++i) {
    BlockInfo *bp = GetBlockInfo(tbl, block_num);
    for (int j = 0; j < bp->GetRecordCount(); ++j) {
      vector<TKey> tkey_value = GetRecord(tbl, block_num, j);
      bool sats = true;
      // Check if the record meets all where conditions.
      for (int k = 0; k < st.wheres().size(); ++k) {
        if (!SatisfyWhere(tbl, tkey_value, st.wheres()[k])) {
          sats = false;
        }
      }
      if (sats) {
        // If an index exists, remove the old key value.
        if (tbl->GetIndexNum() != 0) {
          BPlusTree tree(tbl->GetIndex(0), hdl_, cm_, db_name_);
          int idx = -1;
          for (int i = 0; i < tbl->GetAttributeNum(); ++i) {
            if (tbl->ats()[i].attr_name() == tbl->GetIndex(0)->attr_name()) {
              idx = i;
            }
          }
          tree.Remove(tkey_value[idx]);
        }

        // Update the record with the new values.
        UpdateRecord(tbl, block_num, j, indices, values);

        // After update, get the new record values.
        tkey_value = GetRecord(tbl, block_num, j);

        // If an index exists, add the new key value back into the index.
        if (tbl->GetIndexNum() != 0) {
          BPlusTree tree(tbl->GetIndex(0), hdl_, cm_, db_name_);
          int idx = -1;
          for (int i = 0; i < tbl->GetAttributeNum(); ++i) {
            if (tbl->ats()[i].attr_name() == tbl->GetIndex(0)->attr_name()) {
              idx = i;
            }
          }
          tree.Add(tkey_value[idx], block_num, j);
        }
      }
    }
    block_num = bp->GetNextBlockNum();
  }

  // Write changes to disk.
  hdl_->WriteToDisk();
}

//--------------------------------JOIN FUNCTION----------------------------------//
void RecordManager::Join(SQLJoin &st) {
  if (db_name_.empty()) {
    throw NoDatabaseSelectedException();
  }

  // Retrieve the two tables involved in the join.
  Table *tbl1 = cm_->GetDB(db_name_)->GetTable(st.tb_name1());
  Table *tbl2 = cm_->GetDB(db_name_)->GetTable(st.tb_name2());
  if (tbl1 == NULL || tbl2 == NULL) {
    throw TableNotExistException();
  }

  // Identify the join columns based on the column names provided.
  int colIndex1 = -1;
  for (int i = 0; i < tbl1->GetAttributeNum(); ++i) {
    if (tbl1->ats()[i].attr_name() == st.col_name1()) {
      colIndex1 = i;
      break;
    }
  }
  int colIndex2 = -1;
  for (int i = 0; i < tbl2->GetAttributeNum(); ++i) {
    if (tbl2->ats()[i].attr_name() == st.col_name2()) {
      colIndex2 = i;
      break;
    }
  }
  if (colIndex1 == -1 || colIndex2 == -1) {
    cout<<"Join column not found in one of the tables.";
    throw SyntaxErrorException();
  }

  vector<vector<TKey>> tkey_values;

  // Match values and store the inner join in tkey_values.
  int block1 = tbl1->first_block_num();
  for (int b1 = 0; b1 < tbl1->block_count() && block1 != -1; ++b1) {
    BlockInfo *bp1 = GetBlockInfo(tbl1, block1);
    for (int r1 = 0; r1 < bp1->GetRecordCount(); ++r1) {
      vector<TKey> rec1 = GetRecord(tbl1, block1, r1);
      TKey key1 = rec1[colIndex1];

      int block2 = tbl2->first_block_num();
      for (int b2 = 0; b2 < tbl2->block_count() && block2 != -1; ++b2) {
        BlockInfo *bp2 = GetBlockInfo(tbl2, block2);
        for (int r2 = 0; r2 < bp2->GetRecordCount(); ++r2) {
          vector<TKey> rec2 = GetRecord(tbl2, block2, r2);
          TKey key2 = rec2[colIndex2];

          if (key1 == key2) {
            vector<TKey> joinRow;
            // Add all columns from the first table.
            for (int i = 0; i < rec1.size(); ++i) {
              joinRow.push_back(rec1[i]);
            }
            // Add columns from the second table except the join column.
            for (int i = 0; i < rec2.size(); ++i) {
              if (i == colIndex2) continue;
              joinRow.push_back(rec2[i]);
            }
            tkey_values.push_back(joinRow);
          }
        }
        block2 = bp2->GetNextBlockNum();
      }
    }
    block1 = bp1->GetNextBlockNum();
  }

  // Print the header. We print all columns from tbl1 and only the non-join column from tbl2.
  for (int i = 0; i < tbl1->GetAttributeNum(); ++i) {
    cout << setw(12) << left << tbl1->ats()[i].attr_name();
  }
  for (int i = 0; i < tbl2->GetAttributeNum(); ++i) {
    if (i == colIndex2) continue; // Skip the duplicate join column.
    cout << setw(12) << left << tbl2->ats()[i].attr_name();
  }
  cout << endl;

  // Print all joined rows stored in tkey_values.
  for (auto &row : tkey_values) {
    for (auto &field : row) {
      cout << setw(12) << left << field;
    }
    cout << endl;
  }
}



//-------------------------------END OF JOIN FUNCTION---------------------------//


std::vector<TKey> RecordManager::GetRecord(Table *tbl, int block_num,
                                           int offset) {
  vector<TKey> keys;
  BlockInfo *bp = GetBlockInfo(tbl, block_num);

  char *content = bp->data() + offset * tbl->record_length() + 12;

  for (int i = 0; i < tbl->GetAttributeNum(); ++i) {
    int value_type = tbl->ats()[i].data_type();
    int length = tbl->ats()[i].length();

    TKey tmp(value_type, length);
 //memcpy is used for efficient copying of blocks of memory in situations where you know that the source and destination regions do not intersect
    memcpy(tmp.key(), content, length);

    keys.push_back(tmp);

    content += length;
  }

  return keys;
}

void RecordManager::DeleteRecord(Table *tbl, int block_num, int offset) {
  BlockInfo *bp = GetBlockInfo(tbl, block_num);

  char *content = bp->data() + offset * tbl->record_length() + 12;
  char *replace =
      bp->data() + (bp->GetRecordCount() - 1) * tbl->record_length() + 12;
  memcpy(content, replace, tbl->record_length());

  bp->DecreaseRecordCount();

  // add the block to rubbish block chain
  if (bp->GetRecordCount() == 0) { 
    
    int prevnum = bp->GetPrevBlockNum();
    int nextnum = bp->GetNextBlockNum();

    if (prevnum != -1) {
      BlockInfo *pbp = GetBlockInfo(tbl, prevnum);
      pbp->SetNextBlockNum(nextnum);
      hdl_->WriteBlock(pbp);
    }

    if (nextnum != -1) {
      BlockInfo *nbp = GetBlockInfo(tbl, nextnum);
      nbp->SetPrevBlockNum(prevnum);
      hdl_->WriteBlock(nbp);
    }

    BlockInfo *firstrubbish = GetBlockInfo(tbl, tbl->first_rubbish_num());
    bp->SetNextBlockNum(-1);
    bp->SetPrevBlockNum(-1);
    if (firstrubbish != NULL) {
      firstrubbish->SetPrevBlockNum(block_num);
      bp->SetNextBlockNum(firstrubbish->block_num());
    }
    tbl->set_first_rubbish_num(block_num);
  }

  hdl_->WriteBlock(bp);
}

void RecordManager::UpdateRecord(Table *tbl, int block_num, int offset,
                                 std::vector<int> &indices,
                                 std::vector<TKey> &values) {

  BlockInfo *bp = GetBlockInfo(tbl, block_num);

  char *content = bp->data() + offset * tbl->record_length() + 12;

  for (int i = 0; i < tbl->GetAttributeNum(); i++) {
    vector<int>::iterator iter = find(indices.begin(), indices.end(), i);
    if (iter != indices.end()) {
      memcpy(content, values[iter - indices.begin()].key(),
             values[iter - indices.begin()].length());
    }

    content += tbl->ats()[i].length();
  }

  hdl_->WriteBlock(bp);
}

bool RecordManager::SatisfyWhere(Table *tbl, std::vector<TKey> keys, SQLWhere where) {
  // Find the index of the attribute that matches the condition's key
  int idx = -1;
  for (int i = 0; i < tbl->GetAttributeNum(); ++i) {
    if (tbl->ats()[i].attr_name() == where.key) {
      idx = i;
    }
  }
  
  // Create a temporary key based on the attribute's type and length.
  TKey tmp(tbl->ats()[idx].data_type(), tbl->ats()[idx].length());
  // Convert the WHERE clause value to the appropriate format.
  tmp.ReadValue(where.value.c_str());
  
  switch (where.sign_type) {
    case SIGN_EQ:  // Equality check.
      return keys[idx] == tmp;
    case SIGN_NE:  // Not-equal check.
      return keys[idx] != tmp;
    case SIGN_LT:  // Less-than check.
      return keys[idx] < tmp;
    case SIGN_GT:  // Greater-than check.
      return keys[idx] > tmp;
    case SIGN_LE:  // Less-than or equal check.
      return keys[idx] <= tmp;
    case SIGN_GE:  // Greater-than or equal check.
      return keys[idx] >= tmp;
    default:
      return false;
  }
}

