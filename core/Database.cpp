
#include "Database.h"
#include "DatabaseEnvironment.h"

#define ABSOLUTE_MAX_DB_SIZE 16384

Database::Database(DatabaseEnvironment* databaseEnvironment_,
                   std::string name_,
                   bool createIfDoesntExist)
{
    databaseEnvironment = databaseEnvironment_;
    name = name_;
    
    if(databaseEnvironment->bulkTransaction)
    {
        //MJLog("You shouldn't be creating a new database while a bulk transaction is running.");
        databaseEnvironment->finishBulkTransaction();
    }
    
    MDB_txn *txn;
    
    unsigned int flags = 0;
    if(createIfDoesntExist)
    {
        flags = MDB_CREATE;
    }
    
    LOG_AND_RETURN_ON_FAIL(mdb_txn_begin(databaseEnvironment->env, nullptr, 0, &txn), "mdb_txn_begin");
    LOG_AND_RETURN_ON_FAIL(mdb_dbi_open(txn, name.c_str(), flags, &dbi), "mdb_dbi_open");
    LOG_AND_RETURN_ON_FAIL(mdb_txn_commit(txn), "mdb_txn_commit");
}

Database::~Database()
{
    databaseEnvironment->finishBulkTransaction();
    mdb_dbi_close(databaseEnvironment->env, dbi);
}

bool Database::startBulkTransaction()
{
    return databaseEnvironment->startBulkTransaction();
}

bool Database::finishBulkTransaction()
{
    return databaseEnvironment->finishBulkTransaction();
}

MDB_txn* Database::getTransaction()
{
    MDB_txn* transaction = databaseEnvironment->bulkTransaction;
    if(!transaction)
    {
        LOG_AND_RETURN_VALUE_ON_FAIL(mdb_txn_begin(databaseEnvironment->env, nullptr, 0, &transaction), "mdb startTransaction", nullptr);
    }
    return transaction;
}

