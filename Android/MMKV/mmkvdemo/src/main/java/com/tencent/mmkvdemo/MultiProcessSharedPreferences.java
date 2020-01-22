package com.tencent.mmkvdemo;

import android.content.BroadcastReceiver;
import android.content.ContentProvider;
import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.content.UriMatcher;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.pm.ProviderInfo;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.net.Uri;
import android.os.Bundle;
import java.lang.ref.SoftReference;
import java.lang.reflect.Constructor;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * 使用ContentProvider实现多进程SharedPreferences读写;<br>
 * 1、ContentProvider天生支持多进程访问；<br>
 * 2、使用内部私有BroadcastReceiver实现多进程OnSharedPreferenceChangeListener监听；<br>
 * 
 * 使用方法：AndroidManifest.xml中添加provider申明：<br>
 * <pre>
 * &lt;provider android:name="com.tencent.mm.sdk.patformtools.MultiProcessSharedPreferences"
 * android:authorities="com.tencent.mm.sdk.patformtools.MultiProcessSharedPreferences"
 * android:exported="false" /&gt;
 * &lt;!-- authorities属性里面最好使用包名做前缀，apk在安装时authorities同名的provider需要校验签名，否则无法安装；--!/&gt;<br>
 * </pre>
 * 
 * ContentProvider方式实现要注意：<br>
 * 1、当ContentProvider所在进程android.os.Process.killProcess(pid)时，会导致整个应用程序完全意外退出或者ContentProvider所在进程重启；<br>
 * 重启报错信息：Acquiring provider <processName> for user 0: existing object's process dead；<br>
 * 2、如果设备处在“安全模式”下，只有系统自带的ContentProvider才能被正常解析使用，因此put值时默认返回false，get值时默认返回null；<br>
 * 
 * 其他方式实现SharedPreferences的问题：<br>
 * 使用FileLock和FileObserver也可以实现多进程SharedPreferences读写，但是会有兼容性问题：<br>
 * 1、某些设备上卸载程序时锁文件无法删除导致卸载残留，进而导致无法重新安装该程序（报INSTALL_FAILED_UID_CHANGED错误）；<br>
 * 2、某些设备上FileLock会导致僵尸进程出现进而导致耗电；<br>
 * 3、僵尸进程出现后，正常进程的FileLock会一直阻塞等待僵尸进程中的FileLock释放，导致进程一直阻塞；<br>
 * 
 * @author seven456@gmail.com
 * @version	1.0
 * @since JDK1.6
 *
 */
public class MultiProcessSharedPreferences extends ContentProvider implements SharedPreferences {
    private static final String TAG = "MicroMsg.MultiProcessSharedPreferences";
    public static final boolean DEBUG = BuildConfig.DEBUG;
    private Context mContext;
    private String mName;
    private int mMode;
    private boolean mIsSafeMode;
    private List<SoftReference<OnSharedPreferenceChangeListener>> mListeners;
    private BroadcastReceiver mReceiver;

    private static String AUTHORITY;
    private static volatile Uri AUTHORITY_URI;
    private UriMatcher mUriMatcher;
    private static final String KEY = "value";
    private static final String KEY_NAME = "name";
    private static final String PATH_WILDCARD = "*/";
    private static final String PATH_GET_ALL = "getAll";
    private static final String PATH_GET_STRING = "getString";
    private static final String PATH_GET_INT = "getInt";
    private static final String PATH_GET_LONG = "getLong";
    private static final String PATH_GET_FLOAT = "getFloat";
    private static final String PATH_GET_BOOLEAN = "getBoolean";
    private static final String PATH_CONTAINS = "contains";
    private static final String PATH_APPLY = "apply";
    private static final String PATH_COMMIT = "commit";
    private static final String PATH_REGISTER_ON_SHARED_PREFERENCE_CHANGE_LISTENER =
        "registerOnSharedPreferenceChangeListener";
    private static final String PATH_UNREGISTER_ON_SHARED_PREFERENCE_CHANGE_LISTENER =
        "unregisterOnSharedPreferenceChangeListener";
    private static final int GET_ALL = 1;
    private static final int GET_STRING = 2;
    private static final int GET_INT = 3;
    private static final int GET_LONG = 4;
    private static final int GET_FLOAT = 5;
    private static final int GET_BOOLEAN = 6;
    private static final int CONTAINS = 7;
    private static final int APPLY = 8;
    private static final int COMMIT = 9;
    private static final int REGISTER_ON_SHARED_PREFERENCE_CHANGE_LISTENER = 10;
    private static final int UNREGISTER_ON_SHARED_PREFERENCE_CHANGE_LISTENER = 11;
    private Map<String, Integer> mListenersCount;

