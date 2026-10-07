BentoDBMS: Mini Database Engine in C++
===================================

BentoDBMS is a simplified database engine implemented in C++, for the purpose of understanding the underlying details of database system. This database engine implements the most basic SQL operations as well as index on B+ tree.

⚠️ **Disclaimer:** Built upon [MiniDB](https://github.com/nrthyrk/minidb). On top of it, BentoDBMS adds:
* `JOIN`, comparison operators in `WHERE` clauses, and the `SHOW TABLE` command
* Fixes for data loss, crashes and wrong results in the storage, index and delete code
* Crash-safe catalog writes, a single-instance lock, and pinning in the buffer cache
* A quiet mode, detailed error messages, and friendlier input handling
* A `--stats` flag and a benchmark script

## Compile

To compile, you need the open source C++ library boost installed. The source code does not use readline, but the XCode and Eclipse project files still link against it, so those two builds need readline installed as well.

On Mac OS X, XCode could be used to open "MiniDB.xcodeproj" directly. It's recommended to install the libraries with Homebrew. The project file looks for readline headers in "/usr/local/Cellar/readline/6.2.4/include"; if your readline is somewhere else, you will need to change the header search path and library search path of readline in XCode.

On platforms where Eclipse is availbable, the whole folder could be imported to Eclipse.

To build from the command line, only boost is needed:
```
# macOS (Homebrew)
brew install boost
clang++ -std=c++17 src/*.cc -o minidb -I$(brew --prefix)/include -L$(brew --prefix)/lib -lboost_filesystem -lboost_regex -lboost_serialization

# Linux
sudo apt install g++ libboost-filesystem-dev libboost-regex-dev libboost-serialization-dev
g++ -std=c++17 src/*.cc -o minidb -lboost_filesystem -lboost_regex -lboost_serialization
```

To run it, you need to have global environment variable "HOME" set, the data will be stored at "$HOME/MiniDBData". This folder must exist before starting:
```
mkdir -p ~/MiniDBData && ./minidb
```

By default every statement also prints debug output (the SQL type, the parsed statement, progress messages and the B+ tree after indexed queries). Start with `-q` or `--quiet` to print only results and errors:
```
./minidb -q
```

## Error Messages

Errors are printed to stderr and never end the session. Syntax errors say what was expected and what was found:
```
MiniDB> select id from t;
Syntax Error: expected '*' after 'select' (column lists are not supported) but found 'id'
MiniDB> select * from t where zz = 1;
Column doesn't exist: zz
MiniDB> insert into t values (3);
Number of values doesn't match the number of columns!
MiniDB> foo bar;
Unknown command: foo bar
```

## Crash Safety

*	The catalog is written to "catalog.tmp", flushed to disk and then renamed over "catalog". A crash during the write leaves the previous catalog intact.
*	There is no write-ahead log: a crash in the middle of a statement can still leave that statement half applied in the table and index files.

## Buffer Cache

Blocks are cached in 300 frames of 4 KB. A block that is in use is pinned and cannot be recycled until it is released, so a pointer to it stays valid while other blocks are loaded. B+ tree nodes pin their block for as long as they are alive and are freed at the end of each tree operation.

## Benchmarking

Start the engine with `--stats` to print counters and statement timings to stderr when it exits: statements run, blocks read and written, cache hits, misses and evictions, catalog writes, B+ tree nodes opened, and the median, 99th percentile and slowest statement time in microseconds.
```
./minidb -q --stats < workload.sql > /dev/null
```

"bench/run_bench.py" (needs Python 3) generates workloads, runs each one in a temporary HOME so real data is never touched, and writes "bench/results.csv" and "bench/results.md". Every workload starts from a freshly loaded table, is run 3 times and the median time is kept. If the sqlite3 command line tool is installed, the same statements are run through it as a reference.
```
clang++ -std=c++17 -O2 src/*.cc -o minidb -I$(brew --prefix)/include -L$(brew --prefix)/lib -lboost_filesystem -lboost_regex -lboost_serialization
python3 bench/run_bench.py --bin ./minidb
python3 bench/run_bench.py --bin ./minidb --sizes 1000,8000 --runs 5 --no-sqlite
```

Results on an Apple M2 with a -O2 build, table "t (id int, v int, name char(16))" with 4000 rows (the full table for 1000, 2000 and 4000 rows is in "bench/results.md"):

| Workload | Index | Statements | Ops/s | p50 (us) | p99 (us) | SQLite ops/s |
|---|---|---|---|---|---|---|
| bulk insert | no | 4000 | 1010 | 976 | 1769 | 3532 |
| bulk insert | yes | 4000 | 3705 | 262 | 337 | 3514 |
| point select | no | 500 | 442 | 2220 | 2387 | 58753 |
| point select | yes | 500 | 27060 | 17 | 49 | 59547 |
| update by key | no | 500 | 404 | 2408 | 3644 | 3838 |
| update by key | yes | 500 | 393 | 2499 | 2912 | 3212 |
| delete by key | no | 500 | 428 | 2294 | 2786 | 3180 |
| delete by key | yes | 500 | 3005 | 288 | 592 | 3201 |
| full scan | no | 20 | 338 | 2554 | 2865 | 2462 |
| delete half + reinsert | no | 4000 | 595 | 1578 | 2399 | 3192 |

What the numbers show:
*	Without an index, every insert scans the whole table for a duplicate key, so bulk insert slows down as the table grows (2406, 1650 and 1010 inserts per second at 1000, 2000 and 4000 rows). With an index it stays near 3700.
*	The index makes point selects about 60 times faster and deletes about 7 times faster at 4000 rows. UPDATE does not use the index.
*	Scanning a 600 block table (twice the 300 block cache) is about 23 times slower per statement than scanning a 150 block table, because every scan reloads every block: 30000 block reads and 29700 evictions for 50 scans.
*	The times include starting the program, which matters for the short workloads.

## Concurrency

BentoDBMS is single-process and single-threaded. On startup it takes an exclusive lock on "$HOME/MiniDBData/.lock" and holds it until it exits. A second instance pointed at the same data folder prints an error and exits instead of corrupting the data. The lock is released automatically when the process exits or crashes.

## Features

#### Database Management
*	Create Database
```
Syntax:		CREATE DATABASE database_name;
Example:	CREATE DATABASE abc;
```
*	Drop Database
```
Syntax:		DROP DATABASE database_name;
Example: 	DROP DATABASE abc;
```
*	Show Databases
```
Syntax:		SHOW DATABASES;
Example: 	SHOW DATABASES;
```
*	Use
```
Syntax:		USE database_name;
Example: 	USE abc;
```
####	Table Management
*	Create Table
```
Syntax:		CREATE TABLE table_name
			(
			column_name1 data_type(size),
			column_name2 data_type(size),
			column_name3 data_type(size),
			....,
			PRIMARY KEY (column_name)
			);
Example: 	CREATE TABLE aaa
			(
			col1 int,
			col2 float,
			col3 char(8),
			PRIMARY KEY (col1)
			);
```
Note:	Available data types include int, float and char(N). Only one primary key could be created.

*	Drop Table
```
Syntax:		DROP TABLE table_name;
Example:	DROP TABLE aaa;
```
*	Show Tables
```
Syntax:		SHOW TABLES;
Example: 	SHOW TABLES;
```
*	Show Table (prints every row, same as SELECT * FROM table_name)
```
Syntax:		SHOW TABLE table_name;
Example: 	SHOW TABLE aaa;
```
####	Index Management

*	Create Index
```
Syntax:		CREATE INDEX index_name
			ON table_name (column_name);
Example: 	CREATE INDEX aaacol1 
			ON aaa (col1);
```
Note:	Index can only be created on primary key. B+ tree manipulation is implemented in all data manipulation SQLs.

*	Drop Index
```
Syntax:		DROP INDEX index_name;
Example:	DROP INDEX aaacol1;
```
####	Data Manipulation
*	Insert
```
Syntax:		INSERT INTO table_name
			VALUES (value1, value2, value3, ...);
Example: 	INSERT INTO aaa
			VALUES (111, 222.2, 'xyz');
```
*	Select
```
Syntax:		SELECT * FROM table_name
			WHERE column1 = value1
			and column2 = value2
			and ...;
Example:	SELECT * FROM aaa
			WHERE col1 = 111;
```
Note:		Only "and" logic is allowed. Only "*" is allowed. Besides "=", the comparisons "<>", "<", ">", "<=" and ">=" can be used in every WHERE clause. The index is only used for "=" on the indexed column.

*	Delete
```
Syntax:		DELETE FROM table_name 
			WHERE column1 = value1
			and column2 = value2
			and ...;
Example:	DELETE FROM aaa
			WHERE col1 = 111;
```
Note:		Only "and" logic is allowed. Without a WHERE clause every row is deleted.

*	Update
```
Syntax:		UPDATE table_name
			SET column1 = value1, column2 = value2, ...
			WHERE column1 = value1
			and column2 = value2
			and ...;
Example:	UPDATE aaa
			SET col2 = 555.5
			WHERE col1 = 1;
```
Note:		Only "and" logic is allowed. The WHERE clause is required. UPDATE always scans the whole table, even when an index exists.

*	Join
```
Syntax:		JOIN table_name1 AND table_name2
			ON column1 = column2;
Example:	JOIN aaa AND bbb
			ON col1 = id;
```
Note:		Inner join on one column of each table. All columns of the first table are printed, followed by the columns of the second table except the join column.

####	Other Features Implemented

*	Help
```
Syntax:		HELP;
Example: 	HELP;
```
*	Quit
```
Syntax:		EXIT
			or
			QUIT
Example:	EXIT
			or
			QUIT
```
*	Exec
```
Syntax:		EXEC file_name;
Example: 	EXEC input.txt;
```
Note:		The file name is relative to the folder the program was started from and is converted to lowercase. Every statement in the file must end with ";", a last statement without one is skipped.

####	Limitations

*	The whole statement is converted to lowercase before it is parsed, including text in quotes: 'Hello' is stored as 'hello'.
*	Text values cannot contain spaces or any of the characters ( ) , = < > ;
*	Values are converted to the type of the column: 3.7 in an int column is stored as 3, text in a number column is stored as 0, and text longer than a char(N) column is cut to N characters.
*	A row must fit in one 4 KB block: the column sizes can add up to at most 4084 bytes (int and float take 4 bytes each).
*	A table with an index can have at most 65536 blocks (256 MB), and one index per table is allowed.
*	An UPDATE that sets the primary key of several rows to the same value is not rejected and leaves duplicate keys.
*	Deleted space is reused by later inserts, but the files never shrink.

####	Unimplemented Features
*	Transaction Management
*	User Management
*	Authentication
*	Foreign Keys
*	Views
