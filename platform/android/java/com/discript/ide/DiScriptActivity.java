package com.discript.ide;

import android.app.NativeActivity;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.Intent;
import android.database.Cursor;
import android.graphics.Color;
import android.graphics.Rect;
import android.net.Uri;
import android.os.Bundle;
import android.os.ParcelFileDescriptor;
import android.provider.OpenableColumns;
import android.text.Editable;
import android.text.InputType;
import android.text.TextWatcher;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewTreeObserver;
import android.view.WindowManager;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodManager;
import android.widget.EditText;
import android.widget.FrameLayout;

/**
 * Android NativeActivity bridge for DiScript IDE (Vulkan + C/C++).
 * Provides reliable soft/hardware keyboard streaming for code editing,
 * keyboard height inset reporting, system clipboard access, and SAF file dialogs.
 */
public final class DiScriptActivity extends NativeActivity {

    private static boolean nativeReady;
    static {
        try {
            System.loadLibrary("discript");
            nativeReady = true;
        } catch (UnsatisfiedLinkError error) {
            nativeReady = false;
        }
    }

    private static final String SENTINEL = "\u200B\u200B\u200B\u200B\u200B\u200B\u200B\u200B";
    private static final int SENTINEL_LEN = 8;

    private static final int REQ_OPEN_SCRIPT = 5101;
    private static final int REQ_SAVE_SCRIPT = 5102;

    private EditText codeProxy;
    private boolean mutatingSentinel;
    private volatile boolean wantKeyboard;
    private volatile boolean imeVisible;
    private int lastInsetPx = -1;
    private int showAttempts;

    private native void nativeEditorText(String text);
    private native void nativeEditorBackspace(int count);
    private native void nativeEditorEnter();
    private native boolean nativeEditorSpecialKey(int keyCode, int metaState);
    private native void nativeKeyboardInset(int bottomInsetPx, int visible);
    private native void nativePickedFd(int fd, String name);
    private native void nativeSavedFd(int fd);

    private void resetSentinel() {
        if (codeProxy == null) return;
        mutatingSentinel = true;
        codeProxy.setText(SENTINEL);
        codeProxy.setSelection(SENTINEL_LEN);
        mutatingSentinel = false;
    }

