package com.etlegacy.app;

import android.app.Application;
import android.app.Instrumentation;
import android.util.DisplayMetrics;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;

import com.robotium.solo.Solo;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import static android.os.SystemClock.sleep;
import static org.junit.Assert.assertEquals;

/**
 * Instrumentation test, which will execute on an Android device.
 *
 * @see <a href="http://d.android.com/tools/testing">Testing documentation</a>
 */
@RunWith(AndroidJUnit4.class)
public class TestETL {

    private Solo solo;
    Instrumentation instrumentation;
    Application app;

    @Before
    public void setUp() {
        instrumentation = InstrumentationRegistry.getInstrumentation();
        app = (Application) instrumentation.getTargetContext().getApplicationContext();
        instrumentation.runOnMainSync(() -> instrumentation.callApplicationOnCreate(app));
    }

    @Test
    public void clickOnNickEntry() {
        // Perform click on Nickname Entry
        // TODO: Implement

        assertEquals("com.etlegacy.app", app.getPackageName());
        DisplayMetrics displaymetrics = new DisplayMetrics();

        if (app.getApplicationContext().getClass().getSimpleName().equals("ETLActivity")) {
            sleep(10000);
            solo.drag(0, displaymetrics.widthPixels, 0, (float) displaymetrics.heightPixels / 2, 100);
        }

    }

}
