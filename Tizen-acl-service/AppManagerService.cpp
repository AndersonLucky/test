/**
 * Name        : AppManagerServiceApp
 * Version     :
 * Vendor      :
 * Description :
 */

#include "AclAppControl.h"
#include "AclService.h"
#include "AppManagerService.h"
#include "FShellShortcutManager.h"

using namespace Tizen::App;
using namespace Tizen::Base;
using namespace Tizen::Base::Collection;
using namespace Tizen::System;
using namespace Tizen::Shell;
using namespace AclAppManagement;

#define DBG_APPMGRSVC   1
#define DBG_APPMANAGER  1
#define DBG_SHORTCUTS   1

// ------- Static Public Methods -------

AppManagerService::AppManagerService(App* app) :
        AclAbstractService(app), mCurrentInstallation(NORMAL_PRIORITY, "")
{
    // See if there is an existing database.
    Database *db = AclAppManagerUtils::OpenDatabase(DB_READWRITE);

    // No existing database, create one.
    if (db == NULL) {
        db = AclAppManagerUtils::CreateDatabase();
    }
    else {
        // perform any needed upgrades.
        AclAppManagerUtils::UpgradeDatabase(db);
    }

    // close the database
    if (db != NULL) {
        delete db;
    }

    mPkgManager = PackageManager::GetInstance();

    mNeedWakelock = false;
    mLastBrightness = -2;
}

bool AppManagerService::GetListOfApps(Tizen::Base::Collection::IMap& listOfApps)
{
    Database* pAclDatabase = AclAppManagerUtils::OpenDatabase(DB_READONLY);

    if (pAclDatabase == null) {
        return false;
    }

    DbEnumerator* pEnum = AclAppManagerUtils::GetAppDbRows(pAclDatabase, APPDB_APPID_COLUMN);
    if (pEnum == null) {
        delete pAclDatabase;
        return false;
    }

    PackageManager* pPackageManager = PackageManager::GetInstance();
    int ordinal = 0;

    while (pEnum->MoveNext() == E_SUCCESS) {
        AppId *appId = new String();
        String *key = new String();

        pEnum->GetStringAt(0, *appId);

        // Use the app display name so we can sort it later
        PackageAppInfo* pPackageAppInfo = pPackageManager->GetPackageAppInfoN(*appId);
        if (pPackageAppInfo)
            key->Append(pPackageAppInfo->GetAppDisplayName());
        else
            key->Append(L"Unknown");

        // Make sure the name is unique while not affecting primary ordering
        key->Append('-');
        key->Append(ordinal++);

        listOfApps.Add(key, appId);
    }

    delete pEnum;
    delete pAclDatabase;
    return true;
}

bool AppManagerService::SendAppControl(String& appId, const wchar_t* operation)
{
    return SendAppControl(appId.GetPointer(), operation);
}

bool AppManagerService::SendAppControl(const wchar_t* appId, const wchar_t* operation, const String* uri, const String* mime,
        const IMap* map)
{
    AppControl* pAc = AppManager::FindAppControlN(appId, operation);
    result r = E_FAILURE;

    if (pAc) {
        AppLog(" === sending AppControl %ls to %ls ===", operation, appId);
        r = pAc->Start(uri, mime, map, null);
        AppLogExceptionIf(IsFailed(r), "Cannot send AppControl %ls to %ls: %s", operation, appId, GetErrorMessage(r));
        delete pAc;
    }
    else
        AppLogException("Cannot find AppControl %ls for %ls", operation, appId);

    return (pAc != null) && (r == E_SUCCESS);
}

bool AppManagerService::SendMessage(const String& appId, const wchar_t* operation, HashMap *extraData)
{
    RemoteMessagePort* pRemotePort = MessagePortManager::RequestRemoteMessagePort(appId, _ACL_PORT);
    result r = E_FAILURE;

    if (pRemotePort) {
        HashMap* map = new HashMap(SingleObjectDeleter);
        map->Construct();
        map->Add(new String(_KEY_OPERATION), new String(operation));

        if (extraData) {
            // Copy all the values from the extra data to the map
            IMapEnumerator* pMapEnum = extraData->GetMapEnumeratorN();
            while (pMapEnum->MoveNext() == E_SUCCESS) {
                map->Add(pMapEnum->GetKey(), pMapEnum->GetValue());
            }

            delete pMapEnum;
            extraData->RemoveAll();
        }

        r = pRemotePort->SendMessage(map);
        if (IsFailed(r))
            AppLogException("Cannot send message %ls for operation %ls: %s", appId.GetPointer(), operation, GetErrorMessage(r));
        else
            AppLog(" === sending message %ls to %ls ===", operation, appId.GetPointer());
        delete map;
    }

    return (pRemotePort != null) && (r == E_SUCCESS);
}

bool AppManagerService::SendTerminate(String& appId)
{
    if (SendMessage(appId, _OPERATION_ID_ALIVE))
        return SendAppControl(appId, _OPERATION_ID_TERMINATED);

    return false;
}

// ------- Public Methods -------

AppManagerService::~AppManagerService(void)
{
}

AppManagerService*
AppManagerService::CreateInstance(App* app)
{
    // Create the instance through the constructor.
    if (DBG_APPMGRSVC)
        AppLog("=== AppManagerService CreateInstance ===\n");
    AppManagerService *appMgrService = new AppManagerService(app);
    if (IsFailed(appMgrService->Construct(String("AppManagerService")))) {
        AppLogException("=== AppManagerService could NOT be constructed!");
    }
    return appMgrService;
}

int AppManagerService::GetServiceId()
{
    return TIZEN_SERVICE_APP;
}

