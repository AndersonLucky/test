package com.omww.launcher;

import android.acl.AclCommand;
import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.util.Log;

public class TizenStoreActivity extends AclActivity {
    private static final String TAG = Launcher.TAG;

    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);

        launchTizenStore(getIntent().getData());
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
