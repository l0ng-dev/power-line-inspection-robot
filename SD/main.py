import sys
import time
import gc

# Yahboom K230 上电自动启动入口。
# 上电时按住板载按键可跳过检测，便于进入 IDE 维护程序。
try:
    from ybUtils.YbKey import YbKey
    key = YbKey()
    abort_main = 0
    if key.is_pressed():
        time.sleep_ms(20)
        if key.is_pressed():
            abort_main = 1
except Exception:
    abort_main = 0

if not abort_main:
    APP_PATH = "/sdcard/mp_deployment_source"

    # 等待 SD 卡、摄像头及多媒体服务在上电后稳定。
    time.sleep_ms(2000)
    gc.collect()

    if APP_PATH not in sys.path:
        sys.path.append(APP_PATH)

    try:
        import det_video
        det_video.run()
    except BaseException as e:
        sys.print_exception(e)
        # 异常后不自动重复初始化 Sensor/Display，避免进入 already inited 循环。
        while True:
            time.sleep_ms(1000)