bool AppManagerService::OnStart(void)
{
    if (DBG_APPMGRSVC)
        AppLog("=== AppManagerService::onStart === %#x", (int32_t) pthread_self());

    appManager = new AclAppManager();
    bool b = appManager->Start();
    if (DBG_APPMGRSVC)
        AppLog("=== Starting AppManagerService, success = %d ===", b);

    mIsInstalling = false;
    mCurrentInstallation = Installation(NORMAL_PRIORITY, "");

    PowerManager::AddScreenEventListener(*this);

    return b;
}

void AppManagerService::OnStop()
{
    if (DBG_APPMGRSVC)
        AppLog("=== Stopping AppManagerService ===");

    PowerManager::RemoveScreenEventListener(*this);

    appManager->Quit();
    delete appManager;
}

void AppManagerService::OnAppControlCompleteResponseReceived(const AppId& appId, const String& operationId,
        AppCtrlResult appControlResult, const IMap* pExtraData)
{
    // This method is invoked when an application control callback event occurs
    if (appId.Equals(String(_APP_ID_TIZEN_GALLERY)) && operationId.Equals(String(_OPERATION_ID_TIZEN_PICK))) {

        handleGalleryPickResponse(appControlResult, pExtraData);
    }

    if (appId.Equals(String(_APP_ID_TIZEN_FILEMANAGER)) && operationId.Equals(String(_OPERATION_ID_TIZEN_PICK))) {
        handleMusicPickResponse(appControlResult, pExtraData);
    }
}

// Called by AclService
void AppManagerService::OnPackageInstallationCompleted(const PackageId& packageId, bool processNow)
{
    if (AddOnePendingInstallation(HIGH_PRIORITY, packageId)) {
        AppLog("AddOnePendingInstallation PackageId=%ls Priority=100", packageId.GetPointer());

        if (processNow)
            ProcessOnePendingInstallation();
    }
}

// Called by AclService
void AppManagerService::OnPackageUninstallationCompleted(const PackageId& packageId, bool processNow)
{
    // add it to the pending list
    Installation ins(HIGH_PRIORITY, packageId, true);
    mPendingInstallations.push(ins);

    if (processNow)
        ProcessOnePendingInstallation();
}

void AppManagerService::OnScreenOn(void)
{
    // And send it to Android
    AclCommand android;
    android.Construct(ACT_SCREEN);
    android.AddBoolean(true);
    SendAclCommand(android);
}

void AppManagerService::OnScreenOff(void)
{
    AclCommand android;
    android.Construct(ACT_SCREEN);
    android.AddBoolean(false);
    SendAclCommand(android);
}

bool AppManagerService::SendToAndroid(String &appId, int action)
{
    return appManager->SendToAndroid(appId, action);
}

bool AppManagerService::IsTaskRunning(String &appId)
{
    return appManager->IsTaskRunning(appId);
}

bool AppManagerService::IsInstalling(String &appId)
{
    // Get the intent corresponding to the appId
    String activity = appManager->GetLaunchActivity(appId);
    if (activity.IsEmpty() && mIsInstalling) {
        // Lets go through the pending installations
        String packageId = PackageManager::GetPackageIdByAppId(appId);

        if (mCurrentInstallation.mPackageId.Equals(packageId)) {
            AppLog("Found current installation!");
            return true;
        }

        std::vector<Installation> v = mPendingInstallations.impl();
        for (unsigned int i = 0; i < v.size(); i++) {
            if (v[i].mPackageId.Equals(packageId)) {
                AppLog("Found pending installation!");
                return true;
            }
        }
    }

    return false;
}

String* AppManagerService::GetAppNameN(const char* packageName)
{
    // Get the application name corresponding to the launch activity
    String androidPackage(packageName);
    String appName;
    String appId = appManager->GetAppId(androidPackage);

    if (!appId.IsEmpty()) {
        // First the easy way
        wchar_t c = 0;

        if (appId.GetLength() > 10) {
            appId.GetCharAt(10, c);
        }

        if (c == L'.') {
            appId.SubString(11, appName);
        }
        else {
            int periodIndex;

            if (!IsFailed(appId.IndexOf(L'.', 0, periodIndex))) {
                appId.SubString(periodIndex + 1, appName);
            }
        }
    }

    return new String(appName);
}

// ------- Protected Methods -------

