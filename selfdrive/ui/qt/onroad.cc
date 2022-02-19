#include "selfdrive/ui/qt/onroad.h"

#include <cmath>

#include <QDebug>
#include <QString>

#include "selfdrive/common/timing.h"
#include "selfdrive/ui/qt/util.h"
#ifdef ENABLE_MAPS
#include "selfdrive/ui/qt/maps/map.h"
#include "selfdrive/ui/qt/maps/map_helpers.h"
#endif

OnroadWindow::OnroadWindow(QWidget *parent) : QWidget(parent) {
  QVBoxLayout *main_layout  = new QVBoxLayout(this);
  main_layout->setMargin(bdr_s);
  QStackedLayout *stacked_layout = new QStackedLayout;
  stacked_layout->setStackingMode(QStackedLayout::StackAll);
  main_layout->addLayout(stacked_layout);

  QStackedLayout *road_view_layout = new QStackedLayout;
  road_view_layout->setStackingMode(QStackedLayout::StackAll);
  nvg = new NvgWindow(VISION_STREAM_RGB_BACK, this);
  road_view_layout->addWidget(nvg);
  hud = new OnroadHud(this);
  road_view_layout->addWidget(hud);

  QWidget * split_wrapper = new QWidget;
  split = new QHBoxLayout(split_wrapper);
  split->setContentsMargins(0, 0, 0, 0);
  split->setSpacing(0);
  split->addLayout(road_view_layout);

  stacked_layout->addWidget(split_wrapper);

  alerts = new OnroadAlerts(this);
  alerts->setAttribute(Qt::WA_TransparentForMouseEvents, true);
  stacked_layout->addWidget(alerts);

  // setup stacking order
  alerts->raise();

  setAttribute(Qt::WA_OpaquePaintEvent);
  QObject::connect(uiState(), &UIState::uiUpdate, this, &OnroadWindow::updateState);
  QObject::connect(uiState(), &UIState::offroadTransition, this, &OnroadWindow::offroadTransition);
}

void OnroadWindow::updateState(const UIState &s) {
  QColor bgColor = bg_colors[s.status];
  Alert alert = Alert::get(*(s.sm), s.scene.started_frame);
  if (s.sm->updated("controlsState") || !alert.equal({})) {
    if (alert.type == "controlsUnresponsive") {
      bgColor = bg_colors[STATUS_ALERT];
    } else if (alert.type == "controlsUnresponsivePermanent") {
      bgColor = bg_colors[STATUS_DISENGAGED];
    }
    alerts->updateAlert(alert, bgColor);
  }

  hud->updateState(s);

  if (bg != bgColor) {
    // repaint border
    bg = bgColor;
    update();
  }
}

void OnroadWindow::mousePressEvent(QMouseEvent* e) {
  if (map != nullptr) {
    bool sidebarVisible = geometry().x() > 0;
    map->setVisible(!sidebarVisible && !map->isVisible());
  }

  // propagation event to parent(HomeWindow)
  QWidget::mousePressEvent(e);
}

void OnroadWindow::offroadTransition(bool offroad) {
#ifdef ENABLE_MAPS
  if (!offroad) {
    if (map == nullptr && (uiState()->prime_type || !MAPBOX_TOKEN.isEmpty())) {
      MapWindow * m = new MapWindow(get_mapbox_settings());
      m->setFixedWidth(topWidget(this)->width() / 2);
      m->offroadTransition(offroad);
      QObject::connect(uiState(), &UIState::offroadTransition, m, &MapWindow::offroadTransition);
      split->addWidget(m, 0, Qt::AlignRight);
      map = m;
    }
  }
#endif

  alerts->updateAlert({}, bg);

  // update stream type
  bool wide_cam = Hardware::TICI() && Params().getBool("EnableWideCamera");
  nvg->setStreamType(wide_cam ? VISION_STREAM_RGB_WIDE : VISION_STREAM_RGB_BACK);
}

void OnroadWindow::paintEvent(QPaintEvent *event) {
  QPainter p(this);
  p.fillRect(rect(), QColor(bg.red(), bg.green(), bg.blue(), 255));
}

// ***** onroad widgets *****

// OnroadAlerts
void OnroadAlerts::updateAlert(const Alert &a, const QColor &color) {
  if (!alert.equal(a) || color != bg) {
    alert = a;
    bg = color;
    update();
  }
}

