/**
 * Name        : SharedSettingService
 */

#include "SharedSettingService.h"
#include "AclCommand.h"
#include "AppManagerService.h"
#include <fcntl.h>
#include <iostream>
#include <FContent.h>
#include <FIo.h>

#define DBG_SHARED_SETTING	0
#define WALPAPER_FILE   L"ACL/.Wallpaper/wallpaper"

using namespace Tizen::App;
using namespace Tizen::Base;
using namespace Tizen::System;

/*****************************************************************************************
 * SharedSettingService()
 *
 * Params:      void
 *
 * Description: Constructor.
 *
 * Return:
 *****************************************************************************************/
SharedSettingService::SharedSettingService(App* app) :
        AclAbstractService(app), mPendingContent(SingleObjectDeleter)
{
    mVibrator.Construct();
    mTimer.Construct(*this);
    mContentManager.Construct();

    mRepeatLength = 0;
    pRepeatingPattern = null;
    mAndroidWallpaperUpdated = false;
    mAndroidSetsSoundRingtone = false;
    mAndroidSetsSoundNotification = false;
    mLastBrightness = -1;
}

/*****************************************************************************************
 * SharedSettingService()
 *
 * Params:      void
 *
 * Description: Destructor.
 *
 * Return:
 *****************************************************************************************/
SharedSettingService::~SharedSettingService(void)
{
}

/*****************************************************************************************
 * CreateInstance()
 *
 * Params:      void
 *
 * Description: Creating the instance.
 *
 * Return:      true if successful, false if failed
 *****************************************************************************************/
SharedSettingService*
SharedSettingService::CreateInstance(App* app)
{
    // Create the instance through the constructor.
    SharedSettingService *sharedSettingService = new SharedSettingService(app);
    sharedSettingService->Construct(String("SharedSettingService"));
    return sharedSettingService;
}

int SharedSettingService::GetServiceId()
{
    return TIZEN_SERVICE_SETTINGS;
}

bool SharedSettingService::OnStart(void)
{
    AppLog("SharedSettingsService: %#x", (int32_t) pthread_self());

    result r = E_SUCCESS;
    GetLanguage();
    GetTimeZone();
    GetTimeFormat();
    GetFlightMode();
    GetSilentMode();
    GetVibratorMode();
    GetMediaVolume();
    GetRingtoneVolume();
    GetDateTime();
    GetSystemBrightness();
    GetRingtoneSound();
    GetNotificationSound();
    GetGpsUsage();
    GetWpsUsage();

    r = SettingInfo::AddSettingEventListener(*this);
    if (r != E_SUCCESS) {
        AppLogException("Error: AddSettingEventListener failed");
        return false;
    }

    r = mContentManager.AddContentUpdateEventListener(*this);
    if (IsFailed(r)) {
        AppLogException("Error: ContentManager::AddContentUpdateEventListener failed");
        return false;
    } else {
        AppLogIf(DBG_SHARED_SETTING, "OK: added content update even listener success");
    }

    return true;
}

void SharedSettingService::OnStop()
{
    SettingInfo::RemoveSettingEventListener(*this);

    result r = mContentManager.RemoveContentUpdateEventListener(*this);
    if (IsFailed(r)) {
        AppLogException("Error: ContentManager::RemoveContentUpdateEventListener failed");
        return;
    } else {
        AppLogIf(DBG_SHARED_SETTING, "OK: removed content update even listener success");
    }

}

void SharedSettingService::OnAclCommandReceived(AclCommand& aclCommand)
{
    AppLog(" Host side vibration: %#x", aclCommand.GetId());

    switch (aclCommand.GetId()) {
    case CMD_VIBRATOR_ON:
        handleVibratorOn((int) aclCommand.GetLong(0));
        break;

    case CMD_VIBRATORPATTERN_ON:
        handleVibratorOn(aclCommand);
        break;

    case CMD_VIBRATOR_OFF:
        handleVibratorOff();
        break;

    case CMD_SCAN_MEDIA_FILE:
        handleScanMediaFile(aclCommand);
        break;

    case CMD_SET_WALLPAPER:
        handleSetWallpaper(aclCommand);
        break;

    case CMD_SET_RINGTONE:
        handleSetRingtone(aclCommand);
        break;

    case CMD_SET_SYS_SCREEN_BRIGHTNESS:
        handleSetBrightness(aclCommand);
        break;

    case CMD_GET_TPKVERSION:
        handleGetTpkVersion(aclCommand);
        break;

    default:
        AppLogException("Invalid ACL Shared Setting command ID: 0x%08x", aclCommand.GetId());
        break;
    }
}