void AppManagerService::OnAclCommandReceived(AclCommand& aclCommand)
{
    char* packageName = null;
    char* label = null;
    char* version = null;
    char* activity = null;
    char *statusCode = 0;
    HashMap *extradata = null;
    int brightness = -1;
    String appId;
    String* androidPackage;

    bool installDone = false;
    mDimmingWakelock = false;

    switch (aclCommand.GetId()) {
    case CMD_PACKAGE_DEL:
        packageName = aclCommand.GetCharPN(0);
        statusCode = aclCommand.GetCharPN(1);

        UninstallApplication(packageName);
        installDone = true;
        break;

    case CMD_PACKAGE_ADD:
    case CMD_PACKAGE_UPD:
        packageName = aclCommand.GetCharPN(0);
        statusCode = aclCommand.GetCharPN(1);
        label = aclCommand.GetCharPN(2);
        version = aclCommand.GetCharPN(3);
        activity = aclCommand.GetCharPN(4);

        InstallApplication(packageName, label, version, activity, atoi(statusCode));
        installDone = true;
        break;

    case CMD_PACKAGE_FAIL:
        packageName = aclCommand.GetCharPN(0);
        statusCode = aclCommand.GetCharPN(1);

        AppLog("According to ANDROID, the pkg %s failed to install (%s).", packageName, statusCode);
        FailApplication(packageName, atoi(statusCode), NULL);
        installDone = true;
        break;

    case CMD_INIT_PENDING_INSTALLATION:
        // Since this is sent at Android boot complete, let's make sure we reset our state
        InitPendingInstallation();
        installDone = true;
        break;

    case CMD_RESUME_APP:
        androidPackage = aclCommand.GetStringN(0);
        mResumedAppId = appManager->GetAppId(*androidPackage);
        // Start the app if it isn't already running, or pop it to the foreground
        SendAppControl(mResumedAppId, _OPERATION_ID_RESUMED);

        if (mNeedWakelock && !mResumedAppId.IsEmpty()) {
            if (mDimmingWakelock) {
                extradata = new HashMap(NoOpDeleter);
                extradata->Construct();
                extradata->Add(new String(_KEY_SCREEN_AWAKE), new String(_VALUE_DIM));
            }

            SendMessage(mResumedAppId, _OPERATION_ID_WAKELOCK_ACQUIRE, extradata);
            mNeedWakelock = false;
        }
        break;

    case CMD_PAUSE_APP:
        androidPackage = aclCommand.GetStringN(0);
        appId = appManager->GetAppId(*androidPackage);
        // Normally this would pause the app.  But we can't just let the window go away
        // Instead wait for the CMD_LAUNCHER_APP
        SendMessage(appId, _OPERATION_ID_PAUSED);

        if (appId.Equals(mResumedAppId))
            mResumedAppId.Clear();

        mLastPausedAppId.Clear();
        mLastBrightness = -2;

        if (!appId.IsEmpty())
            mLastPausedAppId = appId;

        break;

    case CMD_LAUNCHER_APP:
        // Using mLastPausedAppId instead of mResumedAppId since mResumedAppId is
        // cleared in CMD_PAUSE_APP
        // Under normal Resume/Pause sequence, the one currently on top is the
        // the one that was last Paused.
        SendMessage(mLastPausedAppId, _OPERATION_ID_HIDDEN);
        break;

    case CMD_TERMINATE_APP:
        androidPackage = aclCommand.GetStringN(0);
        appId = appManager->GetAppId(*androidPackage);
        // This could either be an activity destroy or application terminate; either way send it...
        AppLog("According to ANDROID we should TERMINATE %ls (%ls)", appId.GetPointer(), androidPackage->GetPointer());
        SendTerminate(appId);
        break;

    case CMD_LAUNCH_FAILED:
        androidPackage = aclCommand.GetStringN(0);
        appId = appManager->GetAppId(*androidPackage);
        // The Android package is missing
        AppLog("App launch failed: %ls", appId.GetPointer());
        extradata = new HashMap(NoOpDeleter);
        extradata->Construct();
        extradata->Add(new String(_KEY_RESULT), new String(_VALUE_FAILED));
        SendMessage(appId, _OPERATION_ID_LAUNCH_ME, extradata);
        break;

    case CMD_ACQUIRE_DIMWAKELOCK:
        extradata = new HashMap(NoOpDeleter);
        extradata->Construct();
        extradata->Add(new String(_KEY_SCREEN_AWAKE), new String(_VALUE_DIM));
        mDimmingWakelock = true;
        /* no break */

    case CMD_ACQUIRE_WAKELOCK:
        if (mResumedAppId.IsEmpty() || !SendMessage(mResumedAppId, _OPERATION_ID_WAKELOCK_ACQUIRE, extradata))
            mNeedWakelock = true;
        break;

    case CMD_RELEASE_WAKELOCK:
        SendMessage(mResumedAppId, _OPERATION_ID_WAKELOCK_RELEASE);
        mNeedWakelock = false;
        break;

    case CMD_SET_APP_SCREEN_BRIGHTNESS:
        brightness = aclCommand.GetInt(0);

        if (aclCommand.GetLastResult() == E_SUCCESS && !mResumedAppId.IsEmpty()) {
            int tizenBrightness = brightness == -1 ? -1 : (brightness * 10) / 255;

            // Avoid turning the screen off...
            if (tizenBrightness != 0 && mLastBrightness != tizenBrightness) {
                extradata = new HashMap(NoOpDeleter);
                extradata->Construct();
                extradata->Add(new String(_KEY_BRIGHTNESS), new String(Integer::ToString(brightness)));
                SendMessage(mResumedAppId, _OPERATION_ID_SET_BRIGHTNESS, extradata);
                mLastBrightness = tizenBrightness;
            }
        }
        break;

    case CMD_BROWSE_URI:
        handleBrowseUri(aclCommand);
        break;

    case CMD_PHONE_CALL:
        handlePhoneCall(aclCommand);
        break;

    case CMD_PHONE_DIAL:
        handlePhoneDial(aclCommand);
        break;

    case CMD_EMAIL_SEND:
        handleEmailSend(aclCommand);
        break;

    case CMD_MESSAGE_COMPOSE:
        handleMessageCompose(aclCommand);
        break;

    case CMD_VIEW_IMAGEVIEWER:
        handleImageViewerView(aclCommand);
        break;

    case CMD_GALLERY_PICK: {
        handleGalleryPickRequest(aclCommand);
        break;
    }

    case CMD_VIDEO_PLAY:
        handleVideoPlay(aclCommand);
        break;

    case CMD_VIEW_MUSICPLAYER:
        handleMusicView(aclCommand);
        break;

    case CMD_SETTINGS_LAUNCH: {
        String *setting = aclCommand.GetStringN(0);
        if (setting == 0) {
            break;
        }
        if (setting->Equals(String(L"location"))) {
            handleLocationSettingsLaunch(aclCommand);
        }
        else {
            handleSettingsLaunch(aclCommand);
        }
        delete setting;
        break;
    }
    case CMD_LAUNCH_TIZEN_STORE:
        launchTizenStore();
        break;

    case CMD_CREATE_SHORTCUT:
        createShortcut(aclCommand);
        break;

    case CMD_FILEMANAGER_PICK:
        handleMusicPickRequest(aclCommand);
        break;

    case CMD_MAPS_LAUNCH:
        handleMapsLaunch(aclCommand);
        break;

    default:
        AppLog("Received unknown command from Android: %#x", aclCommand.GetId());
        break;
    }

    if (installDone) {
        // Installation is done
        mIsInstalling = false;
        mCurrentInstallation = Installation(NORMAL_PRIORITY, "");
        AppLog("OnAclCommandReceived %#x, %d installations left...", aclCommand.GetId(), mPendingInstallations.size());

        // New Installations to handle?
        ProcessOnePendingInstallation();
    }

    delete packageName;
    delete label;
    delete version;
    delete activity;
    delete extradata;
}

