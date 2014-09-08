package com.omww.launcher;

import android.acl.AclCommand;
import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.util.Log;

public class BuyActivity extends AclActivity {

    private static final String TAG = "LauncherBuyActivity";

    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);

        Bundle bundleIn = intent.getExtras();
        startPurchase(bundleIn);
    }

    protected void startPurchase(Bundle bundleIn) {
        AclCommand command = null;

        if (bundleIn.getInt("ACTION") == LauncherService.CMD_PURCHASE) {
            Log.d(Launcher.TAG,
                    "start CMD_PURCHASE groupID:" + bundleIn.getString("PACKAGE_NAME") + " ProdID:" + bundleIn.getString("PROD_ID"));
            command = new AclCommand(LauncherService.CMD_PURCHASE);
            command.addString(bundleIn.getString("PACKAGE_NAME"));
            command.addString(bundleIn.getString("PROD_ID"));
            command.addString(bundleIn.getString("DEV_PAYLOAD"));
        } else {
            Log.d(Launcher.TAG, "startPurchase command not known");
        }

        if (command != null) {
            sendCommand(command);
        } else {
            Log.d(Launcher.TAG, "StartPurchase Command is null");
            setResult(LauncherService.IAB_RESULT_USER_CANCELED);
            finish();
        }
    }

    protected void onAclResult(AclCommand command) {

        Bundle orderBundle = null;
        Intent intent = null;

        switch (command.getId()) {
            case LauncherService.CMD_SUCCESS:

                Log.i(TAG, "OnAclResult SUCCESS");
                orderBundle = getMyBundle(command);
                intent = new Intent();
                intent.putExtras(orderBundle);
                setResult(Activity.RESULT_OK, intent);

                finish();
                break;

            case LauncherService.CMD_CANCELED:
                Log.i(TAG, "OnAclResult CANCEL");
                orderBundle = getMyBundle(command);
                intent = new Intent();
                intent.putExtras(orderBundle);
                setResult(Activity.RESULT_OK, intent);
                finish();
                break;

            default:
                setResult(LauncherService.IAB_RESULT_USER_CANCELED);
                finish();
                break;
        }
    }

    public Bundle getMyBundle(AclCommand cmd) {
        Log.i(TAG, "BuyActivity: geyMyBundle");
        Bundle orderBundle = new Bundle();
        try {
            if (cmd.getId() != LauncherService.CMD_CANCELED) {
                Log.i(TAG, "BuyActivity: call fillPurchasedata");
                orderBundle = IabUtils.fillPurchaseData(cmd, orderBundle);
            } else {
                Log.i(TAG, "BuyActivity: CANCEL");
                int responseCode = IabUtils.TizenToGoogleResponseCode(cmd.getInt(0));
                orderBundle.putInt("RESPONSE_CODE", responseCode);
                return orderBundle;
            }
        } catch (Exception e) {
            Log.i(TAG, " BuyActivity getBundle ERROR: " + e.toString());
        }

        Log.i(TAG, "BuyActivity: geyMyBundle end");

        return orderBundle;
    }

}
