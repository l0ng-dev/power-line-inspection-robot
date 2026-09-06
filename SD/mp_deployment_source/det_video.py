import os
import ujson
import aicube
from media.sensor import *
from media.display import *
from media.media import *
from time import *
import nncase_runtime as nn
import ulab.numpy as np
import time
import utime
import image
import gc
import sys
import network
import socket
from machine import UART, FPIOA

# ide: 仅 IDE 虚拟屏预览（推荐 Yahboom 1G）
# lcd: 接 ST7701 屏幕; hdmi: 接 HDMI
display_mode="ide"
if display_mode=="lcd":
    DISPLAY_WIDTH = ALIGN_UP(800, 16)
    DISPLAY_HEIGHT = 480
elif display_mode=="hdmi":
    DISPLAY_WIDTH = ALIGN_UP(1280, 16)
    DISPLAY_HEIGHT = 720
else:
    DISPLAY_WIDTH = ALIGN_UP(640, 16)
    DISPLAY_HEIGHT = 480

OUT_RGB888P_WIDTH = ALIGN_UP(640, 16)
OUT_RGB888P_HEIGH = 480

color_four = [(255, 220, 20, 60), (255, 119, 11, 32), (255, 0, 0, 142), (255, 0, 0, 230),
        (255, 106, 0, 228), (255, 0, 60, 100), (255, 0, 80, 100), (255, 0, 0, 70),
        (255, 0, 0, 192), (255, 250, 170, 30), (255, 100, 170, 30), (255, 220, 220, 0),
        (255, 175, 116, 175), (255, 250, 0, 30), (255, 165, 42, 42), (255, 255, 77, 255),
        (255, 0, 226, 252), (255, 182, 182, 255), (255, 0, 82, 0), (255, 120, 166, 157),
        (255, 110, 76, 0), (255, 174, 57, 255), (255, 199, 100, 0), (255, 72, 0, 118),
        (255, 255, 179, 240), (255, 0, 125, 92), (255, 209, 0, 151), (255, 188, 208, 182),
        (255, 0, 220, 176), (255, 255, 99, 164), (255, 92, 0, 73), (255, 133, 129, 255),
        (255, 78, 180, 255), (255, 0, 228, 0), (255, 174, 255, 243), (255, 45, 89, 255),
        (255, 134, 134, 103), (255, 145, 148, 174), (255, 255, 208, 186),
        (255, 197, 226, 255), (255, 171, 134, 1), (255, 109, 63, 54), (255, 207, 138, 255),
        (255, 151, 0, 95), (255, 9, 80, 61), (255, 84, 105, 51), (255, 74, 65, 105),
        (255, 166, 196, 102), (255, 208, 195, 210), (255, 255, 109, 65), (255, 0, 143, 149),
        (255, 179, 0, 194), (255, 209, 99, 106), (255, 5, 121, 0), (255, 227, 255, 205),
        (255, 147, 186, 208), (255, 153, 69, 1), (255, 3, 95, 161), (255, 163, 255, 0),
        (255, 119, 0, 170), (255, 0, 182, 199), (255, 0, 165, 120), (255, 183, 130, 88),
        (255, 95, 32, 0), (255, 130, 114, 135), (255, 110, 129, 133), (255, 166, 74, 118),
        (255, 219, 142, 185), (255, 79, 210, 114), (255, 178, 90, 62), (255, 65, 70, 15),
        (255, 127, 167, 115), (255, 59, 105, 106), (255, 142, 108, 45), (255, 196, 172, 0),
        (255, 95, 54, 80), (255, 128, 76, 255), (255, 201, 57, 1), (255, 246, 0, 122),
        (255, 191, 162, 208)]

root_path="/sdcard/mp_deployment_source/"
config_path=root_path+"deploy_config.json"
deploy_conf={}
debug_mode=0                 # 量产运行必须关闭逐帧串口计时，串口打印会明显阻塞

if root_path not in sys.path:
    sys.path.append(root_path)
