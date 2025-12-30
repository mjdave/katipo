
#include "DatabaseEnvironment.h"
#include "TuiFileUtils.h"
#include "Database.h"

#ifndef MAX 
#define MAX(a_,b_) ((a_) > (b_) ? (a_) : (b_))
#endif

#define ABSOLUTE_MAX_DB_SIZE 16384

DatabaseEnvironment::DatabaseEnvironment(std::string environmentDirectoryPath_,
                   int maxDatabases,
                   size_t initialSizeInMB)
{
    bulkTransaction = nullptr;
    
    //MJLog("db path:%s", environmentDirectoryPath_.c_str());
    environmentDirectoryPath = environmentDirectoryPath_;
    size_t maxMapSizeInMB = MAX(initialSizeInMB, 8);
    
    Tui::createDirectoriesIfNeededForDirPath(environmentDirectoryPath);
    
    LOG_AND_RETURN_ON_FAIL(mdb_env_create(&env), "mdb_env_create");
    LOG_AND_RETURN_ON_FAIL(mdb_env_set_maxdbs(env, maxDatabases), "mdb_env_set_maxdbs");
    
    int rc = mdb_env_open(env, environmentDirectoryPath.c_str(), 0, 0664);
	if (rc != MDB_SUCCESS)
	{
		MJLog("mdb_env_open failed:%d - %s", rc, mdb_strerror(rc));
	}

	MDB_stat stat;
	mdb_env_stat(env, &stat);
	pageSize = stat.ms_psize;

	MDB_envinfo envStat;
	mdb_env_info(env, &envStat);
	size_t currentMaxSize = (envStat.me_last_pgno + 1) * pageSize;
    
    while(currentMaxSize > ((MAX(maxMapSizeInMB, 8) - 8) * 1024 * 1024) &&
          maxMapSizeInMB < ABSOLUTE_MAX_DB_SIZE)
    {
        maxMapSizeInMB *= 2;
    }
    
    //MJLog("Found DB map size:%.4fMB Setting max map size to:%zuMB - %s", ((float)currentMaxSize)/1024.0f/1024.0f, maxMapSizeInMB, environmentDirectoryPath );


	currentMapSizeInMB = maxMapSizeInMB;
    LOG_AND_RETURN_ON_FAIL(mdb_env_set_mapsize(env, currentMapSizeInMB*1024*1024), "mdb_env_set_mapsize");
}


DatabaseEnvironment::~DatabaseEnvironment()
{
    finishBulkTransaction();
    
    for(auto& nameAndDB : databaseCache)
    {
        delete nameAndDB.second;
    }
    
    mdb_env_close(env);
}


bool DatabaseEnvironment::startBulkTransaction()
{
    if(!bulkTransaction)
    {
		MDB_envinfo envStat;
		mdb_env_info(env, &envStat);
		size_t currentMaxSize = (envStat.me_last_pgno + 1) * pageSize;

		if(currentMaxSize > ((MAX(currentMapSizeInMB, 8) - 8) * 1024 * 1024) &&
			currentMapSizeInMB < ABSOLUTE_MAX_DB_SIZE)
		{
			currentMapSizeInMB *= 2;
			LOG_AND_RETURN_VALUE_ON_FAIL(mdb_env_set_mapsize(env, currentMapSizeInMB*1024*1024), "mdb_env_set_mapsize", false);
		}

        LOG_AND_RETURN_VALUE_ON_FAIL(mdb_txn_begin(env, nullptr, 0, &bulkTransaction), "mdb startTransaction", false);
    }
    
    return (bulkTransaction != nullptr);
}

bool DatabaseEnvironment::finishBulkTransaction()
{
    if(bulkTransaction)
    {
        int rc = mdb_txn_commit(bulkTransaction);
        bulkTransaction = nullptr;
        if(rc != MDB_SUCCESS)
        {
            MJLog("finishTransaction failed:%d - %s", rc, mdb_strerror(rc));
            if(rc == MDB_MAP_FULL)
            {
                MJLog("Database map is full. Exiting, will resize on next launch.");
                exit(0);
            }
            return false;
        }
    }
    
    return true;
}

Database* DatabaseEnvironment::getDatabase(std::string name, bool createIfDoesntExist)
{
    if(databaseCache.count(name) != 0)
    {
        return databaseCache[name];
    }
    
    Database* db = new Database(this, name, createIfDoesntExist);
    databaseCache[name] = db;
    return db;
}
