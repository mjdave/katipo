
#ifndef Database_h
#define Database_h


#define LOG_AND_RETURN_VALUE_ON_FAIL(__rc__, __name__, __return_value__) if(__rc__ != MDB_SUCCESS) { MJLog("%s failed:%d - %s", __name__, __rc__, mdb_strerror(__rc__)); return __return_value__; }

#define LOG_AND_RETURN_ON_FAIL(__rc__, __name__) if(__rc__ != MDB_SUCCESS) { MJLog("%s failed:%d - %s", __name__, __rc__, mdb_strerror(__rc__)); return; }

#include <stdio.h>
#include <string>
#include "lmdb.h"
#include "MathUtils.h"
#include "MJLog.h"
#include "Serialization.h"

class DatabaseEnvironment;

class Database {
public:
    MDB_dbi dbi;
    
    std::string name;

public:
    Database(DatabaseEnvironment* databaseEnvironment_,
             std::string name_,
             bool createIfDoesntExist = true);
    
    ~Database();
    
    
    bool startBulkTransaction();
    bool finishBulkTransaction();

	std::map<std::string, std::string> allData();
    
    bool setDataForKey(std::string data, std::string key);
    bool removeDataForKey(std::string key);
    std::string dataForKey(std::string key);
    bool hasKey(std::string key);
    
    uint64_t getSize();
    
    template<class SaveObject>
    bool getSerialized(SaveObject* sObject, std::string key)
    {
        std::string value = dataForKey(key);
        if(!value.empty())
        {
            return unserializeObject(sObject, value, MJ_SERIALIZATION_BINARY);
        }
        
        return false;
    }
    
    template<class SaveObject>
    bool setSerialized(SaveObject& sObject, std::string key)
    {
        std::string serialized = serializeObject(sObject, MJ_SERIALIZATION_BINARY);
        
        if(!serialized.empty())
        {
            return setDataForKey(serialized, key);
        }
        
        return false;
    }
    
private:
    DatabaseEnvironment* databaseEnvironment;
    
private:
    MDB_txn* getTransaction();
    bool finishTransaction(MDB_txn* transaction);

};

#endif /* Database_h */