void OnroadAlerts::paintEvent(QPaintEvent *event) {
  if (alert.size == cereal::ControlsState::AlertSize::NONE) {
    return;
  }
  static std::map<cereal::ControlsState::AlertSize, const int> alert_sizes = {
    {cereal::ControlsState::AlertSize::SMALL, 271},
    {cereal::ControlsState::AlertSize::MID, 420},
    {cereal::ControlsState::AlertSize::FULL, height()},
  };
  int h = alert_sizes[alert.size];
  QRect r = QRect(0, height() - h, width(), h);

  QPainter p(this);

  // draw background + gradient
  p.setPen(Qt::NoPen);
  p.setCompositionMode(QPainter::CompositionMode_SourceOver);

  p.setBrush(QBrush(bg));
  p.drawRect(r);

  QLinearGradient g(0, r.y(), 0, r.bottom());
  g.setColorAt(0, QColor::fromRgbF(0, 0, 0, 0.05));
  g.setColorAt(1, QColor::fromRgbF(0, 0, 0, 0.35));

  p.setCompositionMode(QPainter::CompositionMode_DestinationOver);
  p.setBrush(QBrush(g));
  p.fillRect(r, g);
  p.setCompositionMode(QPainter::CompositionMode_SourceOver);

  // text
  const QPoint c = r.center();
  p.setPen(QColor(0xff, 0xff, 0xff));
  p.setRenderHint(QPainter::TextAntialiasing);
  if (alert.size == cereal::ControlsState::AlertSize::SMALL) {
    configFont(p, "Open Sans", 74, "SemiBold");
    p.drawText(r, Qt::AlignCenter, alert.text1);
  } else if (alert.size == cereal::ControlsState::AlertSize::MID) {
    configFont(p, "Open Sans", 88, "Bold");
    p.drawText(QRect(0, c.y() - 125, width(), 150), Qt::AlignHCenter | Qt::AlignTop, alert.text1);
    configFont(p, "Open Sans", 66, "Regular");
    p.drawText(QRect(0, c.y() + 21, width(), 90), Qt::AlignHCenter, alert.text2);
  } else if (alert.size == cereal::ControlsState::AlertSize::FULL) {
    bool l = alert.text1.length() > 15;
    configFont(p, "Open Sans", l ? 132 : 177, "Bold");
    p.drawText(QRect(0, r.y() + (l ? 240 : 270), width(), 600), Qt::AlignHCenter | Qt::TextWordWrap, alert.text1);
    configFont(p, "Open Sans", 88, "Regular");
    p.drawText(QRect(0, r.height() - (l ? 361 : 420), width(), 300), Qt::AlignHCenter | Qt::TextWordWrap, alert.text2);
  }
}