try:
    try:
        from k230_cloud_secret_local import WIFI_SSID, WIFI_PASSWORD, BEMFA_UID, IMAGE_TOPIC
    except ImportError:
        from k230_cloud_secret import WIFI_SSID, WIFI_PASSWORD, BEMFA_UID, IMAGE_TOPIC
    CLOUD_CONFIGURED = bool(WIFI_SSID and WIFI_PASSWORD and BEMFA_UID and IMAGE_TOPIC)
except Exception as cloud_config_error:
    CLOUD_CONFIGURED = False
    print("cloud config unavailable:", cloud_config_error)

CLOUD_UPLOAD_HOST = "images.bemfa.com"
CLOUD_UPLOAD_PORT = 80
CLOUD_UPLOAD_PATH = "/upload/v1/upimages.php"
CLOUD_RECONNECT_TIMEOUT_MS = 10000
CLOUD_SOCKET_TIMEOUT_S = 15
CLOUD_RESPONSE_LIMIT = 8192
cloud_upload_attempt_count = 0
cloud_upload_success_count = 0
cloud_upload_failure_count = 0
cloud_last_http_status = 0
cloud_last_url = ""

SAVE_ROOT = "/sdcard/"
SAVE_COOLDOWN_MS = 8000       # 降低JPEG编码和SD卡写入频率
SAVE_SCORE_THRESHOLD = 0.30   # 保存阈值稍高，降低误检照片
DISPLAY_SCORE_THRESHOLD = 0.25
BOX_STALE_MS = 220            # 较短缓存兼顾稳定和跟随性
SAVE_CONFIRM_FRAMES = 3       # 连续检出3帧才保存，过滤瞬时误检
ALERT_CLEAR_MS = 3000         # 连续3秒未检出才解除事件锁，避免漏检一帧导致重复上传
JPEG_QUALITY = 80             # 90改为80，大幅减少编码和写卡耗时
GC_INTERVAL_MS = 5000         # 降低GC频率，减少周期性停顿
# 使用 chn1 的 RGB565 原生帧保存，避免对绑定 VO 的 chn0 做 YUV 转换。
# chn1 不绑定显示层，适合抓取、标注和 JPEG 编码。
SAVE_ENABLED = True
SNAPSHOT_RETRY_LIMIT = 3       # 单次抓帧失败时有限重试，避免偶发错误终止识别
SNAPSHOT_RETRY_DELAY_MS = 50   # 重试前短暂让出媒体缓冲区
SNAPSHOT_TIMEOUT_MS = 1500     # 比默认1秒稍宽裕，兼顾恢复速度

# 最终部署板当前使用 IO32/IO33 对应 UART3；实际接线至少需要 TX 与 STM32 RX 共地。
ALERT_UART_TX_PIN = 32
ALERT_UART_RX_PIN = 33
ALERT_UART_BAUD = 115200
ALERT_DEVICE_ID = "K230_01"
UART_TEST_MESSAGE = "ALERT,break,K230_01\r\n"
UART_TEST_SEND_ON_START = False  # True: 只运行固定报文测试；False: 正常检测
UART_TEST_REPEAT_COUNT = 3       # 重复发送，避免一次性上电时序导致漏收
UART_TEST_INTERVAL_MS = 1000
ALERT_TYPE_BY_LABEL = {
    "break": "break",
    "heat_damage": "heat",
    "heat": "heat",
    "wear": "wear",
}
DISPLAY_LABEL_EN = {
    "break": "BREAK",
    "heat_damage": "HEAT",
    "heat": "HEAT",
    "wear": "WEAR",
}

class ScopedTiming:
    def __init__(self, info="", enable_profile=True):
        self.info = info
        self.enable_profile = enable_profile
    def __enter__(self):
        if self.enable_profile:
            self.start_time = time.time_ns()
        return self
    def __exit__(self, exc_type, exc_value, traceback):
        if self.enable_profile:
            elapsed_time = time.time_ns() - self.start_time
            print(f"{self.info} took {elapsed_time / 1000000:.2f} ms")

def read_deploy_config(config_path):
    with open(config_path, 'r') as json_file:
        config = ujson.load(json_file)
    required = ("kmodel_path", "categories", "confidence_threshold", "nms_threshold",
                "img_size", "num_classes", "nms_option", "model_type")
    for key in required:
        if key not in config:
            raise ValueError("deploy_config缺少字段: " + key)
    if len(config["categories"]) != config["num_classes"]:
        raise ValueError("categories数量与num_classes不一致")
    return config