// ------- Private Methods -------

/*****************************************************************************************
 * FailApplication()
 * Params:      pkgName, failCode, hash
 * Description: marks a package as failed to install (with code) and hash to identify a 
 *              different version of the apk/
 * Return:      void
 *****************************************************************************************/
void AppManagerService::FailApplication(const char* pkgName, int failCode, const char* hash)
{
    Database* pAclDatabase;
    String apkName(pkgName);

    //Access the ACL database
    pAclDatabase = AclAppManagerUtils::OpenDatabase(DB_READWRITE);

    if (pAclDatabase == NULL) {
        AppLog(" ==== Unable to access the ACL Application Database ===\n");
        return;
    }

    result r = AclAppManagerUtils::UpdateApkInstallStatus(pAclDatabase, apkName, failCode, true);

    if (IsFailed(r)) {
        AppLog(" ==== Unable to FAIL the package:  %s  ===\n", pkgName);
    }

    if (pAclDatabase != null) {
        delete pAclDatabase;
    }
}

/*****************************************************************************************
 * InstallApplication()
 * Params:      pkgName, label, version, activity, statusCode
 * Description: uninstalls the given package
 * Return:      void
 *****************************************************************************************/
void AppManagerService::InstallApplication(const char* pkgName, const char* label, const char* version, const char* activity,
        int statusCode)
{
    DbEnumerator* pEnum = null;
    Database* pAclDatabase;
    char query[MAX_PATH_LEN] = { 0 };

    //Access the ACL database
    pAclDatabase = AclAppManagerUtils::OpenDatabase(DB_READWRITE);

    if (pAclDatabase == NULL)
        return;

    memset(query, 0, sizeof(query));
    snprintf(query, MAX_PATH_LEN, "SELECT * FROM android_app_info WHERE PackageName = '%s'", pkgName);
    String sqlRequest(query);
    pEnum = pAclDatabase->QueryN(sqlRequest);

    if (null == pEnum) {
        if (DBG_APPMANAGER)
            AppLog(" ==== This Package: %s doesn't exist! ===\n", pkgName);
    }
    else {
        if (DBG_APPMANAGER)
            AppLog("Package already exists! Update info, package is: %s....\n", pkgName);

        //Update package info in database
        while (pEnum->MoveNext() == E_SUCCESS) {
            String apkName;
            String pkgActivity;
            String androidPkgName;

            pEnum->GetStringAt(APPDB_APK_NAME_COLUMN, apkName);
            pEnum->GetStringAt(APPDB_PKG_NAME_COLUMN, androidPkgName);
            pEnum->GetStringAt(APPDB_ACTIVITY_COLUMN, pkgActivity);

            if (DBG_APPMANAGER)
                AppLog(
                        "===== Got from database, android Pkg Name is: %ls, Activity: %ls =====\n", androidPkgName.GetPointer(), pkgActivity.GetPointer());

            // Remove apk file if it exists
            String androidApk = App::GetInstance()->GetAppDataPath() + ANDROID_TMP_DIR + apkName;

            // remove Apk file
            if (File::IsFileExist(androidApk)) {
                AppLog("=== remove file: %ls... ===\n", androidApk.GetPointer());
                File::Remove(androidApk);
            }

            if (pkgActivity.IsEmpty()) {
                //Update the database
                if (DBG_APPMANAGER)
                    AppLog(
                            "Updating the app activity and pkgName, new activity=%s, new pkgName=%ls\n", activity, androidPkgName.GetPointer());
                AclAppManagerUtils::UpdateDatabase(pAclDatabase, androidPkgName, activity);
            }
            else {
                if (DBG_APPMANAGER)
                    AppLog(" All info is available, skipping...\n");
            }
            AclAppManagerUtils::UpdateApkInstallStatus(pAclDatabase, apkName, statusCode, true);
        }
    }

    if (pEnum != null) {
        delete pEnum;
    }
    if (pAclDatabase != null) {
        delete pAclDatabase;
    }
}

/*****************************************************************************************
 * UninstallApplication()
 * Params:      pkgName
 * Description: uninstalls the given package
 * Return:      void
 *****************************************************************************************/
void AppManagerService::UninstallApplication(const char* pkgName)
{
    DbEnumerator* pEnum;
    Database* pAclDatabase;

    //Access the ACL database
    pAclDatabase = AclAppManagerUtils::OpenDatabase(DB_READWRITE);
    if (pAclDatabase) {
        String packageName = pkgName;
        pEnum = AclAppManagerUtils::GetAppDbRow(pAclDatabase, APPDB_PKG_NAME_COLUMN, packageName);

        if (pEnum) {
            AppLog(" ==== Package exists so uninstall it ====\n");
            while (pEnum->MoveNext() == E_SUCCESS) {
                //Get the  package ID  and activity of this row
                String activity;
                String packageID;
                pEnum->GetStringAt(APPDB_ACTIVITY_COLUMN, activity);
                pEnum->GetStringAt(APPDB_PKGID_COLUMN, packageID);

                if (activity.IsEmpty()) {
                    AppLog("package has no activity, ignoring message...\n");
                }
                else {
                    AppLog(" Uninstalling Application: %ls ...\n", packageID.GetPointer());
                    //TODO: check if this call fails...
                    AclAppManagerUtils::RemoveFromDatabase(pAclDatabase, packageID);
                }
            }

            delete pEnum;
            pEnum = null;
        }
        else {
            AppLog(" Package: %s doesn't exist so ignore it ===\n", pkgName);
        }

        delete pAclDatabase;
        pAclDatabase = null;
    }
}