bool Database::finishTransaction(MDB_txn* transaction)
{
    if(transaction != databaseEnvironment->bulkTransaction)
    {
        int rc = mdb_txn_commit(transaction);
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

bool Database::setDataForKey(std::string data, std::string key)
{
    if(key.length() == 0)
    {
        MJLog("Error, nil key in setData");
        return false;
    }
    
    if(data.length() == 0)
    {
        return removeDataForKey(key);
    }
    
    MDB_txn* transaction = getTransaction();
    if(!transaction)
    {
        return false;
    }
    
    MDB_val mdbKey, mdbData;
    
    
    mdbKey.mv_size = key.size();
    mdbKey.mv_data = (void *)key.data();
    mdbData.mv_size = data.size();
    mdbData.mv_data = (void *)data.data();
    
    int rc = mdb_put(transaction, dbi, &mdbKey, &mdbData, 0);
    
    if(rc != MDB_SUCCESS)
    {
        MJLog("mdb setData failed:%d - %s", rc, mdb_strerror(rc));
        finishTransaction(transaction);
        
        if(rc == MDB_MAP_FULL)
        {
            MJLog("Database map is full. Exiting, will resize on next launch.");
            exit(0);
        }
        return false;
    }
    
    return finishTransaction(transaction);
}

bool Database::removeDataForKey(std::string key)
{
    if(key.length() == 0)
    {
        MJLog("Error, nil key in removeDataForKey");
        return false;
    }
    MDB_txn* transaction = getTransaction();
    if(!transaction)
    {
        return false;
    }
    
    MDB_val mdbKey;
    
    mdbKey.mv_size = key.size();
    mdbKey.mv_data = (void *)key.data();
    
    int rc = mdb_del(transaction, dbi, &mdbKey, nullptr);
    
    finishTransaction(transaction);
    
    if(rc != MDB_SUCCESS &&
       rc != MDB_NOTFOUND)
    {
        MJLog("mdb removeObject failed:%d - %s", rc, mdb_strerror(rc));
        return false;
    }
    
    return true;
}

std::string Database::dataForKey(std::string key)
{
    if(key.length() == 0)
    {
        MJLog("Error, nil key in dataForKey");
        return "";
    }
    
    MDB_txn* transaction = getTransaction();
    if(!transaction)
    {
        return "";
    }
    
    MDB_val mdbKey, mdbData;
    
    mdbKey.mv_size = key.size();
    mdbKey.mv_data = (void *)key.data();
    
    int rc = mdb_get(transaction, dbi, &mdbKey, &mdbData);
    if(rc != MDB_SUCCESS || mdbData.mv_size == 0)
    {
        finishTransaction(transaction);
        return "";
    }
    
    std::string str((char *)mdbData.mv_data,mdbData.mv_size);
    
    finishTransaction(transaction);
    
    return str;
}

std::map<std::string, std::string> Database::allData()
{
	std::map<std::string, std::string> result;
	MDB_txn* transaction = getTransaction();
	if(!transaction)
	{
		return result;
	}

	MDB_cursor* cursor;

	int rc = mdb_cursor_open(transaction, dbi, &cursor);
	if(rc != MDB_SUCCESS)
	{
		finishTransaction(transaction);
		return result;
	}

	MDB_val mdbKey, mdbData;

	while((rc = mdb_cursor_get(cursor, &mdbKey, &mdbData, MDB_NEXT)) == 0) 
	{
		std::string keyStr((char *)mdbKey.mv_data,mdbKey.mv_size);

		result[keyStr] = std::string((char *)mdbData.mv_data,mdbData.mv_size);
	}

	mdb_cursor_close(cursor);
	finishTransaction(transaction);

	return result;
}

bool Database::hasKey(std::string key)
{
    if(key.length() == 0)
    {
        MJLog("Error, nil key in hasKey");
        return false;
    }
    MDB_txn* transaction = getTransaction();
    if(!transaction)
    {
        return false;
    }
    
    MDB_val mdbKey, mdbData;
    
    mdbKey.mv_size = key.size();
    mdbKey.mv_data = (void *)key.data();
    
    int rc = mdb_get(transaction, dbi, &mdbKey, &mdbData);
    if(rc != MDB_SUCCESS || mdbData.mv_size == 0)
    {
        finishTransaction(transaction);
        return false;
    }
    finishTransaction(transaction);
    
    return true;
}

/*

LuaRef Database::allDataLua(lua_State* luaState)
{
	MDB_txn* transaction = getTransaction();
	if(!transaction)
	{
		return LuaRef(luaState);
	}

	MDB_cursor* cursor;

	int rc = mdb_cursor_open(transaction, dbi, &cursor);
	if(rc != MDB_SUCCESS)
	{
		finishTransaction(transaction);
		return LuaRef(luaState);
	}

	MDB_val mdbKey, mdbData;
	LuaRef result = luabridge::newTable(luaState);

	while((rc = mdb_cursor_get(cursor, &mdbKey, &mdbData, MDB_NEXT)) == 0) 
	{
		LuaRef keyRef(luaState);
		std::string keyStr((char *)mdbKey.mv_data,mdbKey.mv_size);
		unserializeObject(&keyRef, keyStr, MJ_SERIALIZATION_BINARY);


		std::string dataStr = std::string((char *)mdbData.mv_data,mdbData.mv_size);
		LuaRef dataRef(luaState);
		unserializeObject(&dataRef, dataStr, MJ_SERIALIZATION_BINARY);

		result[keyRef] = dataRef;
	}

	mdb_cursor_close(cursor);
	finishTransaction(transaction);

	return result;
}


void Database::callFunctionForAllKeys(LuaRef func)
{
    MDB_txn* transaction = getTransaction();
    if(!transaction)
    {
        return;
    }

    MDB_cursor* cursor;

    int rc = mdb_cursor_open(transaction, dbi, &cursor);
    if(rc != MDB_SUCCESS)
    {
        finishTransaction(transaction);
        return;
    }

    MDB_val mdbKey, mdbData;

    while((rc = mdb_cursor_get(cursor, &mdbKey, &mdbData, MDB_NEXT)) == 0)
    {
        LuaRef keyRef(func.state());
        std::string keyStr((char *)mdbKey.mv_data,mdbKey.mv_size);
        unserializeObject(&keyRef, keyStr, MJ_SERIALIZATION_BINARY);

        std::string dataStr = std::string((char *)mdbData.mv_data,mdbData.mv_size);
        LuaRef dataRef(func.state());
        unserializeObject(&dataRef, dataStr, MJ_SERIALIZATION_BINARY);

        func(keyRef, dataRef);
    }

    mdb_cursor_close(cursor);
    finishTransaction(transaction);
}*/

uint64_t Database::getSize()
{
    MDB_txn* transaction = getTransaction();
    if(!transaction)
    {
        return 0;
    }
    MDB_stat stat;
    mdb_stat(transaction, dbi, &stat);
    uint64_t dbSize = stat.ms_psize * (stat.ms_leaf_pages + stat.ms_branch_pages + stat.ms_overflow_pages);
    finishTransaction(transaction);
    return dbSize;
}

/*void Database::eachKey(LuaRef callback, lua_State* luaState)
{
    MDB_txn* transaction = getTransaction();
    if(!transaction)
    {
        return;
    }

    MDB_cursor* cursor;

    int rc = mdb_cursor_open(transaction, dbi, &cursor);
    if(rc != MDB_SUCCESS)
    {
        finishTransaction(transaction);
        return ;
    }

    MDB_val mdbKey, mdbData;

    while((rc = mdb_cursor_get(cursor, &mdbKey, &mdbData, MDB_NEXT)) == 0)
    {
        LuaRef keyRef(luaState);
        std::string keyStr((char *)mdbKey.mv_data,mdbKey.mv_size);
        unserializeObject(&keyRef, keyStr, MJ_SERIALIZATION_BINARY);


        std::string dataStr = std::string((char *)mdbData.mv_data,mdbData.mv_size);
        LuaRef dataRef(luaState);
        unserializeObject(&dataRef, dataStr, MJ_SERIALIZATION_BINARY);
        
        SAFE_LUA_FUNCTION_CALL_WRAPPER_START_INLINE(callback)
            callback(keyRef, dataRef);
        SAFE_LUA_FUNCTION_CALL_WRAPPER_END_INLINE(callback)
        
    }

    mdb_cursor_close(cursor);
    finishTransaction(transaction);

}*/