def ensure_save_dirs(labels):
    for lb in labels:
        path = SAVE_ROOT + lb
        try:
            os.mkdir(path)
        except OSError:
            try:
                os.stat(path)
            except OSError:
                raise OSError("无法创建缺陷目录: " + path)

def ticks_elapsed(now_ms, then_ms):
    return utime.ticks_diff(now_ms, then_ms)

def init_cloud_wifi():
    """启动板载 Wi-Fi 连接，不阻塞视觉和串口初始化。"""
    if not CLOUD_CONFIGURED:
        print("cloud upload disabled: missing config")
        return None
    wifi = network.WLAN(network.STA_IF)
    wifi.connect(WIFI_SSID, WIFI_PASSWORD)
    print("cloud wifi connecting")
    return wifi

def socket_send_all(sock, data):
    offset = 0
    while offset < len(data):
        sent = sock.send(data[offset:])
        if sent is None or sent <= 0:
            raise RuntimeError("cloud socket send interrupted")
        offset += sent

def ensure_cloud_wifi_online(wifi):
    if wifi is None:
        return False
    if wifi.isconnected():
        return True
    print("cloud wifi reconnecting...")
    wifi.connect(WIFI_SSID, WIFI_PASSWORD)
    started = utime.ticks_ms()
    while not wifi.isconnected():
        os.exitpoint()
        if ticks_elapsed(utime.ticks_ms(), started) >= CLOUD_RECONNECT_TIMEOUT_MS:
            print("cloud wifi reconnect timeout, status:", wifi.status())
            return False
        utime.sleep_ms(200)
    print("cloud wifi reconnected:", wifi.ifconfig()[0])
    return True

def upload_photo_to_bemfa(wifi, image_path):
    """上传一张已关闭写句柄的 JPEG，并记录可由 IDE 观察的诊断量。"""
    global cloud_upload_attempt_count, cloud_upload_success_count
    global cloud_upload_failure_count, cloud_last_http_status, cloud_last_url
    if not ensure_cloud_wifi_online(wifi):
        cloud_upload_failure_count += 1
        print("cloud upload skipped: wifi offline")
        return False
    cloud_upload_attempt_count += 1
    cloud_last_http_status = 0
    cloud_last_url = ""
    sock = None
    try:
        image_size = os.stat(image_path)[6]
        address = socket.getaddrinfo(CLOUD_UPLOAD_HOST, CLOUD_UPLOAD_PORT)[0][-1]
        sock = socket.socket()
        sock.settimeout(CLOUD_SOCKET_TIMEOUT_S)
        sock.connect(address)
        header = (
            "POST %s HTTP/1.1\r\nHost: %s\r\nContent-Type: image/jpeg\r\n"
            "Content-Length: %d\r\nAuthorization: %s\r\nAuthtopic: %s\r\n"
            "Connection: close\r\n\r\n"
        ) % (CLOUD_UPLOAD_PATH, CLOUD_UPLOAD_HOST, image_size, BEMFA_UID, IMAGE_TOPIC)
        socket_send_all(sock, header.encode())
        with open(image_path, "rb") as image_file:
            while True:
                chunk = image_file.read(4096)
                if not chunk:
                    break
                socket_send_all(sock, chunk)
                os.exitpoint()
        response = bytearray()
        while True:
            chunk = sock.recv(1024)
            if not chunk:
                break
            if len(response) + len(chunk) > CLOUD_RESPONSE_LIMIT:
                raise RuntimeError("cloud response too large")
            response.extend(chunk)
            os.exitpoint()
        header_end = response.find(b"\r\n\r\n")
        status_line = bytes(response[:response.find(b"\r\n")]).decode("utf-8", "ignore")
        parts = status_line.split(" ")
        if len(parts) >= 2:
            cloud_last_http_status = int(parts[1])
        if header_end >= 0:
            result = ujson.loads(bytes(response[header_end + 4:]).decode("utf-8", "ignore"))
            cloud_last_url = result.get("url", "")
        if cloud_last_http_status == 200 and cloud_last_url:
            cloud_upload_success_count += 1
            print("cloud upload ok:", cloud_last_url)
            return True
        raise RuntimeError("unexpected cloud response: " + status_line)
    except Exception as error:
        cloud_upload_failure_count += 1
        print("cloud upload failed:", error)
        return False
    finally:
        if sock is not None:
            sock.close()