void AppManagerService::handleBrowseUri(AclCommand& aclCommand)
{
    int commandId = ACT_CANCELED; // default to failure
    String *uri = aclCommand.GetStringN(0);

    if (uri != null) {
        if (SendAppControl(_APP_ID_TIZEN_INTERNET, _OPERATION_ID_TIZEN_VIEW, uri))
            commandId = ACT_SUCCESS;

        delete uri;
    }

    SendAclCommand(commandId);
}

bool AppManagerService::ParseKeyValuePair(const String& str, wchar_t delim, String& key, String& value)
{
    int index;

    if (str.IndexOf(delim, 0, index) == E_SUCCESS) {
        key = "";
        value = "";

        str.SubString(0, index, key);
        // Note that the value could be an empty string
        // if delimiter is the last character in the input string
        // It is the caller's responsibility to check for the return values
        str.SubString(index + 1, value);

        return true;
    }

    return false;
}

void AppManagerService::handlePhoneCall(AclCommand& aclCommand)
{
    int commandId = ACT_CANCELED; // default to failure
    String *uri = aclCommand.GetStringN(0);

    if (uri != null) {
        // Even though the document says "voice" is default for type, specifying it
        // just in case Tizen decides to change things later on
        HashMap extraData;
        extraData.Construct();
        String typeKey = _KEY_CALL_TYPE;
        String typeVal = _VALUE_VOICE;
        extraData.Add(&typeKey, &typeVal);

        if (SendAppControl(_APP_ID_TIZEN_CALL, _OPERATION_ID_TIZEN_CALL, uri, null, &extraData))
            commandId = ACT_SUCCESS;

        delete uri;
    }

    SendAclCommand(commandId);
}

void AppManagerService::handlePhoneDial(AclCommand& aclCommand)
{
    int commandId = ACT_CANCELED; // default to failure
    String *uri = aclCommand.GetStringN(0);

    if (uri != null) {
        if (SendAppControl(_APP_ID_TIZEN_PHONE, _OPERATION_ID_TIZEN_DIAL, uri))
            commandId = ACT_SUCCESS;

        delete uri;
    }

    SendAclCommand(commandId);
}

void AppManagerService::handleSettingsLaunch(AclCommand& aclCommand)
{
    int commandId = ACT_CANCELED;
    AppId appId = _APP_ID_COM_SAMSUNG_SETTINGS;
    AppManager* pAppManager = AppManager::GetInstance();
    result res = pAppManager->LaunchApplication(appId, AppManager::LAUNCH_OPTION_DEFAULT);
    if (res == E_SUCCESS)
        commandId = ACT_SUCCESS;
    SendAclCommand(commandId);
}

void AppManagerService::handleLocationSettingsLaunch(AclCommand& aclCommand)
{
    int commandId = ACT_CANCELED;
    AppControl* pAc = AppManager::FindAppControlN(_APP_ID_TIZEN_SETTINGS, _OPERATION_ID_TIZEN_CONFIGURE_LOCATION);
    if(pAc)
    {
        result res = pAc->Start(null, null, null, null);
        if (res == E_SUCCESS)
            commandId = ACT_SUCCESS;
        delete pAc;
    }
    SendAclCommand(commandId);
}

void AppManagerService::handleMessageCompose(AclCommand& aclCommand)
{
    HashMap extraData(SingleObjectDeleter);
    extraData.Construct();
    int argCount = aclCommand.GetArgumentCount();
    if (argCount > 0) {
        for (int i = 0; i < argCount; i++) {
            String *arg = aclCommand.GetStringN(i);

            if (arg != null) {
                // Creates a StringTokenizer instance
                String fieldKey;
                String fieldValue;

                if (ParseKeyValuePair(*arg, L':', fieldKey, fieldValue)) {
                    if (!fieldKey.IsEmpty() && !fieldValue.IsEmpty()) {
                        String controlKey;

                        if (String(fieldKey).Equals(String(L"to"))) {
                            controlKey = _KEY_TO;
                        }
                        else if (String(fieldKey).Equals(String(L"subject"))) {
                            controlKey = _KEY_SUBJECT;
                        }
                        else if (String(fieldKey).Equals(String(L"body"))) {
                            controlKey = _KEY_TEXT;
                        }
                        else if (String(fieldKey).Equals(String(L"attach"))) {
                            controlKey = _KEY_PATH;
                        }
                        else if (String(fieldKey).Equals(String(L"messagetype"))) {
                            controlKey = _KEY_MESSAGE_TYPE;
                        }
                        else {
                            AppLog("AclLauncher: Unrecognized key = %S", fieldKey.GetPointer());
                        }

                        if (!controlKey.IsEmpty()) {
                            if (controlKey.Equals(String(_KEY_PATH))) {
                                StringTokenizer tokenizer(fieldValue, String(L"|"));
                                String attachVal;
                                ArrayList *attachList = new ArrayList(SingleObjectDeleter);
                                attachList->Construct();

                                while (tokenizer.HasMoreTokens()) {
                                    tokenizer.GetNextToken(attachVal);

                                    if (!attachVal.IsEmpty()) {
                                        AppLog("AclService: Attaching File %s", attachVal.GetPointer());
                                        attachList->Add(new String(attachVal));
                                    }
                                }

                                if (attachList->GetCount() > 0) {
                                    extraData.Add(new String(controlKey), attachList);
                                }
                                else {
                                    delete attachList;
                                }
                            }
                            else {
                                extraData.Add(new String(controlKey), new String(fieldValue));
                            }
                        }
                    }
                }

                delete arg;
            }
        }
    }

    int commandId = ACT_CANCELED; // default to failure

    if (SendAppControl(_APP_ID_TIZEN_MESSAGES, _OPERATION_ID_TIZEN_COMPOSE, null, null, &extraData))
        commandId = ACT_SUCCESS;

    SendAclCommand(commandId);
}

