from stable_baselines3 import PPO
from stable_baselines3.common.vec_env import DummyVecEnv, VecNormalize
import numpy as np
import os

os.chdir(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))))
from zcw_rl.envs.cable_env import CableTrackingEnv


def evaluate():
    vec_env = DummyVecEnv([lambda: CableTrackingEnv(max_steps=500)])
    vec_env = VecNormalize.load("./models/vec_normalize.pkl", vec_env)
    vec_env.training = False
    vec_env.norm_reward = False

    model = PPO.load("./models/ppo_cable_tracker_final.zip")

    for ep in range(5):
        obs = vec_env.reset()
        total_reward = 0
        errors = []
        t_vals = []
        step = 0
        while True:
            action, _ = model.predict(obs, deterministic=True)
            obs, reward, done, info = vec_env.step(action)
            total_reward += reward[0]
            errors.append(info[0]["error"])
            t_vals.append(info[0]["t"])
            step += 1
            if done:
                break
        print(f"Ep {ep + 1}: steps={step} t_max={t_vals[-1]:.3f} "
              f"mean_err={np.mean(errors):.3f} max_err={np.max(errors):.3f} "
              f"reward={total_reward:.1f}")


if __name__ == "__main__":
    evaluate()
