/**
 * Name        : IapHelper
 * Version     :
 * Vendor      :
 * Description :
 */

#include "IapHelper.h"
#include "AclAppManagerUtils.h"
#include "IabManagerUtils.h"

//#include "AccountApp.h"
//#include "AccountAppFrame.h"
//#include "AppResourceId.h"

#include "FApp.h"
#include <FBase.h>
#include <FSocial.h>
#include <FUi.h>

#define DBG_IAPHELPER          1

namespace AclAppManagement {
using namespace Tizen::App;
using namespace Tizen::Base;
using namespace Tizen::System;
using namespace Tizen::Io;

using namespace Tizen::Base::Collection;
using namespace Tizen::Social;
using namespace Tizen::App;

String* IapHelper::GetGroupId(String& PackageName)
{
    Database* pAclDatabase = AclAppManagerUtils::OpenDatabase(DB_READWRITE);
    String GroupId;
    if (pAclDatabase) {
        DbEnumerator* pEnum = AclAppManagerUtils::GetAppDbRow(pAclDatabase, APPDB_PKG_NAME_COLUMN, (String&) PackageName);

        if (pEnum) {
            if (DBG_IAPHELPER)
                AppLog("GroupId found level1\n");
            while (pEnum->MoveNext() == E_SUCCESS) {
                if (DBG_IAPHELPER)
                    AppLog("GroupId found level2\n");
                pEnum->GetStringAt(APPDB_GROUP_ID_COLUMN, GroupId);
            }
        }
        else {
            if (DBG_IAPHELPER)
                AppLog("Package doesn't exist: \n");
        }

        delete pAclDatabase;
    }
    else {
        if (DBG_IAPHELPER)
            AppLog("pAclDatabase is null\n");
    }
    return new String(GroupId);
}

String* IapHelper::GetPackageName(String& GroupId)
{
    Database* pAclDatabase = AclAppManagerUtils::OpenDatabase(DB_READWRITE);
    String PackageName;
    if (pAclDatabase) {
        DbEnumerator* pEnum = AclAppManagerUtils::GetAppDbRow(pAclDatabase, APPDB_GROUP_ID_COLUMN, (String&) GroupId);

        if (pEnum) {
            while (pEnum->MoveNext() == E_SUCCESS) {
                pEnum->GetStringAt(APPDB_PKG_NAME_COLUMN, PackageName);
            }
        }
        else {
            if (DBG_IAPHELPER)
                AppLog("Package doesn't exist: \n");
        }

        delete pAclDatabase;
    }
    return new String(PackageName);
}

String* IapHelper::GetTizenItemId(String& GoogleProductId, String& GroupId)
{
    Database* pAclDatabase = IabManagerUtils::OpenDatabase(DB_READWRITE);
    String TizenItemId;
    if (pAclDatabase) {
        DbEnumerator* pEnum = IabManagerUtils::GetRow(pAclDatabase, PRODUCT_ID_COLUMN, (String&) GoogleProductId, GRP_ID_COLUNM,
                (String&) GroupId);

        if (pEnum) {
            while (pEnum->MoveNext() == E_SUCCESS) {
                pEnum->GetStringAt(ITEM_ID_COLUMN, TizenItemId);
            }
        }
        else {
            if (DBG_IAPHELPER)
                AppLog("Package doesn't exist: \n");
            delete pAclDatabase;
            return null;
        }

        delete pAclDatabase;
    }
    return new String(TizenItemId);
}

String* IapHelper::GetGoogleProductId(String& TizenItemId, String& GroupId)
{
    Database* pAclDatabase = IabManagerUtils::OpenDatabase(DB_READWRITE);
    String GoogleProductId;
    if (pAclDatabase) {
        DbEnumerator* pEnum = IabManagerUtils::GetRow(pAclDatabase, ITEM_ID_COLUMN, (String&) TizenItemId, GRP_ID_COLUNM,
                (String&) GroupId);

        if (pEnum) {
            while (pEnum->MoveNext() == E_SUCCESS) {
                pEnum->GetStringAt(PRODUCT_ID_COLUMN, GoogleProductId);
            }
        }
        else {
            if (DBG_IAPHELPER)
                AppLog("Package doesn't exist: \n");
            delete pAclDatabase;
            return null;
        }

        delete pAclDatabase;
    }
    return new String(GoogleProductId);
}

void IapHelper::AddIabInfo(String& GroupId, String& TizenItemId, String& GoogleProductId)
{
    Database* pAclDatabase = IabManagerUtils::OpenDatabase(DB_READWRITE);
    if (pAclDatabase) {
        DbEnumerator* pEnum = IabManagerUtils::GetRow(pAclDatabase, ITEM_ID_COLUMN, (String&) TizenItemId, GRP_ID_COLUNM,
                (String&) GroupId);

        if (pEnum && (pEnum->MoveNext() == E_SUCCESS)) {
            if (DBG_IAPHELPER)
                AppLog("ItemId=%ls found in database", TizenItemId.GetPointer());
        }
        else {
            IabManagerUtils::AddToDatabase(pAclDatabase, GroupId, TizenItemId, GoogleProductId);
            if (DBG_IAPHELPER)
                AppLog("ItemId=%ls not found in database, inserting all info into ACL IAB database.", TizenItemId.GetPointer());
        }

        if (pEnum) {
            delete pEnum;
        }
        delete pAclDatabase;
    }
}

String IapHelper::GetTizenAccountName()
{
    String tizenAccountName = "";

    try {
        if (DBG_IAPHELPER)
            AppLog("GetTizenAccountName");

        AccountAccessor* aa = AccountAccessor::GetInstance();

        Tizen::Base::Collection::IList* listProviders = aa->GetAllAccountProvidersN();

        AccountProvider* pAccountProvider = null;

        Tizen::Base::Collection::IList* accountList = null;

        if (listProviders != null) {
            if (DBG_IAPHELPER)
                AppLog("logProviderNum: %d", listProviders->GetCount());

            for (int i = 0; i < listProviders->GetCount(); i++) {

                pAccountProvider = static_cast<AccountProvider*>(listProviders->GetAt(i));

                if (pAccountProvider != null) {
                    if (DBG_IAPHELPER)
                        AppLog("AccountName: %S", pAccountProvider->GetDisplayName().GetPointer());

                    if (pAccountProvider->GetDisplayName().CompareTo(L"Tizen account") == 0) {
                        accountList = aa->GetAccountsByAccountProviderN(pAccountProvider->GetAppId());

                        for (int j = 0; j < accountList->GetCount(); j++) {
                            Account* pAccount = static_cast<Account*>(accountList->GetAt(j));
                            if (DBG_IAPHELPER)
                                AppLog(": %S", pAccount->GetUserName().GetPointer());

                            tizenAccountName = pAccount->GetUserName();

                            if (DBG_IAPHELPER)
                                AppLog("AccountName: %S", tizenAccountName.GetPointer());
                        }
                        break;

                    } // Tizen account
                } // not null
            } // for
        }

    } catch (...) {
        AppLogException("GetTizenAccountName error");
    }

    return tizenAccountName;
}

}
