package com.cb4;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Build;

/** Notification helper for online presence alerts and exit messages. */
final class GameNotifier {
    private static final String CHANNEL_ID = "online_call";
    private static final int NOTIFICATION_ID = 4201;
    private static final long COOLDOWN_MS = 3L * 60L * 60L * 1000L;
    private static final String PREFS = "presence";
    private static final String KEY_LAST_CALL = "last_call_ms";
    private static final String POST_PERMISSION = "android.permission.POST_NOTIFICATIONS";

    private static final String BYE_CHANNEL_ID = "farewell";
    private static final int BYE_NOTIFICATION_ID = 4202;

    private static final String TITLE = "Cubic Battle 4";
    private static final String TEXT =
            "\u041c\u043d\u0435 \u043e\u0434\u043d\u043e\u043c\u0443 \u0441 \u043d\u0438\u043c\u0438 "
            + "\u043d\u0435 \u0441\u043f\u0440\u0430\u0432\u0438\u0442\u044c\u0441\u044f! "
            + "\u041f\u043e\u043c\u043e\u0433\u0438, \u0437\u0430\u0439\u0434\u0438 \u0432 \u043e\u043d\u043b\u0430\u0439\u043d, "
            + "\u0442\u0430\u043c \u0443\u0436\u0435 \u0438\u0433\u0440\u043e\u043a\u0438!";
    private static final String CHANNEL_NAME = "\u041f\u0440\u0438\u0437\u044b\u0432 \u0432 \u043e\u043d\u043b\u0430\u0439\u043d";
    private static final String CHANNEL_DESC =
            "\u041a\u043e\u0433\u0434\u0430 \u0432 \u043e\u043d\u043b\u0430\u0439\u043d\u0435 \u0435\u0441\u0442\u044c "
            + "\u0434\u0440\u0443\u0433\u0438\u0435 \u0438\u0433\u0440\u043e\u043a\u0438";

    private static final String BYE_CHANNEL_NAME = "\u041f\u0440\u043e\u0449\u0430\u043d\u0438\u0435";
    private static final String BYE_CHANNEL_DESC =
            "\u041f\u0440\u043e\u0449\u0430\u043d\u0438\u0435 \u043f\u0440\u0438 \u0432\u044b\u0445\u043e\u0434\u0435 "
            + "\u0438\u0437 \u0438\u0433\u0440\u044b";

    private GameNotifier() { }

    /** True when a call may be posted now: permission, system toggle and cooldown allow it. */
    static boolean mayCall(Context context) {
        if (Build.VERSION.SDK_INT >= 33
                && context.checkSelfPermission(POST_PERMISSION) != PackageManager.PERMISSION_GRANTED) {
            return false;
        }
        NotificationManager manager = manager(context);
        if (manager == null || !manager.areNotificationsEnabled()) {
            return false;
        }
        long now = System.currentTimeMillis();
        long last = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE).getLong(KEY_LAST_CALL, 0L);
        // If the clock moved backwards (last > now), do not silence calls for hours.
        return !(last > 0 && now >= last && now - last < COOLDOWN_MS);
    }

    static void postCall(Context context) {
        NotificationManager manager = manager(context);
        if (manager == null) {
            return;
        }
        if (Build.VERSION.SDK_INT >= 26) {
            NotificationChannel channel =
                    new NotificationChannel(CHANNEL_ID, CHANNEL_NAME, NotificationManager.IMPORTANCE_DEFAULT);
            channel.setDescription(CHANNEL_DESC);
            manager.createNotificationChannel(channel);
        }
        Notification.Builder builder = new Notification.Builder(context, CHANNEL_ID)
                .setSmallIcon(smallIcon(context))
                .setContentTitle(TITLE)
                .setContentText(TEXT)
                .setStyle(new Notification.BigTextStyle().bigText(TEXT))
                .setAutoCancel(true);
        PendingIntent open = openGame(context);
        if (open != null) {
            builder.setContentIntent(open);
        }
        manager.notify(NOTIFICATION_ID, builder.build());
        context.getSharedPreferences(PREFS, Context.MODE_PRIVATE).edit()
                .putLong(KEY_LAST_CALL, System.currentTimeMillis())
                .apply();
    }

    /** Post exit notification when permissions and system settings allow it. */
    static void postBye(Context context, String text) {
        if (text == null || text.length() == 0) return;
        if (Build.VERSION.SDK_INT >= 33
                && context.checkSelfPermission(POST_PERMISSION) != PackageManager.PERMISSION_GRANTED) return;
        NotificationManager manager = manager(context);
        if (manager == null || !manager.areNotificationsEnabled()) return;
        if (Build.VERSION.SDK_INT >= 26) {
            NotificationChannel channel =
                    new NotificationChannel(BYE_CHANNEL_ID, BYE_CHANNEL_NAME, NotificationManager.IMPORTANCE_DEFAULT);
            channel.setDescription(BYE_CHANNEL_DESC);
            manager.createNotificationChannel(channel);
        }
        Notification.Builder builder = new Notification.Builder(context, BYE_CHANNEL_ID)
                .setSmallIcon(smallIcon(context))
                .setContentTitle(TITLE)
                .setContentText(text)
                .setStyle(new Notification.BigTextStyle().bigText(text))
                .setAutoCancel(true);
        PendingIntent open = openGame(context);
        if (open != null) {
            builder.setContentIntent(open);
        }
        manager.notify(BYE_NOTIFICATION_ID, builder.build());
    }

    private static NotificationManager manager(Context context) {
        return (NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE);
    }

    /** R is not generated by the APK build, so the icon is looked up by name. */
    private static int smallIcon(Context context) {
        int id = context.getResources().getIdentifier("ic_stat_cb4", "drawable", context.getPackageName());
        return id != 0 ? id : context.getApplicationInfo().icon;
    }

    private static PendingIntent openGame(Context context) {
        Intent launch = context.getPackageManager().getLaunchIntentForPackage(context.getPackageName());
        if (launch == null) {
            return null;
        }
        launch.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_SINGLE_TOP);
        return PendingIntent.getActivity(context, 0, launch,
                PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE);
    }
}
