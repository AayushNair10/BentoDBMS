#ifndef MINIDB_COMMONS_H_
#define MINIDB_COMMONS_H_

#include <ostream>

// Quiet mode (-q / --quiet): only results and errors are printed
extern bool quiet_mode;
// Stream for debug output: std::cout normally, discarded in quiet mode
std::ostream &debug_out();

// Counters printed at exit when started with --stats
struct Stats {
  long block_reads;    // blocks read from disk
  long block_writes;   // blocks written to disk
  long cache_hits;     // block requests served from the cache
  long cache_misses;   // block requests that had to load the block
  long evictions;      // cached blocks recycled to make room
  long catalog_writes; // times the catalog was saved
  long node_visits;    // B+ tree nodes opened
};
extern Stats stats;

// File Format
#define FORMAT_RECORD 0
#define FORMAT_INDEX 1

// Data Type
#define T_INT 0
#define T_FLOAT 1
#define T_CHAR 2

//=	<>	<	>	<=	>=
#define SIGN_EQ 0 // ==	Equals
#define SIGN_NE 1 // !=	Not equal
#define SIGN_LT 2 // <	Less than
#define SIGN_GT 3 // >	Greater than
#define SIGN_LE 4 // <=	Less or equal
#define SIGN_GE 5 // >=	Greater or equal

#endif
