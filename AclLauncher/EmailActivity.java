package com.omww.launcher;

import android.acl.AclCommand;
import android.acl.AclManager;
import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.os.Parcelable;
import android.net.Uri;
import android.util.Log;
import android.view.View;

import java.util.List;
import java.util.ArrayList;
import java.lang.StringBuilder;
import java.io.UnsupportedEncodingException;
import java.net.URLDecoder;

public class EmailActivity extends AclActivity {

    private StringBuilder toString;
    private StringBuilder ccString;
    private StringBuilder bccString;
    private StringBuilder attachmentsString;
    private String        subjectString;
    private String        bodyString;
    private boolean pausedAtLeastOnce;
    private boolean finishOnResume;

    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);

        toString = new StringBuilder(); 
        ccString = new StringBuilder(); 
        bccString = new StringBuilder();
        attachmentsString = new StringBuilder();
        subjectString = null; 
        bodyString = null; 

        String action = intent.getAction();
        Log.e(Launcher.TAG, "EmailActivity: Received action " + action);
        
        initFromIntent(intent);
        
        AclCommand command = new AclCommand(LauncherService.CMD_EMAIL_SEND);
        
        if (toString.length() > 0) {
            command.addString("to:" + toString.toString());
        }
                               
        if (ccString.length() > 0) {
            command.addString("cc:" + ccString.toString());
        }

        if (bccString.length() > 0) {
            command.addString("bcc:" + bccString.toString());
        }

        if (subjectString != null && subjectString.length() > 0) {
            command.addString("subject:" + subjectString.toString());
        }

        if (bodyString != null && bodyString.length() > 0) {
            command.addString("body:" + bodyString.toString());
        }

        if (attachmentsString != null && attachmentsString.length() > 0) {
            command.addString("attach:" + attachmentsString.toString());
        }
        
        sendCommand(command);
    }

    protected void onResume() {
        super.onResume();

        if (finishOnResume)
            finish();
    }

    protected void onPause() {
        super.onPause();

        pausedAtLeastOnce = true;
    }

    protected void onAclResult(AclCommand command) {
        switch (command.getId()) {
            case LauncherService.CMD_SUCCESS:
                setResult(RESULT_OK);

                int flags = getIntent().getFlags();
                boolean newTask = (flags & Intent.FLAG_ACTIVITY_NEW_TASK) == Intent.FLAG_ACTIVITY_NEW_TASK;

                // Don't finish when we get an immediate response so we can dim the screen...
                // UNLESS the activity is launched in a separate task
                if (!newTask && !pausedAtLeastOnce) {
                    finishOnResume = true;
                    return;
                }

                break;

            default:
                setResult(Activity.RESULT_CANCELED);
                break;
        }
        finish();
    }
    
    private void initFromIntent(Intent intent) {

        // First, add values stored in top-level extras
        String[] toList = intent.getStringArrayExtra(Intent.EXTRA_EMAIL);

        if (toList != null) {
            addAddresses(toString, toList);
        }
        
        String[] ccList = intent.getStringArrayExtra(Intent.EXTRA_CC);

        if (ccList != null) {
            addAddresses(ccString, ccList);
        }

        String[] bccList = intent.getStringArrayExtra(Intent.EXTRA_BCC);

        if (bccList != null) {
            addAddresses(bccString, bccList);
        }
        
        subjectString = intent.getStringExtra(Intent.EXTRA_SUBJECT);
        

        // Next, if we were invoked with a URI, try to interpret it
        // We'll take two courses here.  If it's mailto:, there is a specific set of rules
        // that define various optional fields.  However, for any other scheme, we'll simply
        // take the entire scheme-specific part and interpret it as a possible list of addresses.

        final Uri dataUri = intent.getData();
        
        if (dataUri != null) {
            Log.e(Launcher.TAG, "EmailActivity: getData " + dataUri.toString());
        
            if ("mailto".equals(dataUri.getScheme())) {
                initializeFromMailTo(dataUri.toString());
            } else {
                String toText = dataUri.getSchemeSpecificPart();
                if (toText != null) {
                    addAddresses(toString, toText.split(","));
                }
            }
        }

        // Next, fill in the plaintext (note, this will override mailto:?body=)

        CharSequence text = intent.getCharSequenceExtra(Intent.EXTRA_TEXT);
        
        if (text != null) {
            bodyString = text.toString();
        }

        String action = intent.getAction();
        
        // Next, convert EXTRA_STREAM into an attachment
        if (AclManager.INTENT_SEND.equals(action) && intent.hasExtra(Intent.EXTRA_STREAM)) {
            String type = intent.getType();
            Uri stream = (Uri) intent.getParcelableExtra(Intent.EXTRA_STREAM);
            
            if (stream != null && type != null) {
                addAttachment(stream);
            }
        }

        if (AclManager.INTENT_SEND_MULTIPLE.equals(action) && intent.hasExtra(Intent.EXTRA_STREAM)) {
            ArrayList<Parcelable> list = intent.getParcelableArrayListExtra(Intent.EXTRA_STREAM);
            if (list != null) {
                for (Parcelable parcelable : list) {
                    Uri uri = (Uri) parcelable;
                    
                    if (uri != null) {
                        addAttachment(uri);
                    }
                }
            }
        }
    }

   /**
     * When we are launched with an intent that includes a mailto: URI, we can actually
     * gather quite a few of our message fields from it.
     *
     * @mailToString the href (which must start with "mailto:").
     */
    private void initializeFromMailTo(String mailToString) {

        // Chop up everything between mailto: and ? to find recipients
        int index = mailToString.indexOf("?");
        int length = "mailto".length() + 1;
        String to;
        try {
            // Extract the recipient after mailto:
            if (index == -1) {

                to = decode(mailToString.substring(length));
                Log.e(Launcher.TAG, "EmailActivity: index = -1 " + to);
            } else {
                to = decode(mailToString.substring(length, index));
                Log.e(Launcher.TAG, "EmailActivity: index != -1 " + to);
            }
            addAddresses(toString, to.split(" ,"));
        } catch (UnsupportedEncodingException e) {
            Log.e(Launcher.TAG, e.getMessage() + " while decoding '" + mailToString + "'");
        }

        // Extract the other parameters

        // We need to disguise this string as a URI in order to parse it
        Uri uri = Uri.parse("foo://" + mailToString);

        List<String> cc = uri.getQueryParameters("cc");
        addAddresses(ccString, cc.toArray(new String[cc.size()]));

        List<String> otherTo = uri.getQueryParameters("to");
        addAddresses(ccString, otherTo.toArray(new String[otherTo.size()]));

        List<String> bcc = uri.getQueryParameters("bcc");
        addAddresses(bccString, bcc.toArray(new String[bcc.size()]));

        List<String> subject = uri.getQueryParameters("subject");
        if (subject.size() > 0) {
            subjectString = subject.get(0);
        }

        List<String> body = uri.getQueryParameters("body");
        if (body.size() > 0) {
            bodyString = body.get(0);
        }
    }
    
    private String decode(String s) throws UnsupportedEncodingException {
        return URLDecoder.decode(s, "UTF-8");
    }

    private void addAddresses(StringBuilder addressBuf, String[] addresses) {
        if (addressBuf == null || addresses == null) {
            return;
        }
        
        for (String oneAddress : addresses) {
            addAddress(addressBuf, oneAddress);
        }
    }

    private void addAddress(StringBuilder addressBuf, String address) {
        if (addressBuf == null || address == null) {
            return;
        }

        if (addressBuf.length() > 0) {
            // add the separator
            addressBuf.append(", ");
        }
        
        addressBuf.append(address);
    }
    
    private void addAttachment(Uri uri) {
        String destPath = AclUtils.saveToTempIfRequired(getContentResolver(), uri);

        if (destPath != null) {
            if (attachmentsString.length() > 0) {
                // add separator
                attachmentsString.append("|");
            }

            attachmentsString.append(destPath);
        }
    }
}
