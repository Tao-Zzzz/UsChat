import cv2
import mediapipe as mp
import pyautogui
import math

# 初始化 MediaPipe 手部模型
mp_hands = mp.solutions.hands
hands = mp_hands.Hands(
    static_image_mode=False,
    max_num_hands=1, # 这里我们只识别一只手来控制鼠标
    min_detection_confidence=0.7,
    min_tracking_confidence=0.7
)
mp_draw = mp.solutions.drawing_utils

# 获取屏幕分辨率
screen_w, screen_h = pyautogui.size()

# 打开摄像头
cap = cv2.VideoCapture(0)

# 点击状态防抖标志
is_clicked = False

print("摄像头已启动！伸出你的手来控制鼠标吧。")
print("【食指】移动鼠标，【食指+大拇指】捏合触发点击。")
print("按 'q' 键退出程序。")

while cap.isOpened():
    success, img = cap.read()
    if not success:
        break

    # 水平翻转图像，让你看起来像照镜子，符合直觉
    img = cv2.flip(img, 1)
    img_h, img_w, _ = img.shape
    
    # 转换颜色空间（OpenCV默认BGR，MediaPipe需要RGB）
    img_rgb = cv2.cvtColor(img, cv2.COLOR_BGR2RGB)
    results = hands.process(img_rgb)

    if results.multi_hand_landmarks:
        for hand_landmarks in results.multi_hand_landmarks:
            # 在画面上画出手部骨架
            mp_draw.draw_landmarks(img, hand_landmarks, mp_hands.HAND_CONNECTIONS)

            # 获取食指指尖 (点8) 和 大拇指指尖 (点4) 的坐标
            index_finger = hand_landmarks.landmark[8]
            thumb_finger = hand_landmarks.landmark[4]

            # 将相对坐标 (0~1) 转换为画面上的绝对像素坐标
            x8, y8 = int(index_finger.x * img_w), int(index_finger.y * img_h)
            x4, y4 = int(thumb_finger.x * img_w), int(thumb_finger.y * img_h)

            # 1. 鼠标移动逻辑 (使用食指)
            # 将摄像头画面坐标映射到电脑屏幕坐标
            # 为了让边缘更好按，可以加个框，但这里展示最简单的等比例映射
            mouse_x = int(index_finger.x * screen_w)
            mouse_y = int(index_finger.y * screen_h)
            
            # 使用 pyautogui 移动鼠标 (加上防错机制，防止移出边界报错)
            try:
                pyautogui.moveTo(mouse_x, mouse_y, _pause=False)
            except Exception:
                pass # 忽略边缘报错

            # 2. 鼠标点击逻辑 (计算食指和大拇指的距离)
            # 使用勾股定理计算两点距离
            distance = math.hypot(x8 - x4, y8 - y4)
            
            # 在两个指尖之间画个圆和线，方便调试
            cv2.circle(img, (x8, y8), 10, (255, 0, 255), cv2.FILLED)
            cv2.circle(img, (x4, y4), 10, (255, 0, 255), cv2.FILLED)
            cv2.line(img, (x8, y8), (x4, y4), (255, 0, 255), 3)

            # 捏合阈值（你可以根据自己的摄像头距离调整这个数字）
            if distance < 30:
                cv2.circle(img, (int((x8+x4)/2), int((y8+y4)/2)), 10, (0, 255, 0), cv2.FILLED)
                if not is_clicked:
                    pyautogui.click()
                    is_clicked = True # 标记为已点击，防止疯狂连点
            else:
                is_clicked = False

    # 显示画面
    cv2.imshow("Hand Gestures", img)

    # 按 'q' 键退出
    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

cap.release()
cv2.destroyAllWindows()