    private static class ReflectionUtil {

        public static ContentValues contentValuesNewInstance(HashMap<String, Object> values) {
            try {
                Constructor<ContentValues> c =
                    ContentValues.class.getDeclaredConstructor(new Class[] {HashMap.class}); // hide
                c.setAccessible(true);
                return c.newInstance(values);
            } catch (IllegalArgumentException e) {
                throw new RuntimeException(e);
            } catch (IllegalAccessException e) {
                throw new RuntimeException(e);
            } catch (InvocationTargetException e) {
                throw new RuntimeException(e);
            } catch (NoSuchMethodException e) {
                throw new RuntimeException(e);
            } catch (InstantiationException e) {
                throw new RuntimeException(e);
            }
        }

        public static Editor editorPutStringSet(Editor editor, String key, Set<String> values) {
            try {
                Method method = editor.getClass().getDeclaredMethod(
                    "putStringSet", new Class[] {String.class, Set.class}); // Android 3.0
                return (Editor) method.invoke(editor, key, values);
            } catch (IllegalArgumentException e) {
                throw new RuntimeException(e);
            } catch (IllegalAccessException e) {
                throw new RuntimeException(e);
            } catch (InvocationTargetException e) {
                throw new RuntimeException(e);
            } catch (NoSuchMethodException e) {
                throw new RuntimeException(e);
            }
        }

        public static void editorApply(Editor editor) {
            try {
                Method method = editor.getClass().getDeclaredMethod("apply"); // Android 2.3
                method.invoke(editor);
            } catch (IllegalArgumentException e) {
                throw new RuntimeException(e);
            } catch (IllegalAccessException e) {
                throw new RuntimeException(e);
            } catch (InvocationTargetException e) {
                throw new RuntimeException(e);
            } catch (NoSuchMethodException e) {
                throw new RuntimeException(e);
            }
        }
    }

    private void checkInitAuthority(Context context) {
        if (AUTHORITY_URI == null) {
            String AUTHORITY = null;
            Uri AUTHORITY_URI = MultiProcessSharedPreferences.AUTHORITY_URI;
            synchronized (MultiProcessSharedPreferences.this) {
                if (AUTHORITY_URI == null) {
                    AUTHORITY = queryAuthority(context);
                    AUTHORITY_URI = Uri.parse(ContentResolver.SCHEME_CONTENT + "://" + AUTHORITY);
                    if (DEBUG) {
                        //						Log.d(TAG, "checkInitAuthority.AUTHORITY = " + AUTHORITY);
                    }
                }
                if (AUTHORITY == null) {
                    throw new IllegalArgumentException("'AUTHORITY' initialize failed.");
                }
            }
            MultiProcessSharedPreferences.AUTHORITY = AUTHORITY;
            MultiProcessSharedPreferences.AUTHORITY_URI = AUTHORITY_URI;
        }
    }

    private static String queryAuthority(Context context) {
        PackageInfo packageInfos = null;
        try {
            PackageManager mgr = context.getPackageManager();
            if (mgr != null) {
                packageInfos = mgr.getPackageInfo(context.getPackageName(), PackageManager.GET_PROVIDERS);
            }
        } catch (NameNotFoundException e) {
            if (DEBUG) {
                //				Log.printErrStackTrace(TAG, e, "");
            }
        }
        if (packageInfos != null && packageInfos.providers != null) {
            for (ProviderInfo providerInfo : packageInfos.providers) {
                if (providerInfo.name.equals(MultiProcessSharedPreferences.class.getName())) {
                    return providerInfo.authority;
                }
            }
        }
        return null;
    }

    /**
	 * mode不使用{@link Context#MODE_MULTI_PROCESS}特可以支持多进程了；
	 * 
	 * @param mode
	 * 
	 * @see Context#MODE_PRIVATE
	 * @see Context#MODE_WORLD_READABLE
	 * @see Context#MODE_WORLD_WRITEABLE
	 */
    public static SharedPreferences getSharedPreferences(Context context, String name, int mode) {
        return new MultiProcessSharedPreferences(context, name, mode);
    }

    public MultiProcessSharedPreferences() {
    }