void SharedSettingService::handleSetWallpaper(AclCommand& command)
{
    if (mAndroidWallpaperUpdated) {
        //Ignoring request since the request is being initiated because we
        //changed the wallpaper file on Android");
        mAndroidWallpaperUpdated = false;

        return;
    }

    String* pFilePath = command.GetStringN(0);
    if (pFilePath) {
        result r = SettingInfo::SetWallpaper(*pFilePath);
        if (IsFailed(r)) {
            AppLogException("Set wallpaper failed: %s", GetErrorMessage(r));
        }

        delete pFilePath;
    } else {
        AppLogException("No wallpaper specified");
    }
}

void SharedSettingService::handleSetRingtone(AclCommand& command)
{
    String* pFilePath = command.GetStringN(0);
    if (pFilePath == 0) {
        AppLogException("No ringtone specified");
        return;
    }
    int type = command.GetInt(1);
    AppLog("command is [%ls, %d]", pFilePath->GetPointer(), type);
    result r;
    if (type == 1) {
        mAndroidSetsSoundRingtone = true;
        //r = SettingInfo::SetRingtone(*pFilePath);
        r = SettingInfo::SetValue(L"http://tizen.org/setting/sound.ringtone", *pFilePath);
        if (IsFailed(r))
            AppLogException("SetValue failed: %s", GetErrorMessage(r));
    } else if (type == 2) {
        mAndroidSetsSoundNotification = true;
        r = SettingInfo::SetValue(L"http://tizen.org/setting/sound.notification", *pFilePath);
        if (IsFailed(r))
            AppLogException("SetValue failed: %s", GetErrorMessage(r));
    } else {
        AppLogException("Setting ringtone of type %d is not supported", type);
    }
    delete pFilePath;
}

void SharedSettingService::handleScanMediaFile(AclCommand& command)
{
    String* pFilePath = command.GetStringN(0);
    if (pFilePath) {
        AppLogIf(DBG_SHARED_SETTING, "scan media file:%ls", pFilePath->GetPointer());
        if (!mPendingContent.Contains(*pFilePath)) {
            AppLogIf(DBG_SHARED_SETTING, "OK: non-pending media, add to pending + scan");
            result r = E_SUCCESS;
            r = mPendingContent.Add(new String(*pFilePath));
            if (IsFailed(r)) {
                AppLogException("Error: unable to add (%ls), result(%s)", pFilePath->GetPointer(), GetErrorMessage(r));
            } else {
                r = Tizen::Content::ContentManager::ScanFile(*pFilePath);
                if (IsFailed (r)) {
                    AppLogException("Error: scan media file failed: %s", GetErrorMessage(r));
                }
            }
        } else {
            AppLogIf(DBG_SHARED_SETTING, "OK: pending media, ignore and remove from pending");
            removePathFromPendingList(*pFilePath);
        }
        delete pFilePath;
    } else {
        AppLogException("null media file path.");
    }
}

void SharedSettingService::handleVibratorOn(int milliSeconds)
{
    IntensityDurationVibrationPattern pattern[] = { { milliSeconds, -1 } };
    AppLog("SIZEOF PATTERN IS %d", sizeof(pattern));
    AppLog("Vibrating for %d milliseconds", milliSeconds);

    ClearRepeating();
    result r = mVibrator.Start(pattern, sizeof(pattern), 1);
    AppLogExceptionIf(IsFailed(r), "Vibrator::Start() failed: %s", GetErrorMessage(r));
}