void AppManagerService::handleEmailSend(AclCommand& aclCommand)
{
    HashMap extraData(SingleObjectDeleter);
    extraData.Construct();
    int argCount = aclCommand.GetArgumentCount();

    if (argCount > 0) {
        for (int i = 0; i < argCount; i++) {
            String *arg = aclCommand.GetStringN(i);

            if (arg != null) {
                String fieldKey;
                String fieldValue;

                if (ParseKeyValuePair(*arg, L':', fieldKey, fieldValue)) {
                    if (!fieldKey.IsEmpty() && !fieldValue.IsEmpty()) {
                        String controlKey;

                        if (String(fieldKey).Equals(String(L"to"))) {
                            controlKey = _KEY_TO;
                        }
                        else if (String(fieldKey).Equals(String(L"cc"))) {
                            controlKey = _KEY_CC;
                        }
                        else if (String(fieldKey).Equals(String(L"bcc"))) {
                            controlKey = _KEY_BCC;
                        }
                        else if (String(fieldKey).Equals(String(L"subject"))) {
                            controlKey = _KEY_SUBJECT;
                        }
                        else if (String(fieldKey).Equals(String(L"body"))) {
                            controlKey = _KEY_TEXT;
                        }
                        else if (String(fieldKey).Equals(String(L"attach"))) {
                            controlKey = _KEY_PATH;
                        }
                        else {
                            // error
                        }

                        if (!controlKey.IsEmpty()) {
                            if (controlKey.Equals(String(_KEY_PATH))) {
                                StringTokenizer tokenizer(fieldValue, String(L"|"));
                                String attachVal;
                                ArrayList *attachList = new ArrayList(SingleObjectDeleter);
                                attachList->Construct();

                                while (tokenizer.HasMoreTokens()) {
                                    tokenizer.GetNextToken(attachVal);

                                    if (!attachVal.IsEmpty()) {
                                        AppLog("AclService: Attaching File %s", attachVal.GetPointer());
                                        attachList->Add(new String(attachVal));
                                    }
                                }

                                if (attachList->GetCount() > 0) {
                                    extraData.Add(new String(controlKey), attachList);
                                }
                                else {
                                    delete attachList;
                                }
                            }
                            else {
                                extraData.Add(new String(controlKey), new String(fieldValue));
                            }
                        }
                    }
                }

                delete arg;
            }
        }
    }

    int commandId = ACT_CANCELED; // default to failure

    if (SendAppControl(_APP_ID_TIZEN_EMAIL, _OPERATION_ID_TIZEN_COMPOSE, null, null, &extraData))
        commandId = ACT_SUCCESS;

    SendAclCommand(commandId);
}

void AppManagerService::handleImageViewerView(AclCommand& aclCommand)
{
    int commandId = ACT_CANCELED;
    String *viewerFile = aclCommand.GetStringN(0);
    String uri = String(_SCHEME_FILE) + *viewerFile;

    if (viewerFile != null) {
        if (SendAppControl(_APP_ID_TIZEN_IMAGEVIEWER, _OPERATION_ID_TIZEN_VIEW, &uri))
            commandId = ACT_SUCCESS;

        delete viewerFile;
    }
    else {
        AppLog("aclCommand get string error!");
    }

    SendAclCommand(commandId);
}

void AppManagerService::handleGalleryPickRequest(AclCommand& aclCommand)
{
    AppControl *appControlP = AppManager::FindAppControlN(_APP_ID_TIZEN_GALLERY, _OPERATION_ID_TIZEN_PICK);

    if (appControlP) {
        String *type = aclCommand.GetStringN(0);
        String mime = _MIME_IMAGE_STAR;

        if (type != null) {
            mime = *type;
            delete type;
        }

        AppLog("Type received = %S, mime type sent = %S", type->GetPointer(), mime.GetPointer());
        String selectKey = _KEY_SELECTION_MODE;
        String selectVal = _VALUE_SINGLE;
        HashMap extraData;
        extraData.Construct();
        extraData.Add(&selectKey, &selectVal);

        appControlP->Start(null, &mime, &extraData, this);

        delete appControlP;
    }
    else {
        AclCommand aclCommand;
        aclCommand.Construct(ACT_CANCELED);
        SendAclCommand(aclCommand);
    }
}

void AppManagerService::handleGalleryPickResponse(AppCtrlResult appControlResult, const IMap* pExtraData)
{
    bool success = false;
    AclCommand aclCommand;

    if (appControlResult == APP_CTRL_RESULT_SUCCEEDED) {
        // Use the selected media paths
        if (pExtraData) {
            IList* pValueList = const_cast<IList*>(dynamic_cast<const IList*>(pExtraData->GetValue(String(_KEY_SELECTED))));

            // Android API 10 does not support multiple selection from the Gallery Intent
            // So for now we will only return the first file
            if (pValueList && pValueList->GetCount() > 0) {
                String* pValue = dynamic_cast<String*>(pValueList->GetAt(0));
                aclCommand.Construct(ACT_PICK_RESULT);
                aclCommand.AddString(*pValue);
                success = true;

            }
        }
    }

    if (!success) {
        aclCommand.Construct(ACT_CANCELED);
    }

    SendAclCommand(aclCommand);
    AppLog("Sending Acl Action: %#x", aclCommand.GetId());
}