def init_alert_uart():
    """配置 IO32/IO33 并打开 UART3（115200, 8N1）。"""
    fpioa = FPIOA()
    fpioa.set_function(ALERT_UART_TX_PIN, FPIOA.UART3_TXD)
    fpioa.set_function(ALERT_UART_RX_PIN, FPIOA.UART3_RXD)
    return UART(
        UART.UART3,
        baudrate=ALERT_UART_BAUD,
        bits=UART.EIGHTBITS,
        parity=UART.PARITY_NONE,
        stop=UART.STOPBITS_ONE
    )
def send_alert(uart, alert_type):
    """发送一条完整告警行；返回是否完整写入 UART。"""
    if uart is None or alert_type not in ("break", "heat", "wear"):
        return False
    message = "ALERT,{0},{1}\r\n".format(alert_type, ALERT_DEVICE_ID)
    try:
        written = uart.write(message)
    except OSError as error:
        print("uart send failed:", alert_type, error)
        return False
    if written != len(message):
        print("uart send failed:", alert_type, written)
        return False
    print("alert sent:", message.strip())
    return True

def send_uart_test_alert(uart):
    """固定发送测试入口：ALERT,break,K230_01\\r\\n。"""
    if uart is None:
        return False
    try:
        written = uart.write(UART_TEST_MESSAGE)
    except OSError as error:
        print("uart test send failed:", error)
        return False
    if written != len(UART_TEST_MESSAGE):
        print("uart test send failed:", written)
        return False
    print("uart test sent:", UART_TEST_MESSAGE.strip())
    return True

def snapshot_with_retry(sensor, channel):
    """抓取一帧图像；仅对snapshot通道错误执行有限重试。"""
    last_error = None
    for attempt in range(SNAPSHOT_RETRY_LIMIT):
        try:
            return sensor.snapshot(chn=channel, timeout=SNAPSHOT_TIMEOUT_MS)
        except RuntimeError as error:
            if "snapshot chn" not in str(error):
                raise
            last_error = error
            print("snapshot retry:", attempt + 1, "/", SNAPSHOT_RETRY_LIMIT, error)
            gc.collect()
            utime.sleep_ms(SNAPSHOT_RETRY_DELAY_MS)
    raise last_error

def clamp_box(box, width, height):
    class_id = int(box[0])
    score = float(box[1])
    x1 = max(0, min(width - 1, int(box[2])))
    y1 = max(0, min(height - 1, int(box[3])))
    x2 = max(x1 + 1, min(width, int(box[4])))
    y2 = max(y1 + 1, min(height, int(box[5])))
    return [class_id, score, x1, y1, x2, y2]