void SharedSettingService::handleVibratorOn(AclCommand& aclCommand)
{
    int length = aclCommand.GetInt(0);
    int repeatIndex = aclCommand.GetInt(1);

    IntensityDurationVibrationPattern *pNonRepeatingPattern = new IntensityDurationVibrationPattern[length];

    // First create the non-repeating pattern
    int totalTime = 0;
    int intensity = 0; // silent

    // The pattern is an alternating sequence of durations in milliseconds.
    // The first duration is off, the next duration on, etc.
    for (int i = 0; i < length; i++) {
        int duration = (int) aclCommand.GetLong(i + 2); // Pattern starts at position 2
        totalTime += duration;

        pNonRepeatingPattern[i].duration = duration;
        pNonRepeatingPattern[i].intensity = intensity;

        AppLog("Adding item to pattern: duration=%d, intensity=%d", duration, intensity);

        intensity = (intensity == 0 ? -1 : 0);
    }

    // Now create the optional repeating pattern
    pRepeatingPattern = null;

    if (repeatIndex >= 0 && repeatIndex < length) {
        mRepeatLength = length - repeatIndex;
        pRepeatingPattern = new IntensityDurationVibrationPattern[mRepeatLength];
        intensity = ((repeatIndex % 2) == 0 ? 0 : -1); // Even is silent

        for (int i = 0; i < mRepeatLength; i++) {
            int duration = (int) aclCommand.GetLong(i + 2 + repeatIndex);

            pRepeatingPattern[i].duration = duration;
            pRepeatingPattern[i].intensity = intensity;

            intensity = (intensity == 0 ? -1 : 0);
        }
    }

    if (totalTime > 0) {
        mTimer.Cancel();
        AppLog("Starting Vibrator with pattern length %d", length);
        result r = mVibrator.Start(pNonRepeatingPattern, length * sizeof(IntensityDurationVibrationPattern), 1);
        AppLogExceptionIf(IsFailed(r), "Vibrator::Start() failed: %s", GetErrorMessage(r));

        if (pRepeatingPattern)
            mTimer.Start(totalTime);
    }

    delete[] pNonRepeatingPattern;
}

void SharedSettingService::handleVibratorOff(void)
{
    ClearRepeating();
    result r = mVibrator.Stop();
    AppLogExceptionIf(IsFailed(r), "Vibrator::Stop() failed: %s", GetErrorMessage(r));
}

void SharedSettingService::handleSetBrightness(AclCommand& command)
{
    int brightness = command.GetInt(0);
    if (brightness < 0 || brightness > 255) {
        AppLogException("Brightness=%d is out of range", brightness);
        return;
    }

    result r = command.GetLastResult();
    if (r == E_SUCCESS) {
        /* Tizen brightness level ranges from 1-100 https://www.tizen.org/setting */
        int tizenBrightness = brightness == -1 ? -1 : (brightness * 100) / 255;

        if (mLastBrightness != tizenBrightness && tizenBrightness > 0 && tizenBrightness <= 100) {
            result r = E_SUCCESS;
            r = SettingInfo::SetValue(String("http://tizen.org/setting/screen.brightness"), tizenBrightness);

            if (IsFailed (r)) {
                AppLogException("SetScreenBrightness(%d) failed: %s", tizenBrightness, GetErrorMessage(r));
            } else {
                AppLogIf(DBG_SHARED_SETTING, "Brightness=(%d) successfully set", tizenBrightness);
                mLastBrightness = tizenBrightness;
            }
        } else {
            AppLogIf(DBG_SHARED_SETTING, "Brightness=(%d) either 0 or equals mLastBrightness", tizenBrightness);
        }
    } else {
        AppLogException("Unable to get brightness: %s", GetErrorMessage(r));
    }
}