void AppManagerService::handleVideoPlay(AclCommand& aclCommand)
{
    int commandId = ACT_CANCELED; //default to failure
    String *videoFile = aclCommand.GetStringN(0);
    String uri = String(_SCHEME_FILE) + *videoFile;

    if (videoFile != null) {
        if (SendAppControl(_APP_ID_TIZEN_VIDEOPLAYER, _OPERATION_ID_TIZEN_VIEW, &uri))
            commandId = ACT_SUCCESS;

        delete videoFile;
    }

    SendAclCommand(commandId);
}

void AppManagerService::handleMusicView(AclCommand& aclCommand)
{
    int commandId = ACT_CANCELED; //default to failure
    String *musicFile = aclCommand.GetStringN(0);
    String uri = String(_SCHEME_FILE) + *musicFile;

    if (musicFile != null) {
        if (SendAppControl(_APP_ID_TIZEN_MUSICPLAYER, _OPERATION_ID_TIZEN_VIEW, &uri))
            commandId = ACT_SUCCESS;

        delete musicFile;
    }

    SendAclCommand(commandId);
}

void AppManagerService::launchTizenStore()
{
    HashMap extraData;
    String reqKey(_KEY_REQUEST_DATA);
    String reqValue(_VALUE_STORE_MAIN_PAGE);

    extraData.Construct();
    extraData.Add(&reqKey, &reqValue);

    int commandId = ACT_CANCELED; //default to failure

    if (SendAppControl(_TIZEN_STORE_ID, _OPERATION_ID_TIZEN_STORE_VIEW, null, null, &extraData)) {
        commandId = ACT_SUCCESS;
    }

    SendAclCommand(commandId);
}

void AppManagerService::handleMapsLaunch(AclCommand& aclCommand)
{
    String *query = aclCommand.GetStringN(0);

    int commandId = ACT_CANCELED;

    if (AppControl::FindAndStart(_OPERATION_ID_TIZEN_VIEW, query, null, null, null, null) == E_SUCCESS)
        commandId = ACT_SUCCESS;

    if (query != null)
        delete query;

    SendAclCommand(commandId);
}

void AppManagerService::handleMusicPickRequest(AclCommand& aclCommand)
{
    bool success = false;
    AppControl* appControlP = AppManager::FindAppControlN(_APP_ID_TIZEN_FILEMANAGER, _OPERATION_ID_TIZEN_PICK);

    if (appControlP) {
        String *type = aclCommand.GetStringN(0);
        String mime = _MIME_AUDIO_STAR;

        if (type != null) {
            mime = *type;
            delete type;
        }

        AppLogIf(DBG_APPMGRSVC, "Type received = %S, mime type sent = %S", type->GetPointer(), mime.GetPointer());

        HashMap extraData;
        extraData.Construct();
        String selectKey = _KEY_SELECTION_MODE;
        String selectVal = _VALUE_MULTIPLE;
        extraData.Add(&selectKey, &selectVal);

        success = appControlP->Start(null, &mime, &extraData, this) == E_SUCCESS;

        delete appControlP;
    }

    if (!success) {
       AclCommand aclCommand;
       aclCommand.Construct(ACT_CANCELED);
       SendAclCommand(aclCommand);
    }
}


void AppManagerService::handleMusicPickResponse(AppCtrlResult appControlResult, const IMap* pExtraData)
{
    bool success = false;
    AclCommand aclCommand;

    if (appControlResult  == APP_CTRL_RESULT_SUCCEEDED) {
        if (pExtraData) {
            IList* pValueList = const_cast< IList* >(dynamic_cast< const IList* >(pExtraData->GetValue(String(_KEY_SELECTED))));

            if (pValueList && pValueList->GetCount() > 0) {
                int count = pValueList->GetCount();

                for (int i = 0; i < count; i++) {
                    String* pValue = dynamic_cast< String* >(pValueList->GetAt(i));
                    aclCommand.Construct(ACT_MUSIC_PICKED);
                    aclCommand.AddString(*pValue);
                    success = true;
                }
            }
        }
    }

    if (!success) {
        aclCommand.Construct(ACT_CANCELED);
    }

    SendAclCommand(aclCommand);
    AppLogIf(DBG_APPMGRSVC, "Sending Acl Action: %#x", aclCommand.GetId());
}

