package com.cb4;

import android.app.job.JobInfo;
import android.app.job.JobParameters;
import android.app.job.JobScheduler;
import android.app.job.JobService;
import android.content.ComponentName;
import android.content.Context;
import android.util.Log;

/** Periodic background JobService that checks online room presence via JNI. */
public final class PresenceJobService extends JobService {
    private static final String TAG = "CB4Presence";
    private static final int JOB_ID = 4201;
    /** JobScheduler does not run periodic jobs more often than this. */
    private static final long PERIOD_MS = 15L * 60L * 1000L;

    private static boolean nativeReady;
    static {
        try {
            System.loadLibrary("cb_game");
            nativeReady = true;
        } catch (UnsatisfiedLinkError error) {
            nativeReady = false;
        }
    }

    /** 1 = other player online, 2 = own account online, 0 = empty, <0 = skipped/error. */
    private static native int nativePresence(String dataDir);

    /** Idempotent: a job with the same id replaces the one already scheduled. */
    static void schedule(Context context) {
        JobScheduler scheduler = (JobScheduler) context.getSystemService(Context.JOB_SCHEDULER_SERVICE);
        if (scheduler == null) {
            return;
        }
        JobInfo job = new JobInfo.Builder(JOB_ID, new ComponentName(context, PresenceJobService.class))
                .setPeriodic(PERIOD_MS)
                .setRequiredNetworkType(JobInfo.NETWORK_TYPE_ANY)
                .setPersisted(true)
                .build();
        scheduler.schedule(job);
    }

    @Override
    public boolean onStartJob(final JobParameters params) {
        final Context app = getApplicationContext();
        if (!nativeReady || !GameNotifier.mayCall(app)) {
            return false;
        }
        final String dataDir = getFilesDir().getAbsolutePath();
        new Thread(new Runnable() {
            @Override
            public void run() {
                try {
                    int result = nativePresence(dataDir);
                    Log.i(TAG, "presence check: " + result);
                    if (result == 1 && GameNotifier.mayCall(app)) {
                        GameNotifier.postCall(app);
                    }
                } catch (Throwable error) {
                    Log.w(TAG, "presence check failed", error);
                } finally {
                    jobFinished(params, false);
                }
            }
        }, "cb4-presence").start();
        return true;
    }

    @Override
    public boolean onStopJob(JobParameters params) {
        return false;
    }
}
