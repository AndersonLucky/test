package com.omww.launcher;

import android.acl.AclCommand;
import android.content.ContentUris;
import android.content.Intent;
import android.content.UriMatcher;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.provider.Contacts.People;
import android.provider.ContactsContract.CommonDataKinds.Phone;
import android.provider.ContactsContract.Contacts;
import android.provider.ContactsContract.Data;
import android.provider.ContactsContract.Intents;
import android.provider.ContactsContract.RawContacts;
import android.util.Log;

@SuppressWarnings("deprecation")
public class ContactsActivity extends AclActivity {

    private static final int CONTACT = 1;
    private static final UriMatcher sUriMatcher = new UriMatcher(UriMatcher.NO_MATCH);
    static {
        sUriMatcher.addURI("com.android.contacts", "contacts/#", CONTACT);
        sUriMatcher.addURI("com.android.contacts", "raw_contacts/#", CONTACT);
        sUriMatcher.addURI("com.android.contacts", "contacts/lookup/*/#", CONTACT);
    }

    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);

        String action = intent.getAction();

        if (Intent.ACTION_EDIT.equals(action)) {
            handleActionOnContact(AddressbookService.CMD_CONTACT_EDIT, intent.getData());
        } else if (Intent.ACTION_VIEW.equals(action)) {
            String type = intent.getType();
            if (RawContacts.CONTENT_ITEM_TYPE.equals(type) ||
                Contacts.CONTENT_ITEM_TYPE.equals(type) ||
                People.CONTENT_ITEM_TYPE.equals(type)) {
                handleActionOnContact(AddressbookService.CMD_CONTACT_EDIT, intent.getData());
            } else {
                AclCommand cmd = new AclCommand(AddressbookService.CMD_VIEW_CONTACTS_APP);
                sendCommand(cmd);
            }
        } else if (Intent.ACTION_GET_CONTENT.equals(action) ||
                   Intent.ACTION_PICK.equals(action)) {
            sendCommand(new AclCommand(AddressbookService.CMD_PICK_PHONE));
        } else if (Intent.ACTION_INSERT.equals(action)) {
            AclCommand cmd = new AclCommand(AddressbookService.CMD_CONTACT_INSERT);
            cmd = appendPhoneAndEmailToAclCommandFromIntent(cmd, intent);
            sendCommand(cmd);
        } else if (Intent.ACTION_INSERT_OR_EDIT.equals(action)) {
            handlePickAndEditContact(intent);
        } else {
            Log.e(Launcher.TAG, "ContactsActivity: Unknown action(" + action + ")");
        }
    }

    private AclCommand appendPhoneAndEmailToAclCommandFromIntent(AclCommand c, Intent i) {
        String phone = i.getStringExtra(Intents.Insert.PHONE);
        String email = i.getStringExtra(Intents.Insert.EMAIL);

        phone = (null == phone) ? "" : phone;
        email = (null == email) ? "" : email;

        c.addString(phone);
        c.addString(email);

        return c;
    }

    private void handleActionOnContact(int aclCommandId, Uri uri) {
        int uriId = sUriMatcher.match(uri);
        switch (uriId) {
            case CONTACT: {
                String contactId = uri.getLastPathSegment();
                AclCommand cmd = new AclCommand(aclCommandId);
                cmd.addInt(Integer.parseInt(contactId));
                sendCommand(cmd);
                break;
            }
            default: {
                Log.e(Launcher.TAG, "ContactsActivity: Unknown URI for aclCommandId("
                                    + aclCommandId +") uri(" + uri.toString() + ")");
                break;
            }
        }
    }

    private void handlePickAndEditContact(Intent intent) {
        AclCommand cmd = new AclCommand(AddressbookService.CMD_PICK_AND_EDIT_CONTACT);
        cmd = appendPhoneAndEmailToAclCommandFromIntent(cmd, intent);
        sendCommand(cmd);
    }

    protected void onAclResult(AclCommand cmd) {
        if (LauncherService.CMD_PHONE_PICKED == cmd.getId()) {
            int androidId = cmd.getInt(0);
            Log.d(Launcher.TAG, "androidId = (" + androidId + ")");
            Intent i = getIntent();
            if (null == i) {
                Log.e(Launcher.TAG, "null intent, bailing");
                return;
            }
            String action = i.getAction();
            String type = i.getType();
            Uri uri = null;
            if (Intent.ACTION_GET_CONTENT.equals(action) &&
                Phone.CONTENT_ITEM_TYPE.equals(type)) {
                uri = getPhoneUri(androidId);
            } else if(Intent.ACTION_PICK.equals(action) &&
                      Contacts.CONTENT_TYPE.equals(type)) {
                uri = getContactUri(androidId);
            }
            if (null != uri) {
                Log.d(Launcher.TAG, "uri(" + uri.toString() + ")");
                final Intent intent = new Intent();
                setResult(RESULT_OK, intent.setData(uri));
            } else {
                Log.e(Launcher.TAG, "Could not find URI for android id (" + androidId + ")");
            }
        } else {
            Log.e(Launcher.TAG, "Unknown command id (" + cmd.getId() + ")");
        }
        finish();
    }

    private Uri getPhoneUri(int androidId) {
        String[] what = new String[] {Data._ID, Phone.NUMBER};
        String where = Data.RAW_CONTACT_ID + "=? AND " +
                       Data.MIMETYPE + "='" + Phone.CONTENT_ITEM_TYPE + "'";
        String[] whereValues = new String[] {String.valueOf(androidId)};
        Cursor c = getContentResolver().query(Data.CONTENT_URI,
                                              what,
                                              where,
                                              whereValues,
                                              null);
        if (null != c) {
            if (c.moveToFirst()) {
                do {
                    long dataId = c.getInt(0);
                    String phoneNumber = c.getString(1);
                    if (null != phoneNumber && !phoneNumber.isEmpty()) {
                        Uri uri = ContentUris.withAppendedId(Data.CONTENT_URI, dataId);
                        return uri;
                    }
                } while (c.moveToNext());
            } else {
                Log.e(Launcher.TAG,
                      "cursor could not move to first for android id (" +
                      androidId + ")");
            }
        } else {
            Log.e(Launcher.TAG, "cursor was null for android id (" + androidId + ")");
        }
        return null;
    }

    private Uri getContactUri(int androidId) {
        String[] what = new String[] {RawContacts.CONTACT_ID};
        String where = RawContacts._ID + "=?";
        String[] whereValues = new String[] {String.valueOf(androidId)};
        Cursor c = getContentResolver().query(RawContacts.CONTENT_URI,
                                              what,
                                              where,
                                              whereValues,
                                              null);
        if (null != c) {
            if (c.moveToFirst()) {
                long contactId = c.getInt(0);
                Uri uri = ContentUris.withAppendedId(Contacts.CONTENT_URI, contactId);
                return uri;
            } else {
                Log.e(Launcher.TAG,
                      "cursor could not move to first for android id (" +
                      androidId + ")");
            }
        } else {
            Log.e(Launcher.TAG, "cursor was null for android id (" + androidId + ")");
        }
        return null;
    }
}

