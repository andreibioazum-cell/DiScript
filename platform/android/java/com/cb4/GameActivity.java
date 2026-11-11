package com.cb4;

import android.app.NativeActivity;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.graphics.Rect;
import android.os.Build;
import android.os.Bundle;
import android.os.ParcelFileDescriptor;
import android.text.Editable;
import android.text.InputFilter;
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
import android.widget.TextView;
import android.widget.FrameLayout;

public final class GameActivity extends NativeActivity {

    private static boolean nativeReady;
    static {
        try {
            System.loadLibrary("cb_game");
            nativeReady = true;
        } catch (UnsatisfiedLinkError error) {
            nativeReady = false;
        }
    }

    private static final String POST_NOTIFICATIONS = "android.permission.POST_NOTIFICATIONS";
    private static final int REQUEST_NOTIFICATIONS = 4201;

    private EditText chatEditor;
    private boolean syncingFromNative;
    private boolean keyboardWasVisible;

    private volatile boolean wantKeyboard;

    private volatile boolean editorActive;

    private volatile boolean imeLooksVisible;
    private int showAttempts;

    private native void nativeBackspaceKey(int action);
    private native void nativeReplaceText(String text);
    private native void nativeSubmitText();
    private native void nativeKeyboardHidden();

    private void replaceTextNative(String text) {
        if (nativeReady) try { nativeReplaceText(text); } catch (UnsatisfiedLinkError ignored) { }
    }
    private void submitTextNative() {
        if (nativeReady) try { nativeSubmitText(); } catch (UnsatisfiedLinkError ignored) { }
    }
    private void keyboardHiddenNative() {
        if (nativeReady) try { nativeKeyboardHidden(); } catch (UnsatisfiedLinkError ignored) { }
    }

    private static final int REQUEST_PICK_ZIP = 4203;
    private static final int REQUEST_SAVE_ZIP = 4204;

    private native void nativePickedFd(int fd);
    private native void nativeSavedFd(int fd);

    private void handFdNative(int fd, boolean saving) {
        if (!nativeReady) {
            closeQuietly(fd);
            return;
        }
        try {
            if (saving) nativeSavedFd(fd); else nativePickedFd(fd);
        } catch (UnsatisfiedLinkError ignored) {
            closeQuietly(fd);
        }
    }

    private static void closeQuietly(int fd) {
        try {
            ParcelFileDescriptor.adoptFd(fd).close();
        } catch (Exception ignored) {
            // Nothing to release.
        }
    }

