
import time
import threading
from datetime import datetime

import pyautogui
from fastapi import FastAPI
from pydantic import BaseModel, Field

# =====================================================
# DoorDetection Receiver v0.2
# Cardputer ADV -> Windows Keyboard Action
# =====================================================

app = FastAPI(title="DoorDetection Receiver")

# 같은 이벤트의 반복 실행 방지 (초)
ACTION_COOLDOWN = 5.0

# Scroll Lock 입력 간격 (초)
KEY_INTERVAL = 0.15

# 마지막 단축키 실행 시각
last_action_time = 0.0

# 동시 HTTP 요청에 의한 중복 실행 방지
action_lock = threading.Lock()


# =====================================================
# Request Model
# =====================================================
class MotionEvent(BaseModel):
    deviceId: str
    event: str
    motionG: float = Field(ge=0)


# =====================================================
# Keyboard Action
# =====================================================
def execute_keyboard_action():
    global last_action_time

    with action_lock:
        now = time.monotonic()

        # 쿨다운 확인
        if now - last_action_time < ACTION_COOLDOWN:
            print("[SKIP] Action cooldown", flush=True)
            return False

        print(
            "[ACTION] Scroll Lock x2",
            flush=True
        )

        # Scroll Lock 두 번 입력
        pyautogui.press(
            "scrolllock",
            presses=2,
            interval=KEY_INTERVAL
        )

        last_action_time = time.monotonic()

        print(
            "[ACTION] Completed",
            flush=True
        )

        return True


# =====================================================
# Health Check
# =====================================================
@app.get("/health")
def health():
    return {
        "status": "ok",
        "service": "DoorDetection"
    }


# =====================================================
# HTTP Event Receiver
# =====================================================
@app.post("/api/events")
def receive_event(data: MotionEvent):

    print(
        f"[{datetime.now():%Y-%m-%d %H:%M:%S}] "
        f"Device={data.deviceId}, "
        f"Event={data.event}, "
        f"Motion={data.motionG:.3f}g",
        flush=True
    )

    if data.event != "MOTION_DETECTED":
        return {
            "accepted": False,
            "reason": "Unsupported event"
        }

    if data.deviceId != "cardputer-door-01":
        return {
            "accepted": False,
            "reason": "Unknown device"
        }

    try:
        executed = execute_keyboard_action()

        return {
            "accepted": True,
            "action": "SCROLLLOCK_DOUBLE",
            "executed": executed
        }

    except Exception as e:
        print(
            f"[ERROR] {type(e).__name__}: {e}",
            flush=True
        )

        return {
            "accepted": False,
            "error": "Keyboard action failed"
        }
