import gymnasium as gym
import numpy as np
from stable_baselines3 import PPO
from stable_baselines3.common.callbacks import EvalCallback
from stable_baselines3.common.vec_env import DummyVecEnv, VecNormalize
import os

os.chdir(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

from zcw_rl.envs.cable_env import CableTrackingEnv


def make_env():
    return CableTrackingEnv()


def main():
    vec_env = DummyVecEnv([make_env])
    vec_env = VecNormalize(vec_env, norm_obs=True, norm_reward=True)

    model = PPO(
        "MlpPolicy",
        vec_env,
        learning_rate=3e-4,
        n_steps=4096,
        batch_size=128,
        n_epochs=20,
        gamma=0.99,
        gae_lambda=0.95,
        clip_range=0.2,
        verbose=1,
    )

    eval_env = VecNormalize(DummyVecEnv([make_env]), training=False, norm_obs=True, norm_reward=True)
    eval_callback = EvalCallback(
        eval_env,
        best_model_save_path="./models/best",
        log_path="./logs_eval",
        eval_freq=10000,
        n_eval_episodes=10,
        deterministic=True,
    )

    model.learn(total_timesteps=200_000, callback=eval_callback)
    model.save("./models/ppo_cable_tracker_v2_final.zip")
    vec_env.save("./models/vec_normalize.pkl")

    print("Training complete. Model saved to models/ppo_cable_tracker_final.zip")


if __name__ == "__main__":
    main()