void AppManagerService::createShortcut(AclCommand& aclCommand)
{
    String *title = aclCommand.GetStringN(0);
    String *androidPkgName = aclCommand.GetStringN(1);
    String *png = aclCommand.GetStringN(2);
    String *uri = aclCommand.GetStringN(3);
    bool allowDuplicate = aclCommand.GetBool(4);

    Database *db = AclAppManagerUtils::OpenDatabase(DB_READWRITE);
    if (!db) {
        AppLogException("Could not get database handle");
        return;
    }

    DbEnumerator* pEnum = AclAppManagerUtils::GetAppDbRow(db, APPDB_PKG_NAME_COLUMN, *androidPkgName);
    if (!pEnum) {
        AppLogException("Could not get package name for %ls", androidPkgName->GetPointer());
        delete db;
        return;
    }

    String appId;
    result r = pEnum->MoveNext();
    if (IsFailed(r)) {
        delete db;
        AppLogException("Could not find package name for %ls, result = %d", androidPkgName->GetPointer(), r);
        return;
    }

    pEnum->GetStringAt(APPDB_APPID_COLUMN, appId);

    int shortcutId = 0;
    r = AclAppManagerUtils::AddShortcut(db, appId, *title, *uri, shortcutId);
    delete db;

    if (IsFailed(r)) {
        AppLogException("AddShortcut() failed, shortcutId = %d, ret_val = %d", shortcutId, r);
        return;
    }

    String shortcutUri;
    shortcutUri.Format(32, L"%ls%d", ACL_SHORTCUT_URI.GetPointer(), shortcutId);

    AppLogIf(DBG_SHORTCUTS,
             "createShortcut() (%ls), (%ls), (%ls), (%ls), (%s)",
             title->GetPointer(),
             shortcutUri.GetPointer(),
             png->GetPointer(),
             uri->GetPointer(),
             allowDuplicate ? "duplicates allowed" : "duplicates not allowed");

    AppLogIf(DBG_SHORTCUTS, "androidPkgName (%ls), appId (%ls)",
             androidPkgName->GetPointer(),
             appId.GetPointer());

    r = ShortcutManager::GetInstance()->AddShortcut(appId, *title, *png, shortcutUri, allowDuplicate);
    if (IsFailed(r)) {
        AppLogException("Cannot create shortcut %ls: %s",
                        title->GetPointer(),
                        GetErrorMessage(r));
        AclAppManagerUtils::RemoveShortcutFromDatabase(db, shortcutId);
    } else
        AppLogIf(DBG_SHORTCUTS, "Shortcut %ls created", title->GetPointer());

    delete title;
    delete androidPkgName;
    delete png;
    delete uri;
}

/*****************************************************************************************
 * InitPendingInstallation
 * Params:      none
 * Description: Collect all the zombie tizen apps
 * Return:      void
 *****************************************************************************************/
void AppManagerService::InitPendingInstallation()
{
    IList* pPackageInfoList = mPkgManager->GetPackageInfoListN();
    if (pPackageInfoList == null) {
        AppLog("Unable to get a list of packages");
        return;
    }

    int packageInfoCount = pPackageInfoList->GetCount();
    AppLog("PackageInfoCount total on device: %d", packageInfoCount);

    HashMap installedMap;
    installedMap.Construct();
    GetListOfApps(installedMap);

    for (int i = 0; i < packageInfoCount; i++) {
        PackageInfo* pPackageInfo = static_cast<PackageInfo*>(pPackageInfoList->GetAt(i));
        String packageId = pPackageInfo->GetId();

        // Only add it if it hasn't been installed yet
        String appId = pPackageInfo->GetMainAppId();
        String activity = appManager->GetLaunchActivity(appId);

        if (activity.IsEmpty()) {
            AddOnePendingInstallation(NORMAL_PRIORITY, packageId);
        }
        // This is an confirmed existing installation, so remove it from the list
        IMapEnumerator* mapEnum = installedMap.GetMapEnumeratorN();
        while (mapEnum->MoveNext() == E_SUCCESS) {
            String* value = static_cast<String*>(mapEnum->GetValue());
            if (value->Equals(appId)) {
                String* key = static_cast<String*>(mapEnum->GetKey());
                installedMap.Remove(*key);
            }
        }

        delete (mapEnum);
    }

    // Anything left should be removed in Android
    IMapEnumerator* mapEnum = installedMap.GetMapEnumeratorN();
    while (mapEnum->MoveNext() == E_SUCCESS) {
        String* value = static_cast<String*>(mapEnum->GetValue());
        String packageId;
        value->SubString(0, 10, packageId); // We have to do it this way because the package has been uninstalled

        AppLog("Uninstalling PackageId %ls because no package found!", packageId.GetPointer());
        Installation del(NORMAL_PRIORITY, packageId, true);
        mPendingInstallations.push(del);
    }

    delete (mapEnum);
    AppLog("Scanning apps done. Total count=%d priority=0", mPendingInstallations.size());
}

/*****************************************************************************************
 * ProcessOnePendingInstallation()
 * Params:
 * Description:
 * Return:
 *****************************************************************************************/
void AppManagerService::ProcessOnePendingInstallation()
{
    // Android is installing apk, skip
    if (mIsInstalling || mPendingInstallations.empty()) {
        return;
    }

    Installation inc = mPendingInstallations.top();
    mPendingInstallations.pop();

    if (inc.mUninstall) {
        if (appManager->UninstallationCompleted(inc.mPackageId, true)) {
            mIsInstalling = true;
            mCurrentInstallation = inc;
        }
    }
    else if (appManager->InstallPackage(inc.mPackageId)) {
        // Great, in this state, we will wait for the response form installation complete from android
        mIsInstalling = true;
        mCurrentInstallation = inc;
    }

    if (!mIsInstalling) {
        AppLog("ProcessOnePendingInstallation InstallPackage failed, Set mIsIntalling: %d", mIsInstalling);
        ProcessOnePendingInstallation();
    }
}

/*****************************************************************************************
 * AddOnePendingInstallation
 * Params:
 * Description:
 * Return:
 *****************************************************************************************/
bool AppManagerService::AddOnePendingInstallation(unsigned int priority, const PackageId& packageId)
{
    // PackageInfo class instance
    PackageInfo* pPackageInfo = mPkgManager->GetPackageInfoN(packageId);
    String appId = pPackageInfo->GetMainAppId();
    String apkInfoFile = AppManager::GetAppSharedPath(appId) + INFO_DIR + INFO_FILE;

    //check if apk info file exists
    File infoFile;
    if (!File::IsFileExist(apkInfoFile) || IsFailed(infoFile.Construct(apkInfoFile, "r"))) {
        return false;
    }

    // add it to the pending list
    Installation ins(priority, packageId);
    mPendingInstallations.push(ins);

    return true;
}