void SharedSettingService::handleGetTpkVersion(AclCommand& command)
{
    AclCommand response;
    response.Construct(ACT_TPKVERSION);

    String* packageName = command.GetStringN(0);

    if (packageName) {
        String appId = AclAppManagement::AclAppManager::GetDBValueByColumn(*packageName, APPDB_PKG_NAME_COLUMN, APPDB_APPID_COLUMN);
        String packageId = Tizen::App::Package::PackageManager::GetPackageIdByAppId(appId);

        if (!packageId.IsEmpty()) {
            Tizen::App::Package::PackageManager* packageManager = Tizen::App::Package::PackageManager::GetInstance();
            Tizen::App::Package::PackageInfo* packageInfo = packageManager->GetPackageInfoN(packageId);

            if (packageInfo) {
                response.AddString(packageInfo->GetVersion());
                delete packageInfo;
            }
        }

        delete packageName;
    }

    SendAclCommand(response);
}

void SharedSettingService::OnTimerExpired(Timer& timer)
{
    if (pRepeatingPattern) {
        mVibrator.Start(pRepeatingPattern, mRepeatLength * sizeof(IntensityDurationVibrationPattern), 100);
        ClearRepeating();
    }
}

void SharedSettingService::ClearRepeating(void)
{
    mTimer.Cancel();

    if (pRepeatingPattern) {
        delete pRepeatingPattern;
        pRepeatingPattern = null;
    }
}

/*****************************************************************************************
 * GetLanguage()
 *
 * Params:      void
 *
 * Description: Get the locale value
 *
 * Return:      true if successful, false if failed
 *****************************************************************************************/
bool SharedSettingService::GetLanguage()
{
    String language;

    result r = SettingInfo::GetValue(L"http://tizen.org/setting/locale.language", language);
    if (IsFailed(r)) {
        AppLogException("SettingInfo::GetValue(L\"http://tizen.org/setting/locale.language\") failed", GetErrorMessage(r));
        return false;
    }

    AppLogIf(DBG_SHARED_SETTING, "Current Language : %ls", language.GetPointer());

    AclCommand command;
    command.Construct(ACT_LAN);
    command.AddString(language);
    SendAclCommand(command);

    return true;
}

/*****************************************************************************************
 * GetTimeZone()
 *
 * Params:      void
 *
 * Description: Get the timezone id
 *
 * Return:      true if successful, false if failed
 *****************************************************************************************/
bool SharedSettingService::GetTimeZone()
{
    String timezone;

    result r = SettingInfo::GetValue(L"http://tizen.org/setting/locale.time_zone", timezone);
    if (IsFailed(r)) {
        AppLogException("SettingInfo::GetValue(L\"http://tizen.org/setting/locale.time_zone\") failed: (%s)", GetErrorMessage(r));
        return false;
    }

    AppLogIf(DBG_SHARED_SETTING, "Current TimeZone : %ls", timezone.GetPointer());

    AclCommand command;
    command.Construct(ACT_TIMEZONE);
    command.AddString(timezone);
    SendAclCommand(command);

    return true;
}

/*****************************************************************************************
 * GetTimeFormat()
 *
 * Params:      void
 *
 * Description: Get timeformat
 *
 * Return:      true if successful, false if failed
 *****************************************************************************************/
bool SharedSettingService::GetTimeFormat()
{
    bool is24Hour;

    result r = SettingInfo::GetValue(L"http://tizen.org/setting/locale.time.format.24hour", is24Hour);
    if (IsFailed(r)) {
        AppLogException(
                "SettingInfo::GetValue(L\"http://tizen.org/setting/locale.time.format.24hour\") failed: (%s)", GetErrorMessage(r));
        return false;
    }

    AppLogIf(DBG_SHARED_SETTING, "Time format is %s", (is24Hour? "24-hour" : "12-hour"));

    AclCommand command;
    command.Construct(ACT_TIMEFORMAT);
    command.AddInt(is24Hour ? 1 : 0);
    SendAclCommand(command);

    return true;
}

/*****************************************************************************************
 * GetFlightMode()
 *
 * Params:      void
 *
 * Description: Get flight mode setting
 *
 * Return:      true if successful, false if failed
 *****************************************************************************************/
