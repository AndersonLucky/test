package com.omww.launcher;

import java.io.File;

import android.acl.AclCommand;
import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.provider.MediaStore;
import android.util.Log;

public class MusicPickerActivity extends AclActivity {

    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);

        handleMusicPickerIntent();
    }

    protected void onAclResult(AclCommand command) {
        switch (command.getId()) {
                case LauncherService.CMD_MUSIC_PICKED:
                    handleMusicPickedResponse(command.getString(0));
                    break;

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

    // ------- Private Scrope -------------- //
    private void handleMusicPickerIntent() {
        AclCommand command = null;
        Intent intent = getIntent();
        String action = intent.getAction();

        if (Intent.ACTION_GET_CONTENT.equals(action) || Intent.ACTION_PICK.equals(action)) {
            command = new AclCommand(LauncherService.CMD_PICK_MUSICPICKER);
            command.addString("audio/*");
        }

        if (command != null) {
            sendCommand(command);
        } else {
            setResult(Activity.RESULT_CANCELED);
            finish();
        }
    }


    private void handleMusicPickedResponse (String musicPath) {
        if (musicPath != null) {
            Uri musicUri = Uri.fromFile(new File(musicPath));
            Intent intent = new Intent();
            intent.setData(musicUri);

            setResult(RESULT_OK, intent);
        } else {
            setResult(Activity.RESULT_CANCELED);
        }

        finish();
    }
}