    @SuppressWarnings("deprecation")
    private void applyWindowChrome() {
        getWindow().getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                        | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                        | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                        | View.SYSTEM_UI_FLAG_FULLSCREEN);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getWindow().setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE);
        applyWindowChrome();

        codeProxy = new EditText(this);
        codeProxy.setSingleLine(false);
        codeProxy.setMaxLines(4);
        codeProxy.setTextColor(Color.TRANSPARENT);
        codeProxy.setHintTextColor(Color.TRANSPARENT);
        codeProxy.setBackgroundColor(Color.TRANSPARENT);
        codeProxy.setCursorVisible(false);
        codeProxy.setFocusable(true);
        codeProxy.setFocusableInTouchMode(true);
        codeProxy.setClickable(false);
        codeProxy.setLongClickable(false);
        codeProxy.setInputType(InputType.TYPE_CLASS_TEXT
                | InputType.TYPE_TEXT_FLAG_MULTI_LINE
                | InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS
                | InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD);
        codeProxy.setImeOptions(EditorInfo.IME_FLAG_NO_EXTRACT_UI
                | EditorInfo.IME_FLAG_NO_FULLSCREEN);
        codeProxy.setVisibility(View.INVISIBLE);

        FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(
                2, 2, Gravity.TOP | Gravity.START);
        addContentView(codeProxy, params);
        resetSentinel();

        codeProxy.setOnKeyListener(new View.OnKeyListener() {
            @Override
            public boolean onKey(View v, int keyCode, KeyEvent event) {
                if (!nativeReady) return false;
                if (event.getAction() != KeyEvent.ACTION_DOWN
                        && event.getAction() != KeyEvent.ACTION_MULTIPLE) {
                    return false;
                }
                int meta = event.getMetaState();
                boolean ctrl = (meta & KeyEvent.META_CTRL_ON) != 0;
                if (keyCode == KeyEvent.KEYCODE_DEL) {
                    try { nativeEditorBackspace(1); } catch (UnsatisfiedLinkError ignored) { }
                    resetSentinel();
                    return true;
                }
                if (keyCode == KeyEvent.KEYCODE_ENTER || keyCode == KeyEvent.KEYCODE_NUMPAD_ENTER) {
                    try { nativeEditorEnter(); } catch (UnsatisfiedLinkError ignored) { }
                    resetSentinel();
                    return true;
                }
                if (ctrl || isNavigationOrFunctionKey(keyCode)) {
                    try {
                        if (nativeEditorSpecialKey(keyCode, meta)) {
                            resetSentinel();
                            return true;
                        }
                    } catch (UnsatisfiedLinkError ignored) { }
                }
                return false;
            }
        });

        codeProxy.addTextChangedListener(new TextWatcher() {
            @Override public void beforeTextChanged(CharSequence s, int start, int count, int after) { }
            @Override public void onTextChanged(CharSequence s, int start, int before, int count) { }
            @Override
            public void afterTextChanged(Editable editable) {
                if (mutatingSentinel || !nativeReady) return;
                String raw = editable.toString();
                int zwspCount = 0;
                StringBuilder payload = new StringBuilder();
                for (int i = 0; i < raw.length(); i++) {
                    char ch = raw.charAt(i);
                    if (ch == '\u200B') {
                        zwspCount++;
                    } else if (ch != '\r') {
                        payload.append(ch);
                    }
                }
                int deleted = SENTINEL_LEN - zwspCount;
                if (deleted > 0) {
                    try { nativeEditorBackspace(deleted); } catch (UnsatisfiedLinkError ignored) { }
                }
                if (payload.length() > 0) {
                    String text = payload.toString();
                    int start = 0;
                    for (int i = 0; i <= text.length(); i++) {
                        if (i == text.length() || text.charAt(i) == '\n') {
                            if (i > start) {
                                String seg = text.substring(start, i);
                                try { nativeEditorText(seg); } catch (UnsatisfiedLinkError ignored) { }
                            }
                            if (i < text.length() && text.charAt(i) == '\n') {
                                try { nativeEditorEnter(); } catch (UnsatisfiedLinkError ignored) { }
                            }
                            start = i + 1;
                        }
                    }
                }
                resetSentinel();
            }
        });

        codeProxy.getRootView().getViewTreeObserver().addOnGlobalLayoutListener(
                new ViewTreeObserver.OnGlobalLayoutListener() {
                    @Override
                    public void onGlobalLayout() {
                        View root = codeProxy.getRootView();
                        Rect visible = new Rect();
                        root.getWindowVisibleDisplayFrame(visible);
                        int rootH = root.getHeight();
                        int diff = Math.max(0, rootH - visible.bottom);
                        boolean kbNow = rootH > 0 && diff > rootH * 0.14f;
                        imeVisible = kbNow;
                        int inset = kbNow ? diff : 0;
                        if (inset != lastInsetPx) {
                            lastInsetPx = inset;
                            if (nativeReady) {
                                try {
                                    nativeKeyboardInset(inset, kbNow ? 1 : 0);
                                } catch (UnsatisfiedLinkError ignored) { }
                            }
                        }
                        if (wantKeyboard && kbNow && !codeProxy.hasFocus()) {
                            codeProxy.requestFocus();
                        }
                    }
                });
    }

    private static boolean isNavigationOrFunctionKey(int keyCode) {
        switch (keyCode) {
            case KeyEvent.KEYCODE_DPAD_LEFT:
            case KeyEvent.KEYCODE_DPAD_RIGHT:
            case KeyEvent.KEYCODE_DPAD_UP:
            case KeyEvent.KEYCODE_DPAD_DOWN:
            case KeyEvent.KEYCODE_MOVE_HOME:
            case KeyEvent.KEYCODE_MOVE_END:
            case KeyEvent.KEYCODE_PAGE_UP:
            case KeyEvent.KEYCODE_PAGE_DOWN:
            case KeyEvent.KEYCODE_TAB:
            case KeyEvent.KEYCODE_FORWARD_DEL:
            case KeyEvent.KEYCODE_ESCAPE:
            case KeyEvent.KEYCODE_F5:
                return true;
            default:
                return false;
        }
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        if (nativeReady && event != null) {
            int action = event.getAction();
            int keyCode = event.getKeyCode();
            int meta = event.getMetaState();
            boolean ctrl = (meta & KeyEvent.META_CTRL_ON) != 0;
            if ((action == KeyEvent.ACTION_DOWN || action == KeyEvent.ACTION_MULTIPLE)
                    && (ctrl || isNavigationOrFunctionKey(keyCode))) {
                try {
                    if (nativeEditorSpecialKey(keyCode, meta)) {
                        return true;
                    }
                } catch (UnsatisfiedLinkError ignored) { }
            }
        }
        return super.dispatchKeyEvent(event);
    }

    public void showCodeKeyboard() {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                if (codeProxy == null) return;
                wantKeyboard = true;
                codeProxy.setVisibility(View.VISIBLE);
                resetSentinel();
                codeProxy.requestFocus();
                showAttempts = 0;
                pumpShowSoftInput();
            }
        });
    }

    private void pumpShowSoftInput() {
        if (codeProxy == null || !wantKeyboard) return;
        final int attempt = showAttempts++;
        if (attempt >= 8) return;
        codeProxy.postDelayed(new Runnable() {
            @Override
            public void run() {
                if (codeProxy == null || !wantKeyboard || imeVisible) return;
                codeProxy.requestFocus();
                InputMethodManager imm = (InputMethodManager)
                        getSystemService(Context.INPUT_METHOD_SERVICE);
                if (imm != null) {
                    imm.showSoftInput(codeProxy, InputMethodManager.SHOW_FORCED);
                }
                pumpShowSoftInput();
            }
        }, attempt == 0 ? 40 : 110);
    }

    public void hideCodeKeyboard() {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                if (codeProxy == null) return;
                wantKeyboard = false;
                imeVisible = false;
                showAttempts = 10;
                InputMethodManager imm = (InputMethodManager)
                        getSystemService(Context.INPUT_METHOD_SERVICE);
                if (imm != null) {
                    imm.hideSoftInputFromWindow(codeProxy.getWindowToken(), 0);
                }
                codeProxy.clearFocus();
                codeProxy.setVisibility(View.INVISIBLE);
                lastInsetPx = 0;
                if (nativeReady) {
                    try { nativeKeyboardInset(0, 0); } catch (UnsatisfiedLinkError ignored) { }
                }
            }
        });
    }

    public boolean isCodeKeyboardVisible() {
        return wantKeyboard || imeVisible;
    }

    public void setClipboardText(final String text) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                try {
                    ClipboardManager cm = (ClipboardManager) getSystemService(Context.CLIPBOARD_SERVICE);
                    if (cm != null) {
                        cm.setPrimaryClip(ClipData.newPlainText("DiScript", text == null ? "" : text));
                    }
                } catch (RuntimeException ignored) { }
            }
        });
    }

    public String getClipboardText() {
        try {
            ClipboardManager cm = (ClipboardManager) getSystemService(Context.CLIPBOARD_SERVICE);
            if (cm != null && cm.hasPrimaryClip()) {
                ClipData clip = cm.getPrimaryClip();
                if (clip != null && clip.getItemCount() > 0) {
                    CharSequence cs = clip.getItemAt(0).coerceToText(this);
                    if (cs != null) return cs.toString();
                }
            }
        } catch (RuntimeException ignored) { }
        return "";
    }

    public long wallClockMillis() {
        return System.currentTimeMillis();
    }

    public void pickScriptFile() {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                try {
                    Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
                    intent.addCategory(Intent.CATEGORY_OPENABLE);
                    intent.setType("*/*");
                    startActivityForResult(intent, REQ_OPEN_SCRIPT);
                } catch (RuntimeException ignored) { }
            }
        });
    }

    public void saveScriptFile(final String name) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                try {
                    Intent intent = new Intent(Intent.ACTION_CREATE_DOCUMENT);
                    intent.addCategory(Intent.CATEGORY_OPENABLE);
                    intent.setType("text/plain");
                    intent.putExtra(Intent.EXTRA_TITLE,
                            (name == null || name.isEmpty()) ? "script.ds" : name);
                    startActivityForResult(intent, REQ_SAVE_SCRIPT);
                } catch (RuntimeException ignored) { }
            }
        });
    }

    private String queryDisplayName(Uri uri) {
        Cursor cursor = null;
        try {
            cursor = getContentResolver().query(uri, null, null, null, null);
            if (cursor != null && cursor.moveToFirst()) {
                int idx = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME);
                if (idx >= 0) {
                    String name = cursor.getString(idx);
                    if (name != null && !name.isEmpty()) return name;
                }
            }
        } catch (Exception ignored) {
        } finally {
            if (cursor != null) cursor.close();
        }
        return "imported.ds";
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (resultCode != RESULT_OK || data == null || data.getData() == null) return;
        if (requestCode != REQ_OPEN_SCRIPT && requestCode != REQ_SAVE_SCRIPT) return;
        boolean saving = requestCode == REQ_SAVE_SCRIPT;
        Uri uri = data.getData();
        try {
            ParcelFileDescriptor pfd = getContentResolver().openFileDescriptor(uri, saving ? "wt" : "r");
            if (pfd == null) return;
            int fd = pfd.detachFd();
            if (!nativeReady) {
                ParcelFileDescriptor.adoptFd(fd).close();
                return;
            }
            if (saving) {
                nativeSavedFd(fd);
            } else {
                nativePickedFd(fd, queryDisplayName(uri));
            }
        } catch (Exception ignored) { }
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) applyWindowChrome();
    }

    @Override
    protected void onResume() {
        super.onResume();
        applyWindowChrome();
    }

    @Override
    protected void onPause() {
        hideCodeKeyboard();
        super.onPause();
    }
}
