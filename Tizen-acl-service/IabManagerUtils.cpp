/*****************************************************************************************
 * Copyright (C) 2014 Open Mobile World Wide Inc. All rights reserved.
 *
 * This code belongs to OpenMobileWW.
 * Use of this code is prohibited except with a license from the appropriate authorities
 *
 * IabManagerUtils.cpp - implements the IabManagerUtils class
 *****************************************************************************************/
#include <FApp.h>
#include <FBase.h>
#include "AclAppManagerUtils.h"
#include "IabManagerUtils.h"
#define DBG_IABMANAGER 0

namespace AclAppManagement {

using namespace Tizen::App;
using namespace Tizen::Base;
using namespace Tizen::Base::Utility;
using namespace Tizen::Base::Collection;

#define GROUP_ID        "GroupId"
#define ITEM_ID         "ItemId"
#define PRODUCT_ID      "ProductId"

Columns iabDbColumns[] = { { GRP_ID_COLUNM, GROUP_ID, "TEXT" },
                           { ITEM_ID_COLUMN, ITEM_ID, "TEXT" },
                           { PRODUCT_ID_COLUMN, PRODUCT_ID, "TEXT" }
};

#define NUM_DB_COLUMNS ((int)(sizeof(iabDbColumns)/sizeof(struct Column)))

IabManagerUtils::IabManagerUtils()
{
}

IabManagerUtils::~IabManagerUtils()
{
}

Database* IabManagerUtils::OpenDatabase(unsigned int flags)
{
    Database* pDatabase = new Database();
    String dbName = (App::GetInstance()->GetAppDataPath() + L".aclIabManager.db");
    const char *opts;

    if (!pDatabase) {
        AppLogException("Couldn't open ACL IAB Database @ %ls.\n", dbName.GetPointer());
        return null;
    }

    switch (flags) {
    default:
    case DB_READONLY:
        opts = "r";
        break;
    case DB_READWRITE:
        opts = "r+";
        break;
    }

    if (IsFailed(pDatabase->Construct(dbName, opts))) {
        AppLogException("Couldn't Construct() ACL IAB Database\n");
        delete pDatabase;
        return null;
    }

    return pDatabase;
}

Database* IabManagerUtils::CreateDatabase()
{
    Database* pDatabase = new Database();
    String dbName = (App::GetInstance()->GetAppDataPath() + L".aclIabManager.db");
    String sqlRequest;

    if (!pDatabase) {
        AppLogException("Couldn't allocate ACL Database.\n");
        return null;
    }

    if (IsFailed(pDatabase->Construct(dbName, "a+"))) {
        AppLogException("Couldn't open ACL Database\n");
        delete pDatabase;
        return null;
    }

    sqlRequest.Append(L"CREATE TABLE IF NOT EXISTS android_app_info "
            "(Key INTEGER PRIMARY KEY AUTOINCREMENT, "
            GROUP_ID " TEXT, " ITEM_ID " TEXT, " PRODUCT_ID " TEXT)");

    result ret_val = pDatabase->ExecuteSql(sqlRequest, false);

    if (IsFailed(ret_val)) {
        AppLogException(
                "couldn't execute database statement: %ls, error is: %s\n", sqlRequest.GetPointer(), GetErrorMessage(ret_val));

        delete pDatabase;

        // Really?  This shouldn't happen... Is the DB corrupted?
        if (ret_val == E_INVALID_FORMAT || ret_val == E_INVALID_ARG || ret_val == E_IO) {
            // Try to recover
            File::Remove(dbName);
            dbName.Append("-journal");
            File::Remove(dbName);
            // TODO: Now we need to start a new scan for ACL-enabled apps
        } else if (ret_val == E_STORAGE_FULL || ret_val == E_SYSTEM) {
            // TODO: Need to inform the user!!!
        }
        return null;
    }

    return pDatabase;
}

result IabManagerUtils::AddToDatabase(Database* pAclDatabase, String& groupId, String& itemId, String& productId)
{
    result ret_val = E_SUCCESS;
    char query[MAX_BUF_SIZE] = { 0 };
    String sqlRequest;

    if (pAclDatabase) {
        pAclDatabase->BeginTransaction();

        snprintf(query, MAX_BUF_SIZE, "INSERT INTO android_app_info "
                "(" GROUP_ID ", " ITEM_ID ", " PRODUCT_ID ") VALUES "
                "('%ls', '%ls', '%ls')", groupId.GetPointer(), itemId.GetPointer(), productId.GetPointer());

        sqlRequest.Append(query);
        AppLogIf(DBG_IABMANAGER, "Sending Query: %s \n", query);
        ret_val = pAclDatabase->ExecuteSql(sqlRequest, false);

        if (IsFailed(ret_val)) {
            AppLogException(
                    "couldn't execute database statement: %ls, error is: %s\n", sqlRequest.GetPointer(), GetErrorMessage(ret_val));
            pAclDatabase->RollbackTransaction();
        } else {
            pAclDatabase->CommitTransaction(); //TODO: check if this fails.
        }
    }

    return ret_val;
}

DbEnumerator* IabManagerUtils::GetRow(Database* pAclDatabase, int column, String& columnId, int column2, String& columnId2)
{
    char query[MAX_BUF_SIZE] = { 0 };

    if (column > NUM_DB_COLUMNS || column <= 0) {
        AppLogException("invalid db column: %d\n", iabDbColumns);
        return null;
    } else {
        snprintf(query, MAX_BUF_SIZE, "SELECT * FROM android_app_info WHERE %s = '%ls' AND %s = '%ls'",
                iabDbColumns[--column].columnName, columnId.GetPointer(), iabDbColumns[--column2].columnName,
                columnId2.GetPointer());
        AppLogIf(DBG_IABMANAGER, "Query is: %s\n", query);
        String sqlRequest(query);

        return (pAclDatabase->QueryN(sqlRequest));
    }
}

DbEnumerator* IabManagerUtils::GetRows(Database* pAclDatabase, int column)
{
    char query[MAX_BUF_SIZE] = { 0 };

    if (column > NUM_DB_COLUMNS || column <= 0) {
        AppLogException("invalid db column: %d\n", iabDbColumns);
        return null;
    } else {
        snprintf(query, MAX_BUF_SIZE, "SELECT %s FROM android_app_info ", iabDbColumns[--column].columnName);
        AppLogIf(DBG_IABMANAGER, "Query is: %s\n", query);
        String sqlRequest(query);
        return (pAclDatabase->QueryN(sqlRequest));
    }
}

result IabManagerUtils::RemoveFromDatabase(Database* pAclDatabase, String& groupId)
{
    result ret_val = E_SUCCESS;
    String sqlRequest;
    char query[MAX_BUF_SIZE] = { 0 };

    if (pAclDatabase) {
        pAclDatabase->BeginTransaction();

        snprintf(query, MAX_BUF_SIZE, "DELETE FROM android_app_info WHERE " GROUP_ID " = '%ls'", groupId.GetPointer());
        sqlRequest.Append(query);
        ret_val = pAclDatabase->ExecuteSql(sqlRequest, false);

        if (IsFailed(ret_val)) {
            AppLogException(
                    "couldn't execute database statement: %ls, error is: %s\n", sqlRequest.GetPointer(), GetErrorMessage(ret_val));
            pAclDatabase->RollbackTransaction();
        } else {
            pAclDatabase->CommitTransaction(); //TODO: maybe use a try catch
        }
    }

    return ret_val;
}

result IabManagerUtils::UpdateDatabase(Database* pAclDatabase, String& groupId, String& itemId, String& productId)
{
    result ret_val = E_SUCCESS;
    String sqlRequest;
    char query[MAX_BUF_SIZE] = { 0 };

    if (pAclDatabase) {
        pAclDatabase->BeginTransaction();

        snprintf(query, MAX_BUF_SIZE, "UPDATE android_app_info SET " GROUP_ID "='%ls', " ITEM_ID " = '%ls' "
                "WHERE " PRODUCT_ID " = '%ls' ", groupId.GetPointer(), itemId.GetPointer(), productId.GetPointer());

        AppLogIf(DBG_IABMANAGER, " SQL = %s", query);
        sqlRequest.Append(query);
        ret_val = pAclDatabase->ExecuteSql(sqlRequest, false);

        if (IsFailed(ret_val)) {
            AppLogException(
                    "couldn't execute database statement: %ls, error is: %s\n", sqlRequest.GetPointer(), GetErrorMessage(ret_val));
            pAclDatabase->RollbackTransaction();
        } else {
            pAclDatabase->CommitTransaction(); //TODO: maybe use a try catch
        }
    }

    return ret_val;
}

}