    private MultiProcessSharedPreferences(Context context, String name, int mode) {
        mContext = context;
        mName = name;
        mMode = mode;
        PackageManager mgr = context.getPackageManager();
        if (mgr != null) {
            mIsSafeMode =
                mgr.isSafeMode(); // 如果设备处在“安全模式”下，只有系统自带的ContentProvider才能被正常解析使用；
        }
    }

    @SuppressWarnings("unchecked")
    @Override
    public Map<String, ?> getAll() {
        return (Map<String, ?>) getValue(PATH_GET_ALL, null, null);
    }

    @Override
    public String getString(String key, String defValue) {
        String v = (String) getValue(PATH_GET_STRING, key, defValue);
        return v != null ? v : defValue;
    }

    // @Override // Android 3.0
    public Set<String> getStringSet(String key, Set<String> defValues) {
        synchronized (this) {
            @SuppressWarnings("unchecked")
            Set<String> v = (Set<String>) getValue(PATH_GET_STRING, key, defValues);
            return v != null ? v : defValues;
        }
    }

    @Override
    public int getInt(String key, int defValue) {
        Integer v = (Integer) getValue(PATH_GET_INT, key, defValue);
        return v != null ? v : defValue;
    }

    @Override
    public long getLong(String key, long defValue) {
        Long v = (Long) getValue(PATH_GET_LONG, key, defValue);
        return v != null ? v : defValue;
    }

    @Override
    public float getFloat(String key, float defValue) {
        Float v = (Float) getValue(PATH_GET_FLOAT, key, defValue);
        return v != null ? v : defValue;
    }

    @Override
    public boolean getBoolean(String key, boolean defValue) {
        Boolean v = (Boolean) getValue(PATH_GET_BOOLEAN, key, defValue);
        return v != null ? v : defValue;
    }

    @Override
    public boolean contains(String key) {
        Boolean v = (Boolean) getValue(PATH_CONTAINS, key, null);
        return v != null ? v : false;
    }

    @Override
    public Editor edit() {
        return new EditorImpl();
    }

    @Override
    public void registerOnSharedPreferenceChangeListener(OnSharedPreferenceChangeListener listener) {
        synchronized (this) {
            if (mListeners == null) {
                mListeners = new ArrayList<SoftReference<OnSharedPreferenceChangeListener>>();
            }
            Boolean result = (Boolean) getValue(PATH_REGISTER_ON_SHARED_PREFERENCE_CHANGE_LISTENER, null, false);
            if (result != null && result) {
                mListeners.add(new SoftReference<OnSharedPreferenceChangeListener>(listener));
                if (mReceiver == null) {
                    mReceiver = new BroadcastReceiver() {
                        @Override
                        public void onReceive(Context context, Intent intent) {
                            String name = intent.getStringExtra(KEY_NAME);
                            @SuppressWarnings("unchecked")
                            List<String> keysModified = (List<String>) intent.getSerializableExtra(KEY);
                            if (mName.equals(name) && keysModified != null) {
                                List arrayListeners;
                                synchronized (MultiProcessSharedPreferences.this) {
                                    arrayListeners = mListeners;
                                }
                                List<SoftReference<OnSharedPreferenceChangeListener>> listeners =
                                    new ArrayList<SoftReference<OnSharedPreferenceChangeListener>>(arrayListeners);
                                for (int i = keysModified.size() - 1; i >= 0; i--) {
                                    final String key = keysModified.get(i);
                                    for (SoftReference<OnSharedPreferenceChangeListener> srlistener : listeners) {
                                        OnSharedPreferenceChangeListener listener = srlistener.get();
                                        if (listener != null) {
                                            listener.onSharedPreferenceChanged(MultiProcessSharedPreferences.this, key);
                                        }
                                    }
                                }
                            }
                        }
                    };
                    mContext.registerReceiver(mReceiver, new IntentFilter(makeAction(mName)));
                }
            }
        }
    }

    @Override
    public void unregisterOnSharedPreferenceChangeListener(OnSharedPreferenceChangeListener listener) {
        synchronized (this) {
            getValue(PATH_UNREGISTER_ON_SHARED_PREFERENCE_CHANGE_LISTENER, null, false);
            if (mListeners != null) {
                List<SoftReference<OnSharedPreferenceChangeListener>> removing = new ArrayList<>();
                for (SoftReference<OnSharedPreferenceChangeListener> srlistener : mListeners) {
                    OnSharedPreferenceChangeListener listenerFromSR = srlistener.get();
                    if (listenerFromSR != null && listenerFromSR.equals(listener)) {
                        removing.add(srlistener);
                    }
                }
                for (SoftReference<OnSharedPreferenceChangeListener> srlistener : removing) {
                    mListeners.remove(srlistener);
                }
                if (mListeners.isEmpty() && mReceiver != null) {
                    mContext.unregisterReceiver(mReceiver);
                    mReceiver = null;
                    mListeners = null;
                }
            }
        }
    }

