import { abilityAccessCtrl, AbilityConstant, ConfigurationConstant, UIAbility, Want } from '@kit.AbilityKit';
import { hilog } from '@kit.PerformanceAnalysisKit';
import { window } from '@kit.ArkUI';
import { i18n } from '@kit.LocalizationKit';
import { inputMethod } from '@kit.IMEKit';
import pointer from "@ohos.multimodalInput.pointer"
import sdl from 'libSDL3.so';

const DOMAIN = 0x0000;

// 与画布背景色一致（platform_sdl/sdl_view.cpp 的 kBg）
const APP_BG = '#18181C';

// 让状态栏/导航栏融入应用背景。
//
// 背景：SDL 的鸿蒙后端完全没有处理系统栏（连安全区设置都是 #if 0 FIXME），
// 所以状态栏用的是系统默认底色（白或黑），与深色画布割裂。
//
// 为什么不写在 onWindowStageCreate：SDL 会把自己的原生实现挂到 ability
// 的原型上，接管 onForeground / onWindowStageCreate 等回调，覆盖不了。
// 因此从 onCreate 异步取主窗口；窗口尚未创建时延迟重试。
function applySystemBars(ability: UIAbility, attempt: number): void {
  window.getLastWindow(ability.context).then((win: window.Window) => {
    win.setWindowSystemBarProperties({
      statusBarColor: APP_BG,
      statusBarContentColor: '#FFFFFF',
      navigationBarColor: APP_BG,
      navigationBarContentColor: '#FFFFFF'
    });
    hilog.info(DOMAIN, 'Cjing', '%{public}s', '系统栏已融入应用背景');
  }).catch(() => {
    if (attempt < 25) {
      setTimeout(() => applySystemBars(ability, attempt + 1), 200);
    }
  });
}

export default class EntryAbility extends UIAbility {
  onCreate(want: Want, launchParam: AbilityConstant.LaunchParam): void {
    hilog.info(DOMAIN, 'SDL/STARTUP', '%{public}s', 'Handing control from ArkTS to native SDL.');
    try {
        // This hands stuff to SDL's C code that it needs to deal with ArkTS interfaces. We are trying not to write scripting code, if we can
        //  handle it awkwardly in C through NAPI calls, so we have a _tiny_ surface area of risk where the script and the native code can go
        //  out of sync. This function call will add methods to `this`, such as onWindowStageCreate(), so we can drive everything from C and
        //  have almost no ArkTS code at all.
        // Which is to say: if this function call fails, your app won't actually start.
        //
        // ONLY EVER ADD PARAMETERS TO THIS FUNCTION CALL. THE C CODE WILL ABORT IF IT DOESN'T GET THE EXACT NUMBER IT EXPECTS.
        sdl.provideArkTSObjects(this, abilityAccessCtrl.createAtManager(), i18n.System, pointer.setPointerVisibleSync, inputMethod.getController());
    } catch (e) {
        hilog.info(DOMAIN, 'SDL/STARTUP', '%{public}s', 'Exception thrown! App will not start.');
    }

    // 系统栏融入背景（不影响 SDL 的窗口/渲染流程）
    applySystemBars(this, 0);
  }
}

