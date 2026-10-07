#include "block_info.h"

#include <cstring>
#include <fstream>

#include "commons.h"

using namespace std;

void BlockInfo::ReadInfo(std::string path) {
  path += file_->db_name() + "/" + file_->file_name();

  if (file_->type() == FORMAT_INDEX) {
    path += ".index";
  } else {
    path += ".records";
  }

  memset(data_, 0, 4 * 1024);
  ifstream ifs(path, ios::binary);
  ifs.seekg(block_num_ * 4 * 1024);
  ifs.read(data_, 4 * 1024);
  ifs.close();
}

void BlockInfo::WriteInfo(std::string path) {
  path += file_->db_name() + "/" + file_->file_name();

  if (file_->type() == FORMAT_INDEX) {
    path += ".index";
  } else {
    path += ".records";
  }

  // in|out opens an existing file without truncating it
  fstream ofs(path, ios::in | ios::out | ios::binary);
  if (!ofs.is_open()) {
    ofs.open(path, ios::out | ios::binary);
  }
  ofs.seekp(block_num_ * 4 * 1024);
  ofs.write(data_, 4 * 1024);
  ofs.close();
}
