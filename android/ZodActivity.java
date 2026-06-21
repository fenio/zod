package io.github.fenio.zod;

import android.os.Build;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import org.libsdl.app.SDLActivity;

// Entry activity: SDLActivity does all the JNI/EGL/event plumbing; we only tell
// it which native libraries to load, in dependency order, ending with our
// engine (libmain.so) - plus, for #157, drive immersive fullscreen ourselves.
public class ZodActivity extends SDLActivity
{
    @Override
    protected String[] getLibraries()
    {
        return new String[] {
            "SDL3",
            "SDL3_image",
            "SDL3_ttf",
            "SDL3_mixer",
            "main"
        };
    }

    // #157: hide the Android status / navigation / gesture bars during play. The
    // manifest's Fullscreen theme isn't enough on modern Android: from Android 15
    // (and the targetSdk-35 edge-to-edge enforcement) the system bars come back as
    // overlays that cut off the top of the game. We hide them programmatically and
    // re-apply on every focus gain, so they stay gone after a notification pull,
    // an app switch, or a transient swipe.
    @Override
    public void onWindowFocusChanged(boolean hasFocus)
    {
        super.onWindowFocusChanged(hasFocus);
        if(hasFocus) hideSystemBars();
    }

    private void hideSystemBars()
    {
        if(Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) // Android 11+ (API 30)
        {
            // Draw edge-to-edge so the area the bars vacate is actually used
            // (Android 15+ enforces edge-to-edge regardless), then hide the bars
            // and let a swipe reveal them only transiently.
            getWindow().setDecorFitsSystemWindows(false);

            WindowInsetsController controller = getWindow().getInsetsController();
            if(controller != null)
            {
                controller.hide(WindowInsets.Type.systemBars());
                controller.setSystemBarsBehavior(
                    WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
            }
        }
        else // legacy immersive-sticky for API 24-29
        {
            getWindow().getDecorView().setSystemUiVisibility(
                  View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_FULLSCREEN
                | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
        }
    }
}
