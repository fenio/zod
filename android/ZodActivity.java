package io.github.fenio.zod;

import org.libsdl.app.SDLActivity;

// Entry activity: SDLActivity does all the JNI/EGL/event plumbing; we only tell
// it which native libraries to load, in dependency order, ending with our
// engine (libmain.so).
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
}
