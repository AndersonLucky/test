package com.omww.launcher;

import android.acl.AclCommand;
import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.util.Log;

public class MusicPlayerActivity extends AclActivity {

    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);

        AclCommand command = null;
        String action = intent.getAction();

        Log.e(Launcher.TAG, "MusicPlayerActivity: Received action " + action);

        if (Intent.ACTION_VIEW.equals(action)) {
            Uri uri = intent.getData();

            if (uri != null) {
                String tizenUri = AclUtils.saveToTempIfRequired(getContentResolver(), uri);

                if (tizenUri != null) {
                    command = new AclCommand(LauncherService.CMD_VIEW_MUSICPLAYER);
                    command.addString(tizenUri);
                }

                Log.e(Launcher.TAG, "MusicPlayerActivity: Received data " + tizenUri);
            }
        }

        if (command != null) {
            sendCommand(command);
        } else {
            setResult(Activity.RESULT_CANCELED);
            finish();
        }
    }

    protected void onAclResult(AclCommand command) {
        switch (command.getId()) {
                case LauncherService.CMD_SUCCESS:
                    setResult(RESULT_OK);
                    finish();
                    break;

                default:
                    setResult(Activity.RESULT_CANCELED);
                    finish();
                    break;
        }
    }
}
