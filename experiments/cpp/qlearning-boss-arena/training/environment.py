"""게임을 다시 구현하지 않고 C++ 전투 DLL의 FSM 행동 단위를 호출한다."""
import ctypes as C
from pathlib import Path
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
OBS, ACTIONS = 24, 6


class Environment:
    def __init__(self, index=0):
        self.index = index
        self.dll = C.CDLL(str(ROOT / "build/Release/ArenaTraining.dll"))
        fp, ip = C.POINTER(C.c_float), C.POINTER(C.c_int)
        self.dll.arena_version.restype = C.c_int
        if self.dll.arena_version() != 1:
            raise RuntimeError("Unsupported arena ABI")
        self.dll.arena_reset.argtypes = [C.c_int, C.c_uint, fp, ip]
        self.dll.arena_step.argtypes = [C.c_int, C.c_int, fp, ip, fp]
        self.dll.policy_forward.argtypes = [C.c_char_p, fp, fp]
        self.x = (C.c_float * OBS)()
        self.mask = (C.c_int * ACTIONS)()
        self.result = (C.c_float * 6)()

    def observation(self):
        return np.array(self.x, dtype=np.float32), np.array(self.mask, dtype=bool)

    def reset(self, seed):
        if self.dll.arena_reset(self.index, seed, self.x, self.mask):
            raise RuntimeError("C++ reset failed")
        return self.observation()

    def step(self, action):
        if self.dll.arena_step(self.index, action, self.x, self.mask, self.result):
            raise RuntimeError("C++ step failed or illegal action")
        x, mask = self.observation()
        r, done, seconds, win, loss, state = self.result
        return x, mask, r, bool(done), seconds, bool(win), bool(loss)

    def cpp_forward(self, path, x):
        out = (C.c_float * ACTIONS)()
        if self.dll.policy_forward(str(path).encode(), (C.c_float * OBS)(*x), out):
            raise RuntimeError("C++ model load failed")
        return np.array(out)


def table_state(x):
    distance = x[6] * 1100
    band = 0 if distance < 110 else 1 if distance < 270 else 2
    cd = int(x[9] > 0) | (int(x[10] > 0) << 1) | (int(x[11] > 0) << 2)
    return ((band * 2 + int(x[8] < 100 / 260)) * 2 + int(x[7] < .4)) * 8 + cd
