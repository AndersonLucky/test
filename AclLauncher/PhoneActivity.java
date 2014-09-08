package com.omww.launcher;

import android.acl.AclCommand;
import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.net.Uri;
import android.util.Log;

public class PhoneActivity extends AclActivity {

    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);

        String action = intent.getAction();
                
        Log.e(Launcher.TAG, "PhoneActivity: Received action " + action);
        
        if (Intent.ACTION_VIEW.equals(action) ||
            Intent.ACTION_CALL.equals(action) ||
            Intent.ACTION_DIAL.equals(action)) {
            
            Uri uri = intent.getData();
            
            if (uri != null) {
                String tizenUri = uri.toString();

                Log.e(Launcher.TAG, "PhoneActivity: Received Call " + tizenUri);

                AclCommand command = null;
                
                if (Intent.ACTION_CALL.equals(action)) {
                    command = new AclCommand(LauncherService.CMD_PHONE_CALL);
                } else if (Intent.ACTION_DIAL.equals(action) ||
                           Intent.ACTION_VIEW.equals(action)) {
                    command = new AclCommand(LauncherService.CMD_PHONE_DIAL);
                }
                
                command.addString(tizenUri);
                sendCommand(command);
                
                Log.e(Launcher.TAG, "Sending command with ID " + Integer.toHexString(command.getId()));                
            } else {
                Log.e(Launcher.TAG, "PhoneActivity: No Phone# specified");

                setResult(Activity.RESULT_CANCELED);
                finish();
            }
        } else {
            // This should never happen since we should be handling all intents we declared in  the manifest but just in case
            Log.e(Launcher.TAG, "PhoneActivity: Unrecognized action");

            setResult(Activity.RESULT_CANCELED);
            finish();
        }
    }
 
    protected void onAclResult(AclCommand command) {
        switch (command.getId()) {
        case LauncherService.CMD_SUCCESS:
            setResult(RESULT_OK);
            break;

        default:
            setResult(Activity.RESULT_CANCELED);
            break;
        }

        finish();
    }
}