def save_defect_photos(rgb565_img, det_boxes, labels, last_save_ms):
    save_boxes = [b for b in det_boxes if b[1] >= SAVE_SCORE_THRESHOLD]
    if not save_boxes or rgb565_img is None:
        return []
    if rgb565_img.width() <= 0 or rgb565_img.height() <= 0:
        return []
    now_ms = utime.ticks_ms()
    save_classes = []
    for b in save_boxes:
        c = int(b[0])
        last_ms = last_save_ms.get(c)
        cooldown_done = last_ms is None or ticks_elapsed(now_ms, last_ms) >= SAVE_COOLDOWN_MS
        if c not in save_classes and cooldown_done:
            save_classes.append(c)
    if not save_classes:
        return []
    # chn1 是独立拍照通道，可直接在当前帧上绘制，避免 copy() 带来的
    # 约600KB内存复制和瞬时卡顿；该帧编码完成后立即释放。
    save_img = rgb565_img
    for raw_box in save_boxes:
        b = clamp_box(raw_box, save_img.width(), save_img.height())
        c, score, x1, y1, x2, y2 = b
        if c < 0 or c >= len(labels):
            continue
        save_img.draw_rectangle(x1, y1, x2 - x1, y2 - y1, color=color_four[c][1:])
        display_label = DISPLAY_LABEL_EN.get(labels[c], labels[c])
        save_img.draw_string(x1, max(0, y1 - 24), display_label + " " + str(round(score, 2)), color=color_four[c][1:], scale=2)
    t = utime.localtime()
    stamp = "{:04d}{:02d}{:02d}_{:02d}{:02d}{:02d}_{:08d}".format(t[0], t[1], t[2], t[3], t[4], t[5], now_ms & 0xFFFFFFFF)
    saved_photos = []
    try:
        jpeg_bytes = save_img.to_jpeg(quality=JPEG_QUALITY)
        for c in save_classes:
            if c < 0 or c >= len(labels):
                continue
            path = SAVE_ROOT + labels[c] + "/" + labels[c] + "_" + stamp + ".jpg"
            f = None
            try:
                f = open(path, "wb")
                f.write(jpeg_bytes)
            finally:
                if f is not None:
                    f.close()
            last_save_ms[c] = now_ms
            saved_photos.append((c, path))
            print("saved:", path)
        del jpeg_bytes
    except Exception as e:
        print("save failed:", e)
    return saved_photos

