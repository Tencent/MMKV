package com.tencent.mmkvdemo;

import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteOpenHelper;

public final class SQLIteKV {
    private class SQLIteKVDBHelper extends SQLiteOpenHelper {
        private static final int DB_VERSION = 1;
        private static final String DB_NAME = "kv.db";
        public static final String TABLE_NAME_STR = "kv_str";
        public static final String TABLE_NAME_INT = "kv_int";

        public SQLIteKVDBHelper(Context context) {
            super(context, DB_NAME, null, DB_VERSION);
        }

        @Override
        public void onCreate(SQLiteDatabase sqLiteDatabase) {
            String sql = "create table if not exists " + TABLE_NAME_STR + " (k text UNIQUE on conflict replace, v text)";
            sqLiteDatabase.execSQL(sql);
            sql = "create table if not exists " + TABLE_NAME_INT + " (k text UNIQUE on conflict replace, v integer)";
            sqLiteDatabase.execSQL(sql);
        }

        @Override
        public void onUpgrade(SQLiteDatabase sqLiteDatabase, int oldVersion, int newVersion) {
            String sql = "DROP TABLE IF EXISTS " + TABLE_NAME_STR;
            sqLiteDatabase.execSQL(sql);
            sql = "DROP TABLE IF EXISTS " + TABLE_NAME_INT;
            sqLiteDatabase.execSQL(sql);
            onCreate(sqLiteDatabase);
        }
    }
    private SQLIteKVDBHelper m_dbHelper;
    private SQLiteDatabase m_writetableDB;

    public SQLIteKV(Context context) {
        m_dbHelper = new SQLIteKVDBHelper(context);
        m_dbHelper.setWriteAheadLoggingEnabled(true);
    }

    public void beginTransaction() {
        if (m_writetableDB == null) {
            m_writetableDB = m_dbHelper.getWritableDatabase();
        }
        m_writetableDB.beginTransaction();
    }

    public void endTransaction() {
        if (m_writetableDB != null) {
            try {
                m_writetableDB.setTransactionSuccessful();
            } finally {
                m_writetableDB.endTransaction();
            }
            m_writetableDB.close();
            m_writetableDB = null;
        }
    }

    private SQLiteDatabase getWritetableDB() {
        return (m_writetableDB != null) ? m_writetableDB : m_dbHelper.getWritableDatabase();
    }

    public boolean putInt(String key, int value) {
        ContentValues contentValues = new ContentValues();
        contentValues.put("k", key);
        contentValues.put("v", value);
        SQLiteDatabase db = getWritetableDB();
        long rowID = db.insert(SQLIteKVDBHelper.TABLE_NAME_INT, null, contentValues);
        return rowID != -1;
    }

    public int getInt(String key) {
        int value = 0;
        Cursor cursor = m_dbHelper.getReadableDatabase().rawQuery("select v from " + SQLIteKVDBHelper.TABLE_NAME_INT + " where k=?", new String[]{ key });
        if(cursor.moveToFirst()) {
            value = cursor.getInt(cursor.getColumnIndex("v"));
        }
        cursor.close();
        return value;
    }

    public boolean putString(String key, String value) {
        ContentValues contentValues = new ContentValues();
        contentValues.put("k", key);
        contentValues.put("v", value);
        SQLiteDatabase db = getWritetableDB();
        long rowID = db.insert(SQLIteKVDBHelper.TABLE_NAME_STR, null, contentValues);
        return rowID != -1;
    }

    public String getString(String key) {
        String value = null;
        Cursor cursor = m_dbHelper.getReadableDatabase().rawQuery("select v from " + SQLIteKVDBHelper.TABLE_NAME_STR + " where k=?", new String[]{ key });
        if(cursor.moveToFirst()) {
            value = cursor.getString(cursor.getColumnIndex("v"));
        }
        cursor.close();
        return value;
    }
}