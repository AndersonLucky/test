package com.omww.launcher;

import android.acl.AclCommand;
import android.app.Activity;
import android.app.SearchManager;
import android.content.ActivityNotFoundException;
import android.content.ComponentName;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.util.Log;

import java.io.IOException;
import java.net.MalformedURLException;
import java.net.URL;
import java.net.URI;
import java.net.HttpURLConnection;
import java.util.ArrayList;

public class BrowserActivity extends AclActivity {
    private static final String EXTENSION_APK = ".apk";
    private static final String HOST_PLAY_STORE = "play.google.com";
    private static final String SCHEME_MARKET = "market";
    private static final int MARKET_REQUEST = 1;
    private boolean mAndroidBrowserLaunched = false;

    @Override
    public void onResume() {
        super.onResume();

        if (mAndroidBrowserLaunched) {
            finish();
        }
    }

    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);

        mAndroidBrowserLaunched = false;
        String action = intent.getAction();
        String tizenUri = null;

        if (Intent.ACTION_VIEW.equals(action)) {
            Uri uri = intent.getData();

            if (uri != null) {
                Uri marketUri = getMarketLink(uri);

                if (marketUri != null) {
                    launchTizenStore(marketUri);
                    return;
                } else {
                    String url = uri.toString();

                    if (url != null) {
                        url = url.toLowerCase();
                        // Wonder if we should check if the url contains .apk or should we limit it to urls ending with .apk?
                        if (url.contains(EXTENSION_APK)) {
                            startBrowserActivity(intent);
                            return;
                        }
                    }
                }

                tizenUri = uri.toString();
            }
        } else if (Intent.ACTION_SEARCH.equals(action) || Intent.ACTION_WEB_SEARCH.equals(action)) {
            // get the query string
            tizenUri = intent.getStringExtra(SearchManager.QUERY);
        }

        if (tizenUri != null) {
            AclCommand command = new AclCommand(LauncherService.CMD_BROWSE_URI);
            command.addString(tizenUri);
            sendCommand(command);
        } else {
            setResult(Activity.RESULT_CANCELED);
            finish();
        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        switch (requestCode) {
            case MARKET_REQUEST:
                setResult(resultCode);
                break;

            default:
                setResult(Activity.RESULT_CANCELED);
                break;
        }

        finish();
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

    private void startBrowserActivity(Intent originalIntent) {
        Intent intent = new Intent(originalIntent);
        // Pass this off to the Android browser
        intent.setComponent(new ComponentName("com.android.browser", "com.android.browser.BrowserActivity"));

        try {
            startActivity(intent);
            mAndroidBrowserLaunched = true;
        } catch (ActivityNotFoundException ex) {
            Log.e(Launcher.TAG, "Failed to start browser activity", ex);
        }
    }

    private Uri getMarketLink(Uri uri) {
        if (HOST_PLAY_STORE.equalsIgnoreCase(uri.getHost()) ||
                SCHEME_MARKET.equalsIgnoreCase(uri.getScheme())) {

            return uri;
        }

        boolean isMarketLink = false;
        Uri destinationUri = uri;

        try {
            URL destinationURL = new URL(uri.toString());
            HttpURLConnection ucon = null;
            int responseCode = HttpURLConnection.HTTP_OK;

            do {
                try {
                    ucon = (HttpURLConnection) destinationURL.openConnection();
                    ucon.setInstanceFollowRedirects(false);
                    responseCode = ucon.getResponseCode();

                    if (responseCode == HttpURLConnection.HTTP_MOVED_TEMP ||
                            responseCode == HttpURLConnection.HTTP_MOVED_PERM ||
                            responseCode == HttpURLConnection.HTTP_SEE_OTHER) {

                        String location = ucon.getHeaderField("Location");

                        if (location == null) {
                            break;
                        }

                        destinationUri = Uri.parse(location);

                        if (destinationUri == null) {
                            Log.e(Launcher.TAG, "Failed to parse string " + location);
                            destinationUri = uri;

                            break;
                        }

                        // If the redirected uri is of type market:// then we need look no further
                        if (SCHEME_MARKET.equalsIgnoreCase(destinationUri.getScheme())) {
                            return destinationUri;
                        }

                        destinationURL = new URL(location);
                    } else {
                        // No more re-directs. break out of the loop
                        break;
                    }
                } catch (MalformedURLException mfe) {
                    Log.e(Launcher.TAG, "getMarketLink: MalformedURLException", mfe);

                    break;
                } catch (IOException ioe) {
                    Log.e(Launcher.TAG, "getMarketLink: IOException", ioe);
                    // If there is no connectivity then let the Browser handle it
                    return null;
                } finally {
                    if (ucon != null) {
                        ucon.disconnect();
                    }
                }
            } while (true);
        } catch (MalformedURLException mfe) {
            Log.e(Launcher.TAG, "getMarketLink: MalformedURLException", mfe);
        }

        if (!isMarketLink && destinationUri != null) {
            String host = destinationUri.getHost();
            String query = destinationUri.getQuery();

            // ad-x.co.uk is a popular app download tracking site, it always redirect the download to play.google.com if running
            // android app, so we redirect it to Tizen Store if it does. Some apps linking to their own webiste to download or
            // buy, then redirect to play.google.com or ad-x.co.uk from inside of the uri query, we also point them to Tizen
            // Store
            ArrayList<String> urlList = AclApplication.getInstance().getMarketUrls();

            if (host != null) {
                host = host.toLowerCase();

                for (String url : urlList) {
                    if (host.equals(url)) {
                        isMarketLink = true;
                        break;
                    }
                }
            }

            if (!isMarketLink && query != null) {
                query = query.toLowerCase();

                for (String url : urlList) {
                    if (query.contains(url)) {
                        isMarketLink = true;
                        break;
                    }
                }
            }
        }

        if (isMarketLink) {
            return destinationUri;
        } else {
            return null;
        }
    }
}
