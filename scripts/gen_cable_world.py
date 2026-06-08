#!/usr/bin/env python3
"""Generate cable inspection world with catenary cables between two TL towers."""
import math

x1, x2 = -30.0, 30.0
span = (x2 - x1) / 2.0
cx = (x1 + x2) / 2.0
ah, sag = 25.0, 4.0
ns, sr = 6, 0.04
cys = [-3.0, -1.8, -0.6, 0.6, 1.8, 3.0]

for cy in cys:
    for i in range(ns):
        fa, f1 = i/ns, (i+1)/ns
        xa = x1 + fa*(x2 - x1); xb = x1 + f1*(x2 - x1)
        za = ah - sag*(1 - ((xa-cx)/span)**2)
        zb = ah - sag*(1 - ((xb-cx)/span)**2)
        mx = (xa + xb) / 2.0; mz = (za + zb) / 2.0
        L = math.hypot(xb - xa, zb - za)
        p = math.atan2(zb - za, xb - xa)
        ys = f'{cy:.1f}'.replace('.','_').replace('-','n')
        print(f'<model name="cable_{ys}_{i}"><static>true</static>'
              f'<pose>{mx:.3f} {cy} {mz:.3f} 0 {p:.6f} 0</pose>'
              f'<link name="l"><visual name="v"><geometry><cylinder>'
              f'<radius>{sr}</radius><length>{L:.6f}</length></cylinder>'
              f'</geometry><material><script><name>Gazebo/Black</name>'
              f'<uri>file://media/materials/scripts/gazebo.material</uri>'
              f'</script></material></visual></link></model>')