    public final class EditorImpl implements Editor {
        private final Map<String, Object> mModified = new HashMap<String, Object>();
        private boolean mClear = false;

        @Override
        public Editor putString(String key, String value) {
            synchronized (this) {
                mModified.put(key, value);
                return this;
            }
        }

        // @Override // Android 3.0
        public Editor putStringSet(String key, Set<String> values) {
            synchronized (this) {
                mModified.put(key, (values == null) ? null : new HashSet<String>(values));
                return this;
            }
        }

        @Override
        public Editor putInt(String key, int value) {
            synchronized (this) {
                mModified.put(key, value);
                return this;
            }
        }

        @Override
        public Editor putLong(String key, long value) {
            synchronized (this) {
                mModified.put(key, value);
                return this;
            }
        }

        @Override
        public Editor putFloat(String key, float value) {
            synchronized (this) {
                mModified.put(key, value);
                return this;
            }
        }

        @Override
        public Editor putBoolean(String key, boolean value) {
            synchronized (this) {
                mModified.put(key, value);
                return this;
            }
        }

        @Override
        public Editor remove(String key) {
            synchronized (this) {
                mModified.put(key, null);
                return this;
            }
        }

        @Override
        public Editor clear() {
            synchronized (this) {
                mClear = true;
                return this;
            }
        }

        @Override
        public void apply() {
            setValue(PATH_APPLY);
        }

        @Override
        public boolean commit() {
            return setValue(PATH_COMMIT);
        }

        private boolean setValue(String pathSegment) {
            if (mIsSafeMode) { // 如果设备处在“安全模式”，返回false；
                return false;
            } else {
                synchronized (MultiProcessSharedPreferences.this) {
                    checkInitAuthority(mContext);
                    String[] selectionArgs = new String[] {String.valueOf(mMode), String.valueOf(mClear)};
                    synchronized (this) {
                        Uri uri = Uri.withAppendedPath(Uri.withAppendedPath(AUTHORITY_URI, mName), pathSegment);
                        ContentValues values =
                            ReflectionUtil.contentValuesNewInstance((HashMap<String, Object>) mModified);
                        return mContext.getContentResolver().update(uri, values, null, selectionArgs) > 0;
                    }
                }
            }
        }
    }

    private Object getValue(String pathSegment, String key, Object defValue) {
        if (mIsSafeMode) { // 如果设备处在“安全模式”，返回null；
            return null;
        } else {
            checkInitAuthority(mContext);
            Object v = null;
            Uri uri = Uri.withAppendedPath(Uri.withAppendedPath(AUTHORITY_URI, mName), pathSegment);
            String[] selectionArgs =
                new String[] {String.valueOf(mMode), key, defValue == null ? null : String.valueOf(defValue)};
            Cursor cursor = mContext.getContentResolver().query(uri, null, null, selectionArgs, null);
            if (cursor != null) {
                try {
                    Bundle bundle = cursor.getExtras();
                    if (bundle != null) {
                        v = bundle.get(KEY);
                        bundle.clear();
                    }
                } catch (Exception e) {
                }
                cursor.close();
            }
            return v != null ? v : defValue;
        }
    }

    private String makeAction(String name) {
        return String.format("%1$s_%2$s", MultiProcessSharedPreferences.class.getName(), name);
    }