bool SharedSettingService::GetFlightMode()
{
    bool isFlightMode;

    result r = SettingInfo::GetValue(L"http://tizen.org/setting/network.flight_mode", isFlightMode);
    if (IsFailed(r)) {
        AppLogException(
                "SettingInfo::GetValue(L\"http://tizen.org/setting/network.flight_mode\") failed: (%s)", GetErrorMessage(r));
        return false;
    }

    AppLogIf(DBG_SHARED_SETTING, "Flight mode is %s", (isFlightMode? "enabled" : "disabled"));

    AclCommand command;
    command.Construct(ACT_FLIGHTMODE);
    command.AddInt(isFlightMode ? 1 : 0);
    SendAclCommand(command);

    return true;
}

/*****************************************************************************************
 * GetSilentMode()
 *
 * Params:      void
 *
 * Description: Get silent mode setting
 *
 * Return:      true if successful, false if failed
 *****************************************************************************************/
bool SharedSettingService::GetSilentMode()
{
    bool isSilentEnabled;

    result r = SettingInfo::GetValue(L"http://tizen.org/setting/sound.silent_mode", isSilentEnabled);
    if (IsFailed(r)) {
        AppLogException("SettingInfo::GetValue(L\"http://tizen.org/setting/sound.silent_mode\") failed: (%s)", GetErrorMessage(r));
        return false;
    }

    AppLogIf(DBG_SHARED_SETTING, "Silent mode is %s", (isSilentEnabled? "enabled" : "disabled"));

    AclCommand command;
    command.Construct(ACT_SILENT);
    command.AddInt(isSilentEnabled ? 1 : 0);
    SendAclCommand(command);

    return true;
}

/*****************************************************************************************
 * GetVibratorMode()
 *
 * Params:      void
 *
 * Description: Get vibrator mode setting
 *
 * Return:      true if successful, false if failed
 *****************************************************************************************/
bool SharedSettingService::GetVibratorMode()
{
    bool isVibrate = false;

    result r = SettingInfo::GetValue(L"http://tizen.org/setting/vibrator", isVibrate);
    if (IsFailed(r)) {
        AppLogException("SettingInfo::GetValue(L\"http://tizen.org/setting/vibrator\") failed: (%s)", GetErrorMessage(r));
        return false;
    }

    AppLogIf(DBG_SHARED_SETTING, "Vibrate mode is %s", (isVibrate? "enabled" : "disabled"));

    AclCommand command;
    command.Construct(ACT_VIBRATOR);
    command.AddInt(isVibrate ? 1 : 0);
    SendAclCommand(command);

    return true;
}

/*****************************************************************************************
 * GetMediaVolume()
 *
 * Params:      void
 *
 * Description: Get media volume value
 *
 * Return:      true if successful, false if failed
 *****************************************************************************************/
bool SharedSettingService::GetMediaVolume()
{
    int volume;
    result r = SettingInfo::GetValue(L"http://tizen.org/setting/sound.media.volume", volume);
    if (IsFailed(r)) {
        AppLogException("SettingInfo::GetValue(L\"http://tizen.org/setting/sound.media.volume\") failed: (%s)", GetErrorMessage(r));
        return false;
    }

    AppLogIf(DBG_SHARED_SETTING, "Media volume : %d", volume);

    AclCommand command;
    command.Construct(ACT_VOLUME);
    command.AddInt((volume * 100) / 15); // Tizen volume is 0-15.  Regularize to 0-100.
    SendAclCommand(command);

    return true;
}

/*****************************************************************************************
 * GetRingtoneVolume()
 *
 * Params:      void
 *
 * Description: Get ringtone volume value
 *
 * Return:      true if successful, false if failed
 *****************************************************************************************/
bool SharedSettingService::GetRingtoneVolume()
{
    int volume;
    result r = SettingInfo::GetValue(L"http://tizen.org/setting/sound.ringtone.volume", volume);
    if (IsFailed(r)) {
        AppLogException(
                "SettingInfo::GetValue(L\"http://tizen.org/setting/sound.ringtone.volume\") failed: (%s)", GetErrorMessage(r));
        return false;
    }

    AppLogIf(DBG_SHARED_SETTING, "Ringtone volume : %d", volume);

    AclCommand command;
    command.Construct(ACT_RINGTONE);
    command.AddInt(volume);
    SendAclCommand(command);

    return true;
}
/*****************************************************************************************
 * GetDateTime()
 *
 * Params:      void
 *
 * Description: Get DateTime
 *
 * Return:      true if successful, false if failed
 *****************************************************************************************/
