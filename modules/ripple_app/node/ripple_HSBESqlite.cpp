
HSBESQLite::HSBESQLite(std::string const& path) : mName(path)
{
    mDb = new DatabaseCon(path, HashNodeDBInit, HashNodeDBCount);
    mDb->getDB()->executeSQL(boost::str(boost::format("PRAGMA cache_size=-%d;") %
        (theConfig.getSize(siHashNodeDBCache) * 1024)));
}

HSBESQLite::~HSBESQLite()
{
    delete mDb;
}

std::string HSBESQLite::getDataBaseName()
{
    return mName;
}

bool HSBESQLite::store(HashedObject::ref object)
{
    ScopedLock sl(mDb->getDBLock());
    static SqliteStatement pSt(mDb->getDB()->getSqliteDB(),
        "INSERT OR IGNORE INTO CommittedObjects "
            "(Hash,ObjType,LedgerIndex,Object) VALUES (?, ?, ?, ?);");
    bind(pSt, object);
    pSt.step();
    pSt.reset();
    return true;
}

bool HSBESQLite::bulkStore(const std::vector< HashedObject::pointer >& objects)
{
    ScopedLock sl(mDb->getDBLock());
    static SqliteStatement pStB(mDb->getDB()->getSqliteDB(), "BEGIN TRANSACTION;");
    static SqliteStatement pStE(mDb->getDB()->getSqliteDB(), "END TRANSACTION;");
    static SqliteStatement pSt(mDb->getDB()->getSqliteDB(),
        "INSERT OR IGNORE INTO CommittedObjects "
            "(Hash,ObjType,LedgerIndex,Object) VALUES (?, ?, ?, ?);");

    pStB.step();
    pStB.reset();

    BOOST_FOREACH(HashedObject::ref object, objects)
    {
        bind(pSt, object);
        pSt.step();
        pSt.reset();
    }

    pStE.step();
    pStE.reset();

    return true;

}

HashedObject::pointer HSBESQLite::retrieve(uint256 const& hash)
{
    HashedObject::pointer ret;

    {
        ScopedLock sl(mDb->getDBLock());
        static SqliteStatement pSt(mDb->getDB()->getSqliteDB(),
            "SELECT ObjType,LedgerIndex,Object FROM CommittedObjects WHERE Hash = ?;");

        pSt.bind(1, hash.GetHex());

        if (pSt.isRow(pSt.step()))
            ret = boost::make_shared<HashedObject>(getType(pSt.peekString(0)), pSt.getUInt32(1), pSt.getBlob(2), hash);

        pSt.reset();
    }

    return ret;
}

void HSBESQLite::visitAll(FUNCTION_TYPE<void (HashedObject::pointer)> func)
{
    uint256 hash;

    static SqliteStatement pSt(mDb->getDB()->getSqliteDB(),
        "SELECT ObjType,LedgerIndex,Object,Hash FROM CommittedObjects;");

    while (pSt.isRow(pSt.step()))
    {
        hash.SetHexExact(pSt.getString(3));
        func(boost::make_shared<HashedObject>(getType(pSt.peekString(0)), pSt.getUInt32(1), pSt.getBlob(2), hash));
    }

    pSt.reset();
}

void HSBESQLite::bind(SqliteStatement& statement, HashedObject::ref object)
{
    char const* type;
    switch (object->getType())
    {
        case hotLEDGER:                type = "L"; break;
        case hotTRANSACTION:        type = "T"; break;
        case hotACCOUNT_NODE:        type = "A"; break;
        case hotTRANSACTION_NODE:    type = "N"; break;
        default:                    type = "U";
    }

    statement.bind(1, object->getHash().GetHex());
    statement.bind(2, type);
    statement.bind(3, object->getIndex());
    statement.bindStatic(4, object->getData());
}

HashedObjectType HSBESQLite::getType(std::string const& type)
{
    HashedObjectType htype = hotUNKNOWN;
    if (!type.empty())
    {
        switch (type[0])
        {
            case 'L': htype = hotLEDGER; break;
            case 'T': htype = hotTRANSACTION; break;
            case 'A': htype = hotACCOUNT_NODE; break;
            case 'N': htype = hotTRANSACTION_NODE; break;
        }
    }
    return htype;
}