    @Override
    public boolean onCreate() {
        checkInitAuthority(getContext());
        mUriMatcher = new UriMatcher(UriMatcher.NO_MATCH);
        mUriMatcher.addURI(AUTHORITY, PATH_WILDCARD + PATH_GET_ALL, GET_ALL);
        mUriMatcher.addURI(AUTHORITY, PATH_WILDCARD + PATH_GET_STRING, GET_STRING);
        mUriMatcher.addURI(AUTHORITY, PATH_WILDCARD + PATH_GET_INT, GET_INT);
        mUriMatcher.addURI(AUTHORITY, PATH_WILDCARD + PATH_GET_LONG, GET_LONG);
        mUriMatcher.addURI(AUTHORITY, PATH_WILDCARD + PATH_GET_FLOAT, GET_FLOAT);
        mUriMatcher.addURI(AUTHORITY, PATH_WILDCARD + PATH_GET_BOOLEAN, GET_BOOLEAN);
        mUriMatcher.addURI(AUTHORITY, PATH_WILDCARD + PATH_CONTAINS, CONTAINS);
        mUriMatcher.addURI(AUTHORITY, PATH_WILDCARD + PATH_APPLY, APPLY);
        mUriMatcher.addURI(AUTHORITY, PATH_WILDCARD + PATH_COMMIT, COMMIT);
        mUriMatcher.addURI(AUTHORITY, PATH_WILDCARD + PATH_REGISTER_ON_SHARED_PREFERENCE_CHANGE_LISTENER,
                           REGISTER_ON_SHARED_PREFERENCE_CHANGE_LISTENER);
        mUriMatcher.addURI(AUTHORITY, PATH_WILDCARD + PATH_UNREGISTER_ON_SHARED_PREFERENCE_CHANGE_LISTENER,
                           UNREGISTER_ON_SHARED_PREFERENCE_CHANGE_LISTENER);
        return true;
    }

    @Override
    public Cursor query(Uri uri, String[] projection, String selection, String[] selectionArgs, String sortOrder) {
        String name = uri.getPathSegments().get(0);
        int mode = Integer.parseInt(selectionArgs[0]);
        String key = selectionArgs[1];
        String defValue = selectionArgs[2];
        Bundle bundle = new Bundle();
        switch (mUriMatcher.match(uri)) {
            case GET_ALL:
                bundle.putSerializable(KEY,
                                       (HashMap<String, ?>) getContext().getSharedPreferences(name, mode).getAll());
                break;
            case GET_STRING:
                bundle.putString(KEY, getContext().getSharedPreferences(name, mode).getString(key, defValue));
                break;
            case GET_INT:
                bundle.putInt(KEY,
                              getContext().getSharedPreferences(name, mode).getInt(key, Integer.parseInt(defValue)));
                break;
            case GET_LONG:
                bundle.putLong(KEY,
                               getContext().getSharedPreferences(name, mode).getLong(key, Long.parseLong(defValue)));
                break;
            case GET_FLOAT:
                bundle.putFloat(
                    KEY, getContext().getSharedPreferences(name, mode).getFloat(key, Float.parseFloat(defValue)));
                break;
            case GET_BOOLEAN:
                bundle.putBoolean(
                    KEY, getContext().getSharedPreferences(name, mode).getBoolean(key, Boolean.parseBoolean(defValue)));
                break;
            case CONTAINS:
                bundle.putBoolean(KEY, getContext().getSharedPreferences(name, mode).contains(key));
                break;
            case REGISTER_ON_SHARED_PREFERENCE_CHANGE_LISTENER: {
                checkInitListenersCount();
                Integer countInteger = mListenersCount.get(name);
                int count = (countInteger == null ? 0 : countInteger) + 1;
                mListenersCount.put(name, count);
                countInteger = mListenersCount.get(name);
                bundle.putBoolean(KEY, count == (countInteger == null ? 0 : countInteger));
            } break;
            case UNREGISTER_ON_SHARED_PREFERENCE_CHANGE_LISTENER: {
                checkInitListenersCount();
                Integer countInteger = mListenersCount.get(name);
                int count = (countInteger == null ? 0 : countInteger) - 1;
                if (count <= 0) {
                    mListenersCount.remove(name);
                    bundle.putBoolean(KEY, !mListenersCount.containsKey(name));
                } else {
                    mListenersCount.put(name, count);
                    countInteger = mListenersCount.get(name);
                    bundle.putBoolean(KEY, count == (countInteger == null ? 0 : countInteger));
                }
            } break;
            default:
                throw new IllegalArgumentException("This is Unknown Uri：" + uri);
        }
        return new BundleCursor(bundle);
    }

    @Override
    public String getType(Uri uri) {
        throw new UnsupportedOperationException("No external call");
    }

    @Override
    public Uri insert(Uri uri, ContentValues values) {
        throw new UnsupportedOperationException("No external insert");
    }

    @Override
    public int delete(Uri uri, String selection, String[] selectionArgs) {
        throw new UnsupportedOperationException("No external delete");
    }

