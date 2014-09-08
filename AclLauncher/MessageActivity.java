package com.omww.launcher;

import android.acl.AclCommand;
import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.os.Parcelable;
import android.net.Uri;
import android.util.Log;

import java.lang.StringBuilder;
import java.io.UnsupportedEncodingException;
import java.net.URLDecoder;
import java.util.List;
import java.util.ArrayList;


public class MessageActivity extends AclActivity {

    private StringBuilder toString;
    private StringBuilder attachmentsString;
    private String        subjectString;
    private String        bodyString;
    private String        messagetypeString;

    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);

        toString = new StringBuilder();
        attachmentsString = new StringBuilder();
        subjectString = null;
        bodyString = null;
        messagetypeString = null;

        initFromIntent(intent);

        AclCommand command = new AclCommand(LauncherService.CMD_MESSAGE_COMPOSE);

        if (toString.length() > 0) {
            command.addString("to:" + toString.toString());
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

        if (messagetypeString != null && messagetypeString.length() >0) {
            command.addString("messagetype:" + messagetypeString.toString());
        }
        
        sendCommand(command);
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


    private void initFromIntent(Intent intent) {
   
        //First, add values stored in top-level extras
        String[] toList = intent.getStringArrayExtra(Intent.EXTRA_EMAIL);

        if (toList != null) {
            addAddresses(toString, toList);
        }
    
        subjectString = intent.getStringExtra(Intent.EXTRA_SUBJECT);

        //Second, if we invoked with a URI, try to interpret it
        // We'll take three cources here. They are mmsto/mms,smsto/sms and other scheme

        final Uri dataUri = intent.getData();
        if (dataUri != null) {
            if ("sms".equals(dataUri.getScheme()) || "smsto".equals(dataUri.getScheme()) ) {
                initializeFromSmsTo(dataUri.toString());
            } else  {
                String toText = dataUri.getSchemeSpecificPart();
                if (toText != null) {
                    addAddresses(toString, toText.split(","));
                }
            }
            
            messagetypeString = dataUri.getScheme();
        }

        //Fill in plaintext
        CharSequence text = null;
        if (intent.hasExtra("sms_body")) {
            text = intent.getCharSequenceExtra("sms_body");
        } else {
            text = intent.getCharSequenceExtra(Intent.EXTRA_TEXT);
        }

        if (text != null) {
            bodyString = text.toString();
        }

        String action = intent.getAction();
        
        // Convert EXTRA_STREAM into attachment
        if (Intent.ACTION_SEND.equals(action)) {
            if (intent.hasExtra(Intent.EXTRA_STREAM)) {        
                Uri stream = (Uri) intent.getParcelableExtra(Intent.EXTRA_STREAM);
                
                if (stream != null) {
                    addAttachment(stream);
                }
            }
        } else if (Intent.ACTION_SEND_MULTIPLE.equals(action) &&
                    intent.hasExtra(Intent.EXTRA_STREAM)) {
                    
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


    /** @smsToString the href
      * sms:phone number?action
      * mms://host:port/path 
      */

    private void initializeFromSmsTo(String smsToString) {

        // Chop up everything between sms: and ? to find recipients
        int index = smsToString.indexOf("?");
        int length = "sms".length() + 1;
        String to;
        try {
            // Extract the recipient after mmsto:
            if (index == -1) {
                to = decode(smsToString.substring(length));
                    Log.e(Launcher.TAG, "MessageActivity: index = -1 " + to);
            } else {
                to = decode(smsToString.substring(length, index));
                    Log.e(Launcher.TAG, "MessageActivity: index != -1 " + to);
            }
            addAddresses(toString, to.split(" ,"));
        } catch (UnsupportedEncodingException e) {
            Log.e(Launcher.TAG, e.getMessage() + " while decoding '" + smsToString + "'");
        }

        // Extract the other parameters

        // We need to disguise this string as a URI in order to parse it
        Uri uri = Uri.parse("foo://" + smsToString);

        List<String> subject = uri.getQueryParameters("subject");
        if (subject.size() > 0) {
            subjectString = subject.get(0);
        }

        List<String> body = uri.getQueryParameters("body");
        if (body.size() > 0) {
            bodyString = body.get(0);
        }

        List<String> messagetype = uri.getQueryParameters("messagetype");
        if (messagetype.size() > 0) {
            messagetypeString = messagetype.get(0);
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
        String desPath = AclUtils.saveToTempIfRequired(getContentResolver(), uri);
        if (desPath != null) {
            if (attachmentsString.length() > 0) {
                // add separator
                attachmentsString.append("|");
            }
        
            attachmentsString.append(desPath);
        }
    }
}