// OnroadHud ( wirelessnet2 init )
OnroadHud::OnroadHud(QWidget *parent) : QWidget(parent) {
  engage_img = QPixmap("../assets/img_chffr_wheel.png").scaled(img_size, img_size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
  dm_img = QPixmap("../assets/img_driver_face.png").scaled(img_size, img_size, Qt::KeepAspectRatio, Qt::SmoothTransformation);

  // crwusiz add
  brake_img = QPixmap("../assets/img_brake_disc.png").scaled(img_size, img_size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
  bsd_l_img = QPixmap("../assets/img_bsd_l.png").scaled(img_size, img_size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
  bsd_r_img = QPixmap("../assets/img_bsd_r.png").scaled(img_size, img_size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
  gps_img = QPixmap("../assets/img_gps.png").scaled(img_size, img_size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
  wifi_img = QPixmap("../assets/img_wifi.png").scaled(img_size, img_size, Qt::KeepAspectRatio, Qt::SmoothTransformation);

  // neokii add
  autohold_warning_img = QPixmap("../assets/img_autohold_warning.png").scaled(img_size, img_size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
  autohold_active_img = QPixmap("../assets/img_autohold_active.png").scaled(img_size, img_size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
  nda_img = QPixmap("../assets/img_nda.png");
  hda_img = QPixmap("../assets/img_hda.png");

  connect(this, &OnroadHud::valueChanged, [=] { update(); });
}

void OnroadHud::updateState(const UIState &s) {
  const SubMaster &sm = *(s.sm);
  const auto cs = sm["controlsState"].getControlsState();
  const auto car_state = sm["carState"].getCarState();
  const auto car_control = sm["carControl"].getCarControl();
  const auto device_state = sm["deviceState"].getDeviceState();
  const auto leadOne = sm["radarState"].getRadarState().getLeadOne();
  const auto scc_smoother = car_control.getSccSmoother();
  auto roadLimitSpeed = sm["roadLimitSpeed"].getRoadLimitSpeed();
  float cur_speed = std::max(0.0, car_state.getVEgo() * (s.scene.is_metric ? MS_TO_KPH : MS_TO_MPH));
  float applyMaxSpeed = scc_smoother.getApplyMaxSpeed();
  float cruiseMaxSpeed = scc_smoother.getCruiseMaxSpeed();
  bool cruise_set = (cruiseMaxSpeed > 0 && cruiseMaxSpeed < 255);

  if (cruise_set && !s.scene.is_metric) {
    applyMaxSpeed *= KM_TO_MILE;
    cruiseMaxSpeed *= KM_TO_MILE;
  }

  QString applymaxspeed_str = cruise_set ? QString::number(std::nearbyint(applyMaxSpeed)) : "-";
  QString cruisemaxspeed_str = cruise_set ? QString::number(std::nearbyint(cruiseMaxSpeed)) : "-";

  setProperty("is_cruise_set", cruise_set);
  setProperty("speed", QString::number(std::nearbyint(cur_speed)));
  setProperty("applyMaxSpeed", applymaxspeed_str);
  setProperty("cruiseMaxSpeed", cruisemaxspeed_str);
  setProperty("speedUnit", s.scene.is_metric ? "km/h" : "mph");
  setProperty("status", s.status);
  setProperty("engageable", cs.getEngageable() || cs.getEnabled());
  setProperty("steeringPressed", car_state.getSteeringPressed());
  setProperty("dmActive", sm["driverMonitoringState"].getDriverMonitoringState().getIsActiveMode());
  setProperty("brake_stat", car_state.getBrakeLights() || car_state.getBrakePressed());
  setProperty("autohold_stat", car_state.getAutoHold());
  setProperty("nda_stat", roadLimitSpeed.getActive());
  setProperty("bsd_l_stat", car_state.getLeftBlindspot());
  setProperty("bsd_r_stat", car_state.getRightBlindspot());
  setProperty("wifi_stat", (int)device_state.getNetworkStrength() > 0);
  setProperty("gps_stat", sm["liveLocationKalman"].getLiveLocationKalman().getGpsOK());
  setProperty("lead_d_rel", leadOne.getDRel());
  setProperty("lead_v_rel", leadOne.getVRel());
  setProperty("lead_status", leadOne.getStatus());
  setProperty("angleSteers", car_state.getSteeringAngleDeg());
  setProperty("steerAngleDesired", car_control.getActuators().getSteeringAngleDeg());
  setProperty("longControl", scc_smoother.getLongControl());
  setProperty("gap", car_state.getCruiseGap());
  setProperty("autoTrGap", scc_smoother.getAutoTrGap());
}

void OnroadHud::paintEvent(QPaintEvent *event) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  // Header gradient
  QLinearGradient bg(0, header_h - (header_h / 2.5), 0, header_h);
  bg.setColorAt(0, QColor::fromRgbF(0, 0, 0, 0.45));
  bg.setColorAt(1, QColor::fromRgbF(0, 0, 0, 0));
  p.fillRect(0, 0, width(), header_h, bg);

  // maxspeed
  QRect rc(30, 30, 184, 202);
  p.setPen(QPen(QColor(0xff, 0xff, 0xff, 100), 10));
  p.setBrush(QColor(0, 0, 0, 100));
  p.drawRoundedRect(rc, 20, 20);
  p.setPen(Qt::NoPen);

  // color define
  QColor yellowColor = QColor(255, 255, 0, 255);
  QColor whiteColor = QColor(255, 255, 255, 255);
  QColor engagedColor = QColor(23, 134, 68, 200);
  QColor warningColor = QColor(218, 111, 37, 200);
  QColor steeringpressedColor = QColor(0, 191, 255, 200);
  QColor iconbgColor = QColor(0, 0, 0, 70);
  QColor wheelbgColor = QColor(0, 0, 0, 70);

  if (is_cruise_set) {
    configFont(p, "Open Sans", 55, "Bold");
    drawTextColor(p, rc.center().x(), 100, applyMaxSpeed, yellowColor);
    configFont(p, "Open Sans", 76, "Bold");
    drawText(p, rc.center().x(), 195, cruiseMaxSpeed, 255);
  } else {
    if (longControl) {
      configFont(p, "Open Sans", 55, "sans-semibold");
      drawTextColor(p, rc.center().x(), 100, "OP", yellowColor);
    } else {
      configFont(p, "Open Sans", 55, "sans-semibold");
      drawTextColor(p, rc.center().x(), 100, "SET", yellowColor);
    }
    configFont(p, "Open Sans", 76, "sans-semibold");
    drawText(p, rc.center().x(), 195, "-", 100);
  }

  // current speed
  configFont(p, "Open Sans", 176, "Bold");
  drawTextColor(p, rect().center().x(), 230, speed, whiteColor);
  configFont(p, "Open Sans", 66, "Regular");
  drawTextColor(p, rect().center().x(), 310, speedUnit, yellowColor);

  // engage-ability icon ( wheel ) (upper right 1)
  int x = rect().right() - radius / 2 - bdr_s * 2;
  int y = radius / 2 + int(bdr_s * 4);

  if (status == STATUS_ENGAGED && ! steeringPressed) {
    wheelbgColor = engagedColor;
  } else if (status == STATUS_WARNING) {
    wheelbgColor = warningColor;
  } else if (steeringPressed) {
    wheelbgColor = steeringpressedColor;
  }

  drawIcon(p, x, y, engage_img, wheelbgColor, 1.0);

  // wifi icon (upper right 2)
  x = rect().right() - (radius / 2) - (bdr_s * 2) - (radius);
  y = radius / 2 + (bdr_s * 4);
  drawIcon(p, x, y, wifi_img, iconbgColor, wifi_stat ? 1.0 : 0.2);
  p.setOpacity(1.0);

  // gps icon (upper right 3)
  x = rect().right() - (radius / 2) - (bdr_s * 2) - (radius * 2);
  y = radius / 2 + (bdr_s * 4);
  drawIcon(p, x, y, gps_img, iconbgColor, gps_stat ? 1.0 : 0.2);
  p.setOpacity(1.0);

  // nda icon (upper center)
  if (nda_stat > 0) {
    int w = 120;
    int h = 54;
    x = (width() + (bdr_s*2))/2 - w/2 - bdr_s;
    y = 40 - bdr_s;
    p.drawPixmap(x, y, w, h, nda_stat == 1 ? nda_img : hda_img);
    p.setOpacity(1.0);
  }

  // Dev UI (Right Side)
  x = rect().right() - radius - bdr_s * 5;
  y = bdr_s * 4 + rc.height();
  drawRightDevUi(p, x, y);
  p.setOpacity(1.0);

  // dm icon (bottom 1 left)
  x = radius / 2 + (bdr_s * 2);
  y = rect().bottom() - footer_h / 2;
  drawIcon(p, x, y, dm_img, iconbgColor, dmActive ? 1.0 : 0.2);
  p.setOpacity(1.0);

 // cruise gap (bottom 1 right)
  x = radius / 2 + (bdr_s * 2) + radius;
  y = rect().bottom() - footer_h / 2;

  p.setPen(Qt::NoPen);
  p.setBrush(QBrush(QColor(0, 0, 0, 255 * .1f)));
  p.drawEllipse(x - radius / 2, y - radius / 2, radius, radius);

  QString str;
  float textSize = 50.f;
  QColor textColor = QColor(255, 255, 255, 200);

  if (gap <= 0) {
    str = "N/A";
  } else if (longControl && gap == autoTrGap) {
    str = "AUTO";
    textColor = QColor(120, 255, 120, 200);
  } else {
    str.sprintf("%d", (int)gap);
    textColor = QColor(120, 255, 120, 200);
    textSize = 70.f;
  }

  configFont(p, "Open Sans", 35, "Bold");
  drawText(p, x, y-20, "GAP", 200);
  configFont(p, "Open Sans", textSize, "Bold");
  drawTextColor(p, x, y+50, str, textColor);
  p.setOpacity(1.0);

  // brake icon (bottom 2 left)
  x = radius / 2 + (bdr_s * 2);
  y = rect().bottom() - (footer_h / 2) - (radius) - 10;
  drawIcon(p, x, y, brake_img, iconbgColor, brake_stat ? 1.0 : 0.2);
  p.setOpacity(1.0);

  // autohold icon (bottom 2 right)
  x = radius / 2 + (bdr_s * 2) + (radius);
  y = rect().bottom() - (footer_h / 2) - (radius) - 10;
  drawIcon(p, x, y, autohold_stat > 1 ? autohold_warning_img : autohold_active_img, iconbgColor, autohold_stat ? 1.0 : 0.2);
  p.setOpacity(1.0);

  // bsd_l icon (bottom 3 left)
  x = radius / 2 + (bdr_s * 2);
  y = rect().bottom() - (footer_h / 2) - (radius * 2) - 20;
  drawIcon(p, x, y, bsd_l_img, iconbgColor, bsd_l_stat ? 1.0 : 0.2);
  p.setOpacity(1.0);

  // bsd_r icon (bottom 3 right)
  x = radius / 2 + (bdr_s * 2) + (radius);
  y = rect().bottom() - (footer_h / 2) - (radius * 2) - 20;
  drawIcon(p, x, y, bsd_r_img, iconbgColor, bsd_r_stat ? 1.0 : 0.2);
  p.setOpacity(1.0);
}

int OnroadHud::devUiDrawElement(QPainter &p, int x, int y, const char* value, const char* label, const char* units, QColor &color) {
  configFont(p, "Open Sans", 45, "SemiBold");
  drawTextColor(p, x + 92, y + 80, QString(value), color);
  configFont(p, "Open Sans", 28, "Regular");
  drawText(p, x + 92, y + 80 + 42, QString(label), 255);

  if (strlen(units) > 0) {
    p.save();
    p.translate(x + 54 + 30 - 3 + 92, y + 37 + 25);
    p.rotate(-90);
    drawText(p, 0, 0, QString(units), 255);
    p.restore();
  }

  return 110;
}

void OnroadHud::drawRightDevUi(QPainter &p, int x, int y) {
  int rh = 5;
  int ry = y;

  QColor valueColor = QColor(255, 255, 255, 255);
  QColor whiteColor = QColor(255, 255, 255, 255);
  QColor limeColor = QColor(120, 255, 120, 255);
  QColor redColor = QColor(255, 0, 0, 255);
  QColor orangeColor = QColor(255, 188, 0, 255);
  QColor blackColor = QColor(0, 0, 0, 255);
  QColor yellowColor = QColor(255, 255, 0, 255);

  // Add Real Steering Angle
  // Unit: Degrees
  if (true) {
    char val_str[8];
    valueColor = limeColor;

    // Red if large steering angle
    // Orange if moderate steering angle
    if (std::fabs(angleSteers) > 90) {
      valueColor = redColor;
    } else if (std::fabs(angleSteers) > 30) {
      valueColor = orangeColor;
    }
    snprintf(val_str, sizeof(val_str), "%.0f%s%s", angleSteers , "°", "");

    //rh += devUiDrawElement(p, x, ry, val_str, "REAL STEER", "", valueColor);
    rh += devUiDrawElement(p, x, ry, val_str, "핸들 조향각", "", valueColor);
    ry = y + rh;
  }

  // Add Desired Steering Angle
  // Unit: Degrees
  if (engageable) {
    char val_str[8];
    valueColor = limeColor;

    // Red if large steering angle
    // Orange if moderate steering angle
    if (std::fabs(angleSteers) > 90) {
      valueColor = redColor;
    } else if (std::fabs(angleSteers) > 30) {
      valueColor = orangeColor;
    }
    snprintf(val_str, sizeof(val_str), "%.0f%s%s", steerAngleDesired, "°", "");

    //rh += devUiDrawElement(p, x, ry, val_str, "DESIR STEER", "", valueColor);
    rh += devUiDrawElement(p, x, ry, val_str, "OP 조향각", "", valueColor);
    ry = y + rh;
  }

  // Add Relative Distance to Primary Lead Car
  // Unit: Meters
  if (engageable) {
    char val_str[8];
    char units_str[8];
    valueColor = whiteColor;

    if (lead_status) {
      // Orange if close, Red if very close
      if (lead_d_rel < 5) {
        valueColor = redColor;
      } else if (lead_d_rel < 15) {
        valueColor = orangeColor;
      }
      snprintf(val_str, sizeof(val_str), "%d", (int)lead_d_rel);
    } else {
      snprintf(val_str, sizeof(val_str), "-");
    }
    snprintf(units_str, sizeof(units_str), "m");

    //rh += devUiDrawElement(p, x, ry, val_str, "REL DIST", units_str, valueColor);
    rh += devUiDrawElement(p, x, ry, val_str, "거리차", units_str, valueColor);
    ry = y + rh;
  }

  // Add Relative Velocity vs Primary Lead Car
  // Unit: kph if metric, else mph
  if (engageable) {
    char val_str[8];
    valueColor = whiteColor;

     if (lead_status) {
       // Red if approaching faster than 10mph
       // Orange if approaching (negative)
       if (lead_v_rel < -4.4704) {
         valueColor = redColor;
       } else if (lead_v_rel < 0) {
         valueColor = orangeColor;
       }

       if (speedUnit == "mph") {
         snprintf(val_str, sizeof(val_str), "%d", (int)(lead_v_rel * 2.236936)); //mph
       } else {
         snprintf(val_str, sizeof(val_str), "%d", (int)(lead_v_rel * 3.6)); //kph
       }
     } else {
       snprintf(val_str, sizeof(val_str), "-");
     }

    //rh += devUiDrawElement(p, x, ry, val_str, "REL SPEED", speedUnit.toStdString().c_str(), valueColor);
    rh += devUiDrawElement(p, x, ry, val_str, "속도차", speedUnit.toStdString().c_str(), valueColor);
    ry = y + rh;
  }

  rh += 25;
  p.setBrush(QColor(0, 0, 0, 0));
  QRect ldu(x, y, 184, rh);
  p.drawRoundedRect(ldu, 20, 20);
}

//-------------------------------------------------------------------------------------------
void OnroadHud::drawIcon(QPainter &p, int x, int y, QPixmap &img, QBrush bg, float opacity) {
  p.setPen(Qt::NoPen);
  p.setBrush(bg);
  p.drawEllipse(x - radius / 2, y - radius / 2, radius, radius);
  p.setOpacity(opacity);
  p.drawPixmap(x - img_size / 2, y - img_size / 2, img);
}

void OnroadHud::drawText(QPainter &p, int x, int y, const QString &text, int alpha) {
  QFontMetrics fm(p.font());
  QRect init_rect = fm.boundingRect(text);
  QRect real_rect = fm.boundingRect(init_rect, 0, text);
  real_rect.moveCenter({x, y - real_rect.height() / 2});
  p.setPen(QColor(0xff, 0xff, 0xff, alpha));
  p.drawText(real_rect.x(), real_rect.bottom(), text);
}

void OnroadHud::drawTextColor(QPainter &p, int x, int y, const QString &text, QColor &color) {
  QFontMetrics fm(p.font());
  QRect init_rect = fm.boundingRect(text);
  QRect real_rect = fm.boundingRect(init_rect, 0, text);
  real_rect.moveCenter({x, y - real_rect.height() / 2});
  p.setPen(color);
  p.drawText(real_rect.x(), real_rect.bottom(), text);
}
//-------------------------------------------------------------------------------------------

// NvgWindow
void NvgWindow::initializeGL() {
  CameraViewWidget::initializeGL();
  qInfo() << "OpenGL version:" << QString((const char*)glGetString(GL_VERSION));
  qInfo() << "OpenGL vendor:" << QString((const char*)glGetString(GL_VENDOR));
  qInfo() << "OpenGL renderer:" << QString((const char*)glGetString(GL_RENDERER));
  qInfo() << "OpenGL language version:" << QString((const char*)glGetString(GL_SHADING_LANGUAGE_VERSION));

  prev_draw_t = millis_since_boot();
  setBackgroundColor(bg_colors[STATUS_DISENGAGED]);

  // neokii
  turnsignal_l_img = QPixmap("../assets/img_turnsignal_l.png").scaled(img_size, img_size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
  turnsignal_r_img = QPixmap("../assets/img_turnsignal_r.png").scaled(img_size, img_size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
  tire_pressure_img = QPixmap("../assets/img_tire_pressure.png");
}

void NvgWindow::updateFrameMat(int w, int h) {
  CameraViewWidget::updateFrameMat(w, h);

  UIState *s = uiState();
  s->fb_w = w;
  s->fb_h = h;
  auto intrinsic_matrix = s->wide_camera ? ecam_intrinsic_matrix : fcam_intrinsic_matrix;
  float zoom = ZOOM / intrinsic_matrix.v[0];
  if (s->wide_camera) {
    zoom *= 0.5;
  }
  // Apply transformation such that video pixel coordinates match video
  // 1) Put (0, 0) in the middle of the video
  // 2) Apply same scaling as video
  // 3) Put (0, 0) in top left corner of video
  s->car_space_transform.reset();
  s->car_space_transform.translate(w / 2, h / 2 + y_offset)
      .scale(zoom, zoom)
      .translate(-intrinsic_matrix.v[2], -intrinsic_matrix.v[5]);
}

void NvgWindow::drawLaneLines(QPainter &painter, const UIScene &scene) {
  if (!scene.end_to_end) {
    // lanelines
    for (int i = 0; i < std::size(scene.lane_line_vertices); ++i) {
      painter.setBrush(QColor::fromRgbF(1.0, 1.0, 1.0, scene.lane_line_probs[i]));
      painter.drawPolygon(scene.lane_line_vertices[i].v, scene.lane_line_vertices[i].cnt);
    }
    // road edges
    for (int i = 0; i < std::size(scene.road_edge_vertices); ++i) {
      painter.setBrush(QColor::fromRgbF(1.0, 0, 0, std::clamp<float>(1.0 - scene.road_edge_stds[i], 0.0, 1.0)));
      painter.drawPolygon(scene.road_edge_vertices[i].v, scene.road_edge_vertices[i].cnt);
    }
  }
  // paint path
  QLinearGradient bg(0, height(), 0, height() / 4);
  // wirelessnet2's rainbow barf path
  if (scene.enabled) {
    // openpilot is not disengaged
    if (scene.steeringPressed) {
      // The user is applying torque to the steering wheel
      bg.setColorAt(0, QColor(0, 191, 255, 255));
      bg.setColorAt(1, QColor(0, 95, 128, 50));
    } else {
      // Draw colored track
      int torqueScale = (int)std::fabs(510 * (float)scene.output_scale);
      int red_lvl = std::fmin(255, torqueScale);
      int green_lvl = std::fmin(255, 510 - torqueScale);
      bg.setColorAt(0, QColor(red_lvl, green_lvl, 0, 255));
      bg.setColorAt(1, QColor((int)(0.5 * red_lvl), (int)(0.5 * green_lvl), 0, 50));
    }
  } else if (!scene.end_to_end) {
    // Draw white track when disengaged and not end_to_end
    bg.setColorAt(0, QColor(255, 255, 255));
    bg.setColorAt(1, QColor(255, 255, 255, 0));
  } else {
    // Draw red vision track when disengaged and end_to_end
    bg.setColorAt(0, redColor());
    bg.setColorAt(1, redColor(0));
  }

  painter.setBrush(bg);
  painter.drawPolygon(scene.track_vertices.v, scene.track_vertices.cnt);
}

void NvgWindow::drawLead(QPainter &painter, const cereal::ModelDataV2::LeadDataV3::Reader &lead_data, const QPointF &vd, bool is_radar) {
  const float speedBuff = 10.;
  const float leadBuff = 40.;
  const float d_rel = lead_data.getX()[0];
  const float v_rel = lead_data.getV()[0];

  float fillAlpha = 0;
  if (d_rel < leadBuff) {
    fillAlpha = 255 * (1.0 - (d_rel / leadBuff));
    if (v_rel < 0) {
      fillAlpha += 255 * (-1 * (v_rel / speedBuff));
    }
    fillAlpha = (int)(fmin(fillAlpha, 255));
  }

  float sz = std::clamp((25 * 30) / (d_rel / 3 + 30), 15.0f, 30.0f) * 2.35;
  float x = std::clamp((float)vd.x(), 0.f, width() - sz / 2);
  float y = std::fmin(height() - sz * .6, (float)vd.y());

  float g_xo = sz / 5;
  float g_yo = sz / 10;

  QColor golden_yellowColor = QColor(255, 223, 0, 255);
  QColor light_orangeColor = QColor(255, 165, 0, 255);

  QPointF glow[] = {{x + (sz * 1.35) + g_xo, y + sz + g_yo}, {x, y - g_yo}, {x - (sz * 1.35) - g_xo, y + sz + g_yo}};
  painter.setBrush(is_radar ? light_orangeColor : golden_yellowColor);
  painter.drawPolygon(glow, std::size(glow));

  // chevron
  QPointF chevron[] = {{x + (sz * 1.25), y + sz}, {x, y}, {x - (sz * 1.25), y + sz}};
  painter.setBrush(redColor(fillAlpha));
  painter.drawPolygon(chevron, std::size(chevron));
}

void NvgWindow::paintGL() {
}

void NvgWindow::paintEvent(QPaintEvent *event) {
  QPainter p;
  p.begin(this);

  p.beginNativePainting();
  CameraViewWidget::paintGL();
  p.endNativePainting();

  UIState *s = uiState();
  if (s->worldObjectsVisible()) {
    drawHud(p);
  }

  p.end();

  double cur_draw_t = millis_since_boot();
  double dt = cur_draw_t - prev_draw_t;
  if (dt > 66) {
    // warn on sub 15fps
    LOGW("slow frame time: %.2f", dt);
  }
  prev_draw_t = cur_draw_t;
}

void NvgWindow::showEvent(QShowEvent *event) {
  CameraViewWidget::showEvent(event);

  ui_update_params(uiState());
  prev_draw_t = millis_since_boot();
}

//-------------------------------------------------------------------------------------------
void NvgWindow::drawText(QPainter &p, int x, int y, const QString &text, int alpha) {
  QFontMetrics fm(p.font());
  QRect init_rect = fm.boundingRect(text);
  QRect real_rect = fm.boundingRect(init_rect, 0, text);
  real_rect.moveCenter({x, y - real_rect.height() / 2});
  p.setPen(QColor(0xff, 0xff, 0xff, alpha));
  p.drawText(real_rect.x(), real_rect.bottom(), text);
}

void NvgWindow::drawTextFlag(QPainter &p, int x, int y, int flags, const QString &text, const QColor &color) {
  QFontMetrics fm(p.font());
  QRect rect = fm.boundingRect(text);
  p.setPen(color);
  p.drawText(QRect(x, y, rect.width(), rect.height()), flags, text);
}

void NvgWindow::drawTextColor(QPainter &p, int x, int y, const QString &text, QColor &color) {
  QFontMetrics fm(p.font());
  QRect init_rect = fm.boundingRect(text);
  QRect real_rect = fm.boundingRect(init_rect, 0, text);
  real_rect.moveCenter({x, y - real_rect.height() / 2});
  p.setPen(color);
  p.drawText(real_rect.x(), real_rect.bottom(), text);
}
//-------------------------------------------------------------------------------------------

void NvgWindow::drawHud(QPainter &p) {
  p.setRenderHint(QPainter::Antialiasing);
  p.setPen(Qt::NoPen);
  p.setOpacity(1.);

  // Header gradient
  QLinearGradient bg(0, header_h - (header_h / 2.5), 0, header_h);
  bg.setColorAt(0, QColor::fromRgbF(0, 0, 0, 0.45));
  bg.setColorAt(1, QColor::fromRgbF(0, 0, 0, 0));
  p.fillRect(0, 0, width(), header_h, bg);

  UIState *s = uiState();
  const SubMaster &sm = *(s->sm);

  drawLaneLines(p, s->scene);

  auto leads = sm["modelV2"].getModelV2().getLeadsV3();
  if (leads[0].getProb() > .5) {
    drawLead(p, leads[0], s->scene.lead_vertices[0], s->scene.lead_radar[0]);
  }
  if (leads[1].getProb() > .5 && (std::abs(leads[1].getX()[0] - leads[0].getX()[0]) > 3.0)) {
    drawLead(p, leads[1], s->scene.lead_vertices[1], s->scene.lead_radar[1]);
  }

  drawSpeedLimit(p);
  drawTurnSignals(p);
  drawTpms(p);

  //bottom info
  const auto controls_state = sm["controlsState"].getControlsState();
  const auto live_params = sm["liveParameters"].getLiveParameters();
  const auto car_params = sm["carParams"].getCarParams();
  const auto car_state = sm["carState"].getCarState();
  const char* lateral_state[] = {"Pid", "Indi", "Lqr"};
  int lateralControlState = controls_state.getLateralControlSelect();

  QString infoText;
  infoText.sprintf("[ %s ] SR[%.2f] MDPS[%d] SCC[%d]",
    lateral_state[lateralControlState],
    live_params.getSteerRatio(),
    car_params.getMdpsBus(),
    car_params.getSccBus()
  );

  configFont(p, "Open Sans", 30, "Regular");
  p.setPen(QColor(0xff, 0xff, 0xff, 0xff));
  p.drawText(rect().left() + 20, rect().height() - 15, infoText);

  //upper gps info
  const auto gps_ext = sm["gpsLocationExternal"].getGpsLocationExternal();
  float verticalAccuracy = gps_ext.getVerticalAccuracy();
  float gpsAltitude = gps_ext.getAltitude();
  float gpsAccuracy = gps_ext.getAccuracy();
  int gpsSatelliteCount = s->scene.satelliteCount;

  if(verticalAccuracy == 0 || verticalAccuracy > 100)
    gpsAltitude = 999.9;

  if (gpsAccuracy > 100)
    gpsAccuracy = 99.9;
  else if (gpsAccuracy == 0)
    gpsAccuracy = 0;

  QString infoGps;
  infoGps.sprintf("GPS [ Alt(%.1f) Acc(%.1f) Sat(%d) ]",
    gpsAltitude,
    gpsAccuracy,
    gpsSatelliteCount
  );
  configFont(p, "Open Sans", 30, "Regular");
  p.setPen(QColor(0xff, 0xff, 0xff, 0xff));
  p.drawText(rect().right() - 520, bdr_s * 3, infoGps);
}

static const QColor get_tpms_color(float tpms) {
    if (tpms < 5 || tpms > 60)
        return QColor(255, 255, 255, 200); // white color
    if (tpms < 31)
        return QColor(255, 0, 0, 200); // red color
    return QColor(255, 255, 255, 200);
}

static const QString get_tpms_text(float tpms) {
    if (tpms < 5 || tpms > 60)
        return "";
    char str[32];
    snprintf(str, sizeof(str), "%.0f", round(tpms));
    return QString(str);
}

void NvgWindow::drawTpms(QPainter &p) {
  const SubMaster &sm = *(uiState()->sm);
  auto car_state = sm["carState"].getCarState();
  auto scc_smoother = sm["carControl"].getCarControl().getSccSmoother();

  // tire pressure (right bottom)
  {
    const int w = 66;
    const int h = 146;
    const int x = rect().right() - h - (bdr_s * 2);
    const int y = height() - h - 80;

    auto tpms = car_state.getTpms();
    const float fl = tpms.getFl();
    const float fr = tpms.getFr();
    const float rl = tpms.getRl();
    const float rr = tpms.getRr();

    p.setOpacity(0.8);
    p.drawPixmap(x, y, w, h, tire_pressure_img);

    configFont(p, "Open Sans", 38, "Bold");
    QFontMetrics fm(p.font());
    QRect rcFont = fm.boundingRect("9");

    int center_x = x + 3;
    int center_y = y + h/2;
    const int marginX = (int)(rcFont.width() * 2.7f);
    const int marginY = (int)((h/2 - rcFont.height()) * 0.7f);

    drawTextFlag(p, center_x-marginX, center_y-marginY-rcFont.height(), Qt::AlignRight, get_tpms_text(fl), get_tpms_color(fl));
    drawTextFlag(p, center_x+marginX+8, center_y-marginY-rcFont.height(), Qt::AlignLeft, get_tpms_text(fr), get_tpms_color(fr));
    drawTextFlag(p, center_x-marginX, center_y+marginY, Qt::AlignRight, get_tpms_text(rl), get_tpms_color(rl));
    drawTextFlag(p, center_x+marginX+8, center_y+marginY, Qt::AlignLeft, get_tpms_text(rr), get_tpms_color(rr));
  }
  p.setOpacity(1.);
}

void NvgWindow::drawSpeedLimit(QPainter &p) {
  const SubMaster &sm = *(uiState()->sm);
  auto roadLimitSpeed = sm["roadLimitSpeed"].getRoadLimitSpeed();
  int camLimitSpeed = roadLimitSpeed.getCamLimitSpeed();
  int camLimitSpeedLeftDist = roadLimitSpeed.getCamLimitSpeedLeftDist();
  int sectionLimitSpeed = roadLimitSpeed.getSectionLimitSpeed();
  int sectionLeftDist = roadLimitSpeed.getSectionLeftDist();
  int limit_speed = 0;
  int left_dist = 0;

  if(camLimitSpeed > 0 && camLimitSpeedLeftDist > 0) {
    limit_speed = camLimitSpeed;
    left_dist = camLimitSpeedLeftDist;
  }
  else if(sectionLimitSpeed > 0 && sectionLeftDist > 0) {
    limit_speed = sectionLimitSpeed;
    left_dist = sectionLeftDist;
  }

  if (limit_speed > 10 && left_dist > 0) {
    int radius = 192;
    int x = radius / 2 + (bdr_s * 2) + (radius) + 40;
    int y = 50;

    p.setPen(Qt::NoPen);
    p.setBrush(QBrush(QColor(255, 0, 0, 255)));
    QRect rect = QRect(x, y, radius, radius);
    p.drawEllipse(rect);
    p.setBrush(QBrush(QColor(255, 255, 255, 255)));

    const int tickness = 14;
    rect.adjust(tickness, tickness, -tickness, -tickness);
    p.drawEllipse(rect);

    QString str_limit_speed, str_left_dist;
    str_limit_speed.sprintf("%d", limit_speed);

    if (left_dist >= 1000)
      str_left_dist.sprintf("%.1fkm", left_dist / 1000.f);
    else
      str_left_dist.sprintf("%dm", left_dist);

    configFont(p, "Open Sans", 80, "Bold");
    p.setPen(QColor(0, 0, 0, 230));
    p.drawText(rect, Qt::AlignCenter, str_limit_speed);
    configFont(p, "Open Sans", 60, "Bold");
    rect.translate(0, radius/2 + 45);
    rect.adjust(-30, 0, 30, 0);
    p.setPen(QColor(255, 255, 255, 230));
    p.drawText(rect, Qt::AlignCenter, str_left_dist);
  }
  p.setOpacity(1.);
}

void NvgWindow::drawTurnSignals(QPainter &p) {
  static int blink_index = 0;
  static int blink_wait = 0;
  static double prev_ts = 0.0;

  if (blink_wait > 0) {
    blink_wait--;
    blink_index = 0;
  } else {
    const SubMaster &sm = *(uiState()->sm);
    auto car_state = sm["carState"].getCarState();
    bool left_on = car_state.getLeftBlinker();
    bool right_on = car_state.getRightBlinker();

    const float img_alpha = 0.8f;
    const int fb_w = width() / 2 - 200;
    const int center_x = width() / 2;
    const int w = fb_w / 25;
    const int h = 300;
    const int gap = fb_w / 25;
    const int margin = (int)(fb_w / 3.8f);
    const int base_y = (height() - h) / 2;
    const int draw_count = 8;

    int x = center_x;
    int y = base_y;

    if (left_on) {
      for (int i = 0; i < draw_count; i++) {
        float alpha = img_alpha;
        int d = std::abs(blink_index - i);
        if (d > 0)
          alpha /= d*2;

        p.setOpacity(alpha);
        float factor = (float)draw_count / (i + draw_count);
        p.drawPixmap(x - w - margin, y + (h-h*factor)/2, w*factor, h*factor, turnsignal_l_img);
        x -= gap + w;
      }
    }

    x = center_x;
    if (right_on) {
      for (int i = 0; i < draw_count; i++) {
        float alpha = img_alpha;
        int d = std::abs(blink_index - i);
        if (d > 0)
          alpha /= d*2;

        float factor = (float)draw_count / (i + draw_count);
        p.setOpacity(alpha);
        p.drawPixmap(x + margin, y + (h-h*factor)/2, w*factor, h*factor, turnsignal_r_img);
        x += gap + w;
      }
    }

    if (left_on || right_on) {
      double now = millis_since_boot();
      if (now - prev_ts > 900/UI_FREQ) {
        prev_ts = now;
        blink_index++;
      }
      if (blink_index >= draw_count) {
        blink_index = draw_count - 1;
        blink_wait = UI_FREQ/4;
      }
    } else {
      blink_index = 0;
    }
  }
  p.setOpacity(1.);
}
