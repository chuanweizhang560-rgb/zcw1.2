import gymnasium as gym
import numpy as np
from gymnasium import spaces


def cable_point(t, x1=-30.0, x2=30.0, cy=0.6, ah=25.0, sag=4.0):
    x = x1 + t * (x2 - x1)
    cx = (x1 + x2) / 2.0
    span = (x2 - x1) / 2.0
    xr = (x - cx) / span
    z = ah - sag * (1 - xr * xr)
    return np.array([x, cy, z], dtype=np.float32)


class CableTrackingEnv(gym.Env):
    def __init__(
        self,
        x1=-30.0,
        x2=30.0,
        cy=0.6,
        ah=25.0,
        sag=4.0,
        max_steps=500,
        recapture_dist=8.0,
        dt=0.05,
    ):
        super().__init__()
        self.x1 = x1
        self.x2 = x2
        self.cy = cy
        self.ah = ah
        self.sag = sag
        self.max_steps = max_steps
        self.recapture_dist = recapture_dist
        self.dt = dt
        self.speed = 3.0

        self.observation_space = spaces.Box(
            low=-np.inf, high=np.inf, shape=(4,), dtype=np.float32
        )
        self.action_space = spaces.Box(
            low=-1.0, high=1.0, shape=(3,), dtype=np.float32
        )

        self.t = 0.0
        self.pos = np.array([x1, cy, ah], dtype=np.float32)
        self.step_count = 0

    def reset(self, *, seed=None, options=None):
        super().reset(seed=seed)
        self.t = 0.0
        self.pos = np.array([self.x1, self.cy, self.ah + 3.0], dtype=np.float32)
        self.step_count = 0
        return self._obs(), {}

    def _obs(self):
        wp = cable_point(self.t, self.x1, self.x2, self.cy, self.ah, self.sag)
        err = self.pos - wp
        return np.array([self.t, err[0], err[1], err[2]], dtype=np.float32)

    def step(self, action):
        self.step_count += 1
        dt_progress = np.clip(action[0], -1.0, 1.0) * 0.03
        dy = np.clip(action[1], -1.0, 1.0) * 2.0
        dz = np.clip(action[2], -1.0, 1.0) * 2.0

        self.t = np.clip(self.t + dt_progress + self.dt * self.speed * 0.05, 0.0, 1.0)

        wp = cable_point(self.t, self.x1, self.x2, self.cy, self.ah, self.sag)
        smooth = 0.3
        self.pos = smooth * (wp + np.array([0.0, dy, dz])) + (1 - smooth) * self.pos

        err = np.linalg.norm(self.pos - wp)
        progress_bonus = dt_progress * 10.0
        advance_reward = self.t * 5.0
        reward = -err * 2.0 + progress_bonus + advance_reward

        terminated = False
        truncated = self.step_count >= self.max_steps

        if err > self.recapture_dist:
            reward -= 20.0
            truncated = True
        elif self.t >= 1.0:
            reward += 50.0 - self.step_count * 0.1
            terminated = True

        return self._obs(), reward, terminated, truncated, {"error": err, "t": self.t}

    def render(self):
        pass