def detection():
    print("det_infer start")
    # 注意：不要在开头调用 Display.deinit()/MediaManager.deinit()/Sensor().stop()
    # 软重启后传感器驱动层状态无法通过 Python 清除，强行清理会触发
    # "didn't call Display.init?" 和 "sensor(2) is already inited" 且卡死。
    # 唯一可靠的恢复方式是物理断电重上电。

    deploy_conf=read_deploy_config(config_path)
    kmodel_name=deploy_conf["kmodel_path"]
    labels=deploy_conf["categories"]
    confidence_threshold = max(float(deploy_conf["confidence_threshold"]), DISPLAY_SCORE_THRESHOLD)
    nms_threshold = float(deploy_conf["nms_threshold"])
    img_size=deploy_conf["img_size"]
    num_classes=deploy_conf["num_classes"]
    nms_option = deploy_conf["nms_option"]
    model_type = deploy_conf["model_type"]
    if model_type == "AnchorBaseDet":
        anchors = deploy_conf["anchors"][0] + deploy_conf["anchors"][1] + deploy_conf["anchors"][2]
    kmodel_frame_size = img_size
    frame_size = [OUT_RGB888P_WIDTH,OUT_RGB888P_HEIGH]
    strides = [8,16,32]
    ensure_save_dirs(labels)
    cloud_wifi = init_cloud_wifi()
    last_save_ms = {}
    alert_active = [False] * num_classes
    class_last_seen_ms = [0] * num_classes
    stable_boxes = []
    stable_box_ts = utime.ticks_ms()
    class_hit_counts = [0] * num_classes

    ori_w = OUT_RGB888P_WIDTH
    ori_h = OUT_RGB888P_HEIGH
    width = kmodel_frame_size[0]
    height = kmodel_frame_size[1]
    ratiow = float(width) / ori_w
    ratioh = float(height) / ori_h
    ratio = ratiow if ratiow < ratioh else ratioh
    new_w = int(ratio * ori_w)
    new_h = int(ratio * ori_h)
    dw = float(width - new_w) / 2
    dh = float(height - new_h) / 2
    top = int(round(dh - 0.1))
    bottom = int(round(dh + 0.1))
    left = int(round(dw - 0.1))
    right = int(round(dw + 0.1))

    print("load kmodel:", kmodel_name)
    kpu = nn.kpu()
    ai2d = nn.ai2d()
    kpu.load_kmodel(root_path+kmodel_name)
    print("kmodel loaded")
    ai2d.set_dtype(nn.ai2d_format.NCHW_FMT, nn.ai2d_format.NCHW_FMT, np.uint8, np.uint8)
    ai2d.set_pad_param(True, [0,0,0,0,top,bottom,left,right], 0, [114,114,114])
    ai2d.set_resize_param(True, nn.interp_method.tf_bilinear, nn.interp_mode.half_pixel )
    ai2d_builder = ai2d.build([1,3,OUT_RGB888P_HEIGH,OUT_RGB888P_WIDTH], [1,3,height,width])
    print("ai2d built")

    # 初始化并配置sensor - 用 try 捕获 already inited 并给出明确提示
    try:
        print("sensor create...")
        sensor = Sensor()
        print("sensor created")
    except OSError as e:
        print("FATAL: Sensor create failed:", e)
        print("原因: 传感器驱动被上次运行占用未释放，软重启无法清除。")
        print("解决: 请【物理拔掉 USB 线和电源线 5 秒】后重上电，再点运行。不要只点 IDE 的运行按钮。")
        raise
    print("sensor reset...")
    sensor.reset()
    print("sensor reset done")
    utime.sleep_ms(200)
    print("sensor config...")
    sensor.set_hmirror(False)
    sensor.set_vflip(False)
    sensor.set_framesize(width = DISPLAY_WIDTH, height = DISPLAY_HEIGHT)
    sensor.set_pixformat(PIXEL_FORMAT_YUV_SEMIPLANAR_420)
    # chn1 使用 RGB565 原生格式抓图，避免 chn0 的 VO/YUV 转换阻塞
    sensor.set_framesize(width = OUT_RGB888P_WIDTH, height = OUT_RGB888P_HEIGH, chn=CAM_CHN_ID_1)
    sensor.set_pixformat(PIXEL_FORMAT_RGB_565, chn=CAM_CHN_ID_1)
    sensor.set_framesize(width = OUT_RGB888P_WIDTH , height = OUT_RGB888P_HEIGH, chn=CAM_CHN_ID_2)
    sensor.set_pixformat(PIXEL_FORMAT_RGB_888_PLANAR, chn=CAM_CHN_ID_2)
    print("sensor config done")
    sensor_bind_info = sensor.bind_info(x = 0, y = 0, chn = CAM_CHN_ID_0)
    Display.bind_layer(**sensor_bind_info, layer = Display.LAYER_VIDEO1)
    print("bind done")
    if display_mode=="lcd":
        Display.init(Display.ST7701, width=DISPLAY_WIDTH, height=DISPLAY_HEIGHT, osd_num=1, to_ide=True)
    elif display_mode=="hdmi":
        Display.init(Display.LT9611, width=DISPLAY_WIDTH, height=DISPLAY_HEIGHT, osd_num=1, to_ide=True)
    else:
        Display.init(Display.VIRT, width=DISPLAY_WIDTH, height=DISPLAY_HEIGHT, fps=30, to_ide=True)
    print("Display done")

    osd_img = image.Image(DISPLAY_WIDTH, DISPLAY_HEIGHT, image.ARGB8888)
    print("osd created")
    alert_uart = None
    try:
        alert_uart = init_alert_uart()
        print("alert uart3 ready: IO32 TX, 115200 8N1")
        print("MediaManager.init...")
        MediaManager.init()
        print("MediaManager done")
        print("sensor.run...")
        sensor.run()
        print("sensor.run ok")
        rgb888p_img = None
        rgb565_img = None
        ai2d_input_tensor = None
        last_dbg_ms = utime.ticks_ms()
        last_gc_ms = last_dbg_ms
        shape_dbg_printed = False
        data = np.ones((1,3,height,width),dtype=np.uint8)
        ai2d_output_tensor = nn.from_numpy(data)
        while True:
            with ScopedTiming("total",False):
                rgb888p_img = snapshot_with_retry(sensor, CAM_CHN_ID_2)
                if rgb888p_img.format() == image.RGBP888:
                    ai2d_input = rgb888p_img.to_numpy_ref()
                    ai2d_input_tensor = nn.from_numpy(ai2d_input)
                    ai2d_builder.run(ai2d_input_tensor, ai2d_output_tensor)
                    kpu.set_input_tensor(0, ai2d_output_tensor)
                    kpu.run()
                    results = []
                    for i in range(kpu.outputs_size()):
                        out_data = kpu.get_output_tensor(i)
                        result = out_data.to_numpy()
                        result = result.reshape((result.shape[0]*result.shape[1]*result.shape[2]*result.shape[3]))
                        del out_data
                        results.append(result)
                    if model_type == "AnchorBaseDet":
                        det_boxes = aicube.anchorbasedet_post_process(results[0], results[1], results[2], kmodel_frame_size, frame_size, strides, num_classes, confidence_threshold, nms_threshold, anchors, nms_option)
                    elif model_type == "GFLDet":
                        det_boxes = aicube.gfldet_post_process(results[0], results[1], results[2], kmodel_frame_size, frame_size, strides, num_classes, confidence_threshold, nms_threshold, nms_option)
                    else:
                        det_boxes = aicube.anchorfreedet_post_process(results[0], results[1], results[2], kmodel_frame_size, frame_size, strides, num_classes, confidence_threshold, nms_threshold, nms_option)
                    del results
                    now_ms = utime.ticks_ms()
                    valid_boxes = []
                    frame_classes = []
                    for raw_box in det_boxes:
                        b = clamp_box(raw_box, OUT_RGB888P_WIDTH, OUT_RGB888P_HEIGH)
                        if b[0] < 0 or b[0] >= num_classes or b[1] < confidence_threshold:
                            continue
                        # 过滤极小噪声框和覆盖大半画面的异常框，减少误检及绘制开销。
                        box_w = b[4] - b[2]
                        box_h = b[5] - b[3]
                        area = box_w * box_h
                        if box_w < 4 or box_h < 4 or area > (OUT_RGB888P_WIDTH * OUT_RGB888P_HEIGH * 3 // 4):
                            continue
                        valid_boxes.append(b)
                        if b[0] not in frame_classes:
                            frame_classes.append(b[0])
                    det_boxes = valid_boxes
                    for c in range(num_classes):
                        if c in frame_classes:
                            class_hit_counts[c] += 1
                            class_last_seen_ms[c] = now_ms
                        else:
                            class_hit_counts[c] = 0
                            # 短时漏检不解除事件锁，防止同一持续目标重复告警和上传。
                            if (alert_active[c] and
                                    ticks_elapsed(now_ms, class_last_seen_ms[c]) >= ALERT_CLEAR_MS):
                                alert_active[c] = False
                    if debug_mode > 0 and ticks_elapsed(now_ms, last_dbg_ms) > 1000:
                        last_dbg_ms = now_ms
                        if det_boxes:
                            print("det:", [(labels[b[0]], round(b[1], 2)) for b in det_boxes])
                        else:
                            print("det: 0")
                    event_boxes = []
                    event_classes = []
                    for b in det_boxes:
                        c = b[0]
                        if (c not in event_classes and
                                not alert_active[c] and
                                b[1] >= SAVE_SCORE_THRESHOLD and
                                class_hit_counts[c] >= SAVE_CONFIRM_FRAMES):
                            event_boxes.append(b)
                            event_classes.append(c)

                    # 串口告警独立于抓图、写卡和联网，避免辅助功能失败时漏报 STM32。
                    for b in event_boxes:
                        c = b[0]
                        alert_type = ALERT_TYPE_BY_LABEL.get(labels[c])
                        if alert_type is None:
                            print("alert skipped: unsupported label", labels[c])
                        elif send_alert(alert_uart, alert_type):
                            alert_active[c] = True

                    save_boxes = []
                    if SAVE_ENABLED:
                        for b in event_boxes:
                            c = b[0]
                            last_ms = last_save_ms.get(c)
                            if (last_ms is None or
                                    ticks_elapsed(now_ms, last_ms) >= SAVE_COOLDOWN_MS):
                                save_boxes.append(b)
                    # 只有真正需要保存时才抓 chn1，避免无效抓图拖慢推理。
                    if save_boxes:
                        try:
                            rgb565_img = snapshot_with_retry(sensor, CAM_CHN_ID_1)
                        except RuntimeError as error:
                            rgb565_img = None
                            print("save snapshot failed:", error)
                        if debug_mode > 0 and not shape_dbg_printed and rgb565_img is not None:
                            shape_dbg_printed = True
                            print("chn1 fmt:", rgb565_img.format(), "w:", rgb565_img.width(), "h:", rgb565_img.height())
                    display_boxes = [b for b in det_boxes if b[1] >= DISPLAY_SCORE_THRESHOLD]
                    if display_boxes:
                        stable_boxes = display_boxes
                        stable_box_ts = now_ms
                        draw_boxes = stable_boxes
                    elif ticks_elapsed(now_ms, stable_box_ts) < BOX_STALE_MS:
                        draw_boxes = stable_boxes
                    else:
                        stable_boxes = []
                        draw_boxes = []
                    osd_img.clear()
                    if draw_boxes:
                        for det_boxe in draw_boxes:
                            x1, y1, x2, y2 = det_boxe[2],det_boxe[3],det_boxe[4],det_boxe[5]
                            w = float(x2 - x1) * DISPLAY_WIDTH // OUT_RGB888P_WIDTH
                            h = float(y2 - y1) * DISPLAY_HEIGHT // OUT_RGB888P_HEIGH
                            osd_img.draw_rectangle(int(x1 * DISPLAY_WIDTH // OUT_RGB888P_WIDTH), int(y1 * DISPLAY_HEIGHT // OUT_RGB888P_HEIGH), int(w), int(h), color=color_four[det_boxe[0]][1:])
                            label = DISPLAY_LABEL_EN.get(labels[det_boxe[0]], labels[det_boxe[0]])
                            score = str(round(det_boxe[1],2))
                            osd_img.draw_string(int(x1 * DISPLAY_WIDTH // OUT_RGB888P_WIDTH), max(0, int(y1 * DISPLAY_HEIGHT // OUT_RGB888P_HEIGH)-50), label + " " + score, color=color_four[det_boxe[0]][1:], scale=3)
                    Display.show_image(osd_img, 0, 0, Display.LAYER_OSD0)
                    if save_boxes and rgb565_img is not None:
                        saved_photos = save_defect_photos(rgb565_img,
                                                         save_boxes,
                                                         labels,
                                                         last_save_ms)
                        for c, image_path in saved_photos:
                            upload_photo_to_bemfa(cloud_wifi, image_path)
                    if ticks_elapsed(now_ms, last_gc_ms) >= GC_INTERVAL_MS:
                        gc.collect()
                        last_gc_ms = now_ms
                rgb888p_img = None
                rgb565_img = None
    except KeyboardInterrupt:
        print("user stop")
    except Exception as e:
        # CanMV v1.4.3会把IDE停止包装成普通Exception，而不是KeyboardInterrupt。
        if "IDE interrupt" in str(e):
            print("user stop")
        else:
            print(f"An error occurred: {e}")
            import sys
            sys.print_exception(e)
    finally:
        # 所有退出路径只清理一次，避免重复Display.deinit()产生误报。
        print("cleanup...")
        try:
            sensor.stop()
        except Exception:
            pass
        try:
            if Display.inited():
                Display.deinit()
        except Exception:
            pass
        os.exitpoint(os.EXITPOINT_ENABLE_SLEEP)
        utime.sleep_ms(100)
        try:
            del ai2d_input_tensor
        except Exception:
            pass
        try:
            del ai2d_output_tensor
        except Exception:
            pass
        try:
            MediaManager.deinit()
        except Exception:
            pass
        try:
            alert_uart.deinit()
        except Exception:
            pass
        gc.collect()
        nn.shrink_memory_pool()
    print("det_infer end")
    return 0

def run():
    """统一 IDE 与 SD 卡上电入口，测试模式不初始化摄像头和模型。"""
    if UART_TEST_SEND_ON_START:
        test_uart = init_alert_uart()
        all_sent = True
        try:
            for test_index in range(UART_TEST_REPEAT_COUNT):
                print("uart test:", test_index + 1, "/", UART_TEST_REPEAT_COUNT)
                if not send_uart_test_alert(test_uart):
                    all_sent = False
                utime.sleep_ms(UART_TEST_INTERVAL_MS)
        finally:
            test_uart.deinit()
        return 0 if all_sent else -1
    return detection()

if __name__=="__main__":
    run()
