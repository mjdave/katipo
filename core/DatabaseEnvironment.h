
#ifndef DatabaseEnvironment_h
#define DatabaseEnvironment_h


#include <stdio.h>
#include "lmdb.h"
//#include "MathUtils.h"

#include <map>

class Database;

class DatabaseEnvironment {
public:
    MDB_env* env;
    MDB_txn* bulkTransaction;

	size_t currentMapSizeInMB;
	unsigned int pageSize;
    
    
public:
    DatabaseEnvironment(std::string environmentDirectoryPath,
             int maxDatabases,
             size_t initialSizeInMB);
    
    bool startBulkTransaction();
    bool finishBulkTransaction();
    
    ~DatabaseEnvironment();
    
    Database* getDatabase(std::string name, bool createIfDoesntExist = true);
    
private:
    std::string environmentDirectoryPath;
    
    std::map<std::string, Database*> databaseCache;

};

#endif /* DatabaseEnvironment_h */