bool SharedSettingService::GetDateTime()
{
    String datetime;

    result r = SettingInfo::GetValue(L"http://tizen.org/setting/locale.date_time", datetime);
    if (IsFailed(r)) {
        AppLogException("SettingInfo::GetValue(L\"http://tizen.org/setting/locale.date_time\") failed: (%s)", GetErrorMessage(r));
        return false;
    }

    AppLogIf(DBG_SHARED_SETTING, "Current Date Time : %ls", datetime.GetPointer());

    AclCommand command;
    command.Construct(ACT_DATETIME);
    command.AddString(datetime);
    SendAclCommand(command);

    return true;
}

/*****************************************************************************************
 * GetSystemBrightness()
 *
 * Params:      void
 *
 * Description: Get System Brightness Setting
 *
 * Return:      true if successful, false if failed
 *****************************************************************************************/
bool SharedSettingService::GetSystemBrightness()
{
    int tizenBrightness;

    result r = SettingInfo::GetValue(L"http://tizen.org/setting/screen.brightness", tizenBrightness);
    if (IsFailed(r)) {
        AppLogException("SettingInfo::GetValue(L\"http://tizen.org/setting/screen.brightness\") failed: (%s)", GetErrorMessage(r));
        return false;
    }

    AppLogIf(DBG_SHARED_SETTING, "System Brightness Setting : %d", tizenBrightness);

    if (mLastBrightness != tizenBrightness) {
        mLastBrightness = tizenBrightness;

        AclCommand command;
        command.Construct(ACT_SYS_BRIGHTNESS);
        command.AddInt(mLastBrightness);
        SendAclCommand(command);
    }

    return true;
}

/*****************************************************************************************
 * GetRingtoneSound()
 *
 * Params:      void
 *
 * Description: Get Ringtone Sound Setting
 *
 * Return:      true if successful, false if failed
 *****************************************************************************************/
bool SharedSettingService::GetRingtoneSound()
{
    if (mAndroidSetsSoundRingtone) {
        mAndroidSetsSoundRingtone = false;
        return true;
    }
    String ringtone;

    result r = SettingInfo::GetValue(L"http://tizen.org/setting/sound.ringtone", ringtone);
    if (IsFailed(r)) {
        AppLogException("SettingInfo::GetValue(L\"http://tizen.org/setting/sound.ringtone\") failed: (%s)", GetErrorMessage(r));
        return false;
    }

    AppLogIf(DBG_SHARED_SETTING, "Ringtone Sound Setting : %ls", ringtone.GetPointer());

    AclCommand command;
    command.Construct(ACT_RINGTONE_SOUND);
    command.AddString(ringtone);
    command.AddInt(1);
    SendAclCommand(command);

    return true;
}

/*****************************************************************************************
 * GetNotificationSound()
 *
 * Params:      void
 *
 * Description: Get Notification Sound Setting
 *
 * Return:      true if successful, false if failed
 *****************************************************************************************/
bool SharedSettingService::GetNotificationSound()
{
    if (mAndroidSetsSoundNotification) {
        mAndroidSetsSoundNotification = false;
        return true;
    }
    String notification;

    result r = SettingInfo::GetValue(L"http://tizen.org/setting/sound.notification", notification);
    if (IsFailed(r)) {
        AppLogException("SettingInfo::GetValue(L\"http://tizen.org/setting/sound.notification\") failed: (%s)", GetErrorMessage(r));
        return false;
    }

    AppLogIf(DBG_SHARED_SETTING, "Notification Sound Setting : %ls", notification.GetPointer());

    AclCommand command;
    command.Construct(ACT_RINGTONE_SOUND);
    command.AddString(notification);
    command.AddInt(2);
    SendAclCommand(command);

    return true;
}

/*****************************************************************************************
 * GetGpsUsage()
 *
 * Params:      void
 *
 * Description: get GPS usage setting
 *
 * Return:      true if successful, false if failed
 *****************************************************************************************/
