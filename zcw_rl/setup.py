from setuptools import find_packages, setup

setup(
    name="zcw_rl",
    version="0.1.0",
    packages=find_packages("src"),
    package_dir={"": "src"},
    install_requires=["gymnasium>=1.0", "stable-baselines3>=2.0", "numpy", "torch"],
)
