from abc import abstractmethod, ABC

from common.realtime import DT_CTRL
from common.numpy_fast import clip
from selfdrive.kegman_kans_conf import kegman_kans_conf

kegman_kans = kegman_kans_conf()

MIN_STEER_SPEED = 0.3


class LatControl(ABC):
  def __init__(self, CP, CI):
    self.sat_count_rate = 1.0 * DT_CTRL
    # self.sat_limit = CP.steerLimitTimer
    self.sat_count = 0.

    # TODO liveTuning
    self.mpc_frame = 0
    self.sat_limit = CP.steerLimitTimer
    self.steer_MaxV = CP.steerMaxV

  @abstractmethod
  def update(self, active, CS, CP, VM, params, last_actuators, desired_curvature, desired_curvature_rate):
    pass
    # Live Tuning for steerLimitTimer & SteerMaxV
    self.mpc_frame += 1
    if self.mpc_frame % 300 == 0:
      self.kegman_kans = kegman_kans_conf()
      self.sat_limit = float(self.kegman_kans.conf['steerLimitTimer'])
      self.steer_MaxV = float(self.kegman_kans.conf['steerMax'])
      self.mpc_frame = 0

  def reset(self):
    self.sat_count = 0.

  def _check_saturation(self, saturated, CS):
    if saturated and CS.vEgo > 10. and not CS.steeringRateLimited and not CS.steeringPressed:
      self.sat_count += self.sat_count_rate
    else:
      self.sat_count -= self.sat_count_rate
    self.sat_count = clip(self.sat_count, 0.0, self.sat_limit)
    return self.sat_count > (self.sat_limit - 1e-3)
