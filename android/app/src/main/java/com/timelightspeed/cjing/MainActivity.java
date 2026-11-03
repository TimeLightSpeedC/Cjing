package com.timelightspeed.cjing;

/**
 * 应用入口。
 *
 * 继承 SDL 提供的 SDLActivity：它负责创建 SurfaceView、把触摸/键盘事件转给原生层、
 * 并按 getLibraries() 的返回值加载 .so。
 *
 * 注意：最后一个库必须是 "main"（即 libmain.so）——
 * SDLActivity 会用它作为本地入口（其中导出 SDL_AppInit 等回调）。
 */
public class MainActivity extends org.libsdl.app.SDLActivity {
    @Override
    protected String[] getLibraries() {
        return new String[] {
            "SDL3",
            "SDL3_ttf",
            "main",
        };
    }
}
