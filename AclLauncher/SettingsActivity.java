package com.omww.launcher;

import android.acl.AclCommand;
import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.util.Log;

public class SettingsActivity extends AclActivity {

    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);

        String action = intent.getAction();

        Log.d(Launcher.TAG, "SettingsActivity: Received action " + action);

        if (action != null) {
            AclCommand command = new AclCommand(LauncherService.CMD_SETTINGS_LAUNCH);

            if (action.equals("android.settings.LOCATION_SOURCE_SETTINGS")) {
                command.addString("location");
            }
            else {
                command.addString("all");
            }

            sendCommand(command);

        } else {
            Log.e(Launcher.TAG, "SettingsActivity: is not specified");

            setResult(Activity.RESULT_CANCELED);
            finish();
        }
    }

    protected void onAclResult(AclCommand command) {
        switch (command.getId()) {
        case LauncherService.CMD_SUCCESS:
            setResult(Activity.RESULT_OK);
            break;

        default:
            setResult(Activity.RESULT_CANCELED);
            break;
        }

        finish();
    }
}