bool SharedSettingService::GetGpsUsage()
{
    bool gps;

    result r = SettingInfo::GetValue(L"http://tizen.org/setting/location.gps", gps);
    if (IsFailed(r)) {
        AppLogException("SettingInfo::GetValue(L\"http://tizen.org/setting/location.gps\") failed: (%s)", GetErrorMessage(r));
        return false;
    }

    AppLogIf(DBG_SHARED_SETTING, "GPS usage is %s", (gps ? "allowed" : "not allowed"));

    AclCommand command;
    command.Construct(ACT_GPS_USAGE);
    command.AddBoolean(gps);
    SendAclCommand(command);

    return true;
}

/*****************************************************************************************
 * GetWpsUsage()
 *
 * Params:      void
 *
 * Description: get WPS usage setting
 *
 * Return:      true if successful, false if failed
 *****************************************************************************************/
bool SharedSettingService::GetWpsUsage()
{
    bool wps;

    result r = SettingInfo::GetValue(L"http://tizen.org/setting/location.wps", wps);
    if (IsFailed(r)) {
        AppLogException("SettingInfo::GetValue(L\"http://tizen.org/setting/location.wps\") failed: (%s)", GetErrorMessage(r));
        return false;
    }

    AppLogIf(DBG_SHARED_SETTING, "WPS usage is %s", (wps ? "allowed" : "not allowed"));

    AclCommand command;
    command.Construct(ACT_WPS_USAGE);
    command.AddBoolean(wps);
    SendAclCommand(command);

    return true;
}

/*****************************************************************************************
 * OnSettingChanged()
 *
 * Params:      key - The key name of the changed setting information
 *
 * Description: Called when a certain setting value has been changed
 *
 * Return:      void
 *****************************************************************************************/
void SharedSettingService::OnSettingChanged(Tizen::Base::String &key)
{
    if (key == "http://tizen.org/setting/locale.time_zone") {
        AppLogIf(DBG_SHARED_SETTING, "timezone changed");
        GetTimeZone();
    } else if (key == "http://tizen.org/setting/sound.silent_mode") {
        AppLogIf(DBG_SHARED_SETTING, "silent mode changed");
        GetSilentMode();
    } else if (key == "http://tizen.org/setting/locale.language") {
        AppLogIf(DBG_SHARED_SETTING, "locale changed");
        GetLanguage();
    } else if (key == "http://tizen.org/setting/vibrator") {
        AppLogIf(DBG_SHARED_SETTING, "vibrator changed");
        GetVibratorMode();
    } else if (key == "http://tizen.org/setting/network.flight_mode") {
        AppLogIf(DBG_SHARED_SETTING, "flight_mode changed");
        GetFlightMode();
    } else if (key == "http://tizen.org/setting/locale.time.format.24hour") {
        AppLogIf(DBG_SHARED_SETTING, "time format changed");
        GetTimeFormat();
    } else if (key == "http://tizen.org/setting/sound.media.volume") {
        AppLogIf(DBG_SHARED_SETTING, "media volume changed");
        GetMediaVolume();
    } else if (key == "http://tizen.org/setting/sound.ringtone.volume") {
        AppLogIf(DBG_SHARED_SETTING, "ringtone volume changed");
        GetRingtoneVolume();
    } else if (key == "http://tizen.org/setting/locale.time") {
        AppLogIf(DBG_SHARED_SETTING, "Time  changed");
        GetDateTime();
    } else if (key == "http://tizen.org/setting/screen.wallpaper") {
        String tizenWallPaperFile;
        String androidWallPaperFile = Environment::GetMediaPath() + WALPAPER_FILE;

        SettingInfo::GetValue(String("http://tizen.org/setting/screen.wallpaper"), tizenWallPaperFile);
        // If the current wallpaper is not the same then we need to copy the file onto Android location
        // This will trigger the file observer on android which will notify the listeners
        if (tizenWallPaperFile != androidWallPaperFile) {
            result r = Tizen::Io::File::Remove(androidWallPaperFile);
            if (r == E_SUCCESS) {
                r = Tizen::Io::File::Copy(tizenWallPaperFile, androidWallPaperFile, true);
                if (r == E_SUCCESS) {
                    mAndroidWallpaperUpdated = true;
                } else {
                    AppLogException("Failed to copy tizen wallpaper to android: %s", GetErrorMessage(r));
                }
            } else {
                AppLogException("Failed to remove android wallpaper: %s", GetErrorMessage(r));
            }
        }
    } else if (key == "http://tizen.org/setting/screen.brightness") {
        AppLogIf(DBG_SHARED_SETTING, "System Brightness Setting changed");
        GetSystemBrightness();
    } else if (key == "http://tizen.org/setting/sound.ringtone") {
        AppLogIf(DBG_SHARED_SETTING, "sound.ringtone changed");
        GetRingtoneSound();
    } else if (key == "http://tizen.org/setting/sound.notification") {
        AppLogIf(DBG_SHARED_SETTING, "sound.notification changed");
        GetNotificationSound();
    } else if (key == "http://tizen.org/setting/location.gps") {
        AppLogIf(DBG_SHARED_SETTING, "location.gps changed");
        GetGpsUsage();
    } else if (key == "http://tizen.org/setting/location.wps") {
        AppLogIf(DBG_SHARED_SETTING, "location.wps changed");
        GetWpsUsage();
    }
}