    @SuppressWarnings("unchecked")
    @Override
    public int update(Uri uri, ContentValues values, String selection, String[] selectionArgs) {
        int result = 0;
        String name = uri.getPathSegments().get(0);
        int mode = Integer.parseInt(selectionArgs[0]);
        SharedPreferences preferences = getContext().getSharedPreferences(name, mode);
        int match = mUriMatcher.match(uri);
        switch (match) {
            case APPLY:
            case COMMIT:
                boolean hasListeners =
                    mListenersCount != null && mListenersCount.get(name) != null && mListenersCount.get(name) > 0;
                ArrayList<String> keysModified = null;
                Map<String, Object> map = new HashMap();
                if (hasListeners) {
                    keysModified = new ArrayList<String>();
                    map = (Map<String, Object>) preferences.getAll();
                }
                Editor editor = preferences.edit();
                boolean clear = Boolean.parseBoolean(selectionArgs[1]);
                if (clear) {
                    if (hasListeners && map != null && !map.isEmpty()) {
                        for (Map.Entry<String, Object> entry : map.entrySet()) {
                            keysModified.add(entry.getKey());
                        }
                    }
                    editor.clear();
                }
                for (Map.Entry<String, Object> entry : values.valueSet()) {
                    String k = entry.getKey();
                    Object v = entry.getValue();
                    // Android 5.L_preview : "this" is the magic value for a removal mutation. In addition,
                    // setting a value to "null" for a given key is specified to be
                    // equivalent to calling remove on that key.
                    if (v instanceof EditorImpl || v == null) {
                        editor.remove(k);
                        if (hasListeners && map != null && map.containsKey(k)) {
                            keysModified.add(k);
                        }
                    } else {
                        if (hasListeners && map != null
                            && (!map.containsKey(k) || (map.containsKey(k) && !v.equals(map.get(k))))) {
                            keysModified.add(k);
                        }
                    }

                    if (v instanceof String) {
                        editor.putString(k, (String) v);
                    } else if (v instanceof Set) {
                        ReflectionUtil.editorPutStringSet(editor, k,
                                                          (Set<String>) v); // Android 3.0
                    } else if (v instanceof Integer) {
                        editor.putInt(k, (Integer) v);
                    } else if (v instanceof Long) {
                        editor.putLong(k, (Long) v);
                    } else if (v instanceof Float) {
                        editor.putFloat(k, (Float) v);
                    } else if (v instanceof Boolean) {
                        editor.putBoolean(k, (Boolean) v);
                    }
                }
                if (hasListeners && keysModified.isEmpty()) {
                    result = 1;
                } else {
                    switch (match) {
                        case APPLY:
                            ReflectionUtil.editorApply(editor); // Android 2.3
                            result = 1;
                            // Okay to notify the listeners before it's hit disk
                            // because the listeners should always get the same
                            // SharedPreferences instance back, which has the
                            // changes reflected in memory.
                            notifyListeners(name, keysModified);
                            break;
                        case COMMIT:
                            if (editor.commit()) {
                                result = 1;
                                notifyListeners(name, keysModified);
                            }
                            break;
                    }
                }
                values.clear();
                break;
            default:
                throw new IllegalArgumentException("This is Unknown Uri：" + uri);
        }
        return result;
    }

    @Override
    public void onLowMemory() {
        if (mListenersCount != null) {
            mListenersCount.clear();
        }
        super.onLowMemory();
    }

    @Override
    public void onTrimMemory(int level) {
        if (mListenersCount != null) {
            mListenersCount.clear();
        }
        super.onTrimMemory(level);
    }

    private void checkInitListenersCount() {
        if (mListenersCount == null) {
            mListenersCount = new HashMap<String, Integer>();
        }
    }

    private void notifyListeners(String name, ArrayList<String> keysModified) {
        if (keysModified != null && !keysModified.isEmpty()) {
            Intent intent = new Intent();
            intent.setAction(makeAction(name));
            intent.setPackage(getContext().getPackageName());
            intent.putExtra(KEY_NAME, name);
            intent.putExtra(KEY, keysModified);
            getContext().sendBroadcast(intent);
        }
    }

    private static final class BundleCursor extends MatrixCursor {
        private Bundle mBundle;

        public BundleCursor(Bundle extras) {
            super(new String[] {}, 0);
            mBundle = extras;
        }

        @Override
        public Bundle getExtras() {
            return mBundle;
        }

        @Override
        public Bundle respond(Bundle extras) {
            mBundle = extras;
            return mBundle;
        }
    }
}