    /** System file manager: choose a mod .zip to import. */
    public void pickModZip() {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                try {
                    Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
                    intent.addCategory(Intent.CATEGORY_OPENABLE);
                    intent.setType("*/*");
                    startActivityForResult(intent, REQUEST_PICK_ZIP);
                } catch (RuntimeException ignored) {
                    // No document picker on this device; the game falls back to its own browser.
                }
            }
        });
    }

    /** System file manager: choose where to save the exported .zip. */
    public void saveModZip(final String name) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                try {
                    Intent intent = new Intent(Intent.ACTION_CREATE_DOCUMENT);
                    intent.addCategory(Intent.CATEGORY_OPENABLE);
                    intent.setType("application/octet-stream");
                    intent.putExtra(Intent.EXTRA_TITLE, name == null ? "mod.zip" : name);
                    startActivityForResult(intent, REQUEST_SAVE_ZIP);
                } catch (RuntimeException ignored) {
                    // Same fallback as above.
                }
            }
        });
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (resultCode != RESULT_OK || data == null || data.getData() == null)
            return;
        if (requestCode != REQUEST_PICK_ZIP && requestCode != REQUEST_SAVE_ZIP)
            return;
        boolean saving = requestCode == REQUEST_SAVE_ZIP;
        try {
            ParcelFileDescriptor descriptor =
                    getContentResolver().openFileDescriptor(data.getData(), saving ? "w" : "r");
            if (descriptor == null)
                return;
            handFdNative(descriptor.detachFd(), saving);
        } catch (Exception ignored) {
            // The document could not be opened; the game shows its own message.
        }
    }

    /** Android 13+ asks for notification permission at runtime; older versions grant it. */
    private void requestNotificationPermission() {
        if (Build.VERSION.SDK_INT >= 33
                && checkSelfPermission(POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED) {
            requestPermissions(new String[] { POST_NOTIFICATIONS }, REQUEST_NOTIFICATIONS);
        }
    }

    @SuppressWarnings("deprecation")
    private void enterImmersiveMode() {
        getWindow().getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                        | View.SYSTEM_UI_FLAG_FULLSCREEN
                        | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                        | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                        | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                        | View.SYSTEM_UI_FLAG_LAYOUT_STABLE);
    }

    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);
        getWindow().setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE);

        enterImmersiveMode();

        requestNotificationPermission();
        try {
            PresenceJobService.schedule(this);
        } catch (RuntimeException ignored) {
            // The background call is optional; the game must start regardless.
        }

        chatEditor = new EditText(this);
        chatEditor.setSingleLine(true);

        chatEditor.setTextColor(Color.TRANSPARENT);
        chatEditor.setHintTextColor(Color.TRANSPARENT);
        chatEditor.setBackgroundColor(Color.TRANSPARENT);
        chatEditor.setCursorVisible(false);
        chatEditor.setAlpha(1f);
        chatEditor.setGravity(Gravity.TOP | Gravity.START);
        chatEditor.setFocusable(true);
        chatEditor.setFocusableInTouchMode(true);
        chatEditor.setClickable(false);
        chatEditor.setLongClickable(false);

        chatEditor.setOnTouchListener(new View.OnTouchListener() {
            @Override
            public boolean onTouch(View view, android.view.MotionEvent event) {
                if (event.getAction() == android.view.MotionEvent.ACTION_DOWN) {
                    hideGameKeyboard();
                }
                return true;
            }
        });

        chatEditor.setInputType(InputType.TYPE_CLASS_TEXT
                | InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS
                | InputType.TYPE_TEXT_VARIATION_FILTER);
        chatEditor.setImeOptions(EditorInfo.IME_ACTION_DONE
                | EditorInfo.IME_FLAG_NO_EXTRACT_UI
                | EditorInfo.IME_FLAG_NO_FULLSCREEN);
        chatEditor.setFilters(new InputFilter[] { new InputFilter.LengthFilter(95) });
        chatEditor.setVisibility(View.INVISIBLE);

        float density = getResources().getDisplayMetrics().density;
        int editorH = Math.max(48, (int) (48f * density));
        FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT, editorH, Gravity.TOP);
        addContentView(chatEditor, params);

        /* IMEs that delete through InputConnection are handled by EditText's
         * own repeat. IMEs / hardware keys that send KeyEvents go to the C
         * timer below, and are consumed here so EditText does not double-delete. */
        chatEditor.setOnKeyListener(new View.OnKeyListener() {
            @Override public boolean onKey(View view, int keyCode, KeyEvent event) {
                if (!nativeReady || !wantKeyboard) return false;
                if (keyCode != KeyEvent.KEYCODE_DEL && keyCode != KeyEvent.KEYCODE_FORWARD_DEL) return false;
                int action = event.getAction();
                if (action == KeyEvent.ACTION_DOWN || action == KeyEvent.ACTION_UP) {
                    try { nativeBackspaceKey(action); } catch (UnsatisfiedLinkError ignored) { return false; }
                    return true;
                }
                return false;
            }
        });
        chatEditor.addTextChangedListener(new TextWatcher() {
            @Override public void beforeTextChanged(CharSequence s, int start, int count, int after) { }
            @Override public void onTextChanged(CharSequence s, int start, int before, int count) { }
            @Override public void afterTextChanged(Editable value) {
                if (syncingFromNative) return;
                replaceTextNative(value.toString());
            }
        });
        chatEditor.setOnFocusChangeListener(new View.OnFocusChangeListener() {
            @Override
            public void onFocusChange(View view, boolean focused) {
                if (focused) {
                    editorActive = nativeReady && wantKeyboard
                            && chatEditor.getVisibility() == View.VISIBLE;
                    return;
                }

                if (wantKeyboard && chatEditor.getVisibility() == View.VISIBLE) {
                    chatEditor.post(new Runnable() {
                        @Override public void run() { claimEditorFocus(); }
                    });
                } else {
                    editorActive = false;
                }
            }
        });
        chatEditor.setOnEditorActionListener(new TextView.OnEditorActionListener() {
            @Override
            public boolean onEditorAction(TextView view, int actionId, KeyEvent event) {
                boolean enter = actionId == EditorInfo.IME_ACTION_SEND
                        || actionId == EditorInfo.IME_ACTION_DONE
                        || (event != null && event.getKeyCode() == KeyEvent.KEYCODE_ENTER
                            && event.getAction() == KeyEvent.ACTION_DOWN);
                if (enter) {
                    submitTextNative();
                    return true;
                }
                return false;
            }
        });

        chatEditor.getRootView().getViewTreeObserver().addOnGlobalLayoutListener(
                new ViewTreeObserver.OnGlobalLayoutListener() {
                    @Override
                    public void onGlobalLayout() {
                        View root = chatEditor.getRootView();
                        Rect visible = new Rect();
                        root.getWindowVisibleDisplayFrame(visible);
                        boolean keyboardVisible = root.getHeight() - visible.bottom
                                > root.getHeight() * 0.15f;
                        imeLooksVisible = keyboardVisible;
                        if (wantKeyboard) claimEditorFocus();
                        if (keyboardVisible) {
                            keyboardWasVisible = true;
                        } else if (keyboardWasVisible) {
                            keyboardWasVisible = false;
                            keyboardHiddenNative();
                        }
                    }
                });
    }

    /** Post exit notification immediately without queuing on UI thread. */
    public void showByeNotice(final String text) {
        try {
            GameNotifier.postBye(getApplicationContext(), text);
        } catch (RuntimeException ignored) {
            // The goodbye is optional; the game must never be blocked on exit.
        }
    }

    private void claimEditorFocus() {
        if (chatEditor == null || !wantKeyboard) return;
        if (chatEditor.getVisibility() != View.VISIBLE) chatEditor.setVisibility(View.VISIBLE);
        if (!chatEditor.hasFocus()) chatEditor.requestFocus();
        editorActive = nativeReady;
    }

    public void showGameKeyboard(final String currentText) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                if (chatEditor == null) return;
                wantKeyboard = true;
                chatEditor.setVisibility(View.VISIBLE);
                chatEditor.bringToFront();
                replaceEditorText(currentText);
                claimEditorFocus();
                getWindow().setSoftInputMode(
                        WindowManager.LayoutParams.SOFT_INPUT_STATE_ALWAYS_VISIBLE
                                | WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE);
                if (imeLooksVisible) {

                    claimEditorFocus();
                    return;
                }
                showAttempts = 0;
                requestShowWhenReady();
            }
        });
    }

    private void requestShowWhenReady() {
        if (chatEditor == null) return;
        final int attempt = showAttempts++;
        if (attempt >= 10) return;
        chatEditor.postDelayed(new Runnable() {
            @Override
            public void run() {
                if (chatEditor == null || !wantKeyboard || imeLooksVisible) return;
                claimEditorFocus();
                InputMethodManager input = (InputMethodManager)
                        getSystemService(Context.INPUT_METHOD_SERVICE);
                if (input != null) {
                    if (!input.isActive(chatEditor)) chatEditor.requestFocus();
                    input.showSoftInput(chatEditor, InputMethodManager.SHOW_FORCED);
                }
                requestShowWhenReady();
            }
        }, attempt == 0 ? 60 : 120);
    }

    public void setGameKeyboardText(final String text) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                replaceEditorText(text);
                if (wantKeyboard) claimEditorFocus();
            }
        });
    }

    public boolean gameKeyboardActive() {
        return wantKeyboard;
    }

    /** Local wall clock in milliseconds, used by the in-game New Year countdown. */
    public long gameWallClockMillis() {
        return System.currentTimeMillis();
    }

    public void hideGameKeyboard() {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                if (chatEditor == null) return;
                wantKeyboard = false;
                InputMethodManager input = (InputMethodManager)
                        getSystemService(Context.INPUT_METHOD_SERVICE);
                if (input != null) {
                    input.hideSoftInputFromWindow(chatEditor.getWindowToken(), 0);
                }
                editorActive = false;
                imeLooksVisible = false;
                showAttempts = 10;
                getWindow().setSoftInputMode(
                        WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE);
                chatEditor.clearFocus();
                chatEditor.setVisibility(View.INVISIBLE);

                keyboardHiddenNative();
            }
        });
    }

    private void replaceEditorText(String text) {
        if (chatEditor == null) return;
        String safe = text == null ? "" : text;
        if (safe.contentEquals(chatEditor.getText())) return;
        boolean shrinking = safe.length() < chatEditor.length();
        syncingFromNative = true;
        chatEditor.setText(safe);
        chatEditor.setSelection(chatEditor.length());
        syncingFromNative = false;

        if (shrinking) {
            InputMethodManager input = (InputMethodManager)
                    getSystemService(Context.INPUT_METHOD_SERVICE);
            if (input != null) input.restartInput(chatEditor);
        }
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);

        if (hasFocus) enterImmersiveMode();
        if (hasFocus && wantKeyboard && chatEditor != null) {
            claimEditorFocus();
            if (!imeLooksVisible) {
                showAttempts = 0;
                requestShowWhenReady();
            }
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        enterImmersiveMode();
    }

    @Override
    protected void onPause() {
        hideGameKeyboard();
        super.onPause();
    }
}