void SharedSettingService::OnContentFileCreated(ContentId contentId, ContentType contentType, result r)
{
    handleContentChanged(contentId, r);
}
void SharedSettingService::OnContentFileDeleted(ContentId contentId, ContentType contentType, result r)
{
    handleContentChanged(contentId, r);
}
void SharedSettingService::OnContentFileUpdated(ContentId contentId, ContentType contentType, result r)
{
    handleContentChanged(contentId, r);
}

void SharedSettingService::handleContentChanged(ContentId contentId, result rIn)
{
    AppLogIf(DBG_SHARED_SETTING, "content changed");
    if (IsFailed(rIn)) {
        AppLogException("Error: incoming result error (%s)", GetErrorMessage(rIn));
        return;
    }

    ContentInfo *ci = mContentManager.GetContentInfoN(contentId);
    result r = GetLastResult();
    if (IsFailed(r)) {
        AppLogException(
                "Error: getting content info for contentId(%ls) (%s)", contentId.ToString().GetPointer(), GetErrorMessage(r));

        /* We could not get content info, known Tizen issue when content is deleted
         Just sync everything, God have mercy on our battery! */
        AclCommand command;
        command.Construct(ACT_SCAN_MEDIA);
        SendAclCommand(command);
        return;
    }

    String path = ci->GetContentPath();

    if (!mPendingContent.Contains(path)) {
        AppLogIf(DBG_SHARED_SETTING, "OK: non-pending media, add to pending + send scan command");
        r = mPendingContent.Add(new String(path));
        if (IsFailed(r)) {
            AppLogException("Error: unable to add (%ls), result(%s)", path.GetPointer(), GetErrorMessage(r));
        } else {
            AppLogIf(DBG_SHARED_SETTING, "OK: sending scan command path(%ls)", path.GetPointer());
            AclCommand command;
            command.Construct(ACT_SCAN_MEDIA_FILE);
            command.AddString(path);
            SendAclCommand(command);
        }
    } else {
        AppLogIf(DBG_SHARED_SETTING, "OK: pending media, ignore and remove from pending");
        removePathFromPendingList(path);
    }
    delete ci;
}

void SharedSettingService::removePathFromPendingList(const String& path)
{
    int i = 0;
    result r = mPendingContent.IndexOf(path, 0, i);
    if (IsFailed(r)) {
        AppLogException("Error: unable to find index of (%ls), result(%s)", path.GetPointer(), GetErrorMessage(r));
    } else {
        r = mPendingContent.RemoveAt(i);
        if (IsFailed(r)) {
            AppLogException(
                    "Error: unable to remove object at index(%d) for path(%ls), result(%s)", i, path.GetPointer(), GetErrorMessage(r));
        } else {
            AppLogIf(DBG_SHARED_SETTING, "OK, removed(%ls)", path.GetPointer());
        }
    }
